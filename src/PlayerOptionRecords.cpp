#include "PlayerOptionRecords.hpp"

#include "EntityHelpers.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "PlayerOptionCallbacks.hpp"

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

inline i32 ReadInt(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline u32 ReadUint(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline void WriteUint(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

inline void WriteInt(u8 *bytes, u32 offset, i32 value)
{
    *reinterpret_cast<i32 *>(bytes + offset) = value;
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
    const u16 gauge = g_PlayerPowerGauge;

    if (gauge >= 100) {
        // Full-power path: recycle and unconditionally respawn the power
        // effect entities stored at R+0x6C.
        for (u32 index = 0; index != 4; ++index) {
            u8 *const record = player + 0x32a0 + index * 0x98;
            u32 *const effect_slot = reinterpret_cast<u32 *>(record + 0x6c);
            if (*effect_slot != 0)
                ReleaseEntity(*effect_slot, false);
            *effect_slot = 0;
            const u32 script_choice = g_PlayerShotType + g_PlayerCharacter * 3;
            static const i32 kScripts[6] = {0x14, 0x15, 0x16,
                                            0x14, 0x15, 0x16};
            if (script_choice < 6) {
                void *const entity = CreateRenderEffectStackAbi(
                    *reinterpret_cast<void *const *>(player + 0x10),
                    kScripts[script_choice], 0xf);
                *effect_slot = *reinterpret_cast<const u32 *>(entity);
            }
        }
    } else {
        for (u32 index = 0; index != 4; ++index) {
            const u32 effect_id =
                ReadUint(player + 0x32a0 + index * 0x98, 0x6c);
            if (effect_id != 0)
                ReleaseEntity(effect_id, true);
        }
    }

    i32 count = static_cast<i32>(gauge) / 20;
    if (count > 4)
        count = 4;
    if (ReadInt(player, 0x3500) == count)
        return;

    const u8 *const shot = *reinterpret_cast<u8 *const *>(player + 0x45c);
    u32 index = 0;
    for (; static_cast<i32>(index) < count; ++index) {
        u8 *const record = player + 0x32a0 + index * 0x98;
        WriteInt(record, 0x3c, ReadInt(player, 0x3cc));
        WriteInt(record, 0x40, ReadInt(player, 0x3d0));
        const u32 old_id = ReadUint(record, 0x68);
        if (old_id != 0)
            ReleaseEntity(old_id, false);
        WriteUint(record, 0x68, 0);
        WriteInt(record, 0x88, static_cast<i32>(index));

        if (g_PlayerCharacter == 0) {
            const u32 script_base = g_OptionIndexTable[count];
            const u32 pair = (script_base + index) * 12;
            WriteInt(record, 0x44, ScaleAndRound(shot, pair + 0x20));
            WriteInt(record, 0x48, ScaleAndRound(shot, pair + 0x24));
            WriteInt(record, 0x4c, ScaleAndRound(shot, pair + 0x98));
            WriteInt(record, 0x50, ScaleAndRound(shot, pair + 0x9c));
            const u32 src = ReadInt(player, 0x4474) != 0 ? 0x4c : 0x44;
            WriteInt(record, 0x34,
                     ReadInt(player, 0x3cc) + ReadInt(record, src));
            WriteInt(record, 0x38,
                     ReadInt(player, 0x3d0) + ReadInt(record, src + 4));
            WriteInt(record, 0x3c, ReadInt(record, 0x34));
            WriteInt(record, 0x40, ReadInt(record, 0x38));
            static const i32 kScripts[3] = {0x11, 0x12, 0x13};
            if (g_PlayerShotType < 3) {
                u32 id = 0;
                SpawnOptionEntity(&id, kScripts[g_PlayerShotType]);
                WriteUint(record, 0x68, id);
            }
        } else if (g_PlayerCharacter == 1) {
            if (g_PlayerShotType == 0) {
                const u32 script_base = g_OptionIndexTable[count];
                const u32 pair = (script_base + index) * 12;
                if (ReadInt(player, 0x4474) == 0) {
                    WriteInt(record, 0x44,
                             ScaleAndRound(shot, pair + 0x20));
                    WriteInt(record, 0x48,
                             ScaleAndRound(shot, pair + 0x24));
                }
                WriteInt(record, 0x4c, ScaleAndRound(shot, pair + 0x98));
                WriteInt(record, 0x50, ScaleAndRound(shot, pair + 0x9c));
                WriteInt(record, 0x3c,
                         ReadInt(g_OptionPositionBase + index * 0x40 +
                                 0x43ac, 0));
                WriteInt(record, 0x40,
                         ReadInt(g_OptionPositionBase + index * 0x40 +
                                 0x43b0, 0));
                if (ReadInt(player, 0x4474) != 0 && ReadInt(record, 0) == 0) {
                    if (index == 0) {
                        WriteInt(record, 0x44,
                                 ScaleAndRound(shot, script_base * 12 +
                                                      0x98));
                        WriteInt(record, 0x48,
                                 ScaleAndRound(shot, script_base * 12 +
                                                      0x9c));
                    } else {
                        WriteInt(record, 0x44,
                                 ReadInt(record - 0x98, 0x44));
                        WriteInt(record, 0x48,
                                 ReadInt(record - 0x98, 0x48));
                    }
                }
                *reinterpret_cast<void **>(record + 0x90) =
                    reinterpret_cast<void *>(&UpdateHomingOptionRecord);
                u32 id = 0;
                SpawnOptionEntity(&id, 0x11);
                WriteUint(record, 0x68, id);
                WriteInt(record, 0x3c,
                         ReadInt(player, 0x436c + index * 0x40));
                WriteInt(record, 0x40,
                         ReadInt(player, 0x4370 + index * 0x40));
            } else if (g_PlayerShotType == 1) {
                WriteInt(record, 0x44, ScaleAndRound(shot, 0x20));
                WriteInt(record, 0x48, ScaleAndRound(shot, 0x24));
                WriteInt(record, 0x4c, ScaleAndRound(shot, 0x98));
                WriteInt(record, 0x50, ScaleAndRound(shot, 0x9c));
                const u32 src = ReadInt(player, 0x4474) == 0 ? 0x44 : 0x4c;
                WriteInt(record, 0x34,
                         ReadInt(player, 0x3cc) + ReadInt(record, src));
                WriteInt(record, 0x38,
                         ReadInt(player, 0x3d0) + ReadInt(record, src + 4));
                u32 id = 0;
                SpawnOptionEntity(&id, 0x12);
                WriteUint(record, 0x68, id);
            } else if (g_PlayerShotType == 2) {
                WriteInt(record, 0x44, ScaleAndRound(shot, 0x20));
                WriteInt(record, 0x48, ScaleAndRound(shot, 0x24));
                WriteInt(record, 0x4c, ScaleAndRound(shot, 0x98));
                WriteInt(record, 0x50, ScaleAndRound(shot, 0x9c));
                if (ReadInt(player, 0x4474) == 0 || ReadInt(record, 0) == 0) {
                    const i32 x = ReadInt(player, 0x3cc) +
                                  ReadInt(record, 0x44);
                    const i32 y = ReadInt(player, 0x3d0) +
                                  ReadInt(record, 0x48);
                    WriteInt(record, 0x34, x);
                    WriteInt(record, 0x38, y);
                    WriteInt(record, 0x4c, x);
                    WriteInt(record, 0x50, y);
                }
                u32 id = 0;
                SpawnOptionEntity(&id, 0x13);
                WriteUint(record, 0x68, id);
                if (ReadInt(player, 0x4474) != 0)
                    SetEntityStateWordEaxEsiAbi(
                        reinterpret_cast<u32 *>(record + 0x68), 3);
                *reinterpret_cast<void **>(record + 0x90) =
                    reinterpret_cast<void *>(&UpdateAngularOptionRecord);
            }
        }

        WriteInt(record, 0, 2);
    }

    for (; index < 4; ++index) {
        u8 *const record = player + 0x32a0 + index * 0x98;
        WriteInt(record, 0, 0);
        const u32 old_id = ReadUint(record, 0x68);
        if (old_id != 0)
            ReleaseEntity(old_id, true);
    }

    WriteInt(player, 0x3500, count);
    WriteInt(player, 0x332c, 1);
    WriteInt(player, 0x33c4, 1);
    WriteInt(player, 0x345c, 1);
    WriteInt(player, 0x34f4, 1);
}

} // namespace th10
