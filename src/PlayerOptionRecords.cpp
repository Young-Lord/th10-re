#include "PlayerOptionRecords.hpp"

#include "EntityHelpers.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "PlayerOptionCallbacks.hpp"
#include "PlayerRecord.hpp"

#include <cmath>

namespace th10 {

// TH10 0x00426f70. Rebuilds the four option records at player+0x32a0
// (stride 0x98) from the power gauge and character/sub-type selection. The
// two entity-id slots per record are independent: R+0x68 (0x3308 family) is
// the option sprite managed by the rebuild, R+0x6C (0x330C family) is the
// full-power effect entity managed by the early loop. Field table:
//   R+0x00 state (2 = built)      R+0x34/0x38 unfocused position
//   R+0x3C/0x40 render position   R+0x44/0x48 offset source A
//   R+0x4C/0x50 offset source B   R+0x68 sprite entity id
//   R+0x6C power-effect entity id R+0x88 option index
//   R+0x8C tier latch flag        R+0x90 per-record update callback

namespace {

extern u32 g_PlayerCharacter; // TH10 DAT_00474c68 (character 0/1)
extern u32 g_PlayerShotType; // TH10 DAT_00474c6c (sub-type 0..2)
extern u16 g_PlayerPowerGauge; // TH10 DAT_00474c48 (low word, 20 per option)
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern u32 g_OptionIndexTable[8]; // TH10 0x476f7c (cumulative first indices)
extern u8 *g_OptionPositionBase; // TH10 DAT_00477834

// TH10 0x00448e30: create a linked render effect (owner, script, kind on the
// stack) and return the entity whose first dword is the assigned id.
void *CreateRenderEffectStackAbi(void *owner, i32 script_id, i32 kind);

const u32 kSoftReleaseFlag = 0x4000000U;

bool FindEntityById(u32 id, u8 **entity_out)
{
    if (id == 0)
        return false;
    const LargeRenderOwnerLayout &owner =
        *static_cast<const LargeRenderOwnerLayout *>(g_MainChainRenderOwner);
    const OwnerLink *const list_heads[2] = {owner.first_list_a,
                                            owner.first_list_b};
    for (u32 list_index = 0; list_index != 2; ++list_index) {
        for (const OwnerLink *node = list_heads[list_index]; node != 0;
             node = node->next) {
            u8 *const entity = static_cast<u8 *>(node->self_node);
            if (entity != 0 &&
                *reinterpret_cast<const u32 *>(entity) == id) {
                *entity_out = entity;
                return true;
            }
        }
    }
    return false;
}

// Soft release keeps the entity for recycling (0x35c flag); hard release
// marks the u16 at +0x304. Both propagate to the child list at ent+0x14
// only when ent+0x18 is zero.
void ReleaseEntity(u32 id, bool hard)
{
    u8 *entity = 0;
    if (!FindEntityById(id, &entity))
        return;
    if (hard)
        *reinterpret_cast<u16 *>(entity + 0x304) = 1;
    else
        *reinterpret_cast<u32 *>(entity + 0x35c) |= kSoftReleaseFlag;
    if (*reinterpret_cast<const u32 *>(entity + 0x18) != 0)
        return;
    const u32 *child = *reinterpret_cast<const u32 *const *>(entity + 0x14);
    for (; child != 0; child = reinterpret_cast<const u32 *>(child[1])) {
        u8 *const child_entity = reinterpret_cast<u8 *>(child[0]);
        if (child_entity == 0)
            continue;
        if (hard)
            *reinterpret_cast<u16 *>(child_entity + 0x304) = 1;
        else
            *reinterpret_cast<u32 *>(child_entity + 0x35c) |=
                kSoftReleaseFlag;
    }
}

void SpawnOptionEntity(u32 *out_id, i32 script_id)
{
    void *const vm = AllocatePoolVmEsiAbi(g_MainChainRenderOwner);
    *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x20) = 0xf;
    *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x35c) |= 0x40000000U;
    AssignPoolVmScriptEcxEaxAbi(vm, script_id);
    LinkEntityAndAssignIdEaxEsiAbi(out_id, vm);
}

inline float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

// FLD dword [shot+off]; FMUL 100.0f; CALL 0x00463b2c.
i32 ScaleAndRound(const u8 *shot, u32 offset)
{
    return ReadFloat(shot, offset) >= 0.0f
        ? static_cast<i32>(std::floor(
              static_cast<double>(ReadFloat(shot, offset) * 100.0f) + 0.5))
        : static_cast<i32>(std::ceil(
              static_cast<double>(ReadFloat(shot, offset) * 100.0f) - 0.5));
}

} // namespace

