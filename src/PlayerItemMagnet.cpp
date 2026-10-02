#include "PlayerItemMagnet.hpp"

#include "PlayerRecord.hpp"
#include "PlayerShotSpawner.hpp"
#include "PlayerTimerHelpers.hpp"

namespace th10 {

// TH10 0x004281d0. Manages the autocollect-line timer (+0x460, whose
// current value mirrors into +0x464) and fires the schedule-driven player
// shots while the line runs. Focusing extends the line as a 0..15 frame
// sawtooth; releasing focus cancels it.

namespace {

extern u8 g_InputMask; // TH10 DAT_00474e5c (bit0 = focus key)
extern i32 g_PlayerPowerGauge; // TH10 DAT_00474c48




} // namespace

// TH10 0x00428160. Walks the 0x34-byte shot descriptors of the schedule row
// selected by power level and focus, spawning when frame % period == phase.
void FireScheduledShotsEdiBbxAbi(void *player_memory, i32 frame)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    i32 row = g_PlayerPowerGauge / 20;
    if (row > 4)
        row = 4;
    if (player_rec.focus_flag != 0)
        row += 5;
    u8 *const buffer = static_cast<u8 *>(player_rec.shot_data);
    u8 *const descriptor =
        *reinterpret_cast<u8 *const *>(buffer + 0x110 +
                                       static_cast<u32>(row) * 8);
    for (u8 *entry = descriptor;
         *reinterpret_cast<const signed char *>(entry) >= 0;
         entry += 0x34) {
        if (frame % static_cast<const i32>(entry[0]) ==
            static_cast<const i32>(entry[1]))
            (void)SpawnPlayerShotStackAbi(player, entry, frame);
    }
}

// TH10 0x004281d0.
i32 TickItemMagnetEaxAbi(void *player_memory)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    if (player_rec.mode != 1) {
        player_rec.homing_target = 0;
        player_rec.homing_target_latch = 0;
        return 0;
    }

    if (player_rec.autocollect_timer.count < 0) {
        if ((g_InputMask & 1) == 0)
            return 0;
        TickPlayerTimerEaxStackAbi(&player_rec.autocollect_timer, 0);
    }

    if (player_rec.autocollect_timer.count !=
        player_rec.autocollect_timer.prev)
        FireScheduledShotsEdiBbxAbi(
            player, player_rec.autocollect_timer.count);

    if (player_rec.autocollect_timer.count > 14) {
        if ((g_InputMask & 1) != 0)
            ShiftTimerByEsiStackAbi(&player_rec.autocollect_timer, -15.0f);
        else
            TickPlayerTimerEaxStackAbi(&player_rec.autocollect_timer, -1);
    } else {
        TickTimerForwardEsiAbi(&player_rec.autocollect_timer);
    }
    return 0;
}

} // namespace th10
