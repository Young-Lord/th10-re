// Enemy damage output pass (TH10 0x00428630 neighborhood). Sums the damage
// the player's 128 shot records and the 32 bomb/sub-effect damage boxes deal
// to one enemy position, awards the matching score, and is consumed by the
// ECL script setup (0x0040dc80) to drain boss HP.
#include <cmath>
#include <string.h>

#include "PlayerDamageOutput.hpp"

#include "EntityHelpers.hpp"
#include "GameContext.hpp"
#include "PlayerRecord.hpp"
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
// the current PlayerShotRecord (the 0x49c shot-table entry being tested),
// stack = enemy position. A nonzero return suppresses the default
// collection path.
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
    const GameContext &ctx =
        *static_cast<const GameContext *>(game_context);
    // +0x28 zero means no bomb/sub-effect is running.
    if (ctx.popup_state == 0)
        return 0;

    const float dx = position[0] - ctx.position_x;
    const float dy = position[1] - ctx.position_y;
    const float radius = ctx.radius;
    const i32 variant = ctx.bomb_variant;
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
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    i32 damage = 0;

    // Frame gate: the player's frame-timer {prev, count} pair at +0x474
    // must have advanced (prev != count).
    if (player_rec.frame_timer.count == player_rec.frame_timer.prev)
        return 0;

    const float half_w = size[0] * 0.5f;
    const float half_h = size[1] * 0.5f;
    const float box_left = position[0] - half_w;
    const float box_top = position[1] - half_h;
    const float box_right = position[0] + half_w;
    const float box_bottom = position[1] + half_h;

    if (out_value != 0)
        *out_value = 0;

    // ---- Pass 1: the 128 player-shot records at player+0x49c (0x5c stride,
    // PlayerShotRecord). The native loop cursor sits at record+0x44 (the
    // entity-id slot; TH10 0x00428630 indexes the player pointer + 312
    // dwords), so the handle helpers all take &record.entity_id.
    PlayerShotRecord *item =
        reinterpret_cast<PlayerShotRecord *>(player_bytes + 0x49CU);
    for (i32 index = 128; index != 0; --index, ++item) {
        if (item->state == 0 || item->state == 2)
            continue;

        const u8 *desc = static_cast<const u8 *>(item->descriptor);
        const float half_iw = ReadF32(desc, 12U) * 0.5f;
        const float half_ih = ReadF32(desc, 16U) * 0.5f;
        const float item_left = item->position[0] - half_iw;
        const float item_top = item->position[1] - half_ih;
        const float item_right = item->position[0] + half_iw;
        const float item_bottom = item->position[1] + half_ih;

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
        if (hit != 0 && hit(player, item, position) != 0)
            continue;

        if (item->magnet_latch == 0) {
            SetPlayerShotEntityHitState(&item->entity_id);
            item->magnet_latch = 1;
        }
        item->hit_flag = 1;
        if (item_type != 3 || IsTimerFrameMultiple(
                reinterpret_cast<const u32 *>(&item->timer_prev), 4)) {
            damage += static_cast<i32>(ReadU16(desc, 2U));
        }
        if (item_type != 3) {
            // Spawn the pickup effect and convert the record into its
            // magnet/collect state. The native shrinks the shot speed and
            // plants the 0.1f magnet step into position z.
            void *slot = ResolveItemSpawnSlotEsiAbi(&item->entity_id);
            const u32 saved_handle = *reinterpret_cast<const u32 *>(
                static_cast<const u8 *>(slot) + 0x2CU);
            ReleaseItemSpawnSlotEsiAbi(&item->entity_id);
            const i32 script_id =
                static_cast<i32>(ReadU16(desc, 0x20U)) + 5;
            const u32 *spawned = SpawnItemPickupEffect(
                player_bytes + 0x10U, script_id, 15U);
            item->entity_id = *spawned;
            void *slot_after = ResolveItemSpawnSlotEsiAbi(&item->entity_id);
            *reinterpret_cast<u32 *>(
                static_cast<u8 *>(slot_after) + 0x2CU) = saved_handle;
            *reinterpret_cast<u32 *>(
                static_cast<u8 *>(slot_after) + 0x35CU) |= 4U;
            const float shrunk_speed = item->speed * kShotFadeStep;
            item->position[2] = 0.1f;
            item->state = 2;
            item->speed = shrunk_speed;
        }
        if (item_type == 2) {
            // Point-of-value items pop a sub-effect with damage/3 as limit.
            SpawnPlayerSubEffectEcxDxStackAbi(
                item->position, player, 2.0f, 1.4f,
                13, static_cast<i32>(ReadU16(desc, 2U)) / 3);
        }
    }

    // Bomb / sub-effect damage from the game context.
    damage += ComputeBombAreaDamageThisAbi(g_GameContextObject, position);

    // ---- Pass 2: the 32 damage-box records at player+0x350c (0x6c stride,
    // PlayerSubEffectRecord).
    PlayerSubEffectRecord *box =
        reinterpret_cast<PlayerSubEffectRecord *>(player_bytes + 0x350CU);
    for (i32 index = 32; index != 0; --index, ++box) {
        // The native reads the flag byte plus its padding as one dword.
        const u32 flags =
            *reinterpret_cast<const u32 *>(&box->flags);
        if ((flags & 1U) == 0)
            continue;
        const u32 cur = static_cast<u32>(box->timer_count);
        const u32 prev = static_cast<u32>(box->timer_prev);
        if (cur == prev || static_cast<i32>(cur) % box->period != 0)
            continue;

        const float width = box->box_width;
        const float height = box->box_height;
        bool hits = false;
        if ((flags & 2U) != 0) {
            // Rotated box: the box test runs against the enemy centre point.
            const float angle = -box->damage_angle;
            const float dx = position[0] - box->position[0];
            const float dy = position[1] - box->position[1];
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
            const float cx = box->position[0];
            const float cy = box->position[1];
            const float hw = width * 0.5f;
            const float hh = height * 0.5f;
            if (cx - hw <= box_right && cx + hw >= box_left
                && cy - hh <= box_bottom && cy + hh >= box_top)
                hits = true;
        }
        if (!hits)
            continue;

        const i32 value = box->value;
        damage += value;
        box->accumulated += value;
        if (box->limit <= box->accumulated)
            *reinterpret_cast<float *>(&box->value) = 0.0f;
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
