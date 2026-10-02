#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "GameContext.hpp"

namespace th10 {

struct ChainElem;

// ---------------------------------------------------------------------------
// The 0x60-byte title-screen controller (TH10 DAT_00477810; aliases
// g_MainChainContext/g_ManagerObject810/g_GameModeRecord/g_SoundGateOwner in
// files that predate the MainChainContext naming). Created by CreateTitleScreen
// 0x004180e0 (AllocateMainChainObject(0x60), sub-object defaults from
// DAT_00491d48, zero-fill, +0x58 |= 4, mode stored at +0x5c), torn down by
// TeardownTitleScreenStackAbi (removes the two scheduler records, g_TitleScreen
// = 0). NOTE: DAT_00491c28 is the different MainChainContext object.
//
// +0x58 flag bits: 0x001 teardown phase, 0x002 game running, 0x004 boot /
//   draw-owner-reset gate, 0x008 shutdown frame, 0x010 pause latched,
//   0x020/0x040 pause-menu mode gates, 0x080 pending shutdown,
//   0x200 sound gate, 0x400 scene active, 0x800 title score anims armed.
// ---------------------------------------------------------------------------
struct TitleScreen {
    u32 field_0000;          // +0x00 (never written by the game)
    void *mode_record;       // +0x04 (published mode-record pointer)
    ChainElem *calc_element; // +0x08 (prio 10)
    ChainElem *draw_element; // +0x0c (prio 4)
    TimerNode timer;         // +0x10 (count = frame counter; drives the
                             //   0/30/2940/3000-frame calc gates)
    u8 sub_object[0x34];     // +0x24..0x58 (manager-state defaults; sub+0x30
                             //   = +0x54 is read as a signed byte by the
                             //   scene-object bar draw)
    u32 flags;               // +0x58
    u32 mode;                // +0x5c (pause item-count gate, script-79 spawn
                             //   gate, game-over sub-state, transition arg)
};

typedef char AssertTitleScreenSize[sizeof(TitleScreen) == 0x60 ? 1 : -1];
typedef char AssertTitleScreenCalcElementOffset[
    offsetof(TitleScreen, calc_element) == 0x8 ? 1 : -1];
typedef char AssertTitleScreenTimerOffset[
    offsetof(TitleScreen, timer) == 0x10 ? 1 : -1];
typedef char AssertTitleScreenSubObjectOffset[
    offsetof(TitleScreen, sub_object) == 0x24 ? 1 : -1];
typedef char AssertTitleScreenFlagsOffset[
    offsetof(TitleScreen, flags) == 0x58 ? 1 : -1];
typedef char AssertTitleScreenModeOffset[
    offsetof(TitleScreen, mode) == 0x5c ? 1 : -1];

} // namespace th10
