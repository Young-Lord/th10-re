// TH10 0x0043b110 — ending script event interpreter.
//
// The ending music player (ending.cpp cluster; see the assertion strings
// at 0x46cfd8) schedules 0x20-byte event blocks (the caller 0x0043af30
// runs every block whose time window covers the current playback time).
// Each block is a standard-MIDI-file event stream:
//
//   block+0x00 active flag (cleared by the 0xff 0x2f end-of-track meta)
//   block+0x04 next-event time (the delta of each event is added here)
//   block+0x0c running status byte
//   block+0x14 pointer to the next stream byte
//   block+0x18 / block+0x1c saved (cursor, time) marks
//
// The player context (0x0043a750 constructor) holds:
//   +0x14[32] ring of 0x40-byte MIDIHDR slots for SysEx messages
//   +0x94     round-robin slot index
//   +0x118    event-block count, +0x138 block array (not used here)
//   +0x120    ticks-per-quarter scalar used by the tempo math
//   +0x124    tempo accumulator (re-seeded from the 0x51 meta bytes)
//   +0x128:+0x12c 64-bit elapsed-time delta, +0x130:+0x134 accumulated
//   +0x13c    MIDI out device handle
//   +0x154    16 per-channel records of 0x17 bytes: a key bitmap (one bit
//             per pressed note) at +0x00.. and, at +0x10..+0x16, the
//             program number and the controller bytes (0x00/0x0a/0x57/
//             0x59/0x07 raw and faded)
//   +0x2c4    note offset byte added to every key-bit index
//   +0x2c8    fade scale multiplied into the 0x07 controller value
//
// Voice messages are re-emitted through midiOutShortMsg in the common
// epilogue (status | data1<<8 | data2<<16), always for status < 0xf0 when
// the device is open — including running-status bytes and status 0 when
// no running status exists yet (native quirk).
#include <cmath>
#include <stdlib.h>
#include <string.h>

#include "EndingMidiSequencer.hpp"
#include "Th10Types.hpp"

