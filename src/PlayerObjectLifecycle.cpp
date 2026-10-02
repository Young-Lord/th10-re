#include "PlayerObjectLifecycle.hpp"

#include "CallbackScheduler.hpp"
#include "ManagerWork.hpp"
#include "PlayerRecord.hpp"
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

void WriteBox(float *box, float position_x, float position_y,
              float position_z, float half_x, float half_y, float half_z)
{
    box[0] = position_x - half_x;
    box[1] = position_y - half_y;
    box[2] = position_z - half_z;
    box[3] = half_x + position_x;
    box[4] = half_y + position_y;
    box[5] = half_z + position_z;
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
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    const char *resource_name = g_PlayerCharacter == 0 ? "pl00.anm" : "pl01.anm";
    void *const anm_work = RequestManagerWork(g_GlobalLifecycleResourceOwner,
                                              8, resource_name);
    player_rec.anm_manager_work = anm_work;
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
        player_rec.shot_data = g_PlayerShotEntryCache;
        g_PlayerShotEntryCache = 0;
    }

    ChainElem *const update_element =
        CallbackSchedulerApi::Create(UpdatePlayerCallbackThunk);
    update_element->arg = player;
    update_element->flags &= ~static_cast<u32>(ChainElemFlag_Enabled);
    (void)CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler,
                                                      update_element, 0x10);
    player_rec.update_element = update_element;

    ChainElem *const draw_element =
        CallbackSchedulerApi::Create(RenderPlayerCallbackThunk);
    draw_element->arg = player;
    draw_element->flags &= ~static_cast<u32>(ChainElemFlag_Enabled);
    (void)CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler,
                                               draw_element, 0x16);
    player_rec.draw_element = draw_element;

    InitializePlayerMainVmEsiStackAbi(&player_rec.anim_vm, anm_work, 0);

    player_rec.position_x = 0.0f;
    player_rec.position_y = 400.0f;
    player_rec.position_x_fixed = 0;
    player_rec.position_y_fixed = 40000;
    u8 *const shot = static_cast<u8 *>(player_rec.shot_data);
    // Each hitbox dimension is the shot-table entry scaled by 100 and
    // truncated through the CRT float-to-int conversion 0x00463b2c.
    player_rec.speed_unfocused =
        static_cast<i32>(ReadFloat(shot, 0x10) * 100.0f);
    player_rec.speed_focused =
        static_cast<i32>(ReadFloat(shot, 0x14) * 100.0f);
    player_rec.speed_unfocused_diagonal =
        static_cast<i32>(ReadFloat(shot, 0x18) * 100.0f);
    player_rec.speed_focused_diagonal =
        static_cast<i32>(ReadFloat(shot, 0x1c) * 100.0f);

    for (u32 index = 0; index != 0x21; ++index) {
        player_rec.trail_history[index * 2] =
            static_cast<u32>(player_rec.position_x_fixed);
        player_rec.trail_history[index * 2 + 1] =
            static_cast<u32>(player_rec.position_y_fixed);
    }

    // Three init-once entity records seeded with a negative-NaN transient
    // that is immediately overwritten, matching the original's write order.
    if ((player_rec.autocollect_timer.flags & 1) == 0) {
        player_rec.autocollect_timer.count = 0;
        player_rec.autocollect_timer.prev = static_cast<i32>(0xfff0bdc1U);
        player_rec.autocollect_timer.accum = 0;
        player_rec.autocollect_timer.rate =
            static_cast<const float *>(g_PlayerDefaultDescriptor);
        player_rec.autocollect_timer.flags |= 1;
    }
    // -1.0f stored through the dword view (bit pattern preserved).
    player_rec.autocollect_timer.accum = static_cast<i32>(0xbf800000U);
    player_rec.autocollect_timer.prev = static_cast<i32>(0xfffffffeU);
    player_rec.autocollect_timer.count = static_cast<i32>(0xffffffffU);

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
    player_rec.hit_half_extent[0] = half_extent_a;
    player_rec.hit_half_extent[1] = half_extent_a;
    player_rec.hit_half_extent[2] = 5.0f;
    player_rec.graze_half_extent[0] = half_extent_b;
    player_rec.graze_half_extent[1] = half_extent_b;
    player_rec.graze_half_extent[2] = 5.0f;
    player_rec.item_half_extent[0] = half_extent_c;
    player_rec.item_half_extent[1] = half_extent_c;
    player_rec.item_half_extent[2] = 5.0f;

    const float position_x = player_rec.position_x;
    const float position_y = player_rec.position_y;
    const float position_z = player_rec.position_z;
    WriteBox(player_rec.hit_box, position_x, position_y, position_z,
             half_extent_a, half_extent_a, 5.0f);
    WriteBox(player_rec.graze_box, position_x, position_y, position_z,
             half_extent_b, half_extent_b, 5.0f);
    WriteBox(player_rec.item_box, position_x, position_y, position_z,
             half_extent_c, half_extent_c, 5.0f);
    WriteBox(player_rec.autocollect_box, position_x, position_y, position_z,
             half_extent_c, half_extent_c, 5.0f);

    if ((player_rec.frame_timer.flags & 1) == 0) {
        player_rec.frame_timer.count = 0;
        player_rec.frame_timer.prev = static_cast<i32>(0xfff0bdc1U);
        player_rec.frame_timer.accum = 0;
        player_rec.frame_timer.rate =
            static_cast<const float *>(g_PlayerDefaultDescriptor);
        player_rec.frame_timer.flags |= 1;
    }
    player_rec.frame_timer.count = 0;
    player_rec.frame_timer.accum = 0;
    player_rec.frame_timer.prev = static_cast<i32>(0xffffffffU);

    if ((player_rec.deathbomb_timer.flags & 1) == 0) {
        player_rec.deathbomb_timer.count = 0;
        player_rec.deathbomb_timer.prev = static_cast<i32>(0xfff0bdc1U);
        player_rec.deathbomb_timer.accum = 0;
        player_rec.deathbomb_timer.rate =
            static_cast<const float *>(g_PlayerDefaultDescriptor);
        player_rec.deathbomb_timer.flags |= 1;
    }
    player_rec.deathbomb_timer.count = 0x78;
    player_rec.deathbomb_timer.accum = static_cast<i32>(0x42f00000U);
    player_rec.deathbomb_timer.prev = 0x77;
    player_rec.deathbomb_lerp_percent = 0x1e;

    UpdatePlayerOptionRecords(player);
    return 0;
}

} // namespace th10
