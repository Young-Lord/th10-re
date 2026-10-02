#include "PlayerModeDispatcher.hpp"

#include "AsciiHudOwner.hpp"
#include "GameContext.hpp"
#include "PlayerDeathProcessor.hpp"
#include "PlayerRecord.hpp"
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

// Player object fields are accessed through the typed PlayerRecord view
// (src/PlayerRecord.hpp); the layout is
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
void AdvancePlayerTimerBlock(TimerNode &block)
{
    block.prev = block.count;
    const float rate = *block.rate;
    if (rate > kRateUnityLow && rate < kRateUnityHigh) {
        *reinterpret_cast<float *>(&block.accum) =
            *reinterpret_cast<const float *>(&block.accum) + 1.0f;
        block.count = block.count + 1;
    } else {
        const float acc =
            *reinterpret_cast<const float *>(&block.accum) + rate;
        *reinterpret_cast<float *>(&block.accum) = acc;
        block.count = FloatToI32(acc);
    }
}

// Returns true when the block was initialized by this call.
bool EnsureTimerBlockInitialized(TimerNode &block)
{
    if ((block.flags & 1) != 0)
        return false;
    block.count = 0;
    block.prev = static_cast<i32>(0xfff0bdc1U);
    *reinterpret_cast<float *>(&block.accum) = 0.0f;
    block.rate = &g_FrameTimeScale;
    block.flags |= 1;
    return true;
}