namespace th10 {

namespace {

// 64-bit helpers for the tempo arithmetic (Th10Types.hpp only carries the
// 8/16/32-bit widths).
typedef long long i64;
typedef unsigned long long u64;

// WINMM imports used by the native body (IAT slots 0x466284 / 0x466298 /
// 0x46629c / 0x4662a0).
extern "C" {
typedef u32 MMRESULT;
MMRESULT midiOutPrepareHeader(u32 device, void *header, u32 size);
MMRESULT midiOutUnprepareHeader(u32 device, void *header, u32 size);
MMRESULT midiOutLongMsg(u32 device, void *header, u32 size);
MMRESULT midiOutShortMsg(u32 device, u32 message);
}

// TH10 0x00463b2c (_ftol2): x87 conversion, round half away from zero.
i32 FloatToI32(float value)
{
    return value >= 0.0f
        ? static_cast<i32>(std::floor(static_cast<double>(value) + 0.5))
        : static_cast<i32>(std::ceil(static_cast<double>(value) - 0.5));
}

// Native 0x40-byte MIDIHDR subset used here.
const u32 kMidiHdrSize = 0x40U;

inline u32 &Ctx32(u8 *ctx, u32 offset)
{
    return *reinterpret_cast<u32 *>(ctx + offset);
}

inline u8 *BlockCursorSlot(u8 *block)
{
    return block + 0x14U;
}

inline u8 *CursorOf(u8 *block)
{
    return *reinterpret_cast<u8 **>(BlockCursorSlot(block));
}

inline void SetCursorOf(u8 *block, u8 *cursor)
{
    *reinterpret_cast<u8 **>(BlockCursorSlot(block)) = cursor;
}

// TH10 0x0043a730. ESI = &block->0x14 (the cursor slot): 7-bit big-endian
// variable-length quantity with the 0x80 continuation bit; the slot is
// updated per byte exactly like the native helper.
i32 ReadStreamVarintEsiAbi(u8 *block)
{
    u8 *cursor = CursorOf(block);
    u32 value = 0;
    u8 byte;
    do {
        byte = *cursor;
        ++cursor;
        SetCursorOf(block, cursor);
        value = (value << 7) + (byte & 0x7FU);
    } while ((byte & 0x80U) != 0U);
    return static_cast<i32>(value);
}

// TH10 0x0043aeb0. EDX = context, ESI = header: find the header in the
// 32-slot ring, unprepare it and release the data pointer and the header.
void ReleaseEndingMidiHeaderEdxAbi(u8 *ctx, u8 *header)
{
    for (u32 slot = 0; slot < 0x20U; ++slot) {
        if (*reinterpret_cast<u8 *const *>(ctx + 0x14U + 4U * slot)
            == header) {
            *reinterpret_cast<u8 **>(ctx + 0x14U + 4U * slot) = 0;
            midiOutUnprepareHeader(Ctx32(ctx, 0x13CU), header,
                                   kMidiHdrSize);
            u8 *data = *reinterpret_cast<u8 **>(header);
            if (data != 0) {
                free(data);
                *reinterpret_cast<u8 **>(header) = 0;
            }
            free(header);
            return;
        }
    }
}

// Set or clear the pressed-key bit of one channel record. The bit index
// is (note + ctx+0x2c4) with u8 wrap; the byte index is unchecked and can
// run past the 0x10-bitmap bytes into the following record fields —
// native quirk preserved.
void WriteKeyBit(u8 *ctx, u8 channel, u8 note, bool set)
{
    const u16 shifted = static_cast<u16>(
        static_cast<u8>(note + ctx[0x2C4U]));
    u8 *cell = ctx + 0x154U + 0x17U * channel + (shifted >> 3);
    const u8 bit = static_cast<u8>(1U << (shifted & 7U));
    if (set) {
        *cell |= bit;
    } else {
        *cell &= static_cast<u8>(~bit);
    }
}

// TH10 0x0043b502 / 0x0043b56e mark / restore: shift each event block's
// event block's (cursor, time) into (or out of) its saved slots and
// snapshot (or restore) the tempo/elapsed state at +0x124..+0x134.
void SavePlaybackMark(u8 *ctx, bool save)
{
    u32 count = Ctx32(ctx, 0x118U);
    u8 *blocks = *reinterpret_cast<u8 **>(ctx + 0x138U);
    for (u32 i = 0; i < count; ++i) {
        u8 *block = blocks + 0x20U * i;
        if (save) {
            *reinterpret_cast<u32 *>(block + 0x18U) =
                *reinterpret_cast<u32 *>(block + 0x14U);
            *reinterpret_cast<u32 *>(block + 0x1CU) =
                *reinterpret_cast<u32 *>(block + 0x04U);
        } else {
            *reinterpret_cast<u32 *>(block + 0x14U) =
                *reinterpret_cast<u32 *>(block + 0x18U);
            *reinterpret_cast<u32 *>(block + 0x04U) =
                *reinterpret_cast<u32 *>(block + 0x1CU);
        }
    }
    const u32 src = save ? 0x124U : 0x2ECU;
    const u32 dst = save ? 0x2ECU : 0x124U;
    for (u32 word = 0; word < 5U; ++word) {
        Ctx32(ctx, dst + 4U * word) = Ctx32(ctx, src + 4U * word);
    }
}

} // namespace

// TH10 0x0043b110 (native `retn 8` stdcall body).
void InterpretEndingMidiEventBlockStackAbi(void *context, void *block)
{
    u8 *const ctx = static_cast<u8 *>(context);
    u8 *const blk = static_cast<u8 *>(block);

    u8 *cursor = CursorOf(blk);
    u8 status = *cursor;
    if (status >= 0x80U) {
        SetCursorOf(blk, cursor + 1);
    } else {
        // Running status: the byte at the cursor is a data byte; reuse the
        // block's stored status without advancing.
        status = blk[0x0CU];
    }
    u8 data1 = 0;
    u8 data2 = 0;
    const u8 channel = static_cast<u8>(status & 0xFU);

    const u32 kind = static_cast<u32>((status & 0xF0U) - 0x80U);
    if (kind <= 0x70U) {
        if (kind == 0x70U) { // --------------------- 0xf0 SysEx / 0xff meta
            cursor = CursorOf(blk);
            if (status == 0xF0U) {
                // SysEx: build a MIDIHDR in the next slot of the ring and
                // submit it through midiOutPrepareHeader + midiOutLongMsg.
                u32 slot_index = Ctx32(ctx, 0x94U);
                u8 *previous =
                    *reinterpret_cast<u8 **>(ctx + 0x14U + 4U * slot_index);
                if (previous != 0) {
                    ReleaseEndingMidiHeaderEdxAbi(ctx, previous);
                }
                u8 *const header =
                    static_cast<u8 *>(malloc(kMidiHdrSize));
                *reinterpret_cast<u8 **>(ctx + 0x14U + 4U * slot_index)
                    = header;
                const i32 length = ReadStreamVarintEsiAbi(blk);
                memset(header, 0, kMidiHdrSize);
                u8 *const payload =
                    static_cast<u8 *>(malloc(static_cast<u32>(length) + 1U));
                *reinterpret_cast<u8 **>(header) = payload;
                payload[0] = 0xF0U;
                *reinterpret_cast<u32 *>(header + 0x10U) = 0; // dwFlags
                *reinterpret_cast<u32 *>(header + 0x04U) =
                    static_cast<u32>(length) + 1U; // dwBufferLength
                for (i32 i = 0; i < length; ++i) {
                    payload[1 + i] = *CursorOf(blk);
                    SetCursorOf(blk, CursorOf(blk) + 1);
                }
                const u32 device = Ctx32(ctx, 0x13CU);
                if (device != 0U) {
                    bool discard = false;
                    if (midiOutPrepareHeader(device, header, kMidiHdrSize)
                        != 0U) {
                        discard = true;
                    } else if (midiOutLongMsg(device, header, kMidiHdrSize)
                               != 0U) {
                        discard = true;
                    }
                    if (discard) {
                        // Failed submission releases the chunk immediately.
                        u8 *data = *reinterpret_cast<u8 **>(header);
                        if (data != 0) {
                            free(data);
                            *reinterpret_cast<u8 **>(header) = 0;
                        }
                        free(header);
                        *reinterpret_cast<u8 **>(ctx + 0x14U
                                                 + 4U * slot_index) = 0;
                    }
                }
                slot_index = static_cast<u32>(
                    (static_cast<i32>(slot_index) + 1) % 0x20);
                Ctx32(ctx, 0x94U) = slot_index;
            } else if (status == 0xFFU) {
                // Meta event: 0xff <type> <length> [bytes]. Statuses
                // 0xf1..0xfe read no operands and only reach the epilogue.
                const u8 meta_type = *CursorOf(blk);
                SetCursorOf(blk, CursorOf(blk) + 1);
                const i32 length = ReadStreamVarintEsiAbi(blk);
                if (meta_type == 0x2FU) {
                    // End of track: deactivate the block and stop.
                    *reinterpret_cast<u32 *>(blk) = 0;
                    return;
                }
                if (meta_type == 0x51U) {
                    // Set-tempo style accumulator: fold the elapsed 64-bit
                    // delta scaled by 1000 and the +0x120 scalar into the
                    // accumulated playback time, then re-seed the tempo
                    // accumulator from `length` stream bytes (multiplier
                    // 0x101 instead of 0x100 — native quirk kept).
                    const i64 lhs = static_cast<i64>(
                        static_cast<i32>(Ctx32(ctx, 0x120U)));
                    const i64 delta =
                        static_cast<i64>(
                            (static_cast<u64>(Ctx32(ctx, 0x128U)))
                            | (static_cast<u64>(Ctx32(ctx, 0x12CU)) << 32));
                    i64 scaled = lhs * delta * 1000;
                    scaled /= static_cast<i64>(
                        static_cast<i32>(Ctx32(ctx, 0x124U)));
                    u64 accumulated =
                        (static_cast<u64>(Ctx32(ctx, 0x134U)) << 32)
                        | static_cast<u64>(Ctx32(ctx, 0x130U));
                    accumulated += static_cast<u64>(scaled);
                    Ctx32(ctx, 0x130U) =
                        static_cast<u32>(accumulated & 0xFFFFFFFFU);
                    Ctx32(ctx, 0x134U) =
                        static_cast<u32>(accumulated >> 32);
                    Ctx32(ctx, 0x128U) = 0;
                    Ctx32(ctx, 0x12CU) = 0;
                    Ctx32(ctx, 0x124U) = 0;
                    if (length > 0) {
                        u32 seeded = 0;
                        for (i32 i = 0; i < length; ++i) {
                            seeded = seeded * 0x101U + *CursorOf(blk);
                            SetCursorOf(blk, CursorOf(blk) + 1);
                        }
                        Ctx32(ctx, 0x124U) = seeded;
                    }
                } else {
                    // Unknown meta: skip its data bytes.
                    SetCursorOf(blk, CursorOf(blk)
                                         + static_cast<u32>(length));
                }
            }
        } else if (kind == 0x00U || kind == 0x10U || kind == 0x20U
                   || kind == 0x30U || kind == 0x60U) {
            // Two-data-byte voice messages (0x80/0x90/0xa0/0xb0/0xe0).
            cursor = CursorOf(blk);
            data1 = *cursor;
            SetCursorOf(blk, cursor + 1);
            cursor = CursorOf(blk);
            data2 = *cursor;
            SetCursorOf(blk, cursor + 1);
        } else if (kind == 0x40U || kind == 0x50U) {
            // One-data-byte voice messages (0xc0/0xd0).
            cursor = CursorOf(blk);
            data1 = *cursor;
            SetCursorOf(blk, cursor + 1);
        }
    }

    // Per-channel state update (table at 0x43b6c0/0x43b6d4 over
    // (status & 0xf0) - 0x80; skipped above the 0x40 entry).
    if (kind <= 0x40U) {
        u8 *const row = ctx + 0x154U + 0x17U * channel;
        switch (kind) {
        case 0x00U: // note-off (0x80) and pitch bend (0xe0): clear the key
            WriteKeyBit(ctx, channel, data1, false);
            break;
        case 0x10U: // note-on (0x90): velocity 0 is a note-off
            WriteKeyBit(ctx, channel, data1, data2 != 0);
            break;
        case 0x30U: // control change (0xb0)
            switch (data1) {
            case 0x00:
                row[0x11U] = data2;
                break;
            case 0x02:
                SavePlaybackMark(ctx, true);
                break;
            case 0x04:
                SavePlaybackMark(ctx, false);
                break;
            case 0x07: {
                row[0x15U] = data2;
                i32 alpha = FloatToI32(
                    static_cast<float>(static_cast<i32>(data2))
                    * *reinterpret_cast<float *>(ctx + 0x2C8U));
                if (alpha < 0) {
                    alpha = 0;
                } else if (alpha > 0x7F) {
                    alpha = 0x7F;
                }
                row[0x16U] = static_cast<u8>(alpha);
                data2 = static_cast<u8>(alpha);
                break;
            }
            case 0x0A:
                row[0x12U] = data2;
                break;
            case 0x57:
                row[0x13U] = data2;
                break;
            case 0x59:
                row[0x14U] = data2;
                break;
            default:
                break;
            }
            break;
        case 0x40U: // program change (0xc0)
            row[0x10U] = data1;
            break;
        default: // 0xa0 (0x20): no per-channel update
            break;
        }
    }

    // Common epilogue: store the running status, forward voice messages
    // through midiOutShortMsg (0x466298) and add the delta-time varint to
    // the block's next-event time.
    blk[0x0CU] = status;
    const u32 device = Ctx32(ctx, 0x13CU);
    if (status < 0xF0U && device != 0U) {
        const u32 message = static_cast<u32>(status)
                            | (static_cast<u32>(data1) << 8)
                            | (static_cast<u32>(data2) << 16);
        midiOutShortMsg(device, message);
    }
    u32 delta = 0;
    u8 byte;
    do {
        byte = *CursorOf(blk);
        SetCursorOf(blk, CursorOf(blk) + 1);
        delta = (delta << 7) + (byte & 0x7FU);
    } while ((byte & 0x80U) != 0U);
    *reinterpret_cast<u32 *>(blk + 0x04U) += delta;
}

} // namespace th10
