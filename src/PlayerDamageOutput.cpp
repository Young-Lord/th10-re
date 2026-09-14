// Enemy damage output pass (TH10 0x00428630 neighborhood). Sums the damage
// the player's 128 shot records and the 32 bomb/sub-effect damage boxes deal
// to one enemy position, awards the matching score, and is consumed by the
// ECL script setup (0x0040dc80) to drain boss HP.
#include <cmath>
#include <string.h>

#include "PlayerDamageOutput.hpp"

#include "EntityHelpers.hpp"
#include "PlayerShotSpawner.hpp"
#include "Th10Platform.hpp"

extern void *g_MainChainRenderOwner; // TH10 0x491c10
#include "Th10Types.hpp"
#include "TimelineRenderObjects.hpp"

namespace th10 {

namespace {

// Player state block global (TH10 DAT_00477834); the caller passes the same
// object as the second argument.
// Current run score (TH10 DAT_00474c44), capped at 999999999 on award.
extern i32 g_CurrentRunScoreValue; // TH10 DAT_00474c44
// Game context object (TH10 DAT_004776ec value) used by the bomb damage rule.
extern void *g_GameContextObject; // TH10 DAT_004776ec
// Spell/bullet gate block (TH10 DAT_004776f4); byte +0x378c bit 0 selects the
// "player shot type" damage variants.
extern u8 *g_SpellBulletGate; // TH10 DAT_004776f4
// Result/HUD sub-object (TH10 DAT_00477704); +0x10 nonzero marks an active
// finish sequence in the bomb damage rule.
extern u8 *g_AsciiHudConditionalStateDamage; // TH10 DAT_00477704
// Character selector (TH10 DAT_00474c68) used by the splash damage variants.
extern u32 g_PlayerCharacterDamage; // TH10 DAT_00474c68

inline i32 ReadI32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline void WriteI32(u8 *bytes, u32 offset, i32 value)
{
    *reinterpret_cast<i32 *>(bytes + offset) = value;
}

inline u16 ReadU16(const u8 *bytes, u32 offset)
{
    u16 value;
    value = static_cast<u16>(bytes[offset])
          | static_cast<u16>(static_cast<u16>(bytes[offset + 1]) << 8);
    return value;
}

inline float ReadF32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void WriteF32(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

// TH10 0x00449450. Native ESI = item record; resolves the shared pickup-text
// slot holder and returns it (the +0x2c field carries the current handle and
// the +0x35c word a flag set).
extern void *ResolveItemSpawnSlotEsiAbi(void *item);

// TH10 0x00449630. Releases the slot handle the previous call resolved and
// clears the slot.
extern void ReleaseItemSpawnSlotEsiAbi(void *item);

// TH10 0x00448d00 as called from the item pickup path: the native also takes
// the player's +0x10 work record and an output slot which the reconstruction
// of the shared spawner does not model.
u32 *SpawnItemPickupEffect(void *work_record, i32 script_id, u32 kind)
{
    (void)work_record;
    return reinterpret_cast<u32 *>(
        SpawnSetupEffectVmListABack(script_id, kind));
}

// Player-shot descriptor callback: native __fastcall ECX = player, EDX =
// player + 0x49c shot entry cache, stack = enemy position. A nonzero return
// suppresses the default collection path.
typedef i32 (TH10_FASTCALL *PlayerShotHitCallback)(void *player,
                                                   void *shot_entry_cache,
                                                   const float *position);

const float kShotFadeStep = 0.125f; // TH10 flt_470b90

// Bomb damage constants selected by 0x004059f0.
const i32 kBombNearDamageBase = 3;
const i32 kBombNearDamageBig = 5;
const i32 kBombNearDamageBoost = 38;
const i32 kBombDirectDamage = 26; // 0x26
const i32 kBombDirectDamageAlt = 28; // 0x28
const i32 kBombSplashLow = 16;
const i32 kBombSplashHigh = 20;

} // namespace

// TH10 0x00427c70.
bool IsTimerFrameMultiple(const u32 *timer_pair, i32 interval)
{
    const u32 current = timer_pair[1];
    return current != timer_pair[0]
        && static_cast<i32>(current) % interval == 0;
}

// TH10 0x0040c480.
void SetPlayerShotEntityHitState(u32 *id_slot)
{
    u8 *entity = FindEntityEdxStackAbi(reinterpret_cast<void *>(0x491C10U),
                                       *id_slot);
    if (entity == 0)
        return;
    u16 *stop_word = reinterpret_cast<u16 *>(entity + 0x304U);
    *stop_word = 2;
    const u32 child_count = *reinterpret_cast<const u32 *>(entity + 0x18U);
    if (child_count != 0)
        return;
    // Child chain nodes are {entity, next}; the stop word goes to the
    // entity's +0x304 (native 0x40c480 dereferences the node first).
    u32 node = *reinterpret_cast<const u32 *>(entity + 0x14U);
    while (node != 0) {
        u8 *child = *reinterpret_cast<u8 *const *>(node);
        *reinterpret_cast<u16 *>(child + 0x304U) = 2;
        node = *reinterpret_cast<const u32 *>(node + 4U);
    }
}

// TH10 0x004243f0.
void SetResultEntityStateSixByHandleSlot(u32 *id_slot)
{
    u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner, *id_slot);
    if (entity == 0)
        return;
    *reinterpret_cast<u16 *>(entity + 0x304U) = 6;
    const u32 child_count = *reinterpret_cast<const u32 *>(entity + 0x18U);
    if (child_count != 0)
        return;
    // Child chain nodes are {entity, next}; the stop word goes to the
    // entity's +0x304.
    u32 node = *reinterpret_cast<const u32 *>(entity + 0x14U);
    while (node != 0) {
        u8 *child = *reinterpret_cast<u8 *const *>(node);
        *reinterpret_cast<u16 *>(child + 0x304U) = 6;
        node = *reinterpret_cast<const u32 *>(node + 4U);
    }
}

// TH10 0x004059f0.
i32 ComputeBombAreaDamageThisAbi(const void *game_context,
                                 const float *position)
{
    const u8 *ctx = static_cast<const u8 *>(game_context);
    // +0x28 zero means no bomb/sub-effect is running.
    if (*reinterpret_cast<const u32 *>(ctx + 0x28U) == 0)
        return 0;

    const float dx = position[0] - ReadF32(ctx, 0x30U);
    const float dy = position[1] - ReadF32(ctx, 0x34U);
    const float radius = ReadF32(ctx, 0x3CU);
    const i32 variant = ReadI32(ctx, 0x44U);
    const bool shot_gate =
        (g_SpellBulletGate[0x378CU] & 1) != 0;
    const bool finish_active =
        *reinterpret_cast<const u32 *>(g_AsciiHudConditionalStateDamage
                                       + 0x10U) != 0;

    // Native compare is radius*radius >= dx*dx + dy*dy (inclusive).
    if (radius * radius >= dx * dx + dy * dy) {
        // Inside the blast.
        if (variant == 0) {
            if (shot_gate)
                return kBombNearDamageBase;
            return finish_active ? kBombNearDamageBoost
                                 : kBombNearDamageBig;
        }
        if (shot_gate)
            return kBombDirectDamage;
        return finish_active ? kBombNearDamageBoost : kBombNearDamageBig;
    }

    // Outside the blast: only special splash rules still apply.
    if (shot_gate) {
        if (variant == 1) {
            return (g_PlayerCharacterDamage != 0) ? kBombDirectDamageAlt
                                                  : kBombDirectDamage;
        }
        return 0;
    }
    if (!finish_active || variant != 0)
        return 0;
    return (g_PlayerCharacterDamage != 0) ? kBombSplashHigh
                                          : kBombSplashLow;
}

// TH10 0x00428630.
i32 ComputeEnemyDamageFromPlayerAttacksStackAbi(u32 *out_value, void *player,
                                                const float *position,
                                                const float *size)
{
    u8 *const player_bytes = static_cast<u8 *>(player);
    i32 damage = 0;

    // Frame gate: the player's +0x474/+0x478 scaled timer pair must have
    // advanced (prev != count).
    if (ReadI32(player_bytes, 0x478U) == ReadI32(player_bytes, 0x474U))
        return 0;

    const float half_w = size[0] * 0.5f;
    const float half_h = size[1] * 0.5f;
    const float box_left = position[0] - half_w;
    const float box_top = position[1] - half_h;
    const float box_right = position[0] + half_w;
    const float box_bottom = position[1] + half_h;

    if (out_value != 0)
        *out_value = 0;

    // ---- Pass 1: the 128 player-shot records at player+0x4a0 (0x5c stride).
    // Fields: position +0x00/+0x04, fade float +0x38, 0.1f step +0x28,
    // state +0x2c, entity id +0x30, hit flag +0x3c, magnet flag +0x40,
    // descriptor +0x44.
    u8 *item = player_bytes + 0x4A0U;
    for (i32 index = 128; index != 0; --index, item += 0x5CU) {
        const i32 state = ReadI32(item, 0x2CU);
        if (state == 0 || state == 2)
            continue;

        const u8 *desc =
            *reinterpret_cast<u8 *const *>(item + 0x44U);
        const float half_iw = ReadF32(desc, 12U) * 0.5f;
        const float half_ih = ReadF32(desc, 16U) * 0.5f;
        const float item_left = ReadF32(item, 0U) - half_iw;
        const float item_top = ReadF32(item, 4U) - half_ih;
        const float item_right = ReadF32(item, 0U) + half_iw;
        const float item_bottom = ReadF32(item, 4U) + half_ih;

        // Box overlap with the player/enemy box.
        if (!(item_top <= box_bottom && item_left <= box_right
              && item_bottom >= box_top && item_right >= box_left))
            continue;

        const u8 item_type = desc[29];
        // Type 3 (autocollect marker) only collides while the enemy box
        // reaches y >= 0; other types must have their top below y = 0.
        if (item_type == 3) {
            if (box_bottom < 0.0f)
                continue;
        } else if (item_top < 0.0f) {
            continue;
        }

        PlayerShotHitCallback hit;
        memcpy(&hit, desc + 48U, sizeof(hit));
        if (hit != 0
            && hit(player, player_bytes + 0x49CU, position) != 0)
            continue;

        if (ReadI32(item, 0x40U) == 0) {
            SetPlayerShotEntityHitState(
                reinterpret_cast<u32 *>(item + 0x30U));
            WriteI32(item, 0x40U, 1);
        }
        WriteI32(item, 0x3CU, 1);
        if (item_type != 3 || IsTimerFrameMultiple(
                reinterpret_cast<const u32 *>(player_bytes + 0x474U), 4)) {
            damage += static_cast<i32>(ReadU16(desc, 2U));
        }
        if (item_type != 3) {
            // Spawn the pickup effect and convert the record into its
            // magnet/collect state.
            void *slot = ResolveItemSpawnSlotEsiAbi(item);
            const u32 saved_handle = *reinterpret_cast<const u32 *>(
                static_cast<const u8 *>(slot) + 0x2CU);
            ReleaseItemSpawnSlotEsiAbi(item);
            const i32 script_id =
                static_cast<i32>(ReadU16(desc, 0x20U)) + 5;
            const u32 *spawned = SpawnItemPickupEffect(
                player_bytes + 0x10U, script_id, 15U);
            *reinterpret_cast<u32 *>(item + 0x30U) = *spawned;
            void *slot_after = ResolveItemSpawnSlotEsiAbi(item);
            *reinterpret_cast<u32 *>(
                static_cast<u8 *>(slot_after) + 0x2CU) = saved_handle;
            *reinterpret_cast<u32 *>(
                static_cast<u8 *>(slot_after) + 0x35CU) |= 4U;
            WriteF32(item, 0x38U, ReadF32(item, 0x38U) * kShotFadeStep);
            WriteF32(item, 0x28U, 0.1f);
            WriteI32(item, 0x2CU, 2);
        }
        if (item_type == 2) {
            // Point-of-value items pop a sub-effect with damage/3 as limit.
            SpawnPlayerSubEffectEcxDxStackAbi(
                item, player, 2.0f, 1.4f,
                13, static_cast<i32>(ReadU16(desc, 2U)) / 3);
        }
    }

    // Bomb / sub-effect damage from the game context.
    damage += ComputeBombAreaDamageThisAbi(g_GameContextObject, position);

    // ---- Pass 2: the 32 damage-box records at player+0x350c (0x6c stride).
    // Fields: angle +0x08, width +0x10, height +0x14, center +0x18/+0x1c,
    // prev/cur counter +0x44/+0x48, value +0x58, accumulated +0x5c,
    // limit +0x60, period +0x64, flags +0x68 (bit0 active, bit2 rotated).
    u8 *box = player_bytes + 0x350CU;
    for (i32 index = 32; index != 0; --index, box += 0x6CU) {
        const u32 flags = *reinterpret_cast<const u32 *>(box + 0x68U);
        if ((flags & 1U) == 0)
            continue;
        const u32 cur = *reinterpret_cast<const u32 *>(box + 0x48U);
        const u32 prev = *reinterpret_cast<const u32 *>(box + 0x44U);
        const i32 period = ReadI32(box, 0x64U);
        if (cur == prev || static_cast<i32>(cur) % period != 0)
            continue;

        const float width = ReadF32(box, 0x10U);
        const float height = ReadF32(box, 0x14U);
        bool hits = false;
        if ((flags & 2U) != 0) {
            // Rotated box: the box test runs against the enemy centre point.
            const float angle = -ReadF32(box, 0x08U);
            const float dx = position[0] - ReadF32(box, 0x18U);
            const float dy = position[1] - ReadF32(box, 0x1CU);
            const float s = std::sin(angle);
            const float c = std::cos(angle);
            const float rx = c * dx - s * dy;
            const float ry = s * dx + c * dy;
            if (!(half_w + rx < -width * 0.5f)
                && !(rx - half_w > width * 0.5f)
                && !(half_h + ry < -height * 0.5f)
                && ry - half_h <= height * 0.5f)
                hits = true;
        } else {
            const float cx = ReadF32(box, 0x18U);
            const float cy = ReadF32(box, 0x1CU);
            const float hw = width * 0.5f;
            const float hh = height * 0.5f;
            if (cx - hw <= box_right && cx + hw >= box_left
                && cy - hh <= box_bottom && cy + hh >= box_top)
                hits = true;
        }
        if (!hits)
            continue;

        const i32 value = ReadI32(box, 0x58U);
        damage += value;
        const i32 accumulated = ReadI32(box, 0x5CU) + value;
        WriteI32(box, 0x5CU, accumulated);
        if (ReadI32(box, 0x60U) <= accumulated)
            WriteF32(box, 0x58U, 0.0f);
    }

    if (damage != 0) {
        // Score award: (damage/10 + 10)/10, score capped at 999999999.
        g_CurrentRunScoreValue += (damage / 10 + 10) / 10;
        if (g_CurrentRunScoreValue >= 1000000000)
            g_CurrentRunScoreValue = 999999999;
    }
    return damage;
}

} // namespace th10
