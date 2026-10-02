// Stage-trigger object per-frame update (0x406240) and spawn-from-
// descriptor (0x4067d0). The sibling interpreter 0x406d90 and the expire
// helpers are in SceneTriggerObject.cpp; the field layout used here is the
// same 0x7f0-stride trigger object record.
#include "SceneTriggerUpdate.hpp"

#include "EclScriptLibrary.hpp"
#include "EffectManagerRoot.hpp"
#include "EntityHelpers.hpp"
#include "PlayerShotData.hpp"
#include "PlayerTimerHelpers.hpp"
#include "SceneTriggerFeatures.hpp"
#include "SceneTriggerObject.hpp"
#include "StageEffectHelpers.hpp"
#include "TimelineRenderObjectSetup.hpp"

namespace th10 {

namespace {

// ---- shared globals (definitions live with the other scene modules) ----

extern void *g_SceneCommandManager;   // TH10 DAT_004776f0 (+0x3e0b50 pool)
extern void *g_ScreenTargetBlock;     // TH10 DAT_00477834
extern void *g_SoundGateContext;      // TH10 DAT_00492590
extern u32 *g_TriggerVmScriptTable;   // TH10 DAT_00474170
extern u32 *g_KindTable4742C0;        // TH10 dword_4742c0 (per-kind mode)
extern u32 *g_KindTable47432C;        // TH10 dword_47432c
extern u32 *g_KindTable474250;        // TH10 dword_474250
extern float *g_KindFloat4741E0;      // TH10 flt_4741e0

// Constant pool -----------------------------------------------------------

// Velocity integrator rate: flt_476F78, the shared frame-time scale
// (default 1.0f; defined alongside the other PRNG/frame globals).
extern float g_FrameTimeScale; // TH10 DAT_00476f78
const float kPi = 3.14159265f;        // flt_470b18
const float kTwoPi = 6.2831855f;      // flt_470b14
const float kHalf = 0.5f;             // flt_470b0c
const float kMinusOne = -1.0f;        // flt_470b60
const float kAnchorScale = 4.0f;      // flt_470c40

// The 0x407xxx feature helpers are semantic bodies in
// SceneTriggerFeatures.cpp (the native call sites set ESI = object; the two
// region-wrap twins run with EBX = object).

// TH10 0x405be0: releases the trigger object (the release thunk runs with
// the object still live in the caller's registers).
extern void ReleaseSceneTriggerObjectAbi(void *object);

// TH10 0x406160: region-exit check against the linked region block
// ([+0x39c]+0x30/+0x34 floats); nonzero means the object leaves the field.
extern i32 CheckSceneTriggerRegionExitAbi(float arg_a, float arg_b);

// TH10 0x44bb20: 16-bit LCG unit draw in [0,1) (combined * 2^-32, ECX =
// 0x4918b0 state pair). Definition below; the state lives with the other
// PRNG globals.
extern u16 g_TimelinePrngStateB[4]; // TH10 DAT_004918b0

// TH10 0x0044bb20. Native ECX = the state pair; returns the combined
// 16-bit draw scaled by 2^-32 (no offset - the centered variant is
// 0x0044bb90).
float RandomUnitFloatAbi()
{
    u16 *const state = g_TimelinePrngStateB;
    u32 x = (static_cast<u16>(*state ^ 0x9630U) - 0x6553U);
    const i32 hi = static_cast<i32>((x >> 14 & 3U) + x * 4U);
    u16 lo = static_cast<u16>((static_cast<u16>(hi) ^ 0x9630U) + 0x9aadU);
    *state = static_cast<u16>(hi);
    lo = static_cast<u16>((lo >> 14) + lo * 4U);
    *reinterpret_cast<i32 *>(state + 2) += 2;
    *state = lo;
    const u32 combined = static_cast<u32>(hi) * 65536U + lo;
    float value = static_cast<float>(combined);
    if (static_cast<i32>(combined) < 0)
        value += 4294967296.0f;
    return value * (1.0f / 4294967296.0f);
}

// TH10 0x44bc10 (SceneTriggerFeatures.cpp): two-float angle wrap; the
// spawn fallback stores the wrapped x angle into the record's +0x3e4.

// TH10 0x004266b0 (defined in EclScriptLibrary.cpp): EAX/EDX/ECX usercall
// checking the timeout region; returns 0/1/2.
extern i32 CheckEnemyTimeoutRegionEaxEdxEcxAbi(const float *half,
                                               void *player_block,
                                               const float *position);

// TH10 0x0043dd10 (defined in EclScriptLibrary.cpp): EBX = sound id,
// ESI = sound manager, stack = float payload.
extern void EnqueueSoundEffectEbxStackAbi(u32 sound_id, void *sound_manager,
                                          float value);

inline u32 LoadU32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline void StoreU32(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

inline i32 LoadI32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline i16 LoadI16(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i16 *>(bytes + offset);
}

inline u16 LoadU16(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u16 *>(bytes + offset);
}

inline void StoreU16(u8 *bytes, u32 offset, u16 value)
{
    *reinterpret_cast<u16 *>(bytes + offset) = value;
}

inline float LoadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void StoreFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

// Lazy timer-block initialization for the {prev, count, acc, rate, flags}
// records whose rate field holds a *pointer* to the shared rate float
// (flt_476f78), guarded by flag bit 0 at offset +0x10.
void LazyInitPointerRateTimer(u8 *base, u32 offset)
{
    const u32 flags = LoadU32(base, offset + 0x10U);
    if ((flags & 1U) == 0U) {
        StoreU32(base, offset, 0xFFF0BDC1U);   // prev = -999999 sentinel
        StoreU32(base, offset + 4U, 0U);
        StoreU32(base, offset + 8U, 0U);
        StoreU32(base, offset + 0xcU, 0x476F78U);
        StoreU32(base, offset + 0x10U, flags | 1U);
    }
}

} // namespace

// TH10 0x406240. Native stdcall, one stack argument = the trigger object;
// the outgoing argument slot doubles as the effect-id output for 0x448db0.
// The object is a 0x7f0-byte EffectTriggerRecord pool record; modeled
// fields go through the typed view, the +0x3c0 screen-anchor velocity
// triple and the +0x43c opcode-flag dword sit in unmodeled regions and
// stay raw.
i32 UpdateSceneTriggerObjectStackAbi(void *object)
{
    u8 *const obj = static_cast<u8 *>(object);
    EffectTriggerRecord &rec = *static_cast<EffectTriggerRecord *>(object);

    if ((rec.flags_0000 & 8U) != 0U)
        goto release;

    if (rec.kind_0446 == 2U) {
        // Screen-space drift while entering; when the vm.reg_10000 gate
        // (+0x314) opens, fall into the active-state body below.
        rec.position_x_03b4 = rec.position_x_03b4
            + g_FrameTimeScale * LoadFloat(obj, 0x3c0U) * 0.5f;
        rec.position_y_03b8 = rec.position_y_03b8
            + g_FrameTimeScale * LoadFloat(obj, 0x3c4U) * 0.5f;
        rec.position_z_03bc = rec.position_z_03bc
            + g_FrameTimeScale * LoadFloat(obj, 0x3c8U) * 0.5f;
        if (rec.vm.reg_10000 == 0)
            goto tail;
        rec.kind_0446 = 1U;
    } else if (rec.kind_0446 == 3U) {
        // Exiting: same integration, then straight to the tail.
        rec.position_x_03b4 = rec.position_x_03b4
            + g_FrameTimeScale * LoadFloat(obj, 0x3c0U) * 0.5f;
        rec.position_y_03b8 = rec.position_y_03b8
            + g_FrameTimeScale * LoadFloat(obj, 0x3c4U) * 0.5f;
        rec.position_z_03bc = rec.position_z_03bc
            + g_FrameTimeScale * LoadFloat(obj, 0x3c8U) * 0.5f;
        goto tail;
    }

    // Active state: instruction queue, then the flag-driven features.
    RunSceneTriggerInstructionQueueEcxAbi(obj);

    {
        const u32 flags = LoadU32(obj, 0x43cU);
        if (flags != 0U) {
            if ((flags & 1U) != 0U)
                UpdateSceneTriggerLaunchSlowdownEsiAbi(obj);
            if ((flags & 0x10U) != 0U)
                UpdateSceneTriggerAccelerationEsiAbi(obj);
            if ((flags & 0x20U) != 0U)
                UpdateSceneTriggerAngleTurnEsiAbi(obj);
            if ((flags & 0x40U) != 0U)
                UpdateSceneTriggerSlowStopRelaunchEsiAbi(obj);
            if ((flags & 0x100U) != 0U)
                UpdateSceneTriggerSlowStopFixedAngleEsiAbi(obj);
            // Native gate: `test al, al; jns` on the low byte — flag bit
            // 0x80, not 0x80000000.
            if ((flags & 0x80U) != 0U)
                UpdateSceneTriggerSlowStopAimPlayerEsiAbi(obj);
            if ((flags & 0x8000C00U) != 0U)
                UpdateSceneTriggerWallBounceEsiAbi(obj);
            if ((flags & 0x4000000U) != 0U)
                UpdateSceneTriggerHomingTurnEsiAbi(obj);
            if ((flags & 0x8000U) != 0U) {
                if (LoadI32(obj, 0x71cU) > 0)
                    ShiftTimerByEsiStackAbi(obj + 0x718U, -1.0f);
                else
                    StoreU32(obj, 0x43cU, flags ^ 0x8000U);
            }
        }
    }

    // World-space drift (no screen scaling) after the queue run.
    rec.position_x_03b4 = rec.position_x_03b4
        + g_FrameTimeScale * LoadFloat(obj, 0x3c0U);
    rec.position_y_03b8 = rec.position_y_03b8
        + g_FrameTimeScale * LoadFloat(obj, 0x3c4U);
    rec.position_z_03bc = rec.position_z_03bc
        + g_FrameTimeScale * LoadFloat(obj, 0x3c8U);

    if ((rec.flags_0000 & 2U) != 0U) {
        // Timeout-region check against the screen target block.
        const i32 region = CheckEnemyTimeoutRegionEaxEdxEcxAbi(
            &rec.radius_03f0,
            g_ScreenTargetBlock,
            &rec.position_x_03b4);
        if (region == 1) {
            const i32 script = LoadI32(obj, 0x438U);
            rec.kind_0446 = 3U;
            rec.vm.state_word = 1U;
            if (script >= 0)
                SpawnStageEffectEdxEbxAbi(g_SceneCommandManager,
                                          &rec.position_x_03b4,
                                          script);
        } else if (region == 2 && (rec.flags_0000 & 4U) == 0U) {
            rec.flags_0000 |= 4U;
            SpawnStageEffectEdxEbxAbi(g_SceneCommandManager,
                                      &rec.position_x_03b4,
                                      434);
            EnqueueSoundEffectEbxStackAbi(0x1cU, g_SoundGateContext,
                                          rec.position_x_03b4);
        }
    }

tail:
    if (LoadU32(obj, 0x39cU) != 0U) {
        const u32 flags = LoadU32(obj, 0x43cU);
        if ((flags & 0x100000U) != 0U)
            WrapSceneTriggerRegionXEbxAbi(obj);
        if ((flags & 0x200000U) != 0U)
            WrapSceneTriggerRegionYEbxAbi(obj);
        const u8 *region =
            reinterpret_cast<const u8 *>(LoadU32(obj, 0x39cU));
        if (LoadI32(obj, 0x434U) <= 0
            && CheckSceneTriggerRegionExitAbi(LoadFloat(region, 0x34U),
                                              LoadFloat(region, 0x30U))
                   != 0)
            goto release;
    }

    if (rec.activation_gate_0004 != 0U)
        rec.activation_gate_0004 -= 1U;
    if (LoadI32(obj, 0x434U) > 0)
        StoreU32(obj, 0x434U,
                 static_cast<u32>(LoadI32(obj, 0x434U) - 1));

    if (FinalizeTimelineRenderObjectSetup(&rec.vm) != 0)
        goto release;
    return 0;

release:
    ReleaseSceneTriggerObjectAbi(obj);
    return -1;
}

// TH10 0x4067d0. Native stdcall ret 0x10 with the descriptor in EBX.
// The manager is the effect manager root (DAT_004776f0); the record
// cursor is the root's +0x10 pool-self pointer into the 0x7f0-byte
// EffectTriggerRecord pool at +0x60. Modeled fields go through typed
// views; descriptor parsing and unmodeled record regions stay raw.
i32 SpawnSceneTriggerFromDescriptorEbxStackAbi(void *manager,
                                               void *descriptor, i32 column,
                                               i32 row, float arg_c)
{
    u8 *const desc = static_cast<u8 *>(descriptor);
    EffectManagerRoot &mgr = *static_cast<EffectManagerRoot *>(manager);

    // ---- free-slot scan (budget 2000 probes; sentinel state 5 wraps) ----
    EffectTriggerRecord *rec =
        static_cast<EffectTriggerRecord *>(mgr.pool_self_0010);
    i32 scanned = 0;
    bool found = false;
    if (rec->kind_0446 == 0U) {
        found = true;
    } else {
        while (!found) {
            for (u32 probe = 1U; probe <= 5U; ++probe) {
                ++rec;
                if (rec->kind_0446 == 5U)
                    rec = mgr.records;
                if (rec->kind_0446 == 0U) {
                    scanned += static_cast<i32>(probe);
                    found = true;
                    break;
                }
                if (probe == 5U) {
                    scanned += 5;
                    if (scanned >= 0x7d0)
                        return 1;
                }
            }
        }
        if (scanned >= 0x7d0)
            return 1;
    }

    // ---- spawn y (var_10): interpolated across the row count ----
    float pos_y;
    const i32 row_count = LoadI16(desc, 0x1f6U);
    if (row_count > 1) {
        const float y0 = LoadFloat(desc, 0x18U);
        const float y1 = LoadFloat(desc, 0x1cU);
        pos_y = y0 - (y0 - y1) * static_cast<float>(row)
                / static_cast<float>(row_count);
    } else {
        pos_y = LoadFloat(desc, 0x18U);
    }

    // ---- spawn x (var_14) by movement mode. Cases 2..5 chain through the
    // native fallthroughs, so the case-3/case-5 direct entries read an
    // uninitialized stack float (stale_x below is only defined when the
    // case-2/case-4 body set it). ----
    float stale_x = 0.0f;   // native var_14 stack slot
    bool stale_valid = false;
    float pos_x = 0.0f;
    const u16 move_mode = LoadU16(desc, 0x1f8U);
    const i16 para_count = LoadI16(desc, 0x1f4U);
    const float speed = LoadFloat(desc, 0x14U);
    const float x0 = LoadFloat(desc, 0x10U);
    const float x1 = LoadFloat(desc, 0x14U);
    const float y0 = LoadFloat(desc, 0x18U);
    const float y1 = LoadFloat(desc, 0x1cU);

    switch (move_mode) {
    case 0U:
    case 1U: {
        float t;
        if ((LoadU16(desc, 0x1f4U) & 1U) != 0U) {
            t = static_cast<float>((column + 1) / 2);
        } else {
            t = static_cast<float>(column / 2) + kHalf;
        }
        t *= speed;
        if ((column & 1) != 0)
            t *= kMinusOne;
        if (move_mode == 0U)
            t += arg_c;
        pos_x = t + x0;
        break;
    }
    case 2U:
        // case 2 sets the slot to arg_c, then falls into the case-3 body.
        stale_x = arg_c;
        stale_valid = true;
        // fallthrough
    case 3U:
        if (!stale_valid) {
            // Direct case-3 entry: the slot is uninitialized stack memory.
            float uninitialized_slot; // native quirk, never pre-set
            stale_x = uninitialized_slot;
        }
        // Native 0x406974 (jump table case 3): column * flt_470b14
        // (2*pi) / para_count — NOT pi (flt_470b18).
        pos_x = static_cast<float>(column) * kTwoPi
                    / static_cast<float>(para_count)
            + stale_x + static_cast<float>(row) * speed + x0;
        break;
    case 4U:
        // case 4 sets the slot to arg_c, then falls into the case-5 body.
        stale_x = arg_c;
        stale_valid = true;
        // fallthrough
    case 5U:
        if (!stale_valid) {
            float uninitialized_slot; // native quirk, never pre-set
            stale_x = uninitialized_slot;
        }
        // Native 0x4069ae (jump table case 5): the leading term is
        // flt_470b18 (pi) / para_count, but the column term uses
        // flt_470b14 (2*pi) / para_count.
        pos_x = kPi / static_cast<float>(para_count) + stale_x
                + static_cast<float>(column) * kTwoPi
                      / static_cast<float>(para_count)
                + static_cast<float>(row) * speed + x0;
        break;
    case 6U:
        pos_x = x1 + (x0 - x1) * RandomUnitFloatAbi();
        break;
    case 7U:
        pos_y = y1 + (y0 - y1) * RandomUnitFloatAbi();
        // Native 0x406a09 (jump table case 7): column * flt_470b14 (2*pi).
        pos_x = static_cast<float>(column) * kTwoPi
                    / static_cast<float>(para_count)
            + static_cast<float>(row) * speed + x0;
        break;
    case 8U:
        pos_x = x1 + (x0 - x1) * RandomUnitFloatAbi();
        pos_y = y1 + (y0 - y1) * RandomUnitFloatAbi();
        break;
    default:
        // The default tail only latches the state words below.
        pos_x = 0.0f;
        break;
    }

    // ---- common record initialization ----
    EffectTriggerRecord &r = *rec;
    // Byte alias for the unmodeled record regions (+0x3c0 screen anchors,
    // +0x40c second timer block, +0x434/+0x438/+0x43c opcode state,
    // +0x454/+0x458, +0x464 queue, +0x7ea/+0x7ec, +0x3d8/+0x3f4).
    u8 *const rec_bytes = reinterpret_cast<u8 *>(rec);

    r.flags_0000 |= 1U;
    r.kind_0446 = 1U;

    // Lazy timer-block init stays raw: it seeds the &flt_476f78 rate
    // pointer literal and covers both the modeled +0x3f8 block (flags word
    // = flags_0408) and the unmodeled +0x40c block (flags word =
    // flags_041c).
    LazyInitPointerRateTimer(rec_bytes, 0x3f8U);
    r.frame_count_03fc = 0U;
    r.frame_accum_0400 = 0.0f;
    *reinterpret_cast<u32 *>(&r.frame_mirror_03f8) = 0xffffffffU;
    LazyInitPointerRateTimer(rec_bytes, 0x40cU);
    StoreU32(rec_bytes, 0x410U, 0U);
    StoreU32(rec_bytes, 0x414U, 0U);
    StoreU32(rec_bytes, 0x40cU, 0xffffffffU);

    StoreFloat(rec_bytes, 0x3d8U, pos_y);
    r.raw_angle_03e4 = WrapAngleSumStackAbi(pos_x, 0.0f);

    r.position_x_03b4 = LoadFloat(desc, 4U);
    r.position_y_03b8 = LoadFloat(desc, 8U);
    r.position_z_03bc = LoadFloat(desc, 0xcU);
    r.position_z_03bc = 0.1f;
    SetPolarVelocityThisAbi(reinterpret_cast<float *>(rec_bytes + 0x3c0U),
                            pos_x, pos_y);

    StoreU32(rec_bytes, 0x43cU, LoadU32(desc, 0x1fcU));
    StoreU16(rec_bytes, 0x7ecU, LoadU16(desc, 2U));
    StoreU16(rec_bytes, 0x7eaU, LoadU16(desc, 0U));
    r.flags_0000 = (r.flags_0000 & 0xfffffff3U) | 2U;
    StoreU32(rec_bytes, 0x454U, 0U);

    // VM bind: script = kind table + sub-kind offset on the +8 VM.
    const u32 kind = static_cast<u32>(LoadI16(desc, 0));
    const i32 script = static_cast<i32>(g_TriggerVmScriptTable[kind])
        + static_cast<i32>(LoadI16(desc, 2));
    InitializePlayerMainVmEsiStackAbi(
        &r.vm, mgr.bullet_resource_3e0b50, script);

    // Per-kind dispatch for the +0x438 dword.
    switch (g_KindTable4742C0[kind]) {
    case 0U:
        StoreU32(rec_bytes, 0x438U,
                 static_cast<u32>(2 * LoadI16(desc, 2) + 0x11));
        break;
    case 1U:
        StoreU32(rec_bytes, 0x438U,
                 g_KindTable47432C[static_cast<u32>(LoadI16(desc, 2))]);
        break;
    case 2U:
        StoreU32(rec_bytes, 0x438U, static_cast<u32>(-1));
        break;
    case 3U:
        StoreU32(rec_bytes, 0x438U, 0x1dU);
        break;
    case 4U:
        StoreU32(rec_bytes, 0x438U, 0x13U);
        break;
    default:
        break;
    }
    r.group_index_0460 = g_KindTable474250[kind];
    StoreU32(rec_bytes, 0x458U, LoadU32(desc, 0x204U));
    StoreU32(rec_bytes, 0x434U, 10U);
    r.radius_03f0 = g_KindFloat4741E0[kind];
    StoreFloat(rec_bytes, 0x3f4U, g_KindFloat4741E0[kind]);

    // Anchor-mode bits from the descriptor flags (word +0x30c; state 2 for
    // any of the anchor bits, otherwise the state word stays 1).
    const u32 desc_flags = LoadU32(desc, 0x1fcU);
    u16 anchor_word = 2U;
    if ((desc_flags & 2U) != 0U) {
        anchor_word = 7U;
        r.kind_0446 = 2U;
    } else if ((desc_flags & 4U) != 0U) {
        anchor_word = 8U;
        r.kind_0446 = 2U;
    } else if ((desc_flags & 8U) != 0U) {
        anchor_word = 9U;
        r.kind_0446 = 2U;
    }
    r.vm.state_word = anchor_word;
    if (anchor_word != 2U) {
        // world -= anchor * flt_470c40 (4.0)
        StoreFloat(rec_bytes, 0x3c0U,
                   LoadFloat(rec_bytes, 0x3c0U) * kAnchorScale);
        StoreFloat(rec_bytes, 0x3c4U,
                   LoadFloat(rec_bytes, 0x3c4U) * kAnchorScale);
        StoreFloat(rec_bytes, 0x3c8U,
                   LoadFloat(rec_bytes, 0x3c8U) * kAnchorScale);
        r.position_x_03b4 =
            r.position_x_03b4 - LoadFloat(rec_bytes, 0x3c0U);
        r.position_y_03b8 =
            r.position_y_03b8 - LoadFloat(rec_bytes, 0x3c4U);
        r.position_z_03bc =
            r.position_z_03bc - LoadFloat(rec_bytes, 0x3c8U);
    }

    // Queue copy: the whole 18-entry instruction stream, then run it.
    for (u32 i = 0; i < 0x6cU; ++i)
        StoreU32(rec_bytes, 0x464U + 4U * i, LoadU32(desc, 0x20U + 4U * i));
    StoreU32(rec_bytes, 0x440U, desc_flags);
    StoreU32(rec_bytes, 0x43cU, 0U);
    StoreU32(rec_bytes, 0x45cU, LoadU32(desc, 0x208U));

    RunSceneTriggerInstructionQueueEcxAbi(rec);
    (void)FinalizeTimelineRenderObjectSetup(&r.vm);

    // Advance the manager cursor; the state-5 sentinel wraps it to the
    // pool base.
    ++rec;
    if (rec->kind_0446 == 5U)
        mgr.pool_self_0010 = mgr.records;
    else
        mgr.pool_self_0010 = rec;
    return 0;
}

} // namespace th10
