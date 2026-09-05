#include "PlayerModeDispatcher.hpp"

#include "PlayerDeathProcessor.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "PlayerStageHelpers.hpp"
#include "PlayerItemMagnet.hpp"
#include "PlayerShotSpawner.hpp"
#include "PlayerMotionHelpers.hpp"
#include "PlayerTimerHelpers.hpp"
#include "PlayerMovement.hpp"
#include "PlayerProjectileManager.hpp"
#include "PlayerOptionRecords.hpp"

#include <cmath>

namespace th10 {

namespace {

inline float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

// Player object fields are addressed with explicit offsets; the layout is
// documented in docs/evidence/player-object-lifecycle.md and
// docs/evidence/player-mode-dispatcher.md.

extern u32 g_PlayerCharacter; // TH10 DAT_00474c68
extern i32 g_PlayerLivesCounter; // TH10 DAT_00474c48 (word ops, 20 per life)
extern i32 g_PlayerLivesRemaining; // TH10 DAT_00474c70 (signed)
extern u8 g_InputMask; // TH10 DAT_00474e5c (bit1 = bomb key)
extern float g_FrameTimeScale; // TH10 DAT_00476f78
extern void *g_GameContext; // TH10 DAT_004776ec
extern void *g_EnemyArrayBase; // TH10 DAT_004776f0
extern void *g_SpellBulletBase; // TH10 DAT_004776f4
extern void *g_BulletListRoot; // TH10 DAT_0047781c
extern void *g_GameStateManager; // TH10 DAT_00477830
extern void *g_GameModeObject; // TH10 DAT_00477838
extern void *g_AsciiHudOwner; // TH10 DAT_0047770c
extern void *g_AsciiHudConditionalState; // TH10 DAT_00477704
extern i32 g_ScorePenaltyCounter; // TH10 DAT_00474c98 (clamp +-0x400)

void UpdatePlayerAnimationVmEcxAbi(void *player, void *vm, void *vm_again); // TH10 0x0043ee30

const float kRateUnityLow = 0.99f; // TH10 DAT_00470b68
const float kRateUnityHigh = 1.01f; // TH10 DAT_00470b64
const float kUnitScale = 0.01f; // TH10 DAT_00470b00
const float kPi = 3.1415927f; // TH10 DAT_00470b18
const float kTwoPi = 6.2831855f; // TH10 DAT_00470b14
const float kAtanFallback = 1.5707964f; // TH10 DAT_00470b94

// TH10 0x00463b2c: x87 conversion rounding half away from zero.
i32 FloatToI32(float value)
{
    return value >= 0.0f
        ? static_cast<i32>(std::floor(static_cast<double>(value) + 0.5))
        : static_cast<i32>(std::ceil(static_cast<double>(value) - 0.5));
}

// Player timer block: prev i32 @0, count i32 @4, accumulator float @8,
// rate pointer @0xc, init flag @0x10 (bit 0). A rate inside (0.99, 1.01)
// steps the count by one; outside the window (or NaN) the count is derived
// from the accumulator, so a ~zero rate freezes it.
void AdvancePlayerTimerBlock(u8 *block)
{
    *reinterpret_cast<i32 *>(block) = *reinterpret_cast<i32 *>(block + 4);
    const float rate = **reinterpret_cast<float *const *>(block + 0xc);
    if (rate > kRateUnityLow && rate < kRateUnityHigh) {
        *reinterpret_cast<float *>(block + 8) =
            *reinterpret_cast<float *>(block + 8) + 1.0f;
        *reinterpret_cast<i32 *>(block + 4) =
            *reinterpret_cast<i32 *>(block + 4) + 1;
    } else {
        const float acc = *reinterpret_cast<float *>(block + 8) + rate;
        *reinterpret_cast<float *>(block + 8) = acc;
        *reinterpret_cast<i32 *>(block + 4) = FloatToI32(acc);
    }
}

// Returns true when the block was initialized by this call.
bool EnsureTimerBlockInitialized(u8 *block)
{
    if ((*reinterpret_cast<u32 *>(block + 0x10) & 1) != 0)
        return false;
    *reinterpret_cast<i32 *>(block + 4) = 0;
    *reinterpret_cast<i32 *>(block) = static_cast<i32>(0xfff0bdc1U);
    *reinterpret_cast<float *>(block + 8) = 0.0f;
    *reinterpret_cast<const float **>(block + 0xc) = &g_FrameTimeScale;
    *reinterpret_cast<u32 *>(block + 0x10) |= 1;
    return true;
}

// The 32-entry option-position history ring at player+0x436c (dword pairs).
void ShiftPositionHistory(u8 *player)
{
    const i32 count = *reinterpret_cast<const i32 *>(player + 0x3500);
    for (i32 index = 31; index > count; --index) {
        *reinterpret_cast<u32 *>(player + 0x436c + index * 8) =
            *reinterpret_cast<const u32 *>(player + 0x436c + (index - 1) * 8);
        *reinterpret_cast<u32 *>(player + 0x4370 + index * 8) =
            *reinterpret_cast<const u32 *>(player + 0x4370 + (index - 1) * 8);
    }
    *reinterpret_cast<u32 *>(player + 0x436c) =
        *reinterpret_cast<const u32 *>(player + 0x3cc);
    *reinterpret_cast<u32 *>(player + 0x4370) =
        *reinterpret_cast<const u32 *>(player + 0x3d0);
}

// Modes 0 and 1 share the stage-start activation pass.
void ActivateStageEnemiesAndBullets(bool require_uninitialized)
{
    u8 *const entry_base = static_cast<u8 *>(g_EnemyArrayBase) + 0x60;
    for (u32 index = 0; index != 2000; ++index) {
        u8 *const entry = entry_base + index * 0x7f0;
        const short kind = *reinterpret_cast<const short *>(entry + 0x446);
        if (kind == 0 || kind == 3)
            continue;
        if (require_uninitialized &&
            *reinterpret_cast<const i32 *>(entry + 4) != 0)
            continue;
        (void)ActivateStageEnemyEsiAbi(entry);
    }
    u8 *const list_root =
        *reinterpret_cast<u8 *const *>(g_BulletListRoot);
    if (list_root != 0) {
        u8 *node = *reinterpret_cast<u8 *const *>(list_root + 0x18);
        for (; node != 0; node = *reinterpret_cast<u8 *const *>(node)) {
            if (*reinterpret_cast<const i32 *>(node + 0xc) != 1) {
                void **const vtable =
                    *reinterpret_cast<void ***>(node);
#if defined(_MSC_VER)
                typedef void (__stdcall *SpawnFnPtr)(void *, i32);
#else
                typedef void __attribute__((stdcall)) SpawnFnT(void *, i32);
                typedef SpawnFnT *SpawnFnPtr;
#endif
                const SpawnFnPtr spawn =
                    reinterpret_cast<SpawnFnPtr>(vtable[5]);
                spawn(node, 0);
            }
        }
    }
}

void RunRespawnIntroBody(u8 *player)
{
    const i32 tick = *reinterpret_cast<const i32 *>(player + 0x478);
    *reinterpret_cast<i32 *>(player + 0x3d0) = 48000 - tick * 8000 / 60;
    *reinterpret_cast<float *>(player + 0x3c4) =
        static_cast<float>(*reinterpret_cast<const i32 *>(player + 0x3d0)) *
        kUnitScale;
    *reinterpret_cast<i32 *>(player + 0x332c) = 1;
    *reinterpret_cast<i32 *>(player + 0x33c4) = 1;
    *reinterpret_cast<i32 *>(player + 0x345c) = 1;
    *reinterpret_cast<i32 *>(player + 0x34f4) = 1;
    ShiftPositionHistory(player);

    if (tick < 30) {
        const float sweep = static_cast<float>(tick) * 17.066668f + 64.0f;
        const float position[3] = {ReadFloat(player, 0x440),
                                   ReadFloat(player, 0x444),
                                   ReadFloat(player, 0x448)};
        ScanIntroActivations(*reinterpret_cast<void *const *>(0x4776f0U),
                             position, sweep, 0, 1);
        ScanIntroActivations(*reinterpret_cast<void *const *>(0x4776f0U),
                             position, static_cast<float>(tick) * 0.25f, 0,
                             0);
        BroadcastEntranceTweenEaxEbxStackAbi(
            *reinterpret_cast<void *const *>(0x4776f0U), position, sweep,
            0);
    } else {
        ActivateStageEnemiesAndBullets(true);
    }
}

void RefreshLivesHud()
{
    const i32 lives = g_PlayerLivesCounter / 20;
    RefreshHudLivesDisplayEdxStackAbi(lives, (lives % 20) * 100 / 20);
}

// Death processor 0x004269d0 is reconstructed separately; the parts this
// dispatcher relies on are its observable mode/timer writes.
void RunDeathExplosionBody(u8 *player, i32 tick);

void RunDeathbombDecisionBody(u8 *player, i32 tick)
{
    if (tick > 8) {
        ProcessPlayerDeathStackAbi(player);
        RunDeathExplosionBody(player,
            *reinterpret_cast<const i32 *>(player + 0x478));
        return;
    }
    if (g_GameContext == 0 ||
        *reinterpret_cast<const i32 *>(
            static_cast<u8 *>(g_GameContext) + 0x28) != 0)
        return;
    if (static_cast<short>(g_PlayerLivesCounter / 20) == 0)
        return;
    if ((g_InputMask & 2) == 0)
        return;
    TickPlayerTimerEaxStackAbi(player + 0x474, 60);
    TickPlayerTimerEaxStackAbi(player + 0x430c, 200);
    (void)TickRespawnDeathEffectStackAbi(g_GameContext);
    g_PlayerLivesCounter -= 20;
    RebuildPlayerOptionRecords(player);
    RefreshLivesHud();
    *reinterpret_cast<i32 *>(player + 0x458) = 1;
}

void RunDeathExplosionBody(u8 *player, i32 tick)
{
    if (tick == 3) {
        g_PlayerLivesCounter -= 0x40;
        if (g_PlayerLivesCounter < 0)
            g_PlayerLivesCounter = 0;
        RefreshLivesHud();
        float burst_target[3] = {0.0f,
            *reinterpret_cast<const float *>(player + 0x3c4) - 224.0f, 0.0f};
        const float base_angle = ComputeDeathBurstAngleEcxEaxAbi(
            player, burst_target);
        for (i32 index = 0; index != 7; ++index) {
            const i32 color = (index % 2) != 0 ? 4 : 1;
            SpawnExplosionParticleEaxEcxEfxAbi(
                *reinterpret_cast<void *const *>(0x477818U), player + 0x3c0,
                color, 0xffffffU,
                static_cast<float>(index) * 0.11219974f + base_angle -
                    0.3926991f, 3.0f);
        }
        RebuildPlayerOptionRecords(player);
    }
    if (tick <= 30)
        return;
    if (g_PlayerLivesRemaining < 0) {
        if (*reinterpret_cast<const i32 *>(
                static_cast<u8 *>(g_GameModeObject) + 0x10) == 1)
            RequestGameStateTransitionEaxStackAbi(
                reinterpret_cast<void *>(0x491c28U), 4);
        else
            RunGameOverPathBStackAbi(g_GameStateManager, 0);
        return;
    }
    *reinterpret_cast<i32 *>(player + 0x458) = 0;
    g_FrameTimeScale = 1.0f;
    (void)SpawnPlayerSubEffectEcxDxStackAbi(player + 0x3c0, player, 32.0f,
                                            16.0f, 30, 150);
    *reinterpret_cast<u32 *>(player + 0x440) =
        *reinterpret_cast<const u32 *>(player + 0x3c0);
    *reinterpret_cast<u32 *>(player + 0x444) =
        *reinterpret_cast<const u32 *>(player + 0x3c4);
    *reinterpret_cast<u32 *>(player + 0x448) =
        *reinterpret_cast<const u32 *>(player + 0x3c8);
    *reinterpret_cast<i32 *>(player + 0x3cc) = 0;
    *reinterpret_cast<i32 *>(player + 0x3d0) = 48000;
    *reinterpret_cast<float *>(player + 0x3c0) = 0.0f;
    *reinterpret_cast<float *>(player + 0x3c4) = 480.0f;
    TickPlayerTimerEaxStackAbi(player + 0x430c, 0x118);
    TickPlayerTimerEaxStackAbi(player + 0x474, 0);
}

void RunBombFreezeBody(u8 *player, i32 tick)
{
    if (tick != 15)
        return;
    u8 *const entry_base = static_cast<u8 *>(g_EnemyArrayBase) + 0x60;
    for (u32 index = 0; index != 2000; ++index) {
        const short kind = *reinterpret_cast<const short *>(
            entry_base + index * 0x7f0 + 0x446);
        if (kind != 0 && kind != 3)
            (void)ActivateStageEnemyEsiAbi(entry_base + index * 0x7f0);
    }
    (void)BroadcastBulletClearEaxAbi(
        *reinterpret_cast<void *const *>(0x47781cU));
}

// Common epilogue: sub-effect records, invincibility flash, VM update, box
// recomputation, timers, score accumulation, and projectile processing.
void RunCommonEpilogue(u8 *player)
{
    u8 *const record_base = player + 0x350c;
    for (u32 index = 0; index != 32; ++index) {
        u8 *const record = record_base + index * 0x6c;
        if ((*reinterpret_cast<const u8 *>(record + 0x68) & 1) == 0)
            continue;
        u8 *const motion = record + 0x24;
        if ((*reinterpret_cast<const u8 *>(record + 0x40) & 1) == 0) {
            PolarToCartesianEdiAbi(motion,
                *reinterpret_cast<const float *>(motion + 0x10),
                *reinterpret_cast<const float *>(motion + 0xc));
            *reinterpret_cast<u32 *>(motion + 8) = 0;
        } else {
            *reinterpret_cast<float *>(motion + 0x14) =
                *reinterpret_cast<const float *>(motion + 0x14) +
                *reinterpret_cast<const float *>(motion + 0x18);
            *reinterpret_cast<float *>(motion + 0x10) = WrapAngleToPi(
                *reinterpret_cast<const float *>(motion + 0xc) +
                *reinterpret_cast<const float *>(motion + 0x10));
        }
        IntegrateSubEffectPositionEsiAbi(record + 0x18);
        *reinterpret_cast<float *>(record) =
            *reinterpret_cast<const float *>(record) +
            *reinterpret_cast<const float *>(record + 4);
        *reinterpret_cast<float *>(record + 8) =
            *reinterpret_cast<const float *>(record + 8) +
            *reinterpret_cast<const float *>(record + 0xc);
        // Sub-record timer: prev i32 @0x48, count i32 @0x44, accumulator
        // float @0x4c, rate pointer @0x50; counts down toward deactivation.
        *reinterpret_cast<i32 *>(record + 0x44) =
            *reinterpret_cast<i32 *>(record + 0x48);
        const float rate = **reinterpret_cast<float *const *>(record + 0x50);
        float acc;
        if (rate > kRateUnityLow && rate < kRateUnityHigh) {
            acc = *reinterpret_cast<float *>(record + 0x4c) - 1.0f;
        } else {
            acc = *reinterpret_cast<float *>(record + 0x4c) - rate;
        }
        *reinterpret_cast<float *>(record + 0x4c) = acc;
        *reinterpret_cast<i32 *>(record + 0x48) = FloatToI32(acc);
        if (*reinterpret_cast<const i32 *>(record + 0x48) <= 0)
            *reinterpret_cast<u8 *>(record + 0x68) &=
                static_cast<u8>(~1U);
    }

    if (*reinterpret_cast<const i32 *>(player + 0x4310) >= 1) {
        *reinterpret_cast<i32 *>(player + 0x430c) =
            *reinterpret_cast<i32 *>(player + 0x4310);
        const float rate = **reinterpret_cast<float *const *>(
            player + 0x4318);
        float acc;
        if (rate > kRateUnityLow && rate < kRateUnityHigh) {
            acc = *reinterpret_cast<float *>(player + 0x4314) - 1.0f;
        } else {
            acc = *reinterpret_cast<float *>(player + 0x4314) - rate;
        }
        *reinterpret_cast<float *>(player + 0x4314) = acc;
        *reinterpret_cast<i32 *>(player + 0x4310) = FloatToI32(acc);
    }
    {
        const i32 tick = *reinterpret_cast<const i32 *>(player + 0x478);
        const i32 prev = *reinterpret_cast<const i32 *>(player + 0x474);
        if (tick == prev || tick % 3 != 0) {
            *reinterpret_cast<u32 *>(player + 0x370) &= ~0x8000U;
        } else {
            *reinterpret_cast<u32 *>(player + 0x314) = 0xff0000ffU;
            *reinterpret_cast<u32 *>(player + 0x370) |= 0x8000U;
        }
    }

    UpdatePlayerAnimationVmEcxAbi(player, player + 0x14, player + 0x14);

    const float position_x = *reinterpret_cast<const float *>(player + 0x3c0);
    const float position_y = *reinterpret_cast<const float *>(player + 0x3c4);
    const float position_z = *reinterpret_cast<const float *>(player + 0x3c8);
    struct BoxWriter {
        static void Write(u8 *player, u32 offset, float x, float y, float z,
                          float hx, float hy, float hz)
        {
            *reinterpret_cast<float *>(player + offset) = x - hx;
            *reinterpret_cast<float *>(player + offset + 4) = y - hy;
            *reinterpret_cast<float *>(player + offset + 8) = z - hz;
            *reinterpret_cast<float *>(player + offset + 0xc) = x + hx;
            *reinterpret_cast<float *>(player + offset + 0x10) = y + hy;
            *reinterpret_cast<float *>(player + offset + 0x14) = z + hz;
        }
    };
    BoxWriter::Write(player, 0x404, position_x, position_y, position_z,
        *reinterpret_cast<const float *>(player + 0x41c),
        *reinterpret_cast<const float *>(player + 0x420),
        *reinterpret_cast<const float *>(player + 0x424));
    BoxWriter::Write(player, 0x4324, position_x, position_y, position_z,
        *reinterpret_cast<const float *>(player + 0x428) * 0.5f,
        *reinterpret_cast<const float *>(player + 0x42c) * 0.5f,
        *reinterpret_cast<const float *>(player + 0x430) * 0.5f);
    BoxWriter::Write(player, 0x433c, position_x, position_y, position_z,
        *reinterpret_cast<const float *>(player + 0x434),
        *reinterpret_cast<const float *>(player + 0x438),
        *reinterpret_cast<const float *>(player + 0x43c));
    BoxWriter::Write(player, 0x4354, position_x, position_y, position_z,
        *reinterpret_cast<const float *>(player + 0x428),
        *reinterpret_cast<const float *>(player + 0x42c),
        *reinterpret_cast<const float *>(player + 0x430));

    AdvancePlayerTimerBlock(player + 0x474);
    AdvancePlayerTimerBlock(player + 0x488);

    const bool in_gameplay = g_AsciiHudOwner != 0 &&
        *reinterpret_cast<const i32 *>(
            static_cast<u8 *>(g_AsciiHudOwner) + 0x9eb8) == 0;
    if (in_gameplay && g_AsciiHudConditionalState != 0 &&
        *reinterpret_cast<const i32 *>(
            static_cast<u8 *>(g_AsciiHudConditionalState) + 0x60) != 0 &&
        *reinterpret_cast<const i32 *>(player + 0x478) % 60 == 0) {
        g_ScorePenaltyCounter += 1;
        if (g_ScorePenaltyCounter > 0x400)
            g_ScorePenaltyCounter = 0x400;
        else if (g_ScorePenaltyCounter < -0x400)
            g_ScorePenaltyCounter = -0x400;
    }
    if (in_gameplay && g_AsciiHudConditionalState != 0 &&
        *reinterpret_cast<const i32 *>(
            static_cast<u8 *>(g_AsciiHudConditionalState) + 0x60) != 0) {
        (void)TickItemMagnetEaxAbi(player);
    } else {
        if ((*reinterpret_cast<u32 *>(player + 0x470) & 1) == 0) {
            *reinterpret_cast<i32 *>(player + 0x464) = 0;
            *reinterpret_cast<i32 *>(player + 0x460) =
                static_cast<i32>(0xfff0bdc1U);
            *reinterpret_cast<i32 *>(player + 0x468) = 0;
            *reinterpret_cast<const float **>(player + 0x46c) =
                &g_FrameTimeScale;
            *reinterpret_cast<u32 *>(player + 0x470) |= 1;
        }
        *reinterpret_cast<i32 *>(player + 0x464) = -1;
        *reinterpret_cast<i32 *>(player + 0x468) =
            static_cast<i32>(0xbf800000U);
        *reinterpret_cast<i32 *>(player + 0x460) = -2;
        *reinterpret_cast<i32 *>(player + 0x3504) = 0;
        *reinterpret_cast<u8 *>(player + 0x3508) = 0;
    }

    (void)UpdatePlayerProjectilesStackAbi(player);
}

} // namespace

i32 UpdatePlayerModeDispatcher(void *player_memory)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    const u32 mode = static_cast<u32>(
        *reinterpret_cast<const i32 *>(player + 0x458));

