#pragma once

#include <stddef.h>

#include "CallbackScheduler.hpp"
#include "GameContext.hpp"
#include "Th10Types.hpp"
#include "VmRecord.hpp"

namespace th10 {

// ---------------------------------------------------------------------------
// Bullet manager (TH10 DAT_00477818; source aliases g_BulletManagerSlot /
// g_EffectPoolManager / g_ExplosionManager), 0x21CEC0 bytes.
//
// Creator 0x41aed0: operator new(0x21CEC0), eh vector constructor iterator
// over 2198 x 0x3f0 slots starting at +0x14 (slot ctor 0x41acf0 / dtor
// 0x41ad60), then the WHOLE object is memset to 0 (ctor-before-wipe quirk),
// bit 1 of the first dword set, published, then 0x41ad90 installs the
// scheduler callbacks. In-place dtor 0x41adf0 releases both chain elements
// (critical-section bracket, activity byte 0x49231c), clears the global and
// runs the eh vector destructor iterator over the slots.
//
// Scheduler: calc node (0x41ba00 gate -> bullet update 0x41afd0, priority
// 0x15) at +0x8, draw node (0x41ba30 gate -> TickEffectPoolSlots 0x41b8e0,
// priority 0x19) at +0xc. Both gates read DAT_00477810+0x58 flags
// (bits 0/2/0x400 skip; the draw gate tests bit 0x400 as a byte).
//
// Spawner 0x41bb00 (EAX = manager, ECX = pos vec3, stack {kind, option,
// speed, angle}; ret 0x14): kind != 8 scans the first 150 slots for
// state_03dc == 0 (kind 8 uses the 2048-entry ring over slots 150..2197
// with state 5 + tiered spawn delay). Deferred kill 0x41ba50 walks the
// first 150 slots, deactivating state != 0 && kind in {1,4,10,11}: the
// state_03dc dword is zeroed (+0x3ac is NOT cleared) and the 0x41bb00
// spawner is invoked with kind 9 for kinds 1/4 and kind 5 for kinds
// 10/11 (angle -pi/2, speed 2.2, color -1), then one death-entity spawn
// per killed bullet.
//
// The game-start reset (title calc body 0x417870 family / EclSelectMenu)
// wipes exactly [+0x14, +0x21CEB4) — the slot array only.
// ---------------------------------------------------------------------------

// One pool slot, 0x3f0 bytes: the 0x3ac-byte animation VM record followed
// by the effect payload. The VM record is owned by the bullet script
// interpreter 0x43ee30 (+0x2fc option int lands in vm.primary_color,
// +0x334..0x33c the vm base position, +0x384 the bound-script u16).
struct BulletSlot {
    VmRecord vm;                  // +0x000..0x3ac
    float position_x_03ac;        // +0x3ac spawn position (x clamped to
    float position_y_03b0;        //   [-192, 192] by 0x41bb00; republished
    float position_z_03b4;        //   +224/+16 by the draw pass 0x41b8e0)
    float velocity_x_03b8;        // +0x3b8 SetPolarVectorThiscall target
    float velocity_y_03bc;        // +0x3bc
    float velocity_z_03c0;        // +0x3c0 (zeroed on spawn)
    u32 timer_tail_zero_03c4;     // +0x3c4 zeroed alongside the timer tail
    TimerNode timer_03c8;         // +0x3c8..0x3dc (prev poison 0xfff0bdc1 ->
                                  //   -1; count doubles as the integer
                                  //   distance, accum as the float distance;
                                  //   rate -> g_FrameTimeScale 0x476f78)
    i32 state_03dc;               // +0x3dc 0 dead / 1 falling / 2 delayed-rise
                                  //   / 3 homing / 4 player-retarget /
                                  //   5 spawn-delay
    i32 kind_03e0;                // +0x3e0 item/bonus kind (11-case switch)
    i32 bound_kind_03e4;          // +0x3e4 PIV-remapped script kind
                                  //   (1/4->9, 10->1, 11->4 when power > 99)
    float speed_03e8;             // +0x3e8 (accelerated +0.2f above 12.0f)
    i32 spawn_delay_03ec;         // +0x3ec kind-8 tiered delay (state 5)
};

typedef char AssertBulletSlotSize[sizeof(BulletSlot) == 0x3f0 ? 1 : -1];
typedef char AssertBulletSlotVmOffset[
    offsetof(BulletSlot, vm) == 0x0 ? 1 : -1];
typedef char AssertBulletSlotPosOffset[
    offsetof(BulletSlot, position_x_03ac) == 0x3ac ? 1 : -1];
typedef char AssertBulletSlotVelOffset[
    offsetof(BulletSlot, velocity_x_03b8) == 0x3b8 ? 1 : -1];
typedef char AssertBulletSlotTimerOffset[
    offsetof(BulletSlot, timer_03c8) == 0x3c8 ? 1 : -1];
typedef char AssertBulletSlotStateOffset[
    offsetof(BulletSlot, state_03dc) == 0x3dc ? 1 : -1];
typedef char AssertBulletSlotKindOffset[
    offsetof(BulletSlot, kind_03e0) == 0x3e0 ? 1 : -1];
typedef char AssertBulletSlotBoundKindOffset[
    offsetof(BulletSlot, bound_kind_03e4) == 0x3e4 ? 1 : -1];
typedef char AssertBulletSlotSpeedOffset[
    offsetof(BulletSlot, speed_03e8) == 0x3e8 ? 1 : -1];
typedef char AssertBulletSlotDelayOffset[
    offsetof(BulletSlot, spawn_delay_03ec) == 0x3ec ? 1 : -1];

struct BulletManager {
    u32 flags_0000;               // +0x0000 bit 1 set by the creation wipe
    u8 gap0004[4];                // +0x0004
    ChainElem *calc_element;      // +0x0008 (0x41ba00, priority 0x15)
    ChainElem *draw_element;      // +0x000c (0x41ba30, priority 0x19)
    u8 gap0010[4];                // +0x0010
    BulletSlot slots[2198];       // +0x0014..0x21ceb4 (kind-8 ring uses
                                  //   slots 150..2197; first 150 are the
                                  //   immediate pool)
    u32 live_count_21ceb4;        // +0x21ceb4 per-frame live counter (cleared
                                  //   and re-incremented by 0x41afd0)
    u32 ring_cursor_21ceb8;       // +0x21ceb8 kind-8 ring cursor, advanced
                                  //   (x+1) % 2048 by 0x41bb00/0x41ba50
    u32 spawn_counter_21cebc;     // +0x21cebc kind-8 per-frame spawn counter
                                  //   (cleared each frame; tiered caps
                                  //   256/512/1024 select the delay tier)
};

typedef char AssertBulletManagerSize[
    sizeof(BulletManager) == 0x21cec0 ? 1 : -1];
typedef char AssertBulletManagerCalcOffset[
    offsetof(BulletManager, calc_element) == 0x8 ? 1 : -1];
typedef char AssertBulletManagerSlotsOffset[
    offsetof(BulletManager, slots) == 0x14 ? 1 : -1];
typedef char AssertBulletManagerLiveCountOffset[
    offsetof(BulletManager, live_count_21ceb4) == 0x21ceb4 ? 1 : -1];
typedef char AssertBulletManagerRingCursorOffset[
    offsetof(BulletManager, ring_cursor_21ceb8) == 0x21ceb8 ? 1 : -1];
typedef char AssertBulletManagerSpawnCounterOffset[
    offsetof(BulletManager, spawn_counter_21cebc) == 0x21cebc ? 1 : -1];

} // namespace th10
