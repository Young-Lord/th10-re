#include "PlayerItemMagnet.hpp"

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




inline i32 ReadInt(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline void WriteInt(u8 *bytes, u32 offset, i32 value)
{
    *reinterpret_cast<i32 *>(bytes + offset) = value;
}

} // namespace

// TH10 0x00428160. Walks the 0x34-byte shot descriptors of the schedule row
// selected by power level and focus, spawning when frame % period == phase.
void FireScheduledShotsEdiBbxAbi(void *player_memory, i32 frame)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    i32 row = g_PlayerPowerGauge / 20;
    if (row > 4)
        row = 4;
    if (ReadInt(player, 0x4474) != 0)
        row += 5;
    u8 *const buffer = *reinterpret_cast<u8 *const *>(player + 0x45c);
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
    if (ReadInt(player, 0x458) != 1) {
        WriteInt(player, 0x3504, 0);
        *reinterpret_cast<u8 *>(player + 0x3508) = 0;
        return 0;
    }

    if (ReadInt(player, 0x464) < 0) {
        if ((g_InputMask & 1) == 0)
            return 0;
        TickPlayerTimerEaxStackAbi(player + 0x460, 0);
    }

    if (ReadInt(player, 0x464) != ReadInt(player, 0x460))
        FireScheduledShotsEdiBbxAbi(player, ReadInt(player, 0x464));

    if (ReadInt(player, 0x464) > 14) {
        if ((g_InputMask & 1) != 0)
            ShiftTimerByEsiStackAbi(player + 0x460, -15.0f);
        else
            TickPlayerTimerEaxStackAbi(player + 0x460, -1);
    } else {
        TickTimerForwardEsiAbi(player + 0x460);
    }
    return 0;
}

} // namespace th10