    if (mode == 0) {
        RunRespawnIntroBody(player);
        if (*reinterpret_cast<const i32 *>(player + 0x478) >= 60) {
            *reinterpret_cast<i32 *>(player + 0x458) = 1;
            (void)EnsureTimerBlockInitialized(player + 0x474);
            *reinterpret_cast<i32 *>(player + 0x478) = 0;
            *reinterpret_cast<i32 *>(player + 0x47c) = 0;
            *reinterpret_cast<i32 *>(player + 0x474) = -1;
            goto mode_1;
        }
    } else if (mode == 1) {
mode_1:
        if (g_AsciiHudOwner != 0 &&
            *reinterpret_cast<const i32 *>(
                static_cast<u8 *>(g_AsciiHudOwner) + 0x9eb8) == 0 &&
            g_GameContext != 0 &&
            *reinterpret_cast<const i32 *>(
                static_cast<u8 *>(g_GameContext) + 0x28) == 0 &&
            static_cast<short>(g_PlayerLivesCounter / 20) != 0 &&
            (g_InputMask & 2) != 0) {
            TickPlayerTimerEaxStackAbi(player + 0x430c, 0x10e);
            (void)TickRespawnDeathEffectStackAbi(g_GameContext);
            g_PlayerLivesCounter -= 20;
            RebuildPlayerOptionRecords(player);
            RefreshLivesHud();
            AddMaximumScorePenalty(3000);
        }
        if (*reinterpret_cast<const i32 *>(player + 0x478) < 30)
            ActivateStageEnemiesAndBullets(true);
        (void)UpdatePlayerMovementEdiAbi(player);
    } else if (mode == 2) {
        RunDeathExplosionBody(player,
            *reinterpret_cast<const i32 *>(player + 0x478));
    } else if (mode == 3) {
        RunBombFreezeBody(player,
            *reinterpret_cast<const i32 *>(player + 0x478));
    } else if (mode == 4) {
        RunDeathbombDecisionBody(player,
            *reinterpret_cast<const i32 *>(player + 0x478));
    }

    RunCommonEpilogue(player);
    return 1;
}

} // namespace th10
