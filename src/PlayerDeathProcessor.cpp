#include "PlayerDeathProcessor.hpp"

#include "LargeRenderOwnerLayout.hpp"
#include "PlayerFrameworkHelpers.hpp"

namespace th10 {

// TH10 0x004269d0. Removes one third of the power above the 5000 floor,
// drops a life with a HUD icon refresh, switches the player to mode 2,
// resets both timer blocks, rebinds the embedded effect record at
// player+0x14, tears down all four option records with hard kill markers,
// and clears boss-owned bullet flags. The position fields are untouched.

namespace {

extern i32 g_PlayerPowerPool; // TH10 DAT_00474c4c (dword, floor 5000)
extern i32 g_PlayerLivesRemaining; // TH10 DAT_00474c70 (signed)
extern i32 g_ScorePenaltyCounter; // TH10 DAT_00474c98 (clamp +-0x400)
extern void *g_AsciiHudOwner; // TH10 DAT_0047770c
extern void *g_GameModeObject; // TH10 DAT_00477838 (+0x10 game mode)
extern void *g_SpellBulletBase; // TH10 DAT_004776f4
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern float g_FrameTimeScale; // TH10 DAT_00476f78

// TH10 0x43e710: ECX = owner, EAX = effect record, EBX = script slot index.
void SpawnDirectionAnimEcxEaxBbxAbi(void *owner, void *record, i32 index);

// TH10 0x424650: ESI = text manager [0x477814], stack = text pointer (the
// pushed position argument is never read).

inline void WriteUint(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

inline void WriteInt(u8 *bytes, u32 offset, i32 value)
{
    *reinterpret_cast<i32 *>(bytes + offset) = value;
}

inline u32 ReadUint(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

// Lazy first-use init of a timer block; the caller's unconditional writes
// overwrite the seeded values, so only the flag and rate pointer survive.
void EnsureTimerBlockInitialized(u8 *block)
{
    if ((*reinterpret_cast<u32 *>(block + 0x10) & 1) != 0)
        return;
    WriteInt(block, 4, 0);
    WriteUint(block, 0, 0xfff0bdc1U);
    WriteUint(block, 8, 0);
    *reinterpret_cast<const float **>(block + 0xc) = &g_FrameTimeScale;
    *reinterpret_cast<u32 *>(block + 0x10) |= 1;
}

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

void HardKillEntity(u32 id)
{
    u8 *entity = 0;
    if (!FindEntityById(id, &entity))
        return;
    *reinterpret_cast<u16 *>(entity + 0x304) = 1;
    if (*reinterpret_cast<const i32 *>(entity + 0x18) != 0)
        return;
    const u32 *child = *reinterpret_cast<const u32 *const *>(entity + 0x14);
    for (; child != 0; child = reinterpret_cast<const u32 *>(child[1])) {
        u8 *const child_entity = reinterpret_cast<u8 *>(child[0]);
        if (child_entity != 0)
            *reinterpret_cast<u16 *>(child_entity + 0x304) = 1;
    }
}

} // namespace

void ProcessPlayerDeathStackAbi(void *player_memory)
{
    u8 *const player = static_cast<u8 *>(player_memory);

    // Power penalty: one third of the amount above the 5000 floor.
    const i32 excess = g_PlayerPowerPool - 5000;
    g_PlayerPowerPool += excess / 3;
    if (g_PlayerPowerPool < 5000)
        g_PlayerPowerPool = 5000;

    g_PlayerLivesRemaining -= 1;
    if (g_PlayerLivesRemaining >= 0)
        RefreshLifeIconsEaxStackAbi(g_PlayerLivesRemaining);

    WriteInt(player, 0x458, 2);

    EnsureTimerBlockInitialized(player + 0x474);
    WriteInt(player, 0x478, 0);
    WriteInt(player, 0x47c, 0);
    WriteUint(player, 0x474, 0xffffffffU);

    EnsureTimerBlockInitialized(player + 0x430c);
    WriteInt(player, 0x4310, 0xb4);
    WriteUint(player, 0x4314, 0x43340000U);
    WriteInt(player, 0x430c, 0xb3);

    SpawnDirectionAnimEcxEaxBbxAbi(
        *reinterpret_cast<void *const *>(player + 0x10), player + 0x14, 0);

    for (u32 index = 0; index != 4; ++index) {
        u8 *const record = player + 0x32a0 + index * 0x98;
        WriteInt(record, 0, 0);
        HardKillEntity(ReadUint(record, 0x68));
        HardKillEntity(ReadUint(record, 0x6c));
    }

    WriteInt(player, 0x3500, 0);
    if (*reinterpret_cast<const i32 *>(
            static_cast<u8 *>(g_GameModeObject) + 0x10) != 1)
        ShowCautionText(reinterpret_cast<const float *>(player + 0x3c0));

    u8 *const bullet_manager = *static_cast<u8 *const *>(g_SpellBulletBase);
    if (*reinterpret_cast<const i32 *>(bullet_manager + 0x3738) >= 60) {
        WriteUint(bullet_manager, 0x3790, 0);
        static const u32 kFlagOffsets[8] = {
            0x378c, 0xad4, 0xe80, 0x15d8, 0x1984, 0x1d30, 0x20dc, 0x2488};
        for (u32 index = 0; index != 8; ++index)
            *reinterpret_cast<u32 *>(bullet_manager + kFlagOffsets[index]) &=
                ~2U;
    }

    g_ScorePenaltyCounter -= 0x400;
    if (g_ScorePenaltyCounter > 0x400)
        g_ScorePenaltyCounter = 0x400;
    if (g_ScorePenaltyCounter < -0x400)
        g_ScorePenaltyCounter = -0x400;
}

} // namespace th10