void RebuildPlayerOptionRecords(void *player_memory)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    // Alias view over DAT_00477834 kept for the trail-window seed reads.
    const PlayerRecord &opt_pos_base =
        *reinterpret_cast<const PlayerRecord *>(g_OptionPositionBase);
    const u16 gauge = g_PlayerPowerGauge;

    if (gauge >= 100) {
        // Full-power path: recycle and unconditionally respawn the power
        // effect entities stored at R+0x6C.
        for (u32 index = 0; index != 4; ++index) {
            PlayerOptionRecord &rec = player_rec.options[index];
            if (rec.power_effect_entity_id != 0)
                ReleaseEntity(rec.power_effect_entity_id, false);
            rec.power_effect_entity_id = 0;
            const u32 script_choice = g_PlayerShotType + g_PlayerCharacter * 3;
            static const i32 kScripts[6] = {0x14, 0x15, 0x16,
                                            0x14, 0x15, 0x16};
            if (script_choice < 6) {
                void *const entity = CreateRenderEffectStackAbi(
                    player_rec.anm_manager_work,
                    kScripts[script_choice], 0xf);
                rec.power_effect_entity_id =
                    *reinterpret_cast<const u32 *>(entity);
            }
        }
    } else {
        for (u32 index = 0; index != 4; ++index) {
            const u32 effect_id =
                player_rec.options[index].power_effect_entity_id;
            if (effect_id != 0)
                ReleaseEntity(effect_id, true);
        }
    }

    i32 count = static_cast<i32>(gauge) / 20;
    if (count > 4)
        count = 4;
    if (player_rec.option_count == count)
        return;

    const u8 *const shot = static_cast<const u8 *>(player_rec.shot_data);
    u32 index = 0;
    for (; static_cast<i32>(index) < count; ++index) {
        PlayerOptionRecord &rec = player_rec.options[index];
        // The option position fields carry x100 fixed-point dwords; the
        // dword view is kept on every access.
        *reinterpret_cast<i32 *>(&rec.render_position[0]) =
            player_rec.position_x_fixed;
        *reinterpret_cast<i32 *>(&rec.render_position[1]) =
            player_rec.position_y_fixed;
        const u32 old_id = rec.sprite_entity_id;
        if (old_id != 0)
            ReleaseEntity(old_id, false);
        rec.sprite_entity_id = 0;
        rec.option_index = index;

        if (g_PlayerCharacter == 0) {
            const u32 script_base = g_OptionIndexTable[count];
            const u32 pair = (script_base + index) * 12;
            *reinterpret_cast<i32 *>(&rec.offset_source_a[0]) =
                ScaleAndRound(shot, pair + 0x20);
            *reinterpret_cast<i32 *>(&rec.offset_source_a[1]) =
                ScaleAndRound(shot, pair + 0x24);
            *reinterpret_cast<i32 *>(&rec.offset_source_b[0]) =
                ScaleAndRound(shot, pair + 0x98);
            *reinterpret_cast<i32 *>(&rec.offset_source_b[1]) =
                ScaleAndRound(shot, pair + 0x9c);
            const float *const src =
                player_rec.focus_flag != 0 ? rec.offset_source_b
                                           : rec.offset_source_a;
            *reinterpret_cast<i32 *>(&rec.unfocused_position[0]) =
                player_rec.position_x_fixed +
                *reinterpret_cast<const i32 *>(&src[0]);
            *reinterpret_cast<i32 *>(&rec.unfocused_position[1]) =
                player_rec.position_y_fixed +
                *reinterpret_cast<const i32 *>(&src[1]);
            *reinterpret_cast<i32 *>(&rec.render_position[0]) =
                *reinterpret_cast<const i32 *>(&rec.unfocused_position[0]);
            *reinterpret_cast<i32 *>(&rec.render_position[1]) =
                *reinterpret_cast<const i32 *>(&rec.unfocused_position[1]);
            static const i32 kScripts[3] = {0x11, 0x12, 0x13};
            if (g_PlayerShotType < 3) {
                u32 id = 0;
                SpawnOptionEntity(&id, kScripts[g_PlayerShotType]);
                rec.sprite_entity_id = id;
            }
        } else if (g_PlayerCharacter == 1) {
            if (g_PlayerShotType == 0) {
                const u32 script_base = g_OptionIndexTable[count];
                const u32 pair = (script_base + index) * 12;
                if (player_rec.focus_flag == 0) {
                    *reinterpret_cast<i32 *>(&rec.offset_source_a[0]) =
                        ScaleAndRound(shot, pair + 0x20);
                    *reinterpret_cast<i32 *>(&rec.offset_source_a[1]) =
                        ScaleAndRound(shot, pair + 0x24);
                }
                *reinterpret_cast<i32 *>(&rec.offset_source_b[0]) =
                    ScaleAndRound(shot, pair + 0x98);
                *reinterpret_cast<i32 *>(&rec.offset_source_b[1]) =
                    ScaleAndRound(shot, pair + 0x9c);
                *reinterpret_cast<i32 *>(&rec.render_position[0]) =
                    static_cast<i32>(
                        opt_pos_base.trail_history[16 + index * 16]);
                *reinterpret_cast<i32 *>(&rec.render_position[1]) =
                    static_cast<i32>(
                        opt_pos_base.trail_history[17 + index * 16]);
                if (player_rec.focus_flag != 0 && rec.state == 0) {
                    if (index == 0) {
                        *reinterpret_cast<i32 *>(&rec.offset_source_a[0]) =
                            ScaleAndRound(shot, script_base * 12 + 0x98);
                        *reinterpret_cast<i32 *>(&rec.offset_source_a[1]) =
                            ScaleAndRound(shot, script_base * 12 + 0x9c);
                    } else {
                        PlayerOptionRecord &previous =
                            player_rec.options[index - 1];
                        *reinterpret_cast<i32 *>(&rec.offset_source_a[0]) =
                            *reinterpret_cast<const i32 *>(
                                &previous.offset_source_a[0]);
                        *reinterpret_cast<i32 *>(&rec.offset_source_a[1]) =
                            *reinterpret_cast<const i32 *>(
                                &previous.offset_source_a[1]);
                    }
                }
                rec.update_callback =
                    reinterpret_cast<void *>(&UpdateHomingOptionRecord);
                u32 id = 0;
                SpawnOptionEntity(&id, 0x11);
                rec.sprite_entity_id = id;
                *reinterpret_cast<i32 *>(&rec.render_position[0]) =
                    static_cast<i32>(player_rec.trail_history[index * 16]);
                *reinterpret_cast<i32 *>(&rec.render_position[1]) =
                    static_cast<i32>(player_rec.trail_history[index * 16 + 1]);
            } else if (g_PlayerShotType == 1) {
                *reinterpret_cast<i32 *>(&rec.offset_source_a[0]) =
                    ScaleAndRound(shot, 0x20);
                *reinterpret_cast<i32 *>(&rec.offset_source_a[1]) =
                    ScaleAndRound(shot, 0x24);
                *reinterpret_cast<i32 *>(&rec.offset_source_b[0]) =
                    ScaleAndRound(shot, 0x98);
                *reinterpret_cast<i32 *>(&rec.offset_source_b[1]) =
                    ScaleAndRound(shot, 0x9c);
                const float *const src =
                    player_rec.focus_flag == 0 ? rec.offset_source_a
                                               : rec.offset_source_b;
                *reinterpret_cast<i32 *>(&rec.unfocused_position[0]) =
                    player_rec.position_x_fixed +
                    *reinterpret_cast<const i32 *>(&src[0]);
                *reinterpret_cast<i32 *>(&rec.unfocused_position[1]) =
                    player_rec.position_y_fixed +
                    *reinterpret_cast<const i32 *>(&src[1]);
                u32 id = 0;
                SpawnOptionEntity(&id, 0x12);
                rec.sprite_entity_id = id;
            } else if (g_PlayerShotType == 2) {
                *reinterpret_cast<i32 *>(&rec.offset_source_a[0]) =
                    ScaleAndRound(shot, 0x20);
                *reinterpret_cast<i32 *>(&rec.offset_source_a[1]) =
                    ScaleAndRound(shot, 0x24);
                *reinterpret_cast<i32 *>(&rec.offset_source_b[0]) =
                    ScaleAndRound(shot, 0x98);
                *reinterpret_cast<i32 *>(&rec.offset_source_b[1]) =
                    ScaleAndRound(shot, 0x9c);
                if (player_rec.focus_flag == 0 || rec.state == 0) {
                    const i32 x = player_rec.position_x_fixed +
                                  *reinterpret_cast<const i32 *>(
                                      &rec.offset_source_a[0]);
                    const i32 y = player_rec.position_y_fixed +
                                  *reinterpret_cast<const i32 *>(
                                      &rec.offset_source_a[1]);
                    *reinterpret_cast<i32 *>(&rec.unfocused_position[0]) = x;
                    *reinterpret_cast<i32 *>(&rec.unfocused_position[1]) = y;
                    *reinterpret_cast<i32 *>(&rec.offset_source_b[0]) = x;
                    *reinterpret_cast<i32 *>(&rec.offset_source_b[1]) = y;
                }
                u32 id = 0;
                SpawnOptionEntity(&id, 0x13);
                rec.sprite_entity_id = id;
                if (player_rec.focus_flag != 0)
                    SetEntityStateWordEaxEsiAbi(&rec.sprite_entity_id, 3);
                rec.update_callback =
                    reinterpret_cast<void *>(&UpdateAngularOptionRecord);
            }
        }

        rec.state = 2;
    }

    for (; index < 4; ++index) {
        PlayerOptionRecord &rec = player_rec.options[index];
        rec.state = 0;
        const u32 old_id = rec.sprite_entity_id;
        if (old_id != 0)
            ReleaseEntity(old_id, true);
    }

    player_rec.option_count = count;
    // +0x332c/+0x33c4/+0x345c/+0x34f4 = options[0..3].tier_latch.
    player_rec.options[0].tier_latch = 1;
    player_rec.options[1].tier_latch = 1;
    player_rec.options[2].tier_latch = 1;
    player_rec.options[3].tier_latch = 1;
}

} // namespace th10
