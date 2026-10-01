#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "CallbackScheduler.hpp"

namespace th10 {

// The shared 0x14-byte script timer node (poison-primed, rate-pointer
// driven). It appears inside GameContext at +0x14 and inside stage enemy
// records at +0x3f8; ResetTimerNode (TH10 0x00404ed0 family) serves both.
struct TimerNode {
    i32 prev;               // +0x00 (poison 0xfff0bdc1 / -1 when stopped)
    i32 count;              // +0x04
    i32 accum;              // +0x08
    const float *rate;      // +0x0c (-> g_FrameTimeScale 0x476f78)
    u32 flags;              // +0x10 (bit 0 = initialized)
};

typedef char AssertTimerNodeSize[sizeof(TimerNode) == 0x14 ? 1 : -1];

// Partial-but-complete layout of the 0x48-byte game context object published
// at TH10 DAT_004776ec (source aliases: g_GameContext, g_GameContextObject,
// g_StageNode). Created by CreateGameContextObject (TH10 0x004056b0) inside
// EnterGameModeSetupEsiAbi (0x0040a350); freed through DestroyGameContextInPlace
// (0x00405620) + the shared delete 0x004524a1. It owns exactly two scheduler
// records (calc priority 0x11 at +0x8, draw priority 0x22 at +0xc) and drives
// the deathbomb gate (+0x28), the scene-trigger popup (+0x14 timer,
// +0x2c handle, +0x30..0x3c position/radius) and bomb-area damage (+0x3c
// radius, +0x44 variant). The same layout is declared in the IDB as
// struct GameContext.
struct GameContext {
    u32 flags;              // +0x00 (bit 1 set by the creation wipe)
    u8 gap0004[4];          // +0x04
    ChainElem *calc_record; // +0x08 (popup update, priority 0x11)
    ChainElem *draw_record; // +0x0c (always-ready stub 0x405850, priority 0x22)
    u8 gap0010[4];          // +0x10
    TimerNode timer;        // +0x14
    u32 popup_state;        // +0x28 (0 idle; 1 effect running; deathbomb gate)
    i32 effect_handle;      // +0x2c (spawned VM entity handle)
    float position_x;       // +0x30
    float position_y;       // +0x34
    float position_z;       // +0x38
    float radius;           // +0x3c (blast radius; seeded 32.0f)
    float field_0040;       // +0x40 (written 4.0f; never read on this object)
    i32 bomb_variant;       // +0x44 (0 normal bomb, 1 direct/variant)
};

typedef char AssertGameContextSize[sizeof(GameContext) == 0x48 ? 1 : -1];
typedef char AssertGameContextCalcRecordOffset[
    offsetof(GameContext, calc_record) == 0x8 ? 1 : -1];
typedef char AssertGameContextDrawRecordOffset[
    offsetof(GameContext, draw_record) == 0xc ? 1 : -1];
typedef char AssertGameContextTimerOffset[
    offsetof(GameContext, timer) == 0x14 ? 1 : -1];
typedef char AssertGameContextPopupStateOffset[
    offsetof(GameContext, popup_state) == 0x28 ? 1 : -1];
typedef char AssertGameContextEffectHandleOffset[
    offsetof(GameContext, effect_handle) == 0x2c ? 1 : -1];
typedef char AssertGameContextPositionXOffset[
    offsetof(GameContext, position_x) == 0x30 ? 1 : -1];
typedef char AssertGameContextRadiusOffset[
    offsetof(GameContext, radius) == 0x3c ? 1 : -1];
typedef char AssertGameContextBombVariantOffset[
    offsetof(GameContext, bomb_variant) == 0x44 ? 1 : -1];

} // namespace th10
