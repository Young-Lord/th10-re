#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "GameContext.hpp"
#include "VmRecord.hpp"

namespace th10 {

struct ChainElem;

// ---------------------------------------------------------------------------
// The player state block (TH10 DAT_00477834, 0x4478 bytes, allocated by
// ::operator new(0x4478) in CreatePlayerStateBlock, TH10 0x00424ed0 dtor).
//
// The embedded animation VM record (VmRecord) occupies +0x14..+0x3c0; the
// per-tick game fields start at +0x3c0. The 128 player-shot records at
// +0x49c (0x5c stride, TH10 0x00427e90 spawner / 0x00428280 update /
// 0x00428630 damage pass all index the player pointer + 295 dwords) end
// exactly at the option record bank at +0x32a0.
// ---------------------------------------------------------------------------

// One player shot (0x5c bytes, 128 entries at player+0x49c). Timer shape
// matches TimerNode except that timer_prev is written as the float NaN
// pattern on first arming (TH10 0x00427e90).
struct PlayerShotRecord {
    float timer_prev;            // +0x00 (NaN-initialized)
    i32 timer_count;             // +0x04
    float timer_accum;           // +0x08
    const float *timer_rate;     // +0x0c (-> g_FrameTimeScale)
    u32 timer_latch;             // +0x10 (bit 0 = initialized)
    float position[3];           // +0x14 (z doubles as the 0.1f magnet step)
    float velocity[2];           // +0x20 (polar -> cartesian output)
    u32 field_0028;              // +0x28 (cleared on polar spawn)
    float speed;                 // +0x2c
    float angle;                 // +0x30
    float angle_delta[2];        // +0x34
    u32 angle_flags;             // +0x3c (bit 0 = integrate deltas)
    u32 state;                   // +0x40 (0 free, 1 active, 2 hit/fading)
    u32 entity_id;               // +0x44
    u32 secondary_entity_id;     // +0x48
    void *homing_target;         // +0x4c
    u32 hit_flag;                // +0x50
    u32 magnet_latch;            // +0x54 (set once on first hit)
    void *descriptor;            // +0x58 (.shot descriptor record)
};

typedef char AssertPlayerShotRecordSize[
    sizeof(PlayerShotRecord) == 0x5c ? 1 : -1];
typedef char AssertPlayerShotRecordStateOffset[
    offsetof(PlayerShotRecord, state) == 0x40 ? 1 : -1];
typedef char AssertPlayerShotRecordDescriptorOffset[
    offsetof(PlayerShotRecord, descriptor) == 0x58 ? 1 : -1];

// One option satellite (0x98 bytes, 4 entries at player+0x32a0; field table
// documented at TH10 0x00426f70 / PlayerOptionRecords.cpp).
struct PlayerOptionRecord {
    u32 state;                   // +0x00 (2 = built)
    u8 unknown_0004[0x30];
    float unfocused_position[2]; // +0x34
    float render_position[2];    // +0x3c
    float offset_source_a[2];    // +0x44
    float offset_source_b[2];    // +0x4c
    u8 unknown_0054[0x14];
    u32 sprite_entity_id;        // +0x68
    u32 power_effect_entity_id;  // +0x6c
    u8 unknown_0070[0x10];
    u32 flag_0080;               // +0x80 (bit 0)
    u32 focus_latch;             // +0x84
    u32 option_index;            // +0x88
    u32 tier_latch;              // +0x8c
    void *update_callback;       // +0x90 (per-record tick callback)
    u8 unknown_0094[4];
};

typedef char AssertPlayerOptionRecordSize[
    sizeof(PlayerOptionRecord) == 0x98 ? 1 : -1];
typedef char AssertPlayerOptionRecordSpriteOffset[
    offsetof(PlayerOptionRecord, sprite_entity_id) == 0x68 ? 1 : -1];
typedef char AssertPlayerOptionRecordCallbackOffset[
    offsetof(PlayerOptionRecord, update_callback) == 0x90 ? 1 : -1];

// One sub-effect / bomb damage record (0x6c bytes, 32 entries at
// player+0x350c; TH10 0x00427b50 spawner, damage scan inside 0x00428630).
struct PlayerSubEffectRecord {
    float velocity[2];           // +0x00
    float damage_angle;          // +0x08 (rotated damage-box angle)
    u8 unknown_000c[4];
    float box_width;             // +0x10
    float box_height;            // +0x14
    float position[3];           // +0x18
    u8 motion_block[0x18];       // +0x24 (pos/vel/angle/radius/flags)
    u8 unknown_003c[8];
    i32 timer_prev;              // +0x44
    i32 timer_count;             // +0x48
    float timer_accum;           // +0x4c
    const float *timer_rate;     // +0x50
    u32 timer_latch;             // +0x54 (bit 0)
    i32 value;                   // +0x58 (damage per tick)
    i32 accumulated;             // +0x5c
    i32 limit;                   // +0x60
    i32 period;                  // +0x64
    u8 flags;                    // +0x68 (bit 0 active, bit 2 rotated)
    u8 pad_0069[3];
};

typedef char AssertPlayerSubEffectRecordSize[
    sizeof(PlayerSubEffectRecord) == 0x6c ? 1 : -1];
typedef char AssertPlayerSubEffectRecordValueOffset[
    offsetof(PlayerSubEffectRecord, value) == 0x58 ? 1 : -1];

// The player state block itself.
struct PlayerRecord {
    u8 unknown_0000[8];
    ChainElem *update_element;     // +0x08 (calc chain, priority 0x10)
    ChainElem *draw_element;       // +0x0c (draw chain, priority 0x16)
    void *anm_manager_work;        // +0x10 (pl00/pl01.anm manager work)
    VmRecord anim_vm;              // +0x14 (0x3ac-byte animation VM record)
    float position_x;              // +0x3c0
    float position_y;              // +0x3c4
    float position_z;              // +0x3c8
    i32 position_x_fixed;          // +0x3cc (x100 fixed point)
    i32 position_y_fixed;          // +0x3d0
    i32 speed_unfocused;           // +0x3d4 (x100, from shot data)
    i32 speed_focused;             // +0x3d8
    i32 speed_unfocused_diagonal;  // +0x3dc
    i32 speed_focused_diagonal;    // +0x3e0
    u8 unknown_03e4[0xc];
    i32 step_x;                    // +0x3f0 (per-frame movement step)
    i32 step_y;                    // +0x3f4
    u8 unknown_03f8[0xc];
    float hit_box[6];              // +0x404 {minx,miny,minz,maxx,maxy,maxz}
    float hit_half_extent[3];      // +0x41c
    float graze_half_extent[3];    // +0x428
    float item_half_extent[3];     // +0x434
    float respawn_position[3];     // +0x440
    i32 direction_x;               // +0x44c
    i32 direction_y;               // +0x450
    i32 input_state;               // +0x454 (decoded 0..8)
    i32 mode;                      // +0x458 (0 respawn, 1 normal, 2 death,
                                   //   3 bomb freeze, 4 deathbomb)
    void *shot_data;               // +0x45c (.shot file buffer)
    TimerNode autocollect_timer;   // +0x460 (item line; count<0 = idle)
    TimerNode frame_timer;         // +0x474 (mode tick / damage gate)
    TimerNode move_gate_timer;     // +0x488 (count gated < 4 in movement)
    PlayerShotRecord shots[128];   // +0x49c..0x329c
    u32 focus_glide_entity_id;     // +0x329c
    PlayerOptionRecord options[4]; // +0x32a0..0x34fc
    i32 option_count;              // +0x3500 (power/20, max 4)
    void *homing_target;           // +0x3504
    u8 homing_target_latch;        // +0x3508
    u8 unknown_3509[3];
    PlayerSubEffectRecord sub_effects[32]; // +0x350c..0x428c
    u8 unknown_428c[0x68];
    u32 type3_slot_latches[4];     // +0x42f4
    u8 unknown_4304[4];
    i32 deathbomb_lerp_percent;    // +0x4308
    TimerNode deathbomb_timer;     // +0x430c
    u8 unknown_4320[4];
    float graze_box[6];            // +0x4324
    float item_box[6];             // +0x433c
    float autocollect_box[6];      // +0x4354
    u32 trail_history[66];         // +0x436c (33 {x,y} fixed-point pairs;
                                   //   per-option windows stride 0x40)
    u32 focus_flag;                // +0x4474 (input bit 2, written per frame)
};

typedef char AssertPlayerRecordSize[sizeof(PlayerRecord) == 0x4478 ? 1 : -1];
typedef char AssertPlayerRecordAnimVmOffset[
    offsetof(PlayerRecord, anim_vm) == 0x14 ? 1 : -1];
typedef char AssertPlayerRecordPositionOffset[
    offsetof(PlayerRecord, position_x) == 0x3c0 ? 1 : -1];
typedef char AssertPlayerRecordPositionZOffset[
    offsetof(PlayerRecord, position_z) == 0x3c8 ? 1 : -1];
typedef char AssertPlayerRecordPositionFixedOffset[
    offsetof(PlayerRecord, position_x_fixed) == 0x3cc ? 1 : -1];
typedef char AssertPlayerRecordHitBoxOffset[
    offsetof(PlayerRecord, hit_box) == 0x404 ? 1 : -1];
typedef char AssertPlayerRecordModeOffset[
    offsetof(PlayerRecord, mode) == 0x458 ? 1 : -1];
typedef char AssertPlayerRecordShotDataOffset[
    offsetof(PlayerRecord, shot_data) == 0x45c ? 1 : -1];
typedef char AssertPlayerRecordAutocollectTimerOffset[
    offsetof(PlayerRecord, autocollect_timer) == 0x460 ? 1 : -1];
typedef char AssertPlayerRecordFrameTimerOffset[
    offsetof(PlayerRecord, frame_timer) == 0x474 ? 1 : -1];
typedef char AssertPlayerRecordMoveGateTimerOffset[
    offsetof(PlayerRecord, move_gate_timer) == 0x488 ? 1 : -1];
typedef char AssertPlayerRecordShotsOffset[
    offsetof(PlayerRecord, shots) == 0x49c ? 1 : -1];
typedef char AssertPlayerRecordShotsEndOffset[
    offsetof(PlayerRecord, focus_glide_entity_id) == 0x329c ? 1 : -1];
typedef char AssertPlayerRecordOptionsOffset[
    offsetof(PlayerRecord, options) == 0x32a0 ? 1 : -1];
typedef char AssertPlayerRecordOptionCountOffset[
    offsetof(PlayerRecord, option_count) == 0x3500 ? 1 : -1];
typedef char AssertPlayerRecordHomingTargetOffset[
    offsetof(PlayerRecord, homing_target) == 0x3504 ? 1 : -1];
typedef char AssertPlayerRecordSubEffectsOffset[
    offsetof(PlayerRecord, sub_effects) == 0x350c ? 1 : -1];
typedef char AssertPlayerRecordSubEffectsEndOffset[
    offsetof(PlayerRecord, unknown_428c) == 0x428c ? 1 : -1];
typedef char AssertPlayerRecordType3LatchesOffset[
    offsetof(PlayerRecord, type3_slot_latches) == 0x42f4 ? 1 : -1];
typedef char AssertPlayerRecordDeathbombLerpOffset[
    offsetof(PlayerRecord, deathbomb_lerp_percent) == 0x4308 ? 1 : -1];
typedef char AssertPlayerRecordDeathbombTimerOffset[
    offsetof(PlayerRecord, deathbomb_timer) == 0x430c ? 1 : -1];
typedef char AssertPlayerRecordGrazeBoxOffset[
    offsetof(PlayerRecord, graze_box) == 0x4324 ? 1 : -1];
typedef char AssertPlayerRecordItemBoxOffset[
    offsetof(PlayerRecord, item_box) == 0x433c ? 1 : -1];
typedef char AssertPlayerRecordAutocollectBoxOffset[
    offsetof(PlayerRecord, autocollect_box) == 0x4354 ? 1 : -1];
typedef char AssertPlayerRecordTrailHistoryOffset[
    offsetof(PlayerRecord, trail_history) == 0x436c ? 1 : -1];
typedef char AssertPlayerRecordFocusFlagOffset[
    offsetof(PlayerRecord, focus_flag) == 0x4474 ? 1 : -1];

} // namespace th10
