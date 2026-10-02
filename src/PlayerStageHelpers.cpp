#include "PlayerStageHelpers.hpp"

#include "AsciiAnimationVm.hpp"
#include "GameContext.hpp"
#include "PlayerRecord.hpp"
#include "StageEffectHelpers.hpp"
#include "BgmRuntime.hpp"
#include "Th10Platform.hpp"

namespace th10 {

// Stage-side helpers used by the player dispatcher: the respawn effect
// tick, the HUD lives digits, the enemy activation sweep, and the two
// linked-list broadcasts. The per-node virtual calls remain boundaries.

namespace {

inline float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

extern u32 g_PlayerCharacter; // TH10 DAT_00474c68
extern i32 g_PlayerPowerGauge; // TH10 DAT_00474c48
extern u8 g_InputMask; // TH10 DAT_00474e5c
extern void *g_AsciiHudOwner; // TH10 DAT_0047770c
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern float g_FrameTimeScale; // TH10 DAT_00476f78
extern u8 *g_OptionPositionBase; // TH10 DAT_00477834 (player pointer)
extern void *g_SpellBulletBase; // TH10 DAT_004776f4 (stage manager slot)
extern void *g_BulletManagerSlot; // TH10 DAT_00477818
extern void *g_BulletListRootSlot; // TH10 DAT_0047781c
extern i32 g_ScorePenaltyCounter; // TH10 DAT_00474c98

extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590

inline i32 ReadInt(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline u32 ReadUint(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline void WriteInt(u8 *bytes, u32 offset, i32 value)
{
    *reinterpret_cast<i32 *>(bytes + offset) = value;
}

inline void WriteFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

inline void WriteUint(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

// The shared timer-node reset: lazy first-init writes the sentinel, then
// the unconditional reset stops the timer until its first tick.
void ResetTimerNode(u8 *node)
{
    TimerNode &timer = *reinterpret_cast<TimerNode *>(node);
    if ((timer.flags & 1) == 0) {
        timer.count = 0;
        timer.prev = static_cast<i32>(0xfff0bdc1U);
        timer.accum = 0;
        timer.rate = &g_FrameTimeScale;
        timer.flags |= 1;
    }
    timer.count = 0;
    timer.accum = 0;
    timer.prev = -1;
}

} // namespace

// TH10 0x00405860. Advances the respawn sequence's per-frame effects and
// requests the respawn sound; the dispatcher performs the lives decrement
// around this call.
i32 TickRespawnDeathEffectStackAbi(void *node_memory)
{
    GameContext &ctx = *static_cast<GameContext *>(node_memory);
    if (ctx.popup_state != 0)
        return -1;
    ctx.popup_state = 1;
    ResetTimerNode(reinterpret_cast<u8 *>(&ctx.timer));
    ctx.radius = 32.0f;
    ctx.field_0040 = 4.0f;

    const PlayerRecord &player_rec =
        *reinterpret_cast<const PlayerRecord *>(g_OptionPositionBase);
    const float position[3] = {player_rec.position_x,
                               player_rec.position_y,
                               player_rec.position_z};
    ctx.position_x = position[0];
    ctx.position_y = position[1];
    ctx.position_z = position[2];

    i32 script_id = 0x190 + (g_PlayerCharacter != 0 ? 7 : 0);
    u8 *const stage = *static_cast<u8 *const *>(g_SpellBulletBase);
    const bool boss_window =
        (ReadUint(stage, 0x378c) & 1) != 0 &&
        ((ReadUint(stage, 0x3788) >= 0x5d &&
          ReadUint(stage, 0x3788) <= 0x60) ||
         ReadUint(stage, 0x3788) == 0x6d);
    if (boss_window)
        script_id = 0x1b9;

    void *const vm =
        SpawnStageEffectEdxEbxAbi(*static_cast<void *const *>(
                                      g_SpellBulletBase),
                                  position, script_id);
    ctx.effect_handle = ReadInt(static_cast<const u8 *>(vm), 0);
    // TH10 0x43dd10 with sound id 0x26; the float payload is not
    // observable from the caller and is modeled as zero.
    EnqueueBgmSoundValueFromFloat(&g_TransitionRoot, 0x26, 0.0f);

    const i32 lives = g_PlayerPowerGauge / 20;
    RefreshHudLivesDisplayEdxStackAbi(lives, (g_PlayerPowerGauge % 20) * 100 / 20);
    CleanupStageLifeFlags(stage);

    g_ScorePenaltyCounter -= 0x80;
    if (g_ScorePenaltyCounter > 0x400)
        g_ScorePenaltyCounter = 0x400;
    else if (g_ScorePenaltyCounter < -0x400)
        g_ScorePenaltyCounter = -0x400;

    ctx.bomb_variant =
             (ReadUint(stage, 0x378c) & 1) != 0 &&
                     ReadInt(stage, 0x3738) > 0x3c
                 ? 1
                 : 0;
    return 0;
}

// TH10 0x004054b0. Re-binds the three numeric lives digits through the
// existing semantic body of 0x0043e5a0; the HUD manager comes from the
// global 0x47770c.
void RefreshHudLivesDisplayEdxStackAbi(i32 lives, i32 percent)
{
    u8 *const hud = static_cast<u8 *>(g_AsciiHudOwner);
    void *const vm = *reinterpret_cast<void *const *>(hud + 0x9ec8);
    InitializeAsciiAnimationVmEntry(vm, static_cast<u32>(lives + 8),
                                    hud + 0x6a8c);
    InitializeAsciiAnimationVmEntry(vm, static_cast<u32>(percent / 10 + 8),
                                    hud + 0x71e4);
    InitializeAsciiAnimationVmEntry(vm, static_cast<u32>(percent % 10 + 8),
                                    hud + 0x7590);
}

// TH10 0x004086b0. Native ECX = the float2 position (x at +0, y at +4),
// stack = (margin_x, margin_y; ret 8). Returns 1 when the position with
// its margins falls outside the fixed playfield rect x = (-192, 192),
// y = (0, 448) and 0 when inside; each axis test is an independent early
// return in the native order.
i32 CheckStageEffectPositionInFieldEcxEcxStackAbi(const float *position,
                                                  float margin_x,
                                                  float margin_y)
{
    if (position[0] + margin_x <= -192.0f)
        return 1;
    if (position[0] - margin_x >= 192.0f)
        return 1;
    if (position[1] + margin_y <= 0.0f)
        return 1;
    if (position[1] - margin_y >= 448.0f)
        return 1;
    return 0;
}

// TH10 0x00408030. State-machine transition plus the on-screen effect and
// timer-node reset; off-screen slots only set the deferred flag.
i32 ActivateStageEnemyEsiAbi(void *enemy_slot_memory)
{
    u8 *const enemy = static_cast<u8 *>(enemy_slot_memory);
    const short state =
        *reinterpret_cast<const short *>(enemy + 0x446);
    if (state != 1 && state != 2)
        return 0;
    // The 0x4086b0 on-screen bounds test (8px margins against the fixed
    // playfield rect) selects between the full and deferred paths.
    const float position[2] = {ReadFloat(enemy, 0x3b4),
                               ReadFloat(enemy, 0x3b8)};
    const bool on_screen =
        CheckStageEffectPositionInFieldEcxEcxStackAbi(position, 8.0f,
                                                      8.0f) == 0;
    *reinterpret_cast<short *>(enemy + 0xc3) = 1;
    *reinterpret_cast<short *>(enemy + 0x446) = 3;
    if (!on_screen) {
        *reinterpret_cast<u32 *>(enemy) |= 8;
        return 1;
    }
    if (ReadInt(enemy, 0x438) >= 0)
        (void)SpawnStageEffectEdxEbxAbi(
            *static_cast<void *const *>(g_SpellBulletBase),
            reinterpret_cast<const float *>(enemy + 0x3b4),
            ReadInt(enemy, 0x438));
    ResetTimerNode(enemy + 0x3f8);
    return 1;
}

// TH10 0x00408100. Sweeps the 2000-slot enemy pool, activating eligible
// entries inside the expanding radius and optionally spawning the
// downward particle burst for on-screen hits.
void ScanIntroActivations(void *enemy_manager_memory,
                          const float player_position[3], float radius,
                          i32 spawn_fx, i32 require_unused)
{
    u8 *const manager = static_cast<u8 *>(enemy_manager_memory);
    for (u32 index = 0; index != 2000; ++index) {
        u8 *const enemy = manager + 0x60 + index * 0x7f0;
        const short state =
            *reinterpret_cast<const short *>(enemy + 0x446);
        if (state == 0 || state == 3)
            continue;
        if (require_unused != 0 && ReadInt(enemy, 4) != 0)
            continue;
        const float dx = ReadFloat(enemy, 0x3b4) - player_position[0];
        const float dy = ReadFloat(enemy, 0x3b8) - player_position[1];
        const float dz = ReadFloat(enemy, 0x3bc) - player_position[2];
        const float reach = ReadFloat(enemy, 0x3f0) * 0.5f + radius;
        if (!(reach * reach > dx * dx + dy * dy + dz * dz))
            continue;
        (void)ActivateStageEnemyEsiAbi(enemy);
        const float position[2] = {ReadFloat(enemy, 0x3b4),
                                   ReadFloat(enemy, 0x3b8)};
        const bool on_screen =
            CheckStageEffectPositionInFieldEcxEcxStackAbi(position, 2.0f,
                                                          2.0f) == 0;
        if (on_screen && spawn_fx != 0) {
            extern void SpawnExplosionParticleEaxEcxEfxAbi(
                void *manager, void *position, i32 kind, u32 color,
                float angle, float speed);
            float position[3] = {ReadFloat(enemy, 0x3b4),
                                 ReadFloat(enemy, 0x3b8),
                                 ReadFloat(enemy, 0x3bc)};
            SpawnExplosionParticleEaxEcxEfxAbi(
                *static_cast<void *const *>(g_BulletManagerSlot), position,
                8, 0xffffffffU, -1.5707964f, 0.6f);
        }
    }
}

// TH10 0x0041c850-style linked-list walk shared by the broadcasts.
static bool NodeIsSkippable(const u8 *node)
{
    return ReadInt(node, 0xc) == 1;
}

// TH10 0x0041c800. Caches the target position and applies the entrance
// tween to every non-skipped node through its virtual slot 7.
i32 BroadcastEntranceTweenEaxEbxStackAbi(void *manager_memory,
                                         const float target[3], float radius,
                                         i32 flag)
{
    u8 *const manager = static_cast<u8 *>(manager_memory);
    WriteFloat(manager, 0x440, target[0]);
    WriteFloat(manager, 0x444, target[1]);
    WriteFloat(manager, 0x448, target[2]);
    i32 total = 0;
    const u32 *node =
        *reinterpret_cast<u32 *const *>(manager + 0x18);
    for (; node != 0;
         node = reinterpret_cast<const u32 *>(node[1])) {
        u8 *const entry = reinterpret_cast<u8 *>(node[0]);
        if (NodeIsSkippable(entry))
            continue;
        void **const vtable = *reinterpret_cast<void ***>(entry);
        typedef i32 (TH10_STDCALL *TweenFn)(void *, const float *, float,
                                            i32);
        const TweenFn tween = reinterpret_cast<TweenFn>(vtable[7]);
        total += tween(entry, target, radius, flag);
    }
    return total;
}

// TH10 0x0041c850. Broadcasts the bullet-clear virtual (slot 5) to every
// non-skipped node of the manager's list.
i32 BroadcastBulletClearEaxAbi(void *manager_memory)
{
    u8 *const manager = static_cast<u8 *>(manager_memory);
    const u32 *node = *reinterpret_cast<u32 *const *>(manager + 0x18);
    for (; node != 0;
         node = reinterpret_cast<const u32 *>(node[1])) {
        u8 *const entry = reinterpret_cast<u8 *>(node[0]);
        if (NodeIsSkippable(entry))
            continue;
        void **const vtable = *reinterpret_cast<void ***>(entry);
        typedef i32 (TH10_STDCALL *ClearFn)(void *);
        const ClearFn clear = reinterpret_cast<ClearFn>(vtable[5]);
        (void)clear(entry);
    }
    return 1;
}

} // namespace th10