// The 32-entry option-position history ring at player+0x436c (dword pairs).
void ShiftPositionHistory(u8 *player)
{
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    const i32 count = player_rec.option_count;
    for (i32 index = 31; index > count; --index) {
        player_rec.trail_history[index * 2] =
            player_rec.trail_history[(index - 1) * 2];
        player_rec.trail_history[index * 2 + 1] =
            player_rec.trail_history[(index - 1) * 2 + 1];
    }
    player_rec.trail_history[0] =
        static_cast<u32>(player_rec.position_x_fixed);
    player_rec.trail_history[1] =
        static_cast<u32>(player_rec.position_y_fixed);
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
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    const i32 tick = player_rec.frame_timer.count;
    player_rec.position_y_fixed = 48000 - tick * 8000 / 60;
    player_rec.position_y =
        static_cast<float>(player_rec.position_y_fixed) * kUnitScale;
    // +0x332c/+0x33c4/+0x345c/+0x34f4 = options[0..3].tier_latch.
    player_rec.options[0].tier_latch = 1;
    player_rec.options[1].tier_latch = 1;
    player_rec.options[2].tier_latch = 1;
    player_rec.options[3].tier_latch = 1;
    ShiftPositionHistory(player);

    if (tick < 30) {
        const float sweep = static_cast<float>(tick) * 17.066668f + 64.0f;
        const float position[3] = {player_rec.respawn_position[0],
                                   player_rec.respawn_position[1],
                                   player_rec.respawn_position[2]};
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
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    if (tick > 8) {
        ProcessPlayerDeathStackAbi(player);
        RunDeathExplosionBody(player, player_rec.frame_timer.count);
        return;
    }
    GameContext *const game_ctx = static_cast<GameContext *>(g_GameContext);
    if (game_ctx == 0 || game_ctx->popup_state != 0)
        return;
    if (static_cast<short>(g_PlayerLivesCounter / 20) == 0)
        return;
    if ((g_InputMask & 2) == 0)
        return;
    TickPlayerTimerEaxStackAbi(&player_rec.frame_timer, 60);
    TickPlayerTimerEaxStackAbi(&player_rec.deathbomb_timer, 200);
    (void)TickRespawnDeathEffectStackAbi(g_GameContext);
    g_PlayerLivesCounter -= 20;
    RebuildPlayerOptionRecords(player);
    RefreshLivesHud();
    player_rec.mode = 1;
}

void RunDeathExplosionBody(u8 *player, i32 tick)
{
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    if (tick == 3) {
        g_PlayerLivesCounter -= 0x40;
        if (g_PlayerLivesCounter < 0)
            g_PlayerLivesCounter = 0;
        RefreshLivesHud();
        float burst_target[3] = {0.0f,
            player_rec.position_y - 224.0f, 0.0f};
        const float base_angle = ComputeDeathBurstAngleEcxEaxAbi(
            player, burst_target);
        for (i32 index = 0; index != 7; ++index) {
            const i32 color = (index % 2) != 0 ? 4 : 1;
            SpawnExplosionParticleEaxEcxEfxAbi(
                *reinterpret_cast<void *const *>(0x477818U),
                &player_rec.position_x,
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
    player_rec.mode = 0;
    g_FrameTimeScale = 1.0f;
    (void)SpawnPlayerSubEffectEcxDxStackAbi(&player_rec.position_x, player,
                                            32.0f, 16.0f, 30, 150);
    player_rec.respawn_position[0] = player_rec.position_x;
    player_rec.respawn_position[1] = player_rec.position_y;
    player_rec.respawn_position[2] = player_rec.position_z;
    player_rec.position_x_fixed = 0;
    player_rec.position_y_fixed = 48000;
    player_rec.position_x = 0.0f;
    player_rec.position_y = 480.0f;
    TickPlayerTimerEaxStackAbi(&player_rec.deathbomb_timer, 0x118);
    TickPlayerTimerEaxStackAbi(&player_rec.frame_timer, 0);
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
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    for (u32 index = 0; index != 32; ++index) {
        PlayerSubEffectRecord &record = player_rec.sub_effects[index];
        if ((record.flags & 1) == 0)
            continue;
        u8 *const motion = record.motion_block;
        if ((*reinterpret_cast<const u8 *>(motion + 0x1c) & 1) == 0) {
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
        IntegrateSubEffectPositionEsiAbi(record.position);
        record.velocity[0] =
            record.velocity[0] + record.velocity[1];
        record.damage_angle =
            record.damage_angle +
            *reinterpret_cast<const float *>(record.unknown_000c);
        // Sub-record timer: prev i32 @0x48, count i32 @0x44, accumulator
        // float @0x4c, rate pointer @0x50; counts down toward deactivation.
        record.timer_prev = record.timer_count;
        const float rate = *record.timer_rate;
        float acc;
        if (rate > kRateUnityLow && rate < kRateUnityHigh) {
            acc = record.timer_accum - 1.0f;
        } else {
            acc = record.timer_accum - rate;
        }
        record.timer_accum = acc;
        record.timer_count = FloatToI32(acc);
        if (record.timer_count <= 0)
            record.flags &= static_cast<u8>(~1U);
    }

    if (player_rec.deathbomb_timer.count >= 1) {
        player_rec.deathbomb_timer.prev = player_rec.deathbomb_timer.count;
        const float rate = *player_rec.deathbomb_timer.rate;
        float acc;
        if (rate > kRateUnityLow && rate < kRateUnityHigh) {
            acc = *reinterpret_cast<const float *>(
                      &player_rec.deathbomb_timer.accum) - 1.0f;
        } else {
            acc = *reinterpret_cast<const float *>(
                      &player_rec.deathbomb_timer.accum) - rate;
        }
        *reinterpret_cast<float *>(&player_rec.deathbomb_timer.accum) = acc;
        player_rec.deathbomb_timer.count = FloatToI32(acc);
    }
    {
        const i32 tick = player_rec.frame_timer.count;
        const i32 prev = player_rec.frame_timer.prev;
        if (tick == prev || tick % 3 != 0) {
            player_rec.anim_vm.flags &= ~0x8000U;
        } else {
            player_rec.anim_vm.secondary_color = 0xff0000ffU;
            player_rec.anim_vm.flags |= 0x8000U;
        }
    }

    UpdatePlayerAnimationVmEcxAbi(player, &player_rec.anim_vm,
                                  &player_rec.anim_vm);

    const float position_x = player_rec.position_x;
    const float position_y = player_rec.position_y;
    const float position_z = player_rec.position_z;
    struct BoxWriter {
        static void Write(float *box, float x, float y, float z,
                          float hx, float hy, float hz)
        {
            box[0] = x - hx;
            box[1] = y - hy;
            box[2] = z - hz;
            box[3] = x + hx;
            box[4] = y + hy;
            box[5] = z + hz;
        }
    };
    BoxWriter::Write(player_rec.hit_box, position_x, position_y, position_z,
        player_rec.hit_half_extent[0],
        player_rec.hit_half_extent[1],
        player_rec.hit_half_extent[2]);
    BoxWriter::Write(player_rec.graze_box, position_x, position_y, position_z,
        player_rec.graze_half_extent[0] * 0.5f,
        player_rec.graze_half_extent[1] * 0.5f,
        player_rec.graze_half_extent[2] * 0.5f);
    BoxWriter::Write(player_rec.item_box, position_x, position_y, position_z,
        player_rec.item_half_extent[0],
        player_rec.item_half_extent[1],
        player_rec.item_half_extent[2]);
    BoxWriter::Write(player_rec.autocollect_box, position_x, position_y,
        position_z,
        player_rec.graze_half_extent[0],
        player_rec.graze_half_extent[1],
        player_rec.graze_half_extent[2]);

    AdvancePlayerTimerBlock(player_rec.frame_timer);
    AdvancePlayerTimerBlock(player_rec.move_gate_timer);

    const bool in_gameplay = g_AsciiHudOwner != 0 &&
        reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner)
                ->result_script_state == 0;
    if (in_gameplay && g_AsciiHudConditionalState != 0 &&
        *reinterpret_cast<const i32 *>(
            static_cast<u8 *>(g_AsciiHudConditionalState) + 0x60) != 0 &&
        player_rec.frame_timer.count % 60 == 0) {
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
        if ((player_rec.autocollect_timer.flags & 1) == 0) {
            player_rec.autocollect_timer.count = 0;
            player_rec.autocollect_timer.prev =
                static_cast<i32>(0xfff0bdc1U);
            player_rec.autocollect_timer.accum = 0;
            player_rec.autocollect_timer.rate = &g_FrameTimeScale;
            player_rec.autocollect_timer.flags |= 1;
        }
        player_rec.autocollect_timer.count = -1;
        // -1.0f stored through the dword view (bit pattern preserved).
        player_rec.autocollect_timer.accum =
            static_cast<i32>(0xbf800000U);
        player_rec.autocollect_timer.prev = -2;
        player_rec.homing_target = 0;
        player_rec.homing_target_latch = 0;
    }

    (void)UpdatePlayerProjectilesStackAbi(player);
}

} // namespace

i32 UpdatePlayerModeDispatcher(void *player_memory)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    const u32 mode = static_cast<u32>(player_rec.mode);
    // Typed view over DAT_004776ec; the goto into mode_1 crosses no
    // initialization because the view is established here.
    GameContext *const game_ctx = static_cast<GameContext *>(g_GameContext);

    if (mode == 0) {
        RunRespawnIntroBody(player);
        if (player_rec.frame_timer.count >= 60) {
            player_rec.mode = 1;
            (void)EnsureTimerBlockInitialized(player_rec.frame_timer);
            player_rec.frame_timer.count = 0;
            player_rec.frame_timer.accum = 0;
            player_rec.frame_timer.prev = -1;
            goto mode_1;
        }
    } else if (mode == 1) {
mode_1:
        if (g_AsciiHudOwner != 0 &&
            reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner)
                    ->result_script_state == 0 &&
            game_ctx != 0 &&
            game_ctx->popup_state == 0 &&
            static_cast<short>(g_PlayerLivesCounter / 20) != 0 &&
            (g_InputMask & 2) != 0) {
            TickPlayerTimerEaxStackAbi(&player_rec.deathbomb_timer, 0x10e);
            (void)TickRespawnDeathEffectStackAbi(g_GameContext);
            g_PlayerLivesCounter -= 20;
            RebuildPlayerOptionRecords(player);
            RefreshLivesHud();
            AddMaximumScorePenalty(3000);
        }
        if (player_rec.frame_timer.count < 30)
            ActivateStageEnemiesAndBullets(true);
        (void)UpdatePlayerMovementEdiAbi(player);
    } else if (mode == 2) {
        RunDeathExplosionBody(player, player_rec.frame_timer.count);
    } else if (mode == 3) {
        RunBombFreezeBody(player, player_rec.frame_timer.count);
    } else if (mode == 4) {
        RunDeathbombDecisionBody(player, player_rec.frame_timer.count);
    }

    RunCommonEpilogue(player);
    return 1;
}

} // namespace th10
