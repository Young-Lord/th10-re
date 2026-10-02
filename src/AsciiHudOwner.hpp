#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "VmRecord.hpp"
#include "GameContext.hpp"

namespace th10 {

struct ChainElem;

// ---------------------------------------------------------------------------
// The ASCII HUD owner (TH10 DAT_0047770c, 0x9ed0 bytes, allocated by
// ::operator new(0x9ed0) in CreateAsciiHudOwner 0x00414830, ctor 0x00413810,
// in-place dtor 0x004145f0).
//
// It is NOT a scaled-down LargeRenderOwner: the layout prefix differs
// (flags + two scheduler elements at +8/+0xc) and the 43 VmRecords live in
// six fixed arrays plus one auxiliary record — 10/10/9/4/2/7 + 1 — instead
// of a cursor-managed pool. All HUD drawing is dispatched into the
// 0x491c10 LargeRenderOwner; the entity lists it resolves against belong
// to the separate owners DAT_00491c10 / DAT_00491c40.
// ---------------------------------------------------------------------------

// One spell-progress bar entry ({value, color} read as owner+0x9e94+i*8).
struct AsciiHudSpellBarEntry {
    float value;
    u32 color;
};

typedef char AssertAsciiHudSpellBarEntrySize[
    sizeof(AsciiHudSpellBarEntry) == 8 ? 1 : -1];

struct AsciiHudOwner {
    u32 flags_0000;                    // +0x000 (ctor sets bit 1)
    u32 field_0004;
    ChainElem *calc_element;           // +0x008 (calc chain slot 0x18)
    ChainElem *draw_element;           // +0x00c (draw chain slot 0x2b)
    VmRecord pool_a[10];               // +0x010 (life / upper score digits)
    VmRecord pool_b[10];               // +0x24c8 (lower score digits)
    VmRecord pool_c[9];                // +0x4980 (boss/stage HUD pool)
    VmRecord pool_d[4];                // +0x6a8c (life gauge digits)
    VmRecord pool_e[2];                // +0x793c (spell timer digits)
    VmRecord pool_f[7];                // +0x8094 (score digit row)
    VmRecord aux_vm;                   // +0x9a48 (boss overlay VM)
    u32 result_digit_handles[8];       // +0x9df4 (MSD first; 0x4000000 kill
                                       //   flag walk in the dtor)
    u32 first_banner_handle;           // +0x9e14 (scripts 0x47/0x48/0x4c)
    u32 second_banner_handle;          // +0x9e18 (scripts 0x49/0x4a/0x4b)
    u8 unknown_9e1c[8];
    u32 stage_boss_handle;             // +0x9e24 (scripts 0x85..0x8d)
    u32 bench_child_handles[10];       // +0x9e28 (scripts 0x5b.., bounded by
                                       //   bench_child_count)
    u32 script_102_overlay_handle;     // +0x9e50
    u8 unknown_9e54[4];
    u32 background_vm_id;              // +0x9e58 (script 0)
    u32 background_vm_id_1;            // +0x9e5c (script 1; read-only)
    TimerNode hud_timer;               // +0x9e60 (best-score gate: count>=20)
    u32 best_score_mirror;             // +0x9e74 (DAT_00474c40 copy)
    u32 displayed_score;               // +0x9e78 (0x421f60 write / 0x417040
                                       //   advance; a scene sub-timer copy
                                       //   shares this dword in
                                       //   OpenSceneScriptResource)
    u32 score_display_rate;            // +0x9e7c (diff/32, clamp 578910)
    void *stage_script_work;           // +0x9e80 (RequestManagerWork slot 28)
    float boss_hp_fill;                // +0x9e84 (+0.025/frame, clamped)
    float boss_hp_fraction;            // +0x9e88
    i32 boss_hp_raw;                   // +0x9e8c
    u32 bench_child_count;             // +0x9e90
    AsciiHudSpellBarEntry spell_bars[4]; // +0x9e94 (renderer reads +i*8; the
                                       //   0x9e9c open-script-handle store and
                                       //   the 0x9ea8 result-list script id
                                       //   overlap bars[1].value / bars[2].color)
    u32 hud_mode_flags;                // +0x9eb4 (bit0 enter latch, bits1-2
                                       //   spell banner, bit3 bench, bit4
                                       //   render-mode/registration, bit5
                                       //   result-screen active)
    void *result_script_state;         // +0x9eb8 (0x90 block from 0x415b00;
                                       //   null = in gameplay)
    void *result_script_blob;          // +0x9ebc (base of [blob+8*slot+4])
    i32 spell_countdown;               // +0x9ec0 (-1 = idle, clamped 99)
    i32 last_spell_countdown;          // +0x9ec4 (drives states 7/8/9)
    void *front_anm_work;              // +0x9ec8 (front.anm manager work,
                                       //   slot 6; the ANM resource argument)
    u32 render_mode_counter;           // +0x9ecc (120-frame render-mode gate)
};

typedef char AssertAsciiHudOwnerSize[sizeof(AsciiHudOwner) == 0x9ed0 ? 1 : -1];
typedef char AssertAsciiHudOwnerPoolAOffset[
    offsetof(AsciiHudOwner, pool_a) == 0x10 ? 1 : -1];
typedef char AssertAsciiHudOwnerPoolBOffset[
    offsetof(AsciiHudOwner, pool_b) == 0x24c8 ? 1 : -1];
typedef char AssertAsciiHudOwnerPoolCOffset[
    offsetof(AsciiHudOwner, pool_c) == 0x4980 ? 1 : -1];
typedef char AssertAsciiHudOwnerPoolDOffset[
    offsetof(AsciiHudOwner, pool_d) == 0x6a8c ? 1 : -1];
typedef char AssertAsciiHudOwnerPoolEOffset[
    offsetof(AsciiHudOwner, pool_e) == 0x793c ? 1 : -1];
typedef char AssertAsciiHudOwnerPoolFOffset[
    offsetof(AsciiHudOwner, pool_f) == 0x8094 ? 1 : -1];
typedef char AssertAsciiHudOwnerAuxVmOffset[
    offsetof(AsciiHudOwner, aux_vm) == 0x9a48 ? 1 : -1];
typedef char AssertAsciiHudOwnerAuxVmEndOffset[
    offsetof(AsciiHudOwner, result_digit_handles) == 0x9df4 ? 1 : -1];
typedef char AssertAsciiHudOwnerFirstBannerOffset[
    offsetof(AsciiHudOwner, first_banner_handle) == 0x9e14 ? 1 : -1];
typedef char AssertAsciiHudOwnerStageBossOffset[
    offsetof(AsciiHudOwner, stage_boss_handle) == 0x9e24 ? 1 : -1];
typedef char AssertAsciiHudOwnerScript102Offset[
    offsetof(AsciiHudOwner, script_102_overlay_handle) == 0x9e50 ? 1 : -1];
typedef char AssertAsciiHudOwnerBackgroundVmIdOffset[
    offsetof(AsciiHudOwner, background_vm_id) == 0x9e58 ? 1 : -1];
typedef char AssertAsciiHudOwnerHudTimerOffset[
    offsetof(AsciiHudOwner, hud_timer) == 0x9e60 ? 1 : -1];
typedef char AssertAsciiHudOwnerDisplayedScoreOffset[
    offsetof(AsciiHudOwner, displayed_score) == 0x9e78 ? 1 : -1];
typedef char AssertAsciiHudOwnerStageScriptWorkOffset[
    offsetof(AsciiHudOwner, stage_script_work) == 0x9e80 ? 1 : -1];
typedef char AssertAsciiHudOwnerBossHpFillOffset[
    offsetof(AsciiHudOwner, boss_hp_fill) == 0x9e84 ? 1 : -1];
typedef char AssertAsciiHudOwnerBenchChildCountOffset[
    offsetof(AsciiHudOwner, bench_child_count) == 0x9e90 ? 1 : -1];
typedef char AssertAsciiHudOwnerSpellBarsOffset[
    offsetof(AsciiHudOwner, spell_bars) == 0x9e94 ? 1 : -1];
typedef char AssertAsciiHudOwnerHudModeFlagsOffset[
    offsetof(AsciiHudOwner, hud_mode_flags) == 0x9eb4 ? 1 : -1];
typedef char AssertAsciiHudOwnerResultScriptStateOffset[
    offsetof(AsciiHudOwner, result_script_state) == 0x9eb8 ? 1 : -1];
typedef char AssertAsciiHudOwnerResultScriptBlobOffset[
    offsetof(AsciiHudOwner, result_script_blob) == 0x9ebc ? 1 : -1];
typedef char AssertAsciiHudOwnerSpellCountdownOffset[
    offsetof(AsciiHudOwner, spell_countdown) == 0x9ec0 ? 1 : -1];
typedef char AssertAsciiHudOwnerFrontAnmWorkOffset[
    offsetof(AsciiHudOwner, front_anm_work) == 0x9ec8 ? 1 : -1];
typedef char AssertAsciiHudOwnerRenderModeCounterOffset[
    offsetof(AsciiHudOwner, render_mode_counter) == 0x9ecc ? 1 : -1];

} // namespace th10
