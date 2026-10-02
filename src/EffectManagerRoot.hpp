#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "VmRecord.hpp"

namespace th10 {

struct ChainElem;

// ---------------------------------------------------------------------------
// The effect manager root (TH10 DAT_004776f0; source aliases g_EffectManager
// Root / g_SceneCommandManager / g_EnemyArrayBase / g_StageRecordHolder),
// 0x3E0B54 bytes. Allocated by ::operator new(0x3E0B54) in
// CreateEffectManagerRoot (0x00406060): eh vector constructor iterator
// constructs 2001 records at +0x60 FIRST, then the whole object is memset to
// 0 (ctor-before-wipe quirk), then the global is published. Created from
// EnterGameModeSetupEsiAbi (0x40a350) and SetupGameSceneFromTitle (0x417870).
// Released by ReleaseEffectManagerRootEsiAbi (0x00406140) / in-place dtor
// (0x00405f70); loader 0x00405e20, teardown 0x00405ed0.
//
// Scheduler: calc node (0x406770, priority 0x14) at +0x8, draw node
// (0x4067a0, priority 0x1d) at +0xc — registered disabled.
// ---------------------------------------------------------------------------

// One scene-trigger / stage-enemy pool record, 0x7f0 bytes, 2001 of them at
// root+0x60. Every gameplay loop iterates only the first 2000 (0x7D0); the
// 2001st record is never scanned, and the loader seeds the constant u16 5
// into ITS kind word at root+0x3e07a6 (= records[2000].kind_0446) as a
// marker — an aliasing quirk to keep raw.
//
// Many record offsets are just the embedded VmRecord's fields
// (record+0x08 + vm_offset): +0x34 = vm+0x2c angle, +0x30c = vm+0x304,
// +0x33c/+0x340/+0x344 = vm.base_pos_x/y/z, +0x360 = vm+0x358 heap buffer,
// +0x364 = vm+0x35c flags (bit 0x8000000 = angle-wrap mode), +0x38c =
// vm+0x384 sprite sentinel u16 (0xFFFF from the ctor).
struct EffectTriggerRecord {
    u32 flags_0000;               // +0x000 bit 0x4 expiry latch (0x406240
                                  //   kind==2 path), bit 0x8 activated-in-field
    u32 activation_gate_0004;     // +0x004 nonzero = activated/reserved (gates
                                  //   require-uninitialized scans)
    VmRecord vm;                  // +0x008..0x3b4 (ctor wipes it; nine busy
                                  //   flags +0x74/+0xb8/+0x104/+0x130/+0x17c/
                                  //   +0x1b8/+0x204/+0x230/+0x380 cleared
                                  //   with &= ~1)
    float position_x_03b4;        // +0x3b4 source coords; published world pos
    float position_y_03b8;        //   = +0x3b4+224.0 / +0x3b8+16.0 / +0x3bc
    float position_z_03bc;
    u8 unknown_03c0[0x24];
    float raw_angle_03e4;         // +0x3e4 angle / effect script-id param
    u8 unknown_03e8[8];
    float radius_03f0;            // +0x3f0 (ScanIntroActivations uses
                                  //   radius*0.5 + sweep)
    u8 unknown_03f4[4];
    i32 frame_mirror_03f8;        // +0x3f8 frame accumulator (mirror / int /
    u32 frame_count_03fc;         //   float triple)
    float frame_accum_0400;
    const float *rate_ptr_0404;   // +0x404 POINTER to the rate float (default
                                  //   &g_FrameTimeScale, stored 0x4080bf)
    u32 flags_0408;               // +0x408 (ctor clears bit 0)
    u8 unknown_040c[0x10];
    u32 flags_041c;               // +0x41c (ctor clears bit 0)
    u8 unknown_0420[0x26];
    u16 kind_0446;                // +0x446 0=free / 1,2=pending activation /
                                  //   3=active (scans skip 0 and 3)
    u8 unknown_0448[4];
    u32 group_next_044c;          // +0x44c next pointer within a trigger
                                  //   group (cleared on bind 0x4065c0)
    u8 unknown_0450[0x10];
    u32 group_index_0460;         // +0x460 trigger group index 0..5
    u8 unknown_0464[0x38c];       // +0x464..0x7f0 (bind/clear walks also
                                  //   touch ten stride-0x34 flag dwords in
                                  //   +0x624..+0x7c4 — kept raw)
};

typedef char AssertEffectTriggerRecordSize[
    sizeof(EffectTriggerRecord) == 0x7f0 ? 1 : -1];
typedef char AssertEffectTriggerRecordVmOffset[
    offsetof(EffectTriggerRecord, vm) == 0x8 ? 1 : -1];
typedef char AssertEffectTriggerRecordPosOffset[
    offsetof(EffectTriggerRecord, position_x_03b4) == 0x3b4 ? 1 : -1];
typedef char AssertEffectTriggerRecordKindOffset[
    offsetof(EffectTriggerRecord, kind_0446) == 0x446 ? 1 : -1];
typedef char AssertEffectTriggerRecordGroupIndexOffset[
    offsetof(EffectTriggerRecord, group_index_0460) == 0x460 ? 1 : -1];

struct EffectManagerRoot {
    u32 flags_0000;               // +0x000 (no |= 2 for this root; wiped only)
    u32 field_0004;
    ChainElem *calc_element;      // +0x008 (0x406770, priority 0x14, disabled)
    ChainElem *draw_element;      // +0x00c (0x4067a0, priority 0x1d, disabled)
    void *pool_self_0010;         // +0x010 = this + 0x60 (loader 0x405e5f /
                                  //   teardown 0x405eef)
    void *trigger_group_heads[6]; // +0x014..0x2b (bind pass 0x4065c0)
    void *trigger_group_tails[6]; // +0x02c..0x43
    u8 unknown_0044[0x18];
    u32 bound_record_count_005c;  // +0x05c (cleared then incremented per bind)
    EffectTriggerRecord records[2001]; // +0x060..0x3e0b50
    void *bullet_resource_3e0b50; // +0x3e0b50 RequestManagerWork(7, render
                                  //   owner, "bullet.anm") — the shared
                                  //   spawn/VM handle passed to
                                  //   SpawnStageEffectEdxEbxAbi (0x448db0)
                                  //   and stored into ConditionalState
                                  //   resource_table[0] by 0x40d285
};

typedef char AssertEffectManagerRootSize[
    sizeof(EffectManagerRoot) == 0x3e0b54 ? 1 : -1];
typedef char AssertEffectManagerRootCalcOffset[
    offsetof(EffectManagerRoot, calc_element) == 0x8 ? 1 : -1];
typedef char AssertEffectManagerRootHeadsOffset[
    offsetof(EffectManagerRoot, trigger_group_heads) == 0x14 ? 1 : -1];
typedef char AssertEffectManagerRootRecordsOffset[
    offsetof(EffectManagerRoot, records) == 0x60 ? 1 : -1];
typedef char AssertEffectManagerRootResourceOffset[
    offsetof(EffectManagerRoot, bullet_resource_3e0b50) == 0x3e0b50 ? 1 : -1];

// NOT part of this root (common misattribution): the render-owner large slots
// +0x3ad090..0x3ad09c released by 0x40d530 belong to g_MainChainRenderOwner
// (DAT_00491c10), and the 2198x0x3f0 bullet-slot array at +0x14 belongs to
// the bullet manager DAT_00477818.

} // namespace th10
