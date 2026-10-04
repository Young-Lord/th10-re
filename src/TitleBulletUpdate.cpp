// Bullet-manager per-frame update cluster (TH10 0x0041afd0 / 0x0041ba00 /
// 0x0041ba50) plus the small effect/score helpers it drives (0x0041beb0,
// 0x00426660, 0x00405b60, 0x0041be80, 0x00418930, 0x0043dd10, 0x00412ff0)
// and the two boundaries 0x004054b0 / 0x0042b9c0. Every offset below is
// from the native disassembly. Slot payloads are accessed through the
// typed BulletSlot/BulletManager views (src/BulletManager.hpp; layout
// MSVC-asserted there): the bullet array is 0x896 slots of 0x3f0 bytes
// at manager+0x14 (the region zeroed by the title calc body game-start
// reset), with live_count_21ceb4 and spawn_counter_21cebc just past the
// array.
//
// Control-flow note: the native jumps straight back to the loop-advance
// (skipping the per-bullet animation-VM tick and distance bookkeeping)
// for the state-0 skip, the state-5 spawn/delay paths and every exit of
// the on-screen bonus switch; only the "option state 2" early-out and the
// off-screen paths run the tick. This reconstruction mirrors that with
// explicit goto targets.
#include <math.h>

#include "BulletManager.hpp"
#include "EntityHelpers.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "PlayerOptionRecords.hpp"
#include "PlayerRecord.hpp"
#include "ResultScreenScript.hpp"
#include "TimelineRenderObjects.hpp"
#include "Th10Types.hpp"

#include <cmath>
#include "TitleBulletUpdate.hpp"
#include "TitleScreenObject.hpp"

