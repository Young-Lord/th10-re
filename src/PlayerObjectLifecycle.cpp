#include "PlayerObjectLifecycle.hpp"

#include "CallbackScheduler.hpp"
#include "ManagerWork.hpp"
#include "PlayerShotData.hpp"

namespace th10 {

namespace {

extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern ManagerWorkOwnerPartial *g_GlobalLifecycleResourceOwner; // 0x491c10
extern u32 g_PlayerCharacter; // TH10 DAT_00474c68 (0=pl00, 1=pl01)
extern i32 g_PlayerShotType; // TH10 DAT_00474c6c
extern void *g_PlayerShotEntryCache; // TH10 DAT_00491bf0
extern const float g_PlayerSpeedTable[4]; // TH10 DAT_00476fa0
extern const float g_PlayerItemBoxHalfTable[4]; // TH10 DAT_00476fa8
extern const float g_PlayerGrazeHalfTable[4]; // TH10 DAT_00476fb0
extern const float g_PlayerHitHalfTable[4]; // TH10 DAT_00476fb8
extern void *const g_PlayerShotEntryTable[6]; // TH10 DAT_00476fc0
extern void *g_PlayerDefaultDescriptor; // TH10 0x476f78 (1.0f)

void AppendMainChainErrorText(const char *text); // TH10 0x0044b810

// Native update thunk 0x00426500 (PUSH ECX; CALL 0x425730) and draw thunk
// 0x00426510 (MOV EAX,ECX; JMP 0x426360); both receive the player in ECX.
i32 TH10_FASTCALL UpdatePlayerCallbackThunk(void *player);
i32 TH10_FASTCALL RenderPlayerCallbackThunk(void *player);


// TH10 0x00426f70 takes the player on the stack and rebuilds the four
// option records at player+0x32a0 from the selected character/shot pair.
void UpdatePlayerOptionRecords(void *player);

inline float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void WriteFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

void WriteBox(u8 *player, u32 offset, float position_x, float position_y,
              float position_z, float half_x, float half_y, float half_z)
{
    WriteFloat(player, offset, position_x - half_x);
    WriteFloat(player, offset + 4, position_y - half_y);
    WriteFloat(player, offset + 8, position_z - half_z);
    WriteFloat(player, offset + 0xc, half_x + position_x);
    WriteFloat(player, offset + 0x10, half_y + position_y);
    WriteFloat(player, offset + 0x14, half_z + position_z);
}

} // namespace

// TH10 0x004247f0. The player object embeds one animation VM at +0x14, a
// shot-data sub-object pointer at +0x45c, four 0x98-byte option records
// rooted at +0x32a0, and a position vector at +0x3c0. Two scheduler chains
// are registered here: the update thunk 0x00426500 on the calculation chain
// (priority 0x10) and the draw thunk 0x00426510 on the draw chain (priority
// 0x16); both thunks forward to the player in their native register.
i32 InitializePlayerObject(void *player_memory)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    const char *resource_name = g_PlayerCharacter == 0 ? "pl00.anm" : "pl01.anm";
    void *const anm_work = RequestManagerWork(g_GlobalLifecycleResourceOwner,
                                              8, resource_name);
    *reinterpret_cast<void **>(player + 0x10) = anm_work;
    if (anm_work == 0) {
        AppendMainChainErrorText(
            "自機データが見つかりません。データが壊れています\r\n");
        return -1;
    }

    if (g_PlayerShotEntryCache == 0) {
        const u32 shot_index =
            static_cast<u32>(g_PlayerShotType + g_PlayerCharacter * 3);
        void *const shot_entry = g_PlayerShotEntryTable[shot_index];
        if (LoadPlayerShotDataEsiEaxAbi(player,
                static_cast<const char *>(shot_entry)) != 0) {
            AppendMainChainErrorText(
                "自機データが見つかりません。データが壊れています\r\n");
            return -1;
        }
    } else {
        *reinterpret_cast<void **>(player + 0x45c) = g_PlayerShotEntryCache;
        g_PlayerShotEntryCache = 0;
    }

