#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "GameContext.hpp"

namespace th10 {

// ---------------------------------------------------------------------------
// The ECL script object (vtable 0x46d0c0 live / 0x46d0d8 destruction plant),
// 0x2518 bytes. Allocated by ::operator new(0x2518) in
// CreateEclScriptObjectEaxStackAbi (0x0040cfb0 — the null-result is not
// checked, a native quirk), initialized by 0x0040d830 (which memsets the
// 0x14dc-byte working sub-record at +0x103c), per-frame ticker 0x0040d750
// (walks the ConditionalState list, head at state+0x58), in-place teardown
// 0x0040dae0, scalar deleting dtor 0x0040cc50. List node embedded at
// +0x116c. The object has NO global home — it lives in the ConditionalState
// script list and is handed around by pointer.
//
// IMPORTANT: +0x10..+0x100f is dispatcher scratch — the creator, ctor,
// ticker and teardown never touch it, and the VmRecord field naming does NOT
// apply to this object (agent-verified 2026-10-03).
// ---------------------------------------------------------------------------

// vec3 eased-anim block (0x4c), ticked by 0x00404610. Same 0.99/1.01 rate
// window and -999999 poison as the other interpolation tickers; completion
// sets timer=duration, prev=duration-1, accum=(float)duration, duration=0.
// Modes: 7 add-delta, 8 cubic Hermite, 17 integrating velocity.
struct EclVec3AnimBlock {
    float cur[3];            // +0x00
    float dst[3];            // +0x0c
    float handle1[3];        // +0x18
    float handle2_vel[3];    // +0x24
    i32 timer_prev;          // +0x30 (poison -999999 -> -1)
    u32 timer_count;         // +0x34
    float timer_accum;       // +0x38
    const float *timer_rate; // +0x3c (-> g_FrameTimeScale 0x476f78)
    u32 flags;               // +0x40 (bit 0 = armed/init latch)
    i32 duration;            // +0x44 (nonzero = armed)
    i32 mode;                // +0x48
};

// vec2 eased-anim block (0x3c), ticked by 0x00412ac0 (7-field tail).
struct EclVec2AnimBlock {
    float cur[2];            // +0x00
    float dst[2];            // +0x08
    float handle1[2];        // +0x10
    float handle2_vel[2];    // +0x18
    i32 timer_prev;          // +0x20
    u32 timer_count;         // +0x24
    float timer_accum;       // +0x28
    const float *timer_rate; // +0x2c
    u32 flags;               // +0x30
    i32 duration;            // +0x34 (nonzero = armed)
    i32 mode;                // +0x38
};

// The working sub-record at record+0x103c, 0x14d8 bytes (ctor memsets 0x14dc
// including the +0x2514 self slot, which is then overwritten).
struct EclScriptWork {
    u32 working_block_0000[11];   // copied from base block every tick
                                  //   (qmemcpy 0x2c bytes, 0x40dcbb)
    float base_pos_002c[3];       // record+0x1068 (player-homing reads it;
                                  //   HUD boss-X)
    float base_velocity_0038[3];  // anchor-sum minus base
    u8 unknown_0044[0x14];
    float anchor1_pos_0058[3];    // creator: descriptor[0..2]
    float anchor1_delta_0064[3];
    i32 anchor1_radius_0070;      // vec2-block A0 output
    float anchor1_angle_0074;     // wrapped via 0x44bc70
    i32 anchor1_radius2_0078;     // vec2-block A2 output
    float anchor1_angle2_007c;
    u8 anchor1_flags_0080;        // bit 0 = polar mode
    u8 unknown_0081[3];
    float anchor2_pos_0084[3];    // shifted by 0x491e6c/e70/e74 under
                                  //   flags_1444 bit 0x40000
    float anchor2_delta_0090[3];
    i32 anchor2_radius_009c;      // vec2-block A1 output
    float anchor2_angle_00a0;
    i32 anchor2_radius2_00a4;     // vec2-block A3 output
    float anchor2_angle2_00a8;
    u8 anchor2_flags_00ac;
    u8 unknown_00ad[3];
    float hitbox_params_00b0[4];  // 24.0f x4 (ctor); passed to 0x428630
                                  //   (item-collision damage) / 0x4266b0
    u32 published_ids_00c0[10];   // first 8 get position publishes
                                  //   (0x40e383..0x40e3d4); teardown releases
                                  //   all 10 via the render-owner lists
    u32 field_00e8;               // 4-byte gap before the mode gate
    i32 mode_gate_00ec;           // ==1 enables the sub-mode table
    i32 sub_mode_00f0;            // 0/0x14/0x31->359; 5/0x19/0x32->356;
                                  //   0xa/0x1e/0x33->362; 0xf/0x23/0x34->365
    i32 bind_id_00f4;             // published from bind ids (0x40e0c6)
    i32 facing_dir_00f8;          // -1/0/1 direction machine
    u32 descriptor_vars_00fc[8];  // creator descriptor+0x20 block: 4x i32 +
                                  //   4x f32 = ECL variables -9985..-9978
    TimerNode frame_tail_011c;    // poison-seeded frame counter (0x40d95a)
    void *list_self_0130;         // +0x116c list node (state+0x58 head)
    void *list_next_0134;
    void *list_prev_0138;
    EclVec3AnimBlock vec3_a_013c;   // arm = duration at sub+0x180
    EclVec3AnimBlock vec3_b_0188;   // arm = duration at sub+0x1cc
    EclVec2AnimBlock vec2_a0_01d4;  // -> anchor1 radius/angle (sub+0x70/74)
    EclVec2AnimBlock vec2_a1_0210;  // -> anchor2 radius/angle (sub+0x9c/a0)
    EclVec2AnimBlock vec2_a2_024c;  // -> anchor1 second pair (sub+0x78/7c)
    EclVec2AnimBlock vec2_a3_0288;  // -> anchor2 second pair (sub+0xa4/a8)
    u8 command_slots_02c4[8][0x210];// reset with -1 at slot+0x204 (0x40cc70)
    u8 unknown_1344[0x60];
    float hitbox_size_13a4[2];    // screen gate +-192 / 0..448
    float clamp_center_13ac[2];   // flags bit 0x200 clamp rect
    float clamp_half_extent_13b4[2];
    i32 death_score_13bc;         // creator descriptor[3]; passed to 0x409d90
                                  //   on death (0x40e23d reads sub+0x13bc)
    i32 hp_13c0;                  // creator descriptor[5]; ECL var -9954
    i32 unknown_13c4;
    i32 unknown_13c8;
    i32 kind_13cc;                // creator descriptor[4]; flag 0x8000
                                  //   remaps 1->10, 4->11
    u8 unknown_13d0[0x30];
    float sprite_size_1400[2];    // 32.0f defaults (ctor 0x40d93d)
    i32 layer_variant_1408;       // (ConditionalState aux_count & 1) + 2
    i32 table_value_140c;         // 359/356/362/365 per sub_mode
    i32 resource_index_1410;      // indexes ConditionalState resource_table
                                  //   (record+0x244c; init 0)
    i32 hit_flicker_timer_1414;   // set 4 on hit, decrements (0x40e496)
    u8 unknown_1418[4];
    TimerNode shift_timer_a_141c; // count seeded 2, accum 2.0f; ticked by
                                  //   0x44bf40(-1.0) while count > 0
    TimerNode shift_timer_b_1430;
    u32 flags_1444;               // 0x1, 0x4 off-screen exempt, 0x8 no HP
                                  //   drain, 0x40 no death, 0x100 on-screen
                                  //   latch (byte +0x2482 = var -9986),
                                  //   0x200 clamp, 0x400 run gate,
                                  //   0x800/0x1000/0x2000/0x8000 (kind remap
                                  //   + published-id release + sound),
                                  //   0x20000 static, 0x40000 base shift,
                                  //   0x100000/0x200000 bind publish
    u32 primary_bind_id_1448;
    u32 alternate_bind_id_144c;
    i32 published_id_index_1450;  // init -1; indexes ConditionalState
                                  //   published_ids (UNBOUNDED natively)
    i32 field_1454;
    u8 request_slots_1458[8][16]; // ctor seeds {-1, -1, 0, untouched} per slot
};

// Full 0x2518-byte ECL script object.
struct EclScriptObject {
    void *vtable_0000;            // 0x46d0c0 live; 0x46d0d8 planted by
                                  //   teardown 0x40dc34
    void *bind_node_self_0004;    // -> +0x0008
    i32 bind_node_0008;           // 0; cleared on rebind 0x40e2b7
    i32 script_table_id_000c;     // resolved via 0x450470 (name -> id)
    u8 dispatcher_scratch_0010[0x1000]; // untouched by creator/ctor/ticker/
                                        // teardown (do NOT model VmRecord
                                        // fields here)
    i32 field_1010;
    i32 field_1014;
    i32 sentinel_1018;            // -1
    void *self_101c;              // back-pointer
    i32 field_1020;
    u8 difficulty_mask_1024;      // 1 << dword_474c74
    u8 unknown_1025[3];
    u32 flags_1028;               // bit 0 cleared at ctor
    void *name_registry_102c;     // ConditionalState->name_registry_0054
    void *bind_node_ptr_1030;     // -> +0x0008
    void *alloc_list_1034;        // malloc'd {name-buffer, next} list head;
                                  //   bit 0 of the ptr = "has allocations"
    i32 field_1038;
    EclScriptWork work;           // +0x103c..0x2514
    void *self_2514;              // ctor 0x40d89f; read as the record handle
};

typedef char AssertEclVec3AnimBlockSize[
    sizeof(EclVec3AnimBlock) == 0x4c ? 1 : -1];
typedef char AssertEclVec2AnimBlockSize[
    sizeof(EclVec2AnimBlock) == 0x3c ? 1 : -1];
typedef char AssertEclScriptWorkSize[
    sizeof(EclScriptWork) == 0x14d8 ? 1 : -1];
typedef char AssertEclScriptObjectSize[
    sizeof(EclScriptObject) == 0x2518 ? 1 : -1];
typedef char AssertEclScriptObjectWorkOffset[
    offsetof(EclScriptObject, work) == 0x103c ? 1 : -1];
typedef char AssertEclScriptObjectSelfOffset[
    offsetof(EclScriptObject, self_2514) == 0x2514 ? 1 : -1];
typedef char AssertEclScriptWorkBasePosOffset[
    offsetof(EclScriptWork, base_pos_002c) == 0x2c ? 1 : -1];
typedef char AssertEclScriptWorkAnchor1Offset[
    offsetof(EclScriptWork, anchor1_pos_0058) == 0x58 ? 1 : -1];
typedef char AssertEclScriptWorkPublishedIdsOffset[
    offsetof(EclScriptWork, published_ids_00c0) == 0xc0 ? 1 : -1];
typedef char AssertEclScriptWorkListNodeOffset[
    offsetof(EclScriptWork, list_self_0130) == 0x130 ? 1 : -1];
typedef char AssertEclScriptWorkVec3AOffset[
    offsetof(EclScriptWork, vec3_a_013c) == 0x13c ? 1 : -1];
typedef char AssertEclScriptWorkVec2A0Offset[
    offsetof(EclScriptWork, vec2_a0_01d4) == 0x1d4 ? 1 : -1];
typedef char AssertEclScriptWorkCommandSlotsOffset[
    offsetof(EclScriptWork, command_slots_02c4) == 0x2c4 ? 1 : -1];
typedef char AssertEclScriptWorkDeathScoreOffset[
    offsetof(EclScriptWork, death_score_13bc) == 0x13bc ? 1 : -1];
typedef char AssertEclScriptWorkHpOffset[
    offsetof(EclScriptWork, hp_13c0) == 0x13c0 ? 1 : -1];
typedef char AssertEclScriptWorkKindOffset[
    offsetof(EclScriptWork, kind_13cc) == 0x13cc ? 1 : -1];
typedef char AssertEclScriptWorkFlagsOffset[
    offsetof(EclScriptWork, flags_1444) == 0x1444 ? 1 : -1];
typedef char AssertEclScriptWorkRequestSlotsOffset[
    offsetof(EclScriptWork, request_slots_1458) == 0x1458 ? 1 : -1];

} // namespace th10