namespace th10 {

namespace {

// ---- globals ------------------------------------------------------------

extern void *g_OptionPositionManager; // TH10 DAT_00477834
extern void *g_StageRecordHolder;     // TH10 DAT_004776f0
extern void *g_PointItemDigitState;   // TH10 DAT_00477840
extern void *g_GameModeRecord;        // TH10 DAT_00477810
extern u32 g_SceneGateFlags;          // TH10 DAT_00474e5c
extern void *g_EntityManager;         // TH10 DAT_00491c40
extern void *g_AsciiHudOwner;         // TH10 DAT_0047770c
extern void *g_HudFragmentSlotOwner;  // TH10 DAT_0047773c (pointer value)
extern u8 g_EffectKindTable;          // TH10 word_4749fe (kind table)
extern u32 g_CurrentDifficulty;       // TH10 DAT_00474c74
extern float g_SceneFadeScale;        // TH10 flt_476f78 (rate scale)

// ---- float constants (bit patterns read from .rdata) ---------------------

inline float FloatFromBits(u32 bits)
{
    union {
        u32 u;
        float f;
    } converter;
    converter.u = bits;
    return converter.f;
}

const float kLandingLineY = FloatFromBits(1124073472U); // flt_470bf4 = 128.0f
const float kGravityStep = FloatFromBits(1022739087U);  // flt_470cdc = 0.03f
const float kZero = FloatFromBits(0U);                  // flt_470b04 = 0.0f
const float kTwo = FloatFromBits(1073741824U);          // flt_470b08 = 2.0f
const float kTerminalX = FloatFromBits(1139539968U);    // flt_470be0 = 472.0f
const float kAccelGate = FloatFromBits(1094713344U);    // flt_470cd8 = 12.0f
const float kAccelStep = FloatFromBits(1045220557U);    // flt_470c38 = 0.2f
const float kBonusLineY = FloatFromBits(1125122048U);   // flt_470cd4 = 144.0f
const float kBonusScale = FloatFromBits(995595318U);    // flt_470cd0
const float kHalf = FloatFromBits(1056964608U);         // flt_470b0c = 0.5f
const float kBonusBias = FloatFromBits(1167867904U);    // flt_470ce0 = 5000.0f
const float kRetargetScale = FloatFromBits(1051372203U); // flt_470cc4 = 0.35f
const float kIntAdvanceLow = FloatFromBits(1065185444U); // flt_470b68
const float kIntAdvanceHigh = FloatFromBits(1065437102U); // flt_470b64
const float kOne = FloatFromBits(1065353216U);          // flt_470afc = 1.0f

// ---- field access helpers ------------------------------------------------

u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

i32 LoadI32At(const void *base, u32 offset)
{
    return static_cast<i32>(LoadU32At(base, offset));
}

void StoreI32At(void *base, u32 offset, i32 value)
{
    StoreU32At(base, offset, static_cast<u32>(value));
}

float LoadFloatAt(const void *base, u32 offset)
{
    return *reinterpret_cast<const float *>(
        static_cast<const u8 *>(base) + offset);
}

void StoreFloatAt(void *base, u32 offset, float value)
{
    *reinterpret_cast<float *>(static_cast<u8 *>(base) + offset) = value;
}

// ---- boundaries (native ABIs noted at each site) -------------------------

// TH10 0x00404f30. EAX = script index (the bullet kind + 0x176), ESI =
// bullet record, stack = [g_StageRecordHolder] + 0x3e0b50.
extern void InitializePlayerScriptVmEaxEsiStackAbi(i32 script_index,
                                                   void *bullet,
                                                   u32 argument);
// TH10 0x0043ee30. One stack argument = the bullet record; ticks the
// bullet's ASCII animation VM pair.
extern void TickBulletAnimationVmStackAbi(void *bullet);
// TH10 0x0044bf40 (shared ShiftTimerByEsiStackAbi body; native ESI = timer).
extern void ShiftTimerRecordBy(void *timer, float delta);
// TH10 0x004188a0. EAX = the 0x474c40 frame-state block, ECX = increment.
// The semantic body lives in TitleCalcCluster.cpp; declared here as a
// boundary because that body is file-local to its translation unit.
extern void AwardExtendLifeEaxEcxAbi(void *frame_state, i32 increment);
// TH10 0x00448db0. usercall (EDX = bullet position slot, stack = script
// list pointer, out-id pointer, kind 393); returns the out pointer.
extern u32 *SpawnBulletDeathEntityEdxStackAbi(void *slot, u32 script_id,
                                              u32 *out_id, i32 kind);

// Slot count of the bullet array walked by the frame update (0x41afd0:
// ESI = manager+0x14, stride 0x3f0, 0x896 iterations).
const u32 kBullets = 0x896U;

// Shared fixed addresses of the 0x474c40 frame-state block and the
// 0x492590 effect manager (the native embeds them as immediates).
const void *kFrameState = reinterpret_cast<const void *>(0x00474C40U);
const void *kEffectManager = reinterpret_cast<const void *>(0x00492590U);

// Native state-1/2/3/4 movement: position += scale * velocity, with the
// scale being the flt_476f78 rate (the z component last).
void MoveByScaledVelocity(BulletSlot &slot)
{
    slot.position_x_03ac = slot.position_x_03ac
        + g_SceneFadeScale * slot.velocity_x_03b8;
    slot.position_y_03b0 = slot.position_y_03b0
        + g_SceneFadeScale * slot.velocity_y_03bc;
    slot.position_z_03b4 = slot.position_z_03b4
        + g_SceneFadeScale * slot.velocity_z_03c0;
}

} // namespace

// TH10 0x0041ba00.
i32 BulletCalcRecordCallbackEcxStackAbi(void *bullet_manager)
{
    void *mode_record = g_GameModeRecord; // TH10 DAT_00477810
    if (mode_record != 0) {
        const TitleScreen &ts = *reinterpret_cast<const TitleScreen *>(mode_record);
        const u32 flags = ts.flags;
        if ((((flags | (flags >> 2)) & 1U) != 0U)
            || ((flags & 0x400U) != 0U)) {
            return 1;
        }
    }
    return UpdateBulletManagerStackAbi(bullet_manager);
}

// TH10 0x0041afd0.
i32 UpdateBulletManagerStackAbi(void *bullet_manager)
{
    BulletManager &mgr = *reinterpret_cast<BulletManager *>(bullet_manager);

    mgr.spawn_counter_21cebc = 0;
    mgr.live_count_21ceb4 = 0;

    // Deferred "run 0x0041ba50" flag (native var_54), set when the
    // life-fragment ladder crosses 100 inside the bonus switch.
    i32 deferred_cleanup = 0;

    for (u32 index = 0; index != kBullets; ++index) {
        BulletSlot &slot = mgr.slots[index];
        const i32 state = slot.state_03dc;
        if (state == 0)
            goto advance; // native: straight to the loop tail, no tick

        if (state == 5) {
            // Spawn pending: count the delay down; when it expires,
            // initialize the bullet's player script VM (0x00404f30).
            slot.spawn_delay_03ec = slot.spawn_delay_03ec - 1;
            if (slot.spawn_delay_03ec >= 0)
                goto advance;
            slot.state_03dc = 2;
            InitializePlayerScriptVmEaxEsiStackAbi(
                slot.kind_03e0 + 0x176, &slot,
                LoadU32At(*static_cast<void **>(g_StageRecordHolder),
                              0x3E0B50U));
            goto advance;
        }

        {
            void *opt_mgr = g_OptionPositionManager;
            PlayerRecord &player =
                *reinterpret_cast<PlayerRecord *>(opt_mgr);
            if (state == 1) {
                if (player.mode != 2
                    && player.mode != 4
                    && player.position_y < kLandingLineY) {
                    // Home in on the player and fall through to the
                    // state-3 angle update (with the option-state 4 exit).
                    slot.speed_03e8 = LoadFloatAt(player.shot_data, 8U);
                    slot.state_03dc = 3;
                    goto state3;
                }
                MoveByScaledVelocity(slot);
                slot.velocity_y_03bc = slot.velocity_y_03bc
                    + g_SceneFadeScale * kGravityStep;
                if (slot.velocity_y_03bc >= kZero)
                    slot.velocity_x_03b8 = 0.0f; // native stores ecx == 0
                if (slot.velocity_y_03bc > kTwo)
                    slot.velocity_y_03bc = kTwo;
                if (slot.velocity_x_03b8 > kTerminalX) {
                    slot.state_03dc = 0;
                    goto advance; // native: no tick on deactivation
                }
                goto kind_gate;
            }
            if (state == 2) {
                MoveByScaledVelocity(slot);
                slot.velocity_y_03bc = slot.velocity_y_03bc
                    + g_SceneFadeScale * kGravityStep;
                if (slot.velocity_y_03bc >= kZero) {
                    slot.speed_03e8 = LoadFloatAt(player.shot_data, 8U);
                    slot.state_03dc = 3;
                    goto state3;
                }
                if (slot.position_y_03b0 > kTerminalX) {
                    slot.state_03dc = 0;
                    AddPowerValueEaxEcxAbi(
                        const_cast<void *>(kFrameState), -4);
                    goto advance; // native: no tick
                }
                goto kind_gate;
            }
            if (state == 4) {
                const float angle = AngleToPlayerPositionEaxEcxAbi(
                    &slot.position_x_03ac, opt_mgr);
                SetPolarVectorThiscall(
                    &slot.velocity_x_03b8, angle, slot.speed_03e8);
                MoveByScaledVelocity(slot);
                if (slot.speed_03e8 >= kAccelGate)
                    slot.speed_03e8 = slot.speed_03e8 + kAccelStep;
                if (player.mode == 4) {
                    slot.state_03dc = 1;
                    slot.velocity_x_03b8 = 0.0f;
                    slot.velocity_y_03bc = 0.0f;
                }
                goto kind_gate;
            }
            // state 3 (and the state-1/2 transitions into it).
        state3:
            {
                const float dx = player.position_x
                    - slot.position_x_03ac;
                const float dy = player.position_y
                    - slot.position_y_03b0;
                float angle;
                if (dx == kZero && dy == kZero)
                    // 0x3FC90FDB is pi/2 (1.5707963f), not pi; the native
                    // materializes 1.5707964 at 0x41b21d for the both-zero
                    // atan2 fallback (state-3 retarget, 0x41b1e7 block).
                    angle = FloatFromBits(0x3FC90FDBU); // pi/2
                else
                    angle = static_cast<float>(
                        std::atan2(static_cast<double>(dy),
                                   static_cast<double>(dx)));
                SetPolarVectorThiscall(
                    &slot.velocity_x_03b8,
                    angle, slot.speed_03e8);
                MoveByScaledVelocity(slot);
                if (slot.speed_03e8 >= kAccelGate)
                    slot.speed_03e8 = slot.speed_03e8 + kAccelStep;
                if (player.mode == 4) {
                    slot.state_03dc = 1;
                    slot.velocity_x_03b8 = 0.0f;
                    slot.velocity_y_03bc = 0.0f;
                }
            }
        }

    kind_gate:
        if ((*reinterpret_cast<PlayerRecord *>(
                g_OptionPositionManager)).mode == 2)
            goto tick; // native 0x41b7fd path: VM tick still runs

        {
            // Play-field rectangle [edx+0x4330, edx+0x4324] x
            // [edx+0x4334, edx+0x4328].
            void *opt_mgr = g_OptionPositionManager;
            PlayerRecord &player =
                *reinterpret_cast<PlayerRecord *>(opt_mgr);
            const float x = slot.position_x_03ac;
            const float y = slot.position_y_03b0;
            const bool inside = x <= player.graze_box[0]
                && y <= player.graze_box[1]
                && x > player.graze_box[3]
                && y > player.graze_box[4];
            if (!inside)
                goto offscreen;

            const i32 kind = slot.kind_03e0;
            void *const frame = const_cast<void *>(kFrameState);
            switch (kind) {
            case 1:
            case 10: {
                const i32 crossed = AddLifeFragmentEaxStackAbi(frame, 1);
                const i32 word = LoadI32At(frame, 8U);
                const i32 quotient = word / 20;
                const i32 rem = word - quotient * 20;
                RunItemGetDigitAnimEdxEsiStackAbi(quotient, g_AsciiHudOwner,
                                                  rem * 5);
                if (crossed != 0) {
                    RebuildPlayerOptionRecords(g_OptionPositionManager);
                    QueueBulletDeathEffectEbxEsiStackAbi(
                        0x1D, const_cast<void *>(kEffectManager),
                        slot.position_x_03ac);
                    SetPointItemDigitsEaxEdiEsiStackAbi(
                        -480, &slot.velocity_x_03b8,
                        g_PointItemDigitState,
                        static_cast<i32>(0xFFFFFF40U));
                    if (word >= 100)
                        deferred_cleanup = 1;
                    AddPowerValueEaxEcxAbi(frame, 0x0C);
                } else {
                    SetPointItemDigitsEaxEdiEsiStackAbi(
                        (word < 0 ? -word : word) / 2,
                        &slot.velocity_x_03b8,
                        g_PointItemDigitState,
                        static_cast<i32>(0xFFFF4040U));
                }
                AdvanceScorePopupTimerEdiStackAbi(frame, 0x3C);
                break;
            }
            case 2: {
                i32 value;
                i32 color;
                i32 power_delta;
                if ((*reinterpret_cast<PlayerRecord *>(
                        g_OptionPositionManager)).position_y
                    >= kBonusLineY) {
                    const i32 piv = LoadI32At(frame, 0x0CU);
                    value = piv * 10 - ((piv * 10) % 10);
                    color = static_cast<i32>(0xFFFFFF00U);
                    power_delta = 8;
                } else {
                    const float py = (*reinterpret_cast<PlayerRecord *>(
                        g_OptionPositionManager)).position_y;
                    const i32 piv = LoadI32At(frame, 0x0CU);
                    const i32 scaled = static_cast<i32>(
                        (py - kBonusLineY) * kBonusScale
                        * (static_cast<float>(piv) * kHalf - kBonusBias));
                    value = piv * 10;
                    value -= value % 10;
                    value = value / 2 - scaled;
                    value -= value % 10;
                    color = -1;
                    power_delta = 1;
                }
                SetPointItemDigitsEaxEdiEsiStackAbi(
                    value, &slot.velocity_x_03b8,
                    g_PointItemDigitState, color);
                AddPowerValueEaxEcxAbi(frame, power_delta);
                AddScoreBlockValueEcxStackAbi(frame, value);
                AdvanceScorePopupTimerEdiStackAbi(frame, 0x64);
                break;
            }
            case 5: {
                const i32 piv = LoadI32At(frame, 0x0CU);
                const i32 value = piv * 10 - ((piv * 10) % 10);
                SetPointItemDigitsEaxEdiEsiStackAbi(
                    value, &slot.velocity_x_03b8,
                    g_PointItemDigitState, static_cast<i32>(0xFFFFFF00U));
                AddPowerValueEaxEcxAbi(frame, 8);
                AddScoreBlockValueEcxStackAbi(frame, value);
                AdvanceScorePopupTimerEdiStackAbi(frame, 0x64);
                break;
            }
            case 3: {
                i32 value = 5000;
                if (g_CurrentDifficulty == 2U)
                    value = 8000;
                else if (g_CurrentDifficulty == 3U
                         || g_CurrentDifficulty == 4U)
                    value = 10000;
                SetPointItemDigitsEaxEdiEsiStackAbi(
                    value, &slot.velocity_x_03b8,
                    g_PointItemDigitState, static_cast<i32>(0xFF00FF00U));
                AddPivValueEcxStackAbi(frame, value);
                AdvanceScorePopupTimerEdiStackAbi(frame, 0x78);
                break;
            }
            case 8:
                AddPivValueEcxStackAbi(frame, 10);
                AdvanceScorePopupTimerEdiStackAbi(frame, 3);
                AddScoreBlockValueEcxStackAbi(frame, 10);
                break;
            case 9:
                SetPointItemDigitsEaxEdiEsiStackAbi(
                    100, &slot.velocity_x_03b8,
                    g_PointItemDigitState, static_cast<i32>(0xFF00FF00U));
                AddPivValueEcxStackAbi(frame, 100);
                AdvanceScorePopupTimerEdiStackAbi(frame, 0x3C);
                break;
            case 4:
            case 11: {
                const i32 crossed = AddLifeFragmentEaxStackAbi(frame, 0x14);
                const i32 word = LoadI32At(frame, 8U);
                const i32 quotient = word / 20;
                const i32 rem = word - quotient * 20;
                RunItemGetDigitAnimEdxEsiStackAbi(quotient, g_AsciiHudOwner,
                                                  rem * 5);
                if (crossed != 0) {
                    RebuildPlayerOptionRecords(g_OptionPositionManager);
                    QueueBulletDeathEffectEbxEsiStackAbi(
                        0x1D, const_cast<void *>(kEffectManager),
                        slot.position_x_03ac);
                    SetPointItemDigitsEaxEdiEsiStackAbi(
                        -480, &slot.velocity_x_03b8,
                        g_PointItemDigitState,
                        static_cast<i32>(0xFFFFFF40U));
                    AddPowerValueEaxEcxAbi(frame, 0x18);
                    if (word >= 100)
                        deferred_cleanup = 1;
                    AdvanceScorePopupTimerEdiStackAbi(frame, 0x14);
                } else {
                    SetPointItemDigitsEaxEdiEsiStackAbi(
                        (word < 0 ? -word : word) / 2,
                        &slot.velocity_x_03b8,
                        g_PointItemDigitState,
                        static_cast<i32>(0xFFFF4040U));
                    AdvanceScorePopupTimerEdiStackAbi(frame, 0x14);
                }
                break;
            }
            case 7:
                // 0x004188a0 extend helper (semantic body reconstructed in
                // TitleCalcCluster.cpp) with the one-extend increment.
                AwardExtendLifeEaxEcxAbi(frame, 1);
                AddPowerValueEaxEcxAbi(frame, 0x100);
                break;
            default:
                // Case 6 and every unhandled kind.
                QueueBulletDeathEffectEbxEsiStackAbi(
                    0x14, const_cast<void *>(kEffectManager),
                    slot.position_x_03ac);
                slot.state_03dc = 0;
                break;
            }
            goto advance; // every switch exit skips the VM tick
        }

    offscreen:
        if (slot.state_03dc == 4 || slot.state_03dc == 3)
            goto tick;
        {
            void *opt_mgr = g_OptionPositionManager;
            PlayerRecord &player =
                *reinterpret_cast<PlayerRecord *>(opt_mgr);
            const u32 gate = g_SceneGateFlags & 4U;
            const float x = slot.position_x_03ac;
            const float y = slot.position_y_03b0;
            bool retarget;
            if (gate != 0U) {
                retarget = x <= player.item_box[0]
                    && y <= player.item_box[1]
                    && x > player.item_box[3]
                    && y > player.item_box[4];
                if (!retarget)
                    goto tick; // native: cx != 0 -> plain tick
            } else {
                retarget = x <= player.autocollect_box[0]
                    && y <= player.autocollect_box[1]
                    && x > player.autocollect_box[3]
                    && y > player.autocollect_box[4];
            }
            if (retarget) {
                slot.state_03dc = 4;
                slot.speed_03e8 = LoadFloatAt(player.shot_data, 8U)
                    * kRetargetScale;
            }
        }

    tick:
        TickBulletAnimationVmStackAbi(&slot);

        // Distance bookkeeping: counter copy, then either an integer frame
        // advance or a fractional advance re-deriving the counter from the
        // float. Native 0x41b810-0x41b82c: fcomp kIntAdvanceLow with
        // `test ah,0x41; jnz` (speed <= 0.99f -> fractional), then fcomp
        // kIntAdvanceHigh with `test ah,5; jp` (speed < 1.01f -> fractional),
        // so the integer path runs iff speed > 0.99f && speed >= 1.01f.
        // That mixed >=/> pair is faithful, not an inversion: the first
        // compare is redundant (1.01f > 0.99f), and the effective threshold
        // is speed >= 1.01f — this is not a [0.99, 1.01] window.
        slot.timer_03c8.prev = slot.timer_03c8.count;
        {
            const float speed = *slot.timer_03c8.rate;
            if (speed > kIntAdvanceLow && speed >= kIntAdvanceHigh) {
                slot.timer_03c8.count = slot.timer_03c8.count + 1;
                *reinterpret_cast<float *>(&slot.timer_03c8.accum) =
                    *reinterpret_cast<const float *>(
                        &slot.timer_03c8.accum) + kOne;
            } else {
                *reinterpret_cast<float *>(&slot.timer_03c8.accum) =
                    *reinterpret_cast<const float *>(
                        &slot.timer_03c8.accum) + speed;
                slot.timer_03c8.count = static_cast<i32>(
                    *reinterpret_cast<const float *>(
                        &slot.timer_03c8.accum));
            }
        }

    advance:
        mgr.live_count_21ceb4 = mgr.live_count_21ceb4 + 1;
    }

    if (deferred_cleanup != 0)
        KillPendingBulletsEsiAbi(bullet_manager);
    return 1;
}

// TH10 0x0041ba50.
void KillPendingBulletsEsiAbi(void *bullet_manager)
{
    // The native walks the first 150 slots through an edi biased to
    // manager+0x3c0 with stride 0x3f0, so its +0x30/+0x34 test dwords are
    // the state_03dc/kind_03e0 of the same slots the frame update walks.
    // Kinds 1/4 respawn as kind 9 and kinds 10/11 (0xa/0xb) as kind 5
    // (0x41baa5-0x41babb); the spawn tail is shared: state zeroed
    // (0x41ba77 — position_x_03ac is NOT cleared), then the 0x41bb00
    // spawner with color -1, angle -pi/2 (0xbfc90fdb) and speed 2.2
    // (0x400ccccd), and one death-entity spawn (kind 393) per killed
    // bullet.
    BulletManager &mgr = *reinterpret_cast<BulletManager *>(bullet_manager);
    for (u32 index = 0; index != 150U; ++index) {
        BulletSlot &slot = mgr.slots[index];
        if (slot.state_03dc == 0)
            continue;
        const i32 kind = slot.kind_03e0;
        i32 respawn_kind;
        if (kind == 1 || kind == 4)
            respawn_kind = 9;
        else if (kind == 10 || kind == 11)
            respawn_kind = 5;
        else
            continue; // native: straight to the loop tail (0x41bae2)
        slot.state_03dc = 0;
        SpawnExplosionParticleEaxEcxEfxAbi(
            bullet_manager, &slot.position_x_03ac, respawn_kind,
            0xFFFFFFFFU, -1.5707964f, 2.2f);
        u32 spawned = 0;
        SpawnBulletDeathEntityEdxStackAbi(
            &slot.position_x_03ac,
            // Native 0x41bacc-0x41bad2 (and 0x41bc2c-0x41bc31 in the
            // spawner) reads the effect manager root's
            // bullet_resource_3e0b50 as [g_StageRecordHolder] + 0x3e0b50,
            // the same source the 0x41afd0 body uses.
            LoadU32At(*static_cast<void **>(g_StageRecordHolder),
                      0x3E0B50U),
            &spawned, 393);
    }
}

// TH10 0x0041beb0.
void *SetPolarVectorThiscall(float *vector, float angle, float radius)
{
    vector[0] = static_cast<float>(std::cos(static_cast<double>(angle)))
        * radius;
    vector[1] = static_cast<float>(std::sin(static_cast<double>(angle)))
        * radius;
    return vector;
}

// TH10 0x00426660.
float AngleToPlayerPositionEaxEcxAbi(const float *position, void *manager)
{
    PlayerRecord &player = *reinterpret_cast<PlayerRecord *>(manager);
    const float dx = player.position_x - position[0];
    const float dy = player.position_y - position[1];
    if (dy == 30.0f && dx == 30.0f)
        return 1.75f;
    return static_cast<float>(std::atan2(static_cast<double>(dy),
                                         static_cast<double>(dx)));
}

// TH10 0x00405b60.
void AddPowerValueEaxEcxAbi(void *frame_state, i32 delta)
{
    i32 value = LoadI32At(frame_state, 0x58U) + delta;
    if (value > 1024)
        value = 1024;
    else if (value < -1024)
        value = -1024;
    StoreI32At(frame_state, 0x58U, value);
}

// TH10 0x0041be80.
i32 AddPivValueEcxStackAbi(void *frame_state, i32 value)
{
    i32 result = value / 10 + LoadI32At(frame_state, 0x0CU);
    StoreI32At(frame_state, 0x0CU, result);
    if (result > 99999)
        StoreI32At(frame_state, 0x0CU, 99999);
    return result;
}

// TH10 0x00418930.
i32 AddLifeFragmentEaxStackAbi(void *frame_state, i16 increment)
{
    i16 current = static_cast<i16>(LoadU32At(frame_state, 8U) & 0xFFFFU);
    if (current >= 100)
        return 0;
    const i16 updated = static_cast<i16>(current + increment);
    StoreU32At(frame_state, 8U,
               (LoadU32At(frame_state, 8U) & 0xFFFF0000U)
                   | static_cast<u32>(static_cast<u16>(updated)));
    if (updated > 100) {
        StoreU32At(frame_state, 8U,
                   (LoadU32At(frame_state, 8U) & 0xFFFF0000U) | 100U);
        u32 *const slot_base =
            *static_cast<u32 **>(g_HudFragmentSlotOwner);
        ReleaseEntityById(g_EntityManager, LoadU32At(slot_base, 40472U));
        StoreU32At(slot_base, 40472U, 0U);
        // Native 0x448d00 third stack argument (15) is stored to
        // entity+0x20; the shared spawn helper does not model that dword
        // (same known omission as TitleCalcCluster.cpp).
        i32 *const spawned = SpawnSetupEffectVmListABack(
            static_cast<i32>(LoadU32At(slot_base, 40648U)), 73);
        StoreU32At(slot_base, 40472U, static_cast<u32>(*spawned));
    }
    const i16 after = static_cast<i16>(LoadU32At(frame_state, 8U)
                                       & 0xFFFFU);
    return ((after - increment) / 20 != after / 20) ? 1 : 0;
}

// TH10 0x0043dd10.
i32 QueueBulletDeathEffectEbxEsiStackAbi(i32 kind, void *manager,
                                         float offset)
{
    u8 *const base = static_cast<u8 *>(manager);
    const i32 table_entry = static_cast<i16>(
        *reinterpret_cast<const u16 *>(&g_EffectKindTable + 8U * kind));
    const i32 encoded = static_cast<i32>(offset * -990.0);

    i32 slot = 0;
    for (;;) {
        const i32 existing = LoadI32At(base, 1568U + 4U * slot);
        if (existing < 0) {
            StoreI32At(base, 1568U + 4U * slot, kind);
            StoreI32At(base, 1032U + 4U * kind, table_entry);
            StoreI32At(base, 1664U + (slot << 9), encoded);
            StoreI32At(base, 1616U + 4U * slot,
                       LoadI32At(base, 1616U + 4U * slot) + 1);
            return encoded;
        }
        if (existing == kind)
            break;
        if (++slot >= 12)
            return encoded;
    }
    const i32 count = LoadI32At(base, 1616U + 4U * slot);
    if (count < 128) {
        StoreI32At(base, 1664U + 4U * (count + (slot << 7)), encoded);
        StoreI32At(base, 1616U + 4U * slot, count + 1);
    }
    return encoded;
}

// TH10 0x00412ff0.
void AdvanceScorePopupTimerEdiStackAbi(void *frame_state, i32 frames)
{
    u8 *const base = static_cast<u8 *>(frame_state);
    if (LoadI32At(base, 0x18U) < 130) {
        ShiftTimerRecordBy(base + 0x14U, static_cast<float>(frames));
        if (LoadI32At(base, 0x18U) > 130) {
            const u32 flags = LoadU32At(base, 0x24U);
            if ((flags & 1U) == 0U) {
                StoreI32At(base, 0x18U, 0);
                StoreI32At(base, 0x14U, -999999);
                StoreI32At(base, 0x1CU, 0);
                StoreU32At(base, 0x20U,
                           reinterpret_cast<u32>(&g_SceneFadeScale));
                StoreU32At(base, 0x24U, flags | 1U);
            }
            StoreI32At(base, 0x18U, 130);
            StoreU32At(base, 0x1CU, 1124204544U); // 150.0f
            StoreI32At(base, 0x14U, 129);
        }
    }
}
} // namespace th10
