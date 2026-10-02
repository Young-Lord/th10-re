// TH10 helpers around the game-manager menu state bodies.
//
//   0x00434a20: splits a text line at its CRLF terminator in place (used
//     three times by RunManagerStateBodyE 0x00433ef0 while parsing the
//     score/ranking text lines).
//   0x00434a80: initializes a VmRecord's position_anim tween block
//     ( adjudicated 2026-10-03: the object at EAX is the 0x3ac VmRecord
//     text-slot entity itself, NOT the game-manager record — both call
//     sites 0x43427b/0x434600 inside RunManagerStateBodyE 0x00433ef0 pass
//     EAX = a text-slot entity and EDX = &vm->base_pos_x, and the fallback
//     branch writes base_pos directly): start triple (from the record's own
//     base_pos_x at +0x334), target triple from the caller, dual time-window
//     words from DAT_00491c14..1c, kind word at +0xb4, flag byte at +0xb8,
//     eased interpolation sub-record at +0xa0.
//   0x00434ba0 / 0x00434bc0: menu flag / disabled-row helpers.
//   0x0043cb80: menu LCG random (state ^ 0x9630, subtract 25939, u16).
#include <string.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

// Eased interpolation sub-record the tween records embed:
//   +0x00 target (-1 sentinel after reset, -999999 while priming)
//   +0x04/+0x08 work words, +0x0c easing-table pointer, +0x10 flag word.
const u32 k_easing_table_default = 0x00476f78U;

extern u32 g_TimeWords[3]; // TH10 DAT_00491c14..00491c1c

u32 ReadTimeWord(u32 index)
{
    return g_TimeWords[index];
}

// TH10 0x00434a80 native registers: EAX = the VmRecord (0x3ac text-slot
// entity) whose position is tweened, EDX = &vm->base_pos_x (+0x334), ECX =
// target triple, stack = kind word, flag byte. The +0x70..+0xb8 block is the
// record's position_anim region (start/end triples, time words, timer).
void BeginManagerVectorTweenBody(u32 *record, const u32 *start,
                                 const u32 *target, u32 kind, u8 flag)
{
    record[45] = kind;                       // +0xb4
    record[34] = ReadTimeWord(0);            // +0x88
    record[35] = ReadTimeWord(1);
    record[36] = ReadTimeWord(2);
    record[37] = ReadTimeWord(0);            // +0x94 (second copy)
    record[38] = ReadTimeWord(1);
    record[39] = ReadTimeWord(2);
    record[46] = flag;                       // +0xb8
    record[28] = start[0];                   // +0x70 start triple
    record[29] = start[1];
    record[30] = start[2];
    record[31] = target[0];                  // +0x7c target triple
    record[32] = target[1];
    record[33] = target[2];
    // Prime the eased interpolation sub-record at +0xa0.
    u32 *interp = record + 40;               // +0xa0
    if ((interp[4] & 1U) == 0U) {
        interp[1] = 0;
        interp[0] = 0xfff0bdc1U;             // -999999
        interp[2] = 0;
        interp[3] = k_easing_table_default;
        interp[4] |= 1U;
    }
    interp[1] = 0;
    interp[2] = 0;
    interp[0] = 0xffffffffU;                 // -1
}

} // namespace

// TH10 0x00434a20. Native ECX = line start, EDI = remaining-bytes counter,
// stack = buffer end. Terminates the line at '\n' or '\r', memmoves the
// remainder (including the terminator) back over the buffer and skips any
// following CR/LF run. Returns the new read cursor.
u8 *TrimLineAtNewlineEcxEdiStackAbi(u8 *line, u32 *remaining, u32 buffer_end)
{
    u8 *cursor = line;
    while (*cursor != 0x0aU) {
        if (*cursor == 0x0dU)
            break;
        if (*remaining == 0U)
            return cursor;
        ++cursor;
        --*remaining;
    }
    if (*remaining == 0U)
        return cursor;

    *cursor = 0;
    const u8 *scan = line;
    const u32 shift = buffer_end - reinterpret_cast<const u32>(line);
    u8 *move_out = line;
    u8 moved;
    do {
        moved = *scan;
        move_out[shift] = *scan;
        ++move_out;
        ++scan;
    } while (moved != 0);
    ++cursor;
    u32 left = *remaining - 1U;
    *remaining = left;
    while (left != 0U && (*cursor == 0x0aU || *cursor == 0x0dU)) {
        ++cursor;
        --left;
        *remaining = left;
    }
    return cursor;
}

// TH10 0x00434a80. Native EAX = the VmRecord whose position is tweened,
// EDX = &vm->base_pos_x, ECX = target triple, stack (ret 8) = kind word +
// flag byte.
void BeginManagerVectorTweenEdxCcxStackAbi(void *vm_record,
                                           const u32 *start_triple,
                                           const u32 *target_triple,
                                           u32 kind, u8 flag)
{
    BeginManagerVectorTweenBody(static_cast<u32 *>(vm_record), start_triple,
                                target_triple, kind, flag);
}

// TH10 0x00434ba0. Native AL = enable flag, ECX = menu record. Toggles bit
// 0x10 of record+0x60 according to the flag shifted into bit 4.
u32 ToggleMenuFlag0x10AlCcxAbi(u8 enable, u32 *record)
{
    const u32 word = record[24];             // +0x60
    const u32 toggled = (word ^ (static_cast<u32>(enable) << 4U)) & 0x10U;
    record[24] = toggled ^ word;
    return toggled;
}

// TH10 0x00434bc0. Native EAX = menu record, EDX = row value. Appends to
// the disabled-row list at +0x90, indexing with the counter at +0xd4.
void AppendDisabledMenuRowEaxEdxAbi(u32 *record, u32 row)
{
    const u32 index = record[53];            // +0xd4
    record[36 + index] = row;                // +0x90 + 4*index
    record[53] = index + 1U;
}

// TH10 0x0043cb80. Native ECX = rng state {u16 state, pad, u32 counter},
// EDI = modulus. Advances the state twice per call (counter += 2) and
// returns the combined 32-bit value modulo the modulus; 0 when modulus 0.
u32 NextMenuRandomEcxEdiAbi(void *rng_state, u32 modulus)
{
    if (modulus == 0U)
        return 0;
    u16 *state16 = static_cast<u16 *>(rng_state);
    u32 *counter = reinterpret_cast<u32 *>(state16 + 2);

    const u16 mixed0 = static_cast<u16>(
        4U * ((*state16 ^ 0x9630U) - 25939U) +
        ((((*state16 ^ 0x9630U) - 25939U) & 0xffffU) >> 14));
    *state16 = mixed0;
    const u16 mixed1 = static_cast<u16>(
        4U * ((mixed0 ^ 0x9630U) - 25939U) +
        (((mixed0 ^ 0x9630U) - 25939U & 0xffffU) >> 14));
    *counter += 2U;
    *state16 = mixed1;
    return (static_cast<u32>(mixed1) |
            (static_cast<u32>(mixed0) << 16)) % modulus;
}

} // namespace th10
