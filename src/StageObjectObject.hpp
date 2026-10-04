#pragma once

#include <stddef.h>

#include "GameContext.hpp"
#include "Th10Types.hpp"
#include "VmRecord.hpp"

namespace th10 {

// ---------------------------------------------------------------------------
// Stage/background objects (TH10 0x41c030..0x41f670): the 0xd58-byte kind A
// (callback table 0x46da60) and 0xd74-byte kind B (table 0x46da10) owned by
// the DAT_0047781c manager, plus the manager's 0x424-byte sentinel header.
//
// Shared header initializer 0x41c030 (native EDX = block): stores the
// all-stub table 0x46dab0 into +0, clears bit 0 of the timer flags +0x20,
// of eighteen 0x34-stride dwords at +0x68+0x34*i and of the entrance timer
// flags +0x420, then memsets 0x424 bytes (all preceding stores dead); the
// surviving tail lazily arms the +0x10 timer and stops it (+0x10 = -1).
//
// The per-kind ctors 0x41c5b0/0x41c680 re-run the defaults, install their
// table, wipe the descriptor image (A 0x1dc / B 0x1f8 bytes; B seeds 8.0f
// at +0x450) and reset both embedded VM records (0x3ac wipes with dead
// flag clears ahead, sprite u16 0xffff at rec+0x384).
//
// Layout verified against every body in src/StageObjectVtable.cpp /
// src/StageObjectManager.cpp (native 0x41c510..0x41f670).
// ---------------------------------------------------------------------------

// Header shared by both kinds (also the manager's list sentinel at
// manager+0x10). The feature-record region 0x68..0x424 is kept RAW: its
// consumers alias it (drift record 0x8c..0xb4, distance-shrink timer
// 0xf4..0x128, the 0x8000 shift timer 0x15c..0x170 and the kind A script
// interpreter records 0x460..0x610 — the latter overlap the descriptor
// image and VM record 1), so forcing a uniform sub-struct would mislead.
struct StageObjectHeader {
    void *callback_table_0000;    // +0x000 (T1 0x46da60 A / T2 0x46da10 B /
                                  //   T0 0x46dab0 sentinel)
    void *list_prev_0004;         // +0x004 manager list node (head
                                  //   manager+0x434, push-front)
    void *list_next_0008;         // +0x008
    i32 state_000c;               // +0x00c (A spawn 2, B spawn 3, spreads
                                  //   latch 1; kind-1 nodes skip virtuals)
    TimerNode timer_0010;         // +0x010..0x24 (lazy armed, stopped -1)
    float position_x_0024;        // +0x024
    float position_y_0028;        // +0x028
    float position_z_002c;        // +0x02c (boss-drop VM seed depth)
    float velocity_x_0030;        // +0x030 kind A live; kind B drifts via
    float velocity_y_0034;        //   the descriptor image instead
    float velocity_z_0038;        // +0x038
    float angle_003c;             // +0x03c
    float depth_0040;             // +0x040 z progress (12-degree sweep axis)
    float alpha_0044;             // +0x044 alpha/scale
    float zspeed_0048;            // +0x048 depth rate
    float zvel_004c;              // +0x04c accumulated depth velocity
    u8 done_latch_0050;           // +0x050 byte sweep-completion latch
    u8 gap0051[3];
    u32 spawn_id_0054;            // +0x054 (manager cursor; stored before
                                  //   the allocation null check)
    u8 raw_0058[0x10];            // +0x058..0x68
    u8 raw_0068[0x39c];           // +0x068..0x404 — feature records
                                  //   (18 x 0x34, flag dword at rec+0x00
                                  //   cleared by 0x41c030). Known overlays:
                                  //   drift 0x8c publish / 0x90 count /
                                  //   0x94 accum / 0x98 rate ptr /
                                  //   0xa0+0xa8+0xac+0xb0 multipliers /
                                  //   0xb4 limit; shrink timer 0xf4
                                  //   (TimerNode) + 0x108 base zrate /
                                  //   0x10c base drift x / 0x11c distance /
                                  //   0x120 repeat limit / 0x124 counter;
                                  //   0x8000 shift timer 0x15c (TimerNode,
                                  //   count = the +0x160 gate counter)
    u32 feature_flags_0404;       // +0x404 ten-way dispatch flags
    u32 gap0408;                  // +0x408
    u32 cutoff_kind_040c;         // +0x40c (24 on spawn; cutoff ring entry)
    TimerNode entrance_timer_0410; // +0x410..0x424 kind A entrance timer
                                   //   (B leaves it zeroed)
};

typedef char AssertStageObjectHeaderSize[
    sizeof(StageObjectHeader) == 0x424 ? 1 : -1];
typedef char AssertStageObjectHeaderTimerOffset[
    offsetof(StageObjectHeader, timer_0010) == 0x10 ? 1 : -1];
typedef char AssertStageObjectHeaderPosOffset[
    offsetof(StageObjectHeader, position_x_0024) == 0x24 ? 1 : -1];
typedef char AssertStageObjectHeaderAngleOffset[
    offsetof(StageObjectHeader, angle_003c) == 0x3c ? 1 : -1];
typedef char AssertStageObjectHeaderDepthOffset[
    offsetof(StageObjectHeader, depth_0040) == 0x40 ? 1 : -1];
typedef char AssertStageObjectHeaderAlphaOffset[
    offsetof(StageObjectHeader, alpha_0044) == 0x44 ? 1 : -1];
typedef char AssertStageObjectHeaderZSpeedOffset[
    offsetof(StageObjectHeader, zspeed_0048) == 0x48 ? 1 : -1];
typedef char AssertStageObjectHeaderZVelOffset[
    offsetof(StageObjectHeader, zvel_004c) == 0x4c ? 1 : -1];
typedef char AssertStageObjectHeaderDoneOffset[
    offsetof(StageObjectHeader, done_latch_0050) == 0x50 ? 1 : -1];
typedef char AssertStageObjectHeaderSpawnIdOffset[
    offsetof(StageObjectHeader, spawn_id_0054) == 0x54 ? 1 : -1];
typedef char AssertStageObjectHeaderFlagsOffset[
    offsetof(StageObjectHeader, feature_flags_0404) == 0x404 ? 1 : -1];
typedef char AssertStageObjectHeaderKindOffset[
    offsetof(StageObjectHeader, cutoff_kind_040c) == 0x40c ? 1 : -1];
typedef char AssertStageObjectHeaderEntranceOffset[
    offsetof(StageObjectHeader, entrance_timer_0410) == 0x410 ? 1 : -1];

// Kind A descriptor image (0x1dc bytes copied at +0x424 by 0x41c8c0). The
// cutoff respawn 0x41cfd0 writes the snapshot position (+0x000..0x008) and
// the reversed angle (+0x00c) back into this image before re-spawning.
struct StageObjectKindADescriptor {
    float spawn_x_0000;           // +0x000 (snapshot position)
    float spawn_y_0004;           // +0x004
    float spawn_z_0008;           // +0x008
    float angle_000c;             // +0x00c (spawn angle; cutoff writes
                                  //   the reversed angle here)
    u8 raw_0010[4];               // +0x010
    float depth_0014;             // +0x014 spawn depth (object +0x40)
    u8 raw_0018[4];               // +0x018
    float alpha_001c;             // +0x01c spawn alpha/scale (object +0x44)
    float zspeed_0020;            // +0x020 spawn depth rate (object +0x48;
                                  //   cutoff copies header+0x13c here)
    u16 script_slot_0024;         // +0x024 DAT_00474170 index (i16)
    u16 script_kind_0026;         // +0x026 ring-effect kind (i16; scripts
                                  //   kind*2+0x11 / kind+0x103)
    u8 flags_0028;                // +0x028 bit 0 lifts rec1 flag 0x10
    u8 raw_0029[0x1b3];           // +0x029..0x1dc
};

typedef char AssertStageObjectKindADescriptorSize[
    sizeof(StageObjectKindADescriptor) == 0x1dc ? 1 : -1];
typedef char AssertStageObjectKindADescriptorAngleOffset[
    offsetof(StageObjectKindADescriptor, angle_000c) == 0xc ? 1 : -1];
typedef char AssertStageObjectKindADescriptorDepthOffset[
    offsetof(StageObjectKindADescriptor, depth_0014) == 0x14 ? 1 : -1];
typedef char AssertStageObjectKindADescriptorAlphaOffset[
    offsetof(StageObjectKindADescriptor, alpha_001c) == 0x1c ? 1 : -1];
typedef char AssertStageObjectKindADescriptorZSpeedOffset[
    offsetof(StageObjectKindADescriptor, zspeed_0020) == 0x20 ? 1 : -1];
typedef char AssertStageObjectKindADescriptorSlotOffset[
    offsetof(StageObjectKindADescriptor, script_slot_0024) == 0x24 ? 1 : -1];
typedef char AssertStageObjectKindADescriptorFlagsOffset[
    offsetof(StageObjectKindADescriptor, flags_0028) == 0x28 ? 1 : -1];

// Kind B descriptor image (0x1f8 bytes copied at +0x424 by 0x41e5c0). All
// of kind B's live parameters stay INSIDE this image: the update 0x41e700
// drifts along velocity_000c, wraps the angle with angle_rate_001c, clamps
// the depth at depth_clamp_0020, runs the four-state entrance machine on
// the entrance timers, and reads the follow/lift latch byte.
struct StageObjectKindBDescriptor {
    u8 raw_0000[0xc];             // +0x000
    float velocity_x_000c;        // +0x00c entrance drift per frame
    float velocity_y_0010;        // +0x010
    float velocity_z_0014;        // +0x014
    float angle_0018;             // +0x018 spawn angle (object +0x3c)
    float angle_rate_001c;        // +0x01c per-frame angle wrap delta
    float depth_clamp_0020;       // +0x020 depth limit (advance/clamp)
    float depth_0024;             // +0x024 spawn depth (object +0x40)
    float alpha_target_0028;      // +0x028 entrance alpha ramp target
    float zspeed_002c;            // +0x02c depth rate (ctor seeds 8.0f)
    i32 entrance_timer_a_0030;    // +0x030 state 3 -> 4 threshold
    i32 entrance_timer_b_0034;    // +0x034 state 4 -> 2 threshold
    i32 entrance_timer_c_0038;    // +0x038 state 2 -> 5 threshold
    i32 entrance_timer_d_003c;    // +0x03c state 5 release threshold
    u16 script_slot_0040;         // +0x040 DAT_00474170 index (i16)
    u16 script_kind_0042;         // +0x042 ring-effect kind (i16)
    u8 flags_0044;                // +0x044 bit 0 follow
                                  //   [DAT_00477704+0x10]+0x1068, bit 1
                                  //   lifts rec1 flag 0x10
    u8 raw_0045[0x1b3];           // +0x045..0x1f8
};

typedef char AssertStageObjectKindBDescriptorSize[
    sizeof(StageObjectKindBDescriptor) == 0x1f8 ? 1 : -1];
typedef char AssertStageObjectKindBDescriptorVelocityOffset[
    offsetof(StageObjectKindBDescriptor, velocity_x_000c) == 0xc ? 1 : -1];
typedef char AssertStageObjectKindBDescriptorAngleOffset[
    offsetof(StageObjectKindBDescriptor, angle_0018) == 0x18 ? 1 : -1];
typedef char AssertStageObjectKindBDescriptorRateOffset[
    offsetof(StageObjectKindBDescriptor, angle_rate_001c) == 0x1c ? 1 : -1];
typedef char AssertStageObjectKindBDescriptorClampOffset[
    offsetof(StageObjectKindBDescriptor, depth_clamp_0020) == 0x20 ? 1 : -1];
typedef char AssertStageObjectKindBDescriptorAlphaOffset[
    offsetof(StageObjectKindBDescriptor, alpha_target_0028) == 0x28 ? 1 : -1];
typedef char AssertStageObjectKindBDescriptorZSpeedOffset[
    offsetof(StageObjectKindBDescriptor, zspeed_002c) == 0x2c ? 1 : -1];
typedef char AssertStageObjectKindBDescriptorTimersOffset[
    offsetof(StageObjectKindBDescriptor, entrance_timer_a_0030) == 0x30
        ? 1 : -1];
typedef char AssertStageObjectKindBDescriptorSlotOffset[
    offsetof(StageObjectKindBDescriptor, script_slot_0040) == 0x40 ? 1 : -1];
typedef char AssertStageObjectKindBDescriptorFlagsOffset[
    offsetof(StageObjectKindBDescriptor, flags_0044) == 0x44 ? 1 : -1];

// Kind A object, 0xd58 bytes. The kind A script interpreter 0x41ca80 keeps
// its 18 x 0x18-byte records at +0x460..0x610 with the record counter at
// header+0x400 — those overlay the descriptor image tail and VM record 1's
// first 0x10 bytes, so the interpreter stays a boundary and both regions
// stay raw.
struct StageObjectKindA {
    StageObjectHeader header;     // +0x000..0x424
    StageObjectKindADescriptor descriptor_0424; // +0x424..0x600 (image; the
                                  //   interpreter records alias its tail)
    VmRecord vm1_0600;            // +0x600..0x9ac
    VmRecord vm2_09ac;            // +0x9ac..0xd58
};

typedef char AssertStageObjectKindASize[
    sizeof(StageObjectKindA) == 0xd58 ? 1 : -1];
typedef char AssertStageObjectKindADescriptorMemberOffset[
    offsetof(StageObjectKindA, descriptor_0424) == 0x424 ? 1 : -1];
typedef char AssertStageObjectKindAVm1Offset[
    offsetof(StageObjectKindA, vm1_0600) == 0x600 ? 1 : -1];
typedef char AssertStageObjectKindAVm2Offset[
    offsetof(StageObjectKindA, vm2_09ac) == 0x9ac ? 1 : -1];

// Kind B object, 0xd74 bytes.
struct StageObjectKindB {
    StageObjectHeader header;     // +0x000..0x424
    StageObjectKindBDescriptor descriptor_0424; // +0x424..0x61c (LIVE)
    VmRecord vm1_061c;            // +0x61c..0x9c8
    VmRecord vm2_09c8;            // +0x9c8..0xd74
};

typedef char AssertStageObjectKindBSize[
    sizeof(StageObjectKindB) == 0xd74 ? 1 : -1];
typedef char AssertStageObjectKindBDescriptorMemberOffset[
    offsetof(StageObjectKindB, descriptor_0424) == 0x424 ? 1 : -1];
typedef char AssertStageObjectKindBVm1Offset[
    offsetof(StageObjectKindB, vm1_061c) == 0x61c ? 1 : -1];
typedef char AssertStageObjectKindBVm2Offset[
    offsetof(StageObjectKindB, vm2_09c8) == 0x9c8 ? 1 : -1];

} // namespace th10
