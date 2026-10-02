#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "GameContext.hpp"
#include "ThreadControl.hpp"

namespace th10 {

struct ChainElem;

// Menu cursor record embedded in the game manager (0xd8 bytes). Instance A
// lives at GameManager+0x24; a value/copy/max prefix is reused at +0xfc and
// the 13-column alphabet grid starts at +0x58f4.
struct ManagerCursorRecord {
    i32 value;             // +0x00
    i32 previous;          // +0x04
    i32 maximum;           // +0x08
    i32 step_values[16];   // +0x0c
    i32 step_maxima[16];   // +0x4c
    i32 step_count;        // +0x8c (cap 15)
    u32 disabled_rows[16]; // +0x90
    u32 wrap_flag;         // +0xd0 (dual duty: worker-ready flag at +0xf4)
    u32 disabled_count;    // +0xd4
};

typedef char AssertManagerCursorRecordSize[
    sizeof(ManagerCursorRecord) == 0xd8 ? 1 : -1];

// ThreadControl prefix embedded at GameManager+0x5ab0. The 0x5acc allocation
// ends four bytes into the ThreadControl (its update-status dword at +0x1c
// is outside the object), so this is the 0x1c-byte prefix.
struct GameManagerWorkerControl {
    void *marker;        // +0x5ab0 (0x004703e4)
    void *thread_handle; // +0x5ab4
    u32 thread_id;       // +0x5ab8
    u32 stop_requested;  // +0x5abc
    u32 field_0010;      // +0x5ac0 (active)
    u32 unknown_0014;    // +0x5ac4
    void *thread_entry;  // +0x5ac8 (worker 0x0042c9f0)
};

typedef char AssertGameManagerWorkerControlSize[
    sizeof(GameManagerWorkerControl) == 0x1c ? 1 : -1];

// ---------------------------------------------------------------------------
// The game manager (TH10 DAT_0047784c, 0x5acc bytes): state machine host for
// the menu/score/result flow. ctor 0x0042c920 (vtable + ThreadControl marker
// at +0x5ab0, memset 0x5acc, flags bit 1), create 0x0042cd50, worker channels
// 0x0042caa0, in-place dtor 0x0042cb60 (50-object pool walk from +0x59e4,
// owned buffer +0x5aac, global cleared).
//
// Deliberately NOT modeled here (attribution unresolved, left raw):
// StageEffectHost.cpp's EnterGameModeSetupEsiAbi (+0x34/+0x114/+0x1ec/+0x1f4
// — likely the 0x688 stage-host object) and MenuStateHelpers'
// vector-tween +0x70..+0xb8 block (likely a 0x3ac pool VM record).
// ---------------------------------------------------------------------------
struct GameManager {
    u8 unknown_0000[4];              // +0x000 (dtor vtable 0x0046ecf0)
    u32 flags_0004;                  // +0x004 (ctor sets bit 1)
    u8 unknown_0008[4];
    ChainElem *calc_element;         // +0x00c (prio 6, installed disabled)
    ChainElem *draw_element;         // +0x010 (prio 3)
    void *anm_work_title;            // +0x014 ("title.anm" manager work)
    void *anm_work_title_v;          // +0x018 ("title_v.anm" manager work)
    i32 state;                       // +0x01c (0..0x10 dispatch key)
    i32 sub_state;                   // +0x020
    ManagerCursorRecord cursor_a;    // +0x024..0xfc
    i32 cursor_b_value;              // +0x0fc
    i32 cursor_b_previous;           // +0x100
    i32 cursor_b_maximum;            // +0x104
    u8 unknown_0108[0x6c];
    u32 teardown_entity_id;          // +0x174 (stop-cancelled by the dtor)
    u8 unknown_0178[0x54];
    u32 state_b_flag_01cc;           // +0x1cc
    u8 unknown_01d0[4];
    i32 state_b_page_cursor;         // +0x1d4
    i32 state_b_page_previous;       // +0x1d8
    i32 state_b_page_count;          // +0x1dc
    u8 unknown_01e0[0xc4];
    u32 state_b_flag_02a4;           // +0x2a4
    u8 unknown_02a8[4];
    i32 state_row_counter;           // +0x2ac (-1 init in state C)
    TimerNode frame_timer;           // +0x2b0 (prev -999999/-1 seeds)
    u32 script_entity_handles[180];  // +0x2c4..0x594 (0x2c4 + 4*script_id;
                                     //   slots 0..182 in use)
    i32 state_e_row_count;           // +0x594
    u8 unknown_0598[0x38];
    u32 ascii_work_handle;           // +0x5d0
    u32 state_b_option_handles[10];  // +0x5d4..0x5fc
    u8 unknown_05fc[0x88];
    u32 state1_handle;               // +0x684
    u8 unknown_0688[4];
    u32 staged_text_counter;         // +0x68c (state E, 0..8)
    u8 unknown_0690[0x5248];
    u32 state_e_window_start;        // +0x58d8
    u8 name_buffer[9];               // +0x58dc (8 chars + NUL)
    u8 unknown_58e5[3];
    u32 caret_column;                // +0x58e8
    u32 name_entry_done;             // +0x58ec
    u32 name_length;                 // +0x58f0 (difficulty stash in state 2)
    u32 alphabet_cursor;             // +0x58f4
    u32 alphabet_cursor_previous;    // +0x58f8
    u8 unknown_58fc[0x10];
    u32 alphabet_length;             // +0x590c
    u8 unknown_5910[0xb4];
    u32 selected_replay_slot;        // +0x59c4
    u8 unknown_59c8[4];
    u16 result_stats[5];             // +0x59cc (result screen, kinds 67-86;
                                     //   overlaps the state-C parse-hook
                                     //   region that starts at +0x59d4)
    u8 unknown_59d6[4];
    u8 unknown_59da[2];
    u32 selected_replay_index;       // +0x59dc
    u8 unknown_59e0[4];
    u32 replay_parse_handles[50];    // +0x59e4..0x5aac (dtor-walked pool)
    void *owned_buffer;              // +0x5aac (state-E score records)
    GameManagerWorkerControl worker_control; // +0x5ab0..0x5acc
};

typedef char AssertGameManagerSize[sizeof(GameManager) == 0x5acc ? 1 : -1];
typedef char AssertGameManagerStateOffset[
    offsetof(GameManager, state) == 0x1c ? 1 : -1];
typedef char AssertGameManagerSubStateOffset[
    offsetof(GameManager, sub_state) == 0x20 ? 1 : -1];
typedef char AssertGameManagerCursorAOffset[
    offsetof(GameManager, cursor_a) == 0x24 ? 1 : -1];
typedef char AssertGameManagerCursorBOffset[
    offsetof(GameManager, cursor_b_value) == 0xfc ? 1 : -1];
typedef char AssertGameManagerTeardownEntityOffset[
    offsetof(GameManager, teardown_entity_id) == 0x174 ? 1 : -1];
typedef char AssertGameManagerFrameTimerOffset[
    offsetof(GameManager, frame_timer) == 0x2b0 ? 1 : -1];
typedef char AssertGameManagerScriptHandlesOffset[
    offsetof(GameManager, script_entity_handles) == 0x2c4 ? 1 : -1];
typedef char AssertGameManagerScriptHandlesEndOffset[
    offsetof(GameManager, state_e_row_count) == 0x594 ? 1 : -1];
typedef char AssertGameManagerAsciiWorkHandleOffset[
    offsetof(GameManager, ascii_work_handle) == 0x5d0 ? 1 : -1];
typedef char AssertGameManagerStateBOptionsOffset[
    offsetof(GameManager, state_b_option_handles) == 0x5d4 ? 1 : -1];
typedef char AssertGameManagerState1HandleOffset[
    offsetof(GameManager, state1_handle) == 0x684 ? 1 : -1];
typedef char AssertGameManagerStagedTextCounterOffset[
    offsetof(GameManager, staged_text_counter) == 0x68c ? 1 : -1];
typedef char AssertGameManagerWindowStartOffset[
    offsetof(GameManager, state_e_window_start) == 0x58d8 ? 1 : -1];
typedef char AssertGameManagerNameBufferOffset[
    offsetof(GameManager, name_buffer) == 0x58dc ? 1 : -1];
typedef char AssertGameManagerNameEntryDoneOffset[
    offsetof(GameManager, name_entry_done) == 0x58ec ? 1 : -1];
typedef char AssertGameManagerAlphabetCursorOffset[
    offsetof(GameManager, alphabet_cursor) == 0x58f4 ? 1 : -1];
typedef char AssertGameManagerAlphabetLengthOffset[
    offsetof(GameManager, alphabet_length) == 0x590c ? 1 : -1];
typedef char AssertGameManagerSelectedReplaySlotOffset[
    offsetof(GameManager, selected_replay_slot) == 0x59c4 ? 1 : -1];
typedef char AssertGameManagerResultStatsOffset[
    offsetof(GameManager, result_stats) == 0x59cc ? 1 : -1];
typedef char AssertGameManagerReplayParseHandlesOffset[
    offsetof(GameManager, replay_parse_handles) == 0x59e4 ? 1 : -1];
typedef char AssertGameManagerReplayParseHandlesEndOffset[
    offsetof(GameManager, owned_buffer) == 0x5aac ? 1 : -1];
typedef char AssertGameManagerWorkerControlOffset[
    offsetof(GameManager, worker_control) == 0x5ab0 ? 1 : -1];

} // namespace th10
