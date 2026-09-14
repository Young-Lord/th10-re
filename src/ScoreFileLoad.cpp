// Score-save state loading (TH10 0x0042af20 / 0x0042ae60 / 0x0042b030 /
// 0x0042af90 / 0x0042afe0) plus the 0x0044b0d0 unscramble codec that the
// loader shares with the packed-resource pipeline. This is the read side of
// src/ScoreSave.cpp (writer 0x0042b1e0).
#include "ScoreFileLoad.hpp"

#include <stdlib.h>
#include <string.h>

namespace th10 {

namespace {

// TH10 0x00452493 (operator new) / 0x004524a1 (operator delete) /
// 0x00452706 (CRT malloc) / 0x00452422 (free).
extern void *AllocateMainChainObject(u32 bytes);
extern void FreeMainChainObject(void *object);
extern void *AllocateResourceBuffer(u32 bytes);
extern void ReleaseResourceBuffer(void *pointer);

// TH10 0x0044b360 (body in MainChainFileProbe.cpp): whole-file load with an
// optional size out-param and a filesystem mode flag.
extern void *LoadMainChainFile(const char *path, u32 *file_size,
                               i32 filesystem_mode);

// TH10 0x00435dc0: LZSS-style decoder (MSB-first flags, 13-bit absolute
// history offsets, 4-bit match length, DAT_0048f868 ring starting at
// cursor one).
extern u8 *DecompressPackedBytes(const u8 *encoded, u32 encoded_size,
                                 u8 *output, u32 output_size);

extern void *g_ScoreSaveState; // TH10 DAT_0047783c

u32 LoadU32From(const void *address)
{
    return *static_cast<const u32 *>(address);
}

void StoreU32To(void *address, u32 value)
{
    *static_cast<u32 *>(address) = value;
}

void StoreU16To(void *address, u16 value)
{
    *static_cast<u16 *>(address) = value;
}

// movsx of a pattern byte (the 0x4743c0 condition bytes are signed).
i32 SignExtendByte(u8 value)
{
    return static_cast<i32>(static_cast<signed char>(value));
}

const u32 k_stage_slot_stride = 0x437cU;
const u32 k_stage_slot_count = 7U;
const u32 k_stage_payload_checksum_words = 0xb3eU;  // 6-byte groups
const u32 k_clear_checksum_words = 0x110U;          // 4-byte groups
const u32 k_clear_region_size = 0x448U;
const u32 k_clear_region_offset = 0x1d86cU;
const u32 k_score_header_size = 0x18U;
const u32 k_score_state_size = 0x1dcb4U;

// The 0x4918b0 LCG pair (see EnemyDeathEffects.cpp for the full-draw
// variant): a u16 state at +0 whose dword read also drags in the u16 at
// +2, plus a u32 step counter at +4. 0x0042add0 uses one raw draw per
// output word exactly as written below.
extern u16 g_ScorePrngState[4]; // TH10 DAT_004918b0 (+4 counter in [2]/[3])

// TH10 0x42add0's LCG step: full = (u32{state[0],state[1]} ^ 0x9630) -
// 0x6553; the new state word is (full << 2) + (low16(full) >> 14) and the
// counter dword advances by one.
u16 ScorePrngStep()
{
    u32 full = static_cast<u32>(g_ScorePrngState[0])
             | (static_cast<u32>(g_ScorePrngState[1]) << 16);
    full = (full ^ 0x9630U) - 0x6553U;
    const u32 rotated = (full << 2) + ((full & 0xffffU) >> 14);
    g_ScorePrngState[0] = static_cast<u16>(rotated);
    const u32 counter = static_cast<u32>(g_ScorePrngState[2])
        | (static_cast<u32>(g_ScorePrngState[3]) << 16);
    const u32 next = counter + 1U;
    g_ScorePrngState[2] = static_cast<u16>(next);
    g_ScorePrngState[3] = static_cast<u16>(next >> 16);
    return static_cast<u16>(rotated);
}

// TH10 0x0042acb0 (`ret 4`): zero the 0x437c-byte stage-record slot, write
// the 0x5243 magic, size 0x437c, thirty default 0x18-byte score entries
// (five groups of six, scores 1000000 down to 500000, stage byte 1,
// "--------" name from 0x46e858) and the 22-entry default spell-card table
// at +0x628 whose condition words come from the 0x4743c0 pattern table.
void InitializeScoreStageSlotStackAbi(void *slot)
{
    u8 *const bytes = static_cast<u8 *>(slot);
    for (u32 i = 0; i < k_stage_slot_stride; ++i)
        bytes[i] = 0U;

    StoreU16To(bytes, 0x5243U);          // "CR"
    StoreU16To(bytes + 2U, 0U);
    StoreU32To(bytes + 8U, 0x437cU);

    // Thirty 0x18-byte score entries starting at +0x14. The native keeps
    // one cursor: each of the five groups re-seeds the score at 1000000
    // and writes six entries 100000 apart, the score sitting in the dword
    // immediately before its 0x18-byte record (entry 0's at +0x10).
    u32 offset = 0x10U;
    for (u32 group = 0; group < 5U; ++group) {
        u32 score = 0xf4240U * 10U; // 1000000
        for (u32 entry = 0; entry < 6U; ++entry) {
            StoreU32To(bytes + offset, score);
            bytes[offset + 4U] = 1U;                       // stage byte
            bytes[offset + 5U] = 0U;
            bytes[offset + 6U] = 0x2dU;                    // "--------"
            bytes[offset + 7U] = 0x2dU;
            bytes[offset + 8U] = 0x2dU;
            bytes[offset + 9U] = 0x2dU;
            bytes[offset + 10U] = 0x2dU;
            bytes[offset + 11U] = 0x2dU;
            bytes[offset + 12U] = 0x2dU;
            bytes[offset + 13U] = 0x2dU;
            bytes[offset + 14U] = 0U;                      // name NUL
            StoreU32To(bytes + offset + 16U, 0U);
            score -= 0x186a0U;                             // -100000
            offset += 0x18U;
        }
    }

    // 22 spell-card default entries of 0x2d0 bytes at +0x628. Three
    // indices step through the repeating 02 03 00 01 pattern at
    // 0x4743c0 in lockstep (bases 1, 3 and 0, stride 5); the second
    // index's byte read carries the native's constant 0x4743bf offset.
    u32 entry = 0x628U;
    u32 first_index = 1U;
    u32 second_index = 3U;
    u32 third_index = 0U;
    while (first_index < 0x6fU) {
        StoreU32To(bytes + entry - 4U, 0U);
        const u8 first_byte = *reinterpret_cast<const u8 *>(
            0x4743c0U + first_index);
        const u8 second_byte = *reinterpret_cast<const u8 *>(
            0x4743bfU + second_index);
        const u8 third_byte = *reinterpret_cast<const u8 *>(
            0x4743c0U + second_index);
        const u8 fourth_byte = *reinterpret_cast<const u8 *>(
            0x4743c4U + third_index);
        StoreU32To(bytes + entry, SignExtendByte(first_byte));
        StoreU32To(bytes + entry + 0x8cU, first_index);
        StoreU32To(bytes + entry + 0x90U, SignExtendByte(first_byte));
        StoreU32To(bytes + entry + 0x11cU, second_index - 1U);
        StoreU32To(bytes + entry + 0x120U, SignExtendByte(second_byte));
        StoreU32To(bytes + entry + 0x1acU, second_index);
        StoreU32To(bytes + entry + 0x1b0U, SignExtendByte(third_byte));
        StoreU32To(bytes + entry + 0x23cU, third_index + 4U);
        StoreU32To(bytes + entry + 0x240U, SignExtendByte(fourth_byte));
        first_index += 5U;
        second_index += 5U;
        third_index += 5U;
        entry += 0x2d0U;
    }
}

// TH10 0x0042add0 (native EDX): zero the 0x448-byte clear-data region and
// write the 0x5453 magic, size 0x448 and the blank 8-space name from
// 0x46e354, then 0x200 scrambled 16-bit spell-card phase words from +0x46
// drawn from the 0x4918b0 LCG.
void InitializeClearDataRegionEdxAbi(void *region)
{
    u8 *const bytes = static_cast<u8 *>(region);
    for (u32 i = 0; i < k_clear_region_size; ++i)
        bytes[i] = 0U;

    StoreU16To(bytes, 0x5453U);          // "ST"
    StoreU32To(bytes + 8U, 0x448U);
    // 9-byte blank name "        \0" from 0x46e354 into the name slot
    // at +0xc (the remaining slots are already zero).
    for (u32 i = 0; i < 8U; ++i)
        bytes[0xcU + i] = 0x20U;
    bytes[0x14U] = 0U;

    u32 offset = 0x46U;
    for (u32 i = 0; i < 0x200U; ++i) {
        StoreU16To(bytes + offset, ScorePrngStep());
        offset += 2U;
    }
}


// Install a fresh blank 0x18-byte "TH10" header record at [state] after
// freeing the previous one (native 0x42b183 failure tail / 0x42b198 null
// tail).
void ResetScoreHeaderRecord(u8 *state)
{
    void *const previous =
        reinterpret_cast<void *>(LoadU32From(state));
    if (previous != 0) {
        ReleaseResourceBuffer(previous);
    }
    StoreU32To(state, 0U);

    u8 *const header =
        static_cast<u8 *>(AllocateResourceBuffer(k_score_header_size));
    StoreU32To(state, reinterpret_cast<u32>(header));
    for (u32 i = 0; i < k_score_header_size; i += 4U) {
        StoreU32To(header + i, 0U);
    }
    StoreU32To(header, 0x30314854U); // "TH10"
    header[8] = 3U;                  // word at +8 = 3 (native 16-bit store)
    StoreU32To(header + 0xcU, 0x100U);
}

} // namespace

// TH10 0x0044b0d0 semantic body. Reads the scratch copy linearly and writes
// the buffer's odd offsets descending then even offsets descending — the
// exact inverse permutation and key stream of 0x0044b220.
void UnscramblePackedImageUserpurgeAbi(u8 initial_key, void *buffer,
                                       i32 size, u8 key_step, i32 block_size,
                                       i32 size_again)
{
    // Copy span: the smaller of `size_again` and `size`.
    u32 copy_size = static_cast<u32>(size_again);
    if (size_again > size) {
        copy_size = static_cast<u32>(size);
    }

    // Trailing partial block is skipped when it is at least a quarter of a
    // block; the odd trailing byte is always skipped.
    const i32 remainder = size % block_size;
    const i32 skipped_tail = (remainder >= block_size / 4) ? 0 : remainder;
    i32 span = size - (skipped_tail + (size & 1));

    u8 *const scratch = static_cast<u8 *>(malloc(copy_size));
    if (scratch == 0) {
        return;
    }
    memcpy(scratch, buffer, copy_size);

    u8 *out = static_cast<u8 *>(buffer);
    const u8 *src = scratch;
    u8 key = initial_key;
    i32 block_local = block_size;
    i32 remaining_again = size_again;
    while (span > 0 && remaining_again > 0) {
        i32 block = block_local;
        if (span < block_local) {
            block_local = span; // native persists the shrink
            block = span;
        }
        u8 *const block_end = out + static_cast<u32>(block);

        // Odd offsets of the block, descending, then even offsets,
        // descending; the running key advances once per output byte and
        // the scratch source walks linearly across blocks.
        u8 *write = block_end - 1;
        for (i32 i = (block + 1) / 2; i > 0; --i) {
            *write = static_cast<u8>(key ^ *src);
            ++src;
            write -= 2;
            key = static_cast<u8>(key + key_step);
        }
        write = block_end - 2;
        for (i32 i = block / 2; i > 0; --i) {
            *write = static_cast<u8>(key ^ *src);
            ++src;
            write -= 2;
            key = static_cast<u8>(key + key_step);
        }

        out += static_cast<u32>(block);
        span -= block;
        remaining_again -= block;
    }

    free(scratch);
}

// TH10 0x0042b030 semantic body.
i32 LoadScoreRecordFileEbx(void *save_state)
{
    u8 *const state = static_cast<u8 *>(save_state);
    if (LoadU32From(state) == 0U) {
        ResetScoreHeaderRecord(state);
        return 0;
    }

    const u8 *const header =
        reinterpret_cast<const u8 *>(LoadU32From(state));
    if (LoadU32From(header) != 0x30314854U              // "TH10"
        || (LoadU32From(header + 0x8U) & 0xffffU) != 3U) {
        ResetScoreHeaderRecord(state);
        return 0;
    }

    // Unscramble the packed body at header+0x18 in place (the writer's
    // 0xac/0x35-over-0x10 pass applied in reverse).
    const u32 packed_size = LoadU32From(header + 0x10U);
    UnscramblePackedImageUserpurgeAbi(
        0xacU, const_cast<u8 *>(header + k_score_header_size),
        static_cast<i32>(packed_size), 0x35U, 0x10,
        static_cast<i32>(packed_size));

    // Decompress into a deliberately 4x-oversized scratch buffer (the
    // native shl 2 quirk) published at state+4.
    const u32 unpacked_size = LoadU32From(header + 0x14U);
    u8 *const body =
        static_cast<u8 *>(AllocateResourceBuffer(unpacked_size * 4U));
    StoreU32To(state + 4U, reinterpret_cast<u32>(body));
    DecompressPackedBytes(header + k_score_header_size, packed_size, body,
                          unpacked_size);

    i32 remaining = static_cast<i32>(unpacked_size);
    if (remaining <= 0) {
        return 0; // no fresh-record fallback on this path (quirk)
    }

    u8 *record = body;
    while (remaining > 0) {
        const u16 magic = static_cast<u16>(
            LoadU32From(record) & 0xffffU);
        bool recognized = (magic == 0x5243U || magic == 0x5453U);
        if (!recognized) {
            // Unknown magic: the whole load fails and a blank header is
            // installed (the state+4 scratch is intentionally leaked).
            ResetScoreHeaderRecord(state);
            return 0;
        }

        if ((LoadU32From(record + 2U) & 0xffffU) == 0U) { // word at +2
            if (magic == 0x5243U) { // "CR" stage record
                // Byte sum of record+8 .. record+0x437b against +4.
                u32 sum = 0U;
                for (u32 i = 0; i < k_stage_payload_checksum_words; ++i) {
                    const u8 *const group = record + 0x8U + i * 6U;
                    for (u32 j = 0; j < 6U; ++j) {
                        sum += group[j];
                    }
                }
                if (sum == LoadU32From(record + 4U)
                    && LoadU32From(record + 8U) == k_stage_slot_stride) {
                    // Unchecked stage index straight from the record.
                    const u32 index = LoadU32From(record + 0xcU);
                    u8 *const dest =
                        state + 8U + index * k_stage_slot_stride;
                    for (u32 i = 0; i < k_stage_slot_stride; i += 4U) {
                        StoreU32To(dest + i, LoadU32From(record + i));
                    }
                }
            } else { // "ST" clear-data record
                // Byte sum of record+8 .. record+0x447 against +4.
                u32 sum = 0U;
                for (u32 i = 0; i < k_clear_checksum_words; ++i) {
                    const u8 *const group = record + 0x8U + i * 4U;
                    for (u32 j = 0; j < 4U; ++j) {
                        sum += group[j];
                    }
                }
                if (sum == LoadU32From(record + 4U)
                    && LoadU32From(record + 8U) == k_clear_region_size) {
                    u8 *const dest = state + k_clear_region_offset;
                    for (u32 i = 0; i < k_clear_region_size; i += 4U) {
                        StoreU32To(dest + i, LoadU32From(record + i));
                    }
                }
            }
        }

        // Advance by the record's declared size even when its checks
        // failed; a negative remainder is treated as failure.
        const u32 declared_size = LoadU32From(record + 8U);
        remaining -= static_cast<i32>(declared_size);
        if (remaining < 0) {
            ResetScoreHeaderRecord(state);
            return 0;
        }
        record += declared_size;
    }
    return 0;
}

// TH10 0x0042ae60 semantic body.
void *InitializeScoreSaveStateEaxAbi(void *state)
{
    u8 *const bytes = static_cast<u8 *>(state);
    for (u32 i = 0; i < k_score_state_size; ++i) {
        bytes[i] = 0U;
    }

    // Whole-file load of "scoreth10.dat" (mode 1); the native points the
    // size out-param at a caller stack slot and ignores it.
    u32 unused_size = 0U;
    StoreU32To(bytes, reinterpret_cast<u32>(
                          LoadMainChainFile("scoreth10.dat", &unused_size,
                                            1)));

    InitializeClearDataRegionEdxAbi(bytes + k_clear_region_offset);
    for (u32 i = 0; i < k_stage_slot_count; ++i) {
        InitializeScoreStageSlotStackAbi(bytes + 8U
                                         + i * k_stage_slot_stride);
    }

    (void)LoadScoreRecordFileEbx(bytes);
    return bytes;
}

// TH10 0x0042af20 semantic body (SEH frame elided).
void *CreateScoreSaveState()
{
    void *state = AllocateMainChainObject(k_score_state_size);
    if (state == 0) {
        g_ScoreSaveState = 0;
        return 0;
    }
    state = InitializeScoreSaveStateEaxAbi(state);
    g_ScoreSaveState = state;
    return state;
}

// TH10 0x0042af90 semantic body.
void DestroyScoreSaveStateGlobal()
{
    u8 *const state = static_cast<u8 *>(g_ScoreSaveState);
    if (state != 0) {
        void *const header =
            reinterpret_cast<void *>(LoadU32From(state));
        if (header != 0) {
            ReleaseResourceBuffer(header);
            StoreU32To(state, 0U);
        }
        void *const body =
            reinterpret_cast<void *>(LoadU32From(state + 4U));
        if (body != 0) {
            ReleaseResourceBuffer(body);
            StoreU32To(state + 4U, 0U);
        }
        FreeMainChainObject(state);
    }
    g_ScoreSaveState = 0;
}

// TH10 0x0042afe0 semantic body (`ret 4`).
void *ReleaseScoreSaveStateEsiStackAbi(void *state, i32 free_flag)
{
    u8 *const bytes = static_cast<u8 *>(state);
    void *const header = reinterpret_cast<void *>(LoadU32From(bytes));
    if (header != 0) {
        ReleaseResourceBuffer(header);
        StoreU32To(bytes, 0U);
    }
    void *const body = reinterpret_cast<void *>(LoadU32From(bytes + 4U));
    if (body != 0) {
        ReleaseResourceBuffer(body);
        StoreU32To(bytes + 4U, 0U);
    }
    if ((free_flag & 1) != 0) {
        FreeMainChainObject(bytes);
    }
    return bytes;
}

} // namespace th10
