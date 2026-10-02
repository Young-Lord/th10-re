#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "GameContext.hpp"
#include "GameManagerObject.hpp"

namespace th10 {

struct ChainElem;

// ---------------------------------------------------------------------------
// The game-state manager object (TH10 DAT_00477830; source aliases
// g_GameStateManager / g_ScoreRecordOwner), 0x2c8 bytes. Allocated by
// ::operator new(0x2c8) in CreateScoreRecordOwner (0x00422360), defaults +
// full 0x2c8 wipe by ResetScoreRecordDefaultsEdxAbi (0x004220e0 — the 11
// pre-wipe default stores are dead stores natively, preserved as a quirk),
// which then publishes the global. Scheduler nodes installed by
// InstallReplayNameEntryCallbacksEbxAbi (0x00422150, calc → +0x8 / draw →
// +0xc, slot 0; arms the +0x10 timer). Destroyed in place by
// DestroyGameStateObjectInPlace (0x00422220, __stdcall, returns 0).
//
// Sub-state machine at mode (+0x4): 0 = game/idle (render gate tests == 0,
// 0x00426423), 1-5 = pause menu, 6-13 = post-run / replay-save menus (6
// doubles as the game-over & spell-practice scene state written by 0x00423370
// / 0x004231d0 / 0x00422ab0).
// ---------------------------------------------------------------------------

struct GameStateManager {
    u32 flags_0000;                  // +0x000 (creator sets bit 1)
    i32 mode_0004;                   // +0x004 (0 game / 1-5 pause / 6-13
                                     //   post-run & replay-save menus)
    ChainElem *calc_element;         // +0x008 (node+4 bit 2 toggled by
                                     //   0x418190 / 0x417c80)
    ChainElem *draw_element;         // +0x00c
    TimerNode frame_timer;           // +0x010 (rate -> g_FrameTimeScale
                                     //   0x476f78; tick epilogue 0x422450)
    ManagerCursorRecord cursor_a;    // +0x024..0xfc pause / main cursor
                                     //   (wrap_flag = record+0xf4)
    ManagerCursorRecord cursor_b;    // +0x0fc..0x1d4 name-entry cursor
                                     //   (wrap_flag = record+0x1cc)
    u32 handle_a_01d4;               // +0x1d4 second overlay VM entity id
                                     //   (scripts 129 spell-practice / 128
                                     //   game-over / 121 pause-enter)
    u32 handle_b_01d8;               // +0x1d8 first overlay VM entity id
                                     //   (script 0)
    u32 handle_c_01dc;               // +0x1dc third pause sprite handle; the
                                     //   only one the dtor releases (inlined
                                     //   0x4492a0 semantics)
    i32 name_length_01e0;            // +0x1e0 typed replay-name length (0-8)
    i32 spell_practice_flag_01e4;    // +0x1e4 (1 = spell practice, 0 = game
                                     //   over; gates mode-13 status 10 vs 13)
    i32 replay_gate_01e8;            // +0x1e8 (1 when score didn't place /
                                     //   replay list usable)
    void *parsed_replay_files[25];   // +0x1ec..0x250 (each torn down via
                                     //   0x004294a0 + freed by the dtor and
                                     //   by the mode-10 cancel path)
    u8 unknown_0250[0x64];           // +0x250..0x2b3 unreferenced by every
                                     //   accessor examined (dead space)
    char replay_name_02b4[12];       // +0x2b4 (trailing-space scan touches
                                     //   +0x2b4..+0x2bb; 0x423ba7..0x423bed)
    float saved_time_scale_02c0;     // +0x2c0 raw copy of g_FrameTimeScale
                                     //   (restored by 0x422c73 / 0x423510)
    void *front_anm_work_02c4;       // +0x2c4 copy of AsciiHudOwner
                                     //   ->front_anm_work, planted on pause /
                                     //   spell-practice / game-over entry
};

typedef char AssertGameStateManagerSize[
    sizeof(GameStateManager) == 0x2c8 ? 1 : -1];
typedef char AssertGameStateManagerModeOffset[
    offsetof(GameStateManager, mode_0004) == 0x4 ? 1 : -1];
typedef char AssertGameStateManagerTimerOffset[
    offsetof(GameStateManager, frame_timer) == 0x10 ? 1 : -1];
typedef char AssertGameStateManagerCursorAOffset[
    offsetof(GameStateManager, cursor_a) == 0x24 ? 1 : -1];
typedef char AssertGameStateManagerCursorBOffset[
    offsetof(GameStateManager, cursor_b) == 0xfc ? 1 : -1];
typedef char AssertGameStateManagerHandleAOffset[
    offsetof(GameStateManager, handle_a_01d4) == 0x1d4 ? 1 : -1];
typedef char AssertGameStateManagerFilesOffset[
    offsetof(GameStateManager, parsed_replay_files) == 0x1ec ? 1 : -1];
typedef char AssertGameStateManagerReplayNameOffset[
    offsetof(GameStateManager, replay_name_02b4) == 0x2b4 ? 1 : -1];
typedef char AssertGameStateManagerSavedScaleOffset[
    offsetof(GameStateManager, saved_time_scale_02c0) == 0x2c0 ? 1 : -1];
typedef char AssertGameStateManagerFrontAnmOffset[
    offsetof(GameStateManager, front_anm_work_02c4) == 0x2c4 ? 1 : -1];

} // namespace th10