    ChainElem *const update_element =
        CallbackSchedulerApi::Create(UpdatePlayerCallbackThunk);
    update_element->arg = player;
    update_element->flags &= ~static_cast<u32>(ChainElemFlag_Enabled);
    (void)CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler,
                                                      update_element, 0x10);
    *reinterpret_cast<ChainElem **>(player + 8) = update_element;

    ChainElem *const draw_element =
        CallbackSchedulerApi::Create(RenderPlayerCallbackThunk);
    draw_element->arg = player;
    draw_element->flags &= ~static_cast<u32>(ChainElemFlag_Enabled);
    (void)CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler,
                                               draw_element, 0x16);
    *reinterpret_cast<ChainElem **>(player + 0xc) = draw_element;

    InitializePlayerMainVmEsiStackAbi(player + 0x14, anm_work, 0);

    WriteFloat(player, 0x3c0, 0.0f);
    WriteFloat(player, 0x3c4, 400.0f);
    *reinterpret_cast<u32 *>(player + 0x3cc) = 0;
    *reinterpret_cast<u32 *>(player + 0x3d0) = 40000;
    u8 *const shot = *reinterpret_cast<u8 **>(player + 0x45c);
    // Each hitbox dimension is the shot-table entry scaled by 100 and
    // truncated through the CRT float-to-int conversion 0x00463b2c.
    *reinterpret_cast<i32 *>(player + 0x3d4) =
        static_cast<i32>(ReadFloat(shot, 0x10) * 100.0f);
    *reinterpret_cast<i32 *>(player + 0x3d8) =
        static_cast<i32>(ReadFloat(shot, 0x14) * 100.0f);
    *reinterpret_cast<i32 *>(player + 0x3dc) =
        static_cast<i32>(ReadFloat(shot, 0x18) * 100.0f);
    *reinterpret_cast<i32 *>(player + 0x3e0) =
        static_cast<i32>(ReadFloat(shot, 0x1c) * 100.0f);

    for (u32 index = 0; index != 0x21; ++index) {
        *reinterpret_cast<u32 *>(player + 0x436c + index * 8) =
            *reinterpret_cast<const u32 *>(player + 0x3cc);
        *reinterpret_cast<u32 *>(player + 0x4370 + index * 8) =
            *reinterpret_cast<const u32 *>(player + 0x3d0);
    }

    // Three init-once entity records seeded with a negative-NaN transient
    // that is immediately overwritten, matching the original's write order.
    if ((*reinterpret_cast<u32 *>(player + 0x470) & 1) == 0) {
        *reinterpret_cast<u32 *>(player + 0x464) = 0;
        *reinterpret_cast<u32 *>(player + 0x460) = 0xfff0bdc1U;
        *reinterpret_cast<u32 *>(player + 0x468) = 0;
        *reinterpret_cast<void **>(player + 0x46c) = g_PlayerDefaultDescriptor;
        *reinterpret_cast<u32 *>(player + 0x470) |= 1;
    }
    *reinterpret_cast<u32 *>(player + 0x468) = 0xbf800000U;
    *reinterpret_cast<u32 *>(player + 0x460) = 0xfffffffeU;
    *reinterpret_cast<u32 *>(player + 0x464) = 0xffffffffU;

    // Shot-table entries land on the sub-object before the boxes use them;
    // the first half-extent re-reads the just-written hitbox entry.
    *reinterpret_cast<u32 *>(shot + 4) =
        *reinterpret_cast<const u32 *>(&g_PlayerHitHalfTable[
            static_cast<u32>(g_PlayerCharacter)]);
    *reinterpret_cast<u32 *>(shot + 0xc) =
        *reinterpret_cast<const u32 *>(&g_PlayerGrazeHalfTable[
            static_cast<u32>(g_PlayerCharacter)]);
    *reinterpret_cast<u32 *>(shot + 8) =
        *reinterpret_cast<const u32 *>(&g_PlayerSpeedTable[
            static_cast<u32>(g_PlayerCharacter)]);

    const float half_extent_a = ReadFloat(shot, 4) * 0.5f;
    const float half_extent_b =
        g_PlayerGrazeHalfTable[static_cast<u32>(g_PlayerCharacter)] * 0.5f;
    const float half_extent_c =
        g_PlayerItemBoxHalfTable[static_cast<u32>(g_PlayerCharacter)] * 0.5f;
    WriteFloat(player, 0x41c, half_extent_a);
    WriteFloat(player, 0x420, half_extent_a);
    WriteFloat(player, 0x424, 5.0f);
    WriteFloat(player, 0x428, half_extent_b);
    WriteFloat(player, 0x42c, half_extent_b);
    WriteFloat(player, 0x430, 5.0f);
    WriteFloat(player, 0x434, half_extent_c);
    WriteFloat(player, 0x438, half_extent_c);
    WriteFloat(player, 0x43c, 5.0f);

    const float position_x = ReadFloat(player, 0x3c0);
    const float position_y = ReadFloat(player, 0x3c4);
    const float position_z = ReadFloat(player, 0x3c8);
    WriteBox(player, 0x404, position_x, position_y, position_z,
             half_extent_a, half_extent_a, 5.0f);
    WriteBox(player, 0x4324, position_x, position_y, position_z,
             half_extent_b, half_extent_b, 5.0f);
    WriteBox(player, 0x433c, position_x, position_y, position_z,
             half_extent_c, half_extent_c, 5.0f);
    WriteBox(player, 0x4354, position_x, position_y, position_z,
             half_extent_c, half_extent_c, 5.0f);

    if ((*reinterpret_cast<u32 *>(player + 0x484) & 1) == 0) {
        *reinterpret_cast<u32 *>(player + 0x478) = 0;
        *reinterpret_cast<u32 *>(player + 0x474) = 0xfff0bdc1U;
        *reinterpret_cast<u32 *>(player + 0x47c) = 0;
        *reinterpret_cast<void **>(player + 0x480) = g_PlayerDefaultDescriptor;
        *reinterpret_cast<u32 *>(player + 0x484) |= 1;
    }
    *reinterpret_cast<u32 *>(player + 0x478) = 0;
    *reinterpret_cast<u32 *>(player + 0x47c) = 0;
    *reinterpret_cast<u32 *>(player + 0x474) = 0xffffffffU;

    if ((*reinterpret_cast<u32 *>(player + 0x431c) & 1) == 0) {
        *reinterpret_cast<u32 *>(player + 0x4310) = 0;
        *reinterpret_cast<u32 *>(player + 0x430c) = 0xfff0bdc1U;
        *reinterpret_cast<u32 *>(player + 0x4314) = 0;
        *reinterpret_cast<void **>(player + 0x4318) =
            g_PlayerDefaultDescriptor;
        *reinterpret_cast<u32 *>(player + 0x431c) |= 1;
    }
    *reinterpret_cast<u32 *>(player + 0x4310) = 0x78;
    *reinterpret_cast<u32 *>(player + 0x4314) = 0x42f00000U;
    *reinterpret_cast<u32 *>(player + 0x430c) = 0x77;
    *reinterpret_cast<u32 *>(player + 0x4308) = 0x1e;

    UpdatePlayerOptionRecords(player);
    return 0;
}

} // namespace th10
