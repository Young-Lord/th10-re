#pragma once

#include "Th10Platform.hpp"
#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00415E90 EBP frame (the native one-argument object). The executor is
// driven once per frame with a timer A block at +0x18 that keeps the stream
// cursor in sync with the stream's own u16 frame stamps.
//
// Stream record layout (same family as the timeline stream): u16 frame stamp,
// u8 opcode, u8 payload length, payload. Records advance by 4 + payload_len.
struct ResultScreenScriptState {
    i32 selector;              // +0x00 (>= 1 gates two branches)
    u32 reserved_04[5];        // +0x04..0x17 (unused by this executor)
    u32 timer_a[5];            // +0x18: prev, current, f32 view, rate ptr, flags
    u32 timer_b[5];            // +0x2C: same layout; text/BGM wait timer
    u32 handle_a;              // +0x40 (expire/state-1 family)
    u32 handle_b;              // +0x44
    u32 handle_c;              // +0x48
    u32 handle_d;              // +0x4C (text line A)
    u32 handle_e;              // +0x50 (text line B)
    u32 handle_f;              // +0x54
    u32 reserved_58;           // +0x58
    u8 *stream;                // +0x5C cursor
    float positions[6][3];     // +0x60..0x77 (native indexes (select+8)*12,
                               // i.e. select 0 -> slot 0 and select 1 -> slot 3)
    i32 repeat_counter;        // +0x78 (decremented each call, set by opcode 11)
    u32 flags;                 // +0x7C (bit0 re-arms the frame seeding block)
    i32 sub_counter;           // +0x80 (two-line text latch)
    i32 select_index;          // +0x84 (position/owner selector)
    u32 text_owners[2];        // +0x88 per-select text owner handles
};

// TH10 0x00415E90. Result/spell-practice screen event-stream executor: one
// stack argument (state), returns with ret 4. Returns 0 while running, -1 on
// the end opcode, and 0 without timer advance when the early-exit gate hits.
i32 TH10_STDCALL RunResultScreenScriptStreamStackAbi(
    ResultScreenScriptState *state);

// TH10 0x00449670 (reconstructed locally; native EAX = &handle, stack =
// entry, ret 4). Resolves the handle's entity and initializes animation VM
// entry `entry_index` with the entity's own resource at +0x308.
void SetEntityGlyphFromHandleEaxStackAbi(u32 *handle, u32 entry_index);

// TH10 0x004496a0 (reconstructed locally; native EAX = &handle, stack =
// resource then entry, ret 8). Same, but the resource pointer arrives on the
// stack (call sites pass the dword at 0x00477704+0x38).
void SetEntityGlyphWithResourceEaxStackAbi(u32 *handle, u32 resource,
                                           u32 entry_index);

// 0x00409d90, implemented in this module: native __thiscall ECX = score
// block (0x00474c40), stack = value, ret 4; returns the new value.
i32 AddScoreBlockValueEcxStackAbi(void *score_block, i32 value);
// 0x00423370: EDI = game state manager (0x00477830), no stack args
// (still a boundary).
i32 RecordSpellPracticeCaptureEdiAbi(void *spell_state);
// 0x004175e0, implemented in this module: native EAX = score block
// (0x00474c40); advances the 48-byte slot index at +0x3c (max 7) and
// publishes the slot pointer to DAT_00477848.
void *UpdateScoreBlockEaxAbi(void *score_block);

} // namespace th10
