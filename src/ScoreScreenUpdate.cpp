// Game-manager state-2 menu controller (TH10 0x0042d420), dispatched by the
// per-frame calculation controller 0x0042cdf0 with the manager in EBX. All
// cursor / spawn / boundary helpers are the semantic bodies exported from
// GameManagerState.cpp and EntityHelpers.cpp; the score-save unlock probe
// (0x0042c850) is reconstructed here because it is this menu's private
// leaf. Offsets are byte offsets into the manager; the timer at +0x2b4 and
// the cursor record at +0x24 follow the shared manager-state layout.
#include "ScoreScreenUpdate.hpp"

#include "GameManagerState.hpp"
#include "EntityHelpers.hpp"
#include "Th10Types.hpp"

// The controller itself stays at global scope to match the extern
// declaration in TitleGameManagerLifecycle.cpp; the th10 typedefs are
// pulled in for the helpers below.
using namespace th10;

namespace {

// Menu input gates shared with the other manager-state modules.
extern u8 g_MenuInputFlagsByte; // TH10 DAT_00474e34
extern u32 g_ManagerSubGateFlags; // TH10 DAT_00474e36

// Global mode flags (bit 0x10 is the practice-mode latch this menu and the
// state-6 body hand to each other) and the current difficulty index.
extern u32 g_GlobalModeFlags; // TH10 DAT_00474ca0
extern u32 g_CurrentDifficulty; // TH10 DAT_00474c74

// Score-save state pointer (TH10 DAT_0047783c); the unlock-flag bank sits
// at +0x1d802..+0x1d811 inside its first stage-record slot block.
extern void *g_ScoreSaveState; // TH10 DAT_0047783c

// Main-chain render owner (TH10 DAT_00491c10) used as the entity manager
// for the handle lookups.
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

// TH10 0x0042c850. Native EAX = score-save state: true when any of the six
// unlock-flag bytes at +0x1d808..+0x1d80d is non-zero.
bool IsScoreUnlockBankActiveEaxAbi(const void *save_state)
{
    const u8 *const bank = static_cast<const u8 *>(save_state);
    for (u32 i = 0x1d808U; i <= 0x1d80dU; ++i) {
        if (bank[i] != 0U)
            return true;
    }
    return false;
}

// Append one pending-flag 1 to the list at +0xb4 (index/count word at
// +0xf8); the native increments that word in place.
void AppendScoreScreenPendingFlag(u32 *words)
{
    const u32 index = words[0xf8 / 4];
    words[(0xb4 + 4 * index) / 4] = 1U;
    words[0xf8 / 4] = index + 1U;
}

} // namespace

// TH10 0x0042d420. Native EBX = game manager, plain retn, always 1.
int RunManagerStateBody2(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    u32 *const cursor = words + 9; // +0x24 value, +0x28 copy, +0x2c max

    switch (words[8]) { // +0x20 sub-state
    case 0: {
        cursor[2] = 8U;
        if (!IsScoreUnlockBankActiveEaxAbi(g_ScoreSaveState))
            AppendScoreScreenPendingFlag(words);

        // Practice-mode latch: seed the cursor value toward 2 (native
        // sign-split: maximum 0 -> 2, > 2 -> 2, else maximum - 1) and
        // clear the bit.
        if ((g_GlobalModeFlags & 0x10U) != 0U) {
            const u32 maximum = cursor[2];
            if (maximum == 0U || maximum > 2U)
                cursor[0] = 2U;
            else
                cursor[0] = maximum - 1U;
            g_GlobalModeFlags &= ~0x10U;
        }

        if (FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                  words[0x424 / 4]) == 0)
            SpawnManagerEntityFromScript(game_manager, 0x58);
        SpawnManagerEntityFromScript(game_manager, 0);
        SetGameManagerSubState(game_manager, 1);
        // Native case 0 falls through into the case 1 body.
    }
    // fallthrough
    case 1:
        if (static_cast<i32>(words[0x2b4 / 4]) > 10) {
            SetGameManagerSubState(game_manager, 2);
            SetEntityStopWordByIdAndRun(words[0x2c4 / 4], 3U);
            SetManagerSlotEntityStopWord(
                game_manager,
                static_cast<u32>(static_cast<u16>(cursor[0]) + 17U), 0U);
        }
        return 1;
    case 2: {
        cursor[1] = cursor[0];

        if ((g_ManagerSubGateFlags & 0x10U) != 0U
            || (g_MenuInputFlagsByte & 0x10U) != 0U)
            ShiftManagerSelector(cursor, -1);
        if ((g_ManagerSubGateFlags & 0x20U) != 0U
            || (g_MenuInputFlagsByte & 0x20U) != 0U)
            ShiftManagerSelector(cursor, 1);

        if (cursor[1] != cursor[0]) {
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590U),
                                  0xcU, 0U);
            SetEntityStopWordByIdAndRun(words[0x2c4 / 4], 3U);
            SetManagerSlotEntityStopWord(
                game_manager,
                static_cast<u32>(static_cast<u16>(cursor[0]) + 7U), 0U);
        }

        // Exit / row accept. Native tests the low byte of the 0x474e36
        // flag dword against 0xa.
        if ((g_ManagerSubGateFlags & 0xaU) != 0U) {
            if (cursor[0] == 7U) {
                ReserveContextChannel(reinterpret_cast<void *>(0x00492590U),
                                      0xbU, 0U);
                SetGameManagerSubState(game_manager, 4U);
                return 1;
            }
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590U),
                                  0xbU, 0U);
            // Clamp the cursor onto the last row (maximum - 1 with the
            // native sign-split idiom).
            const u32 maximum = cursor[2];
            if (maximum == 0U || maximum > 7U)
                cursor[0] = 7U;
            else
                cursor[0] = maximum - 1U;
            Call42C750(game_manager, 0U);
            SetManagerSlotEntityStopWord(
                game_manager,
                static_cast<u32>(static_cast<u16>(cursor[0]) + 7U), 0U);
        }

        // Row accept dispatch (0x1001 flag mask). The slot-6 stop word is
        // queued before the row switch.
        if ((g_ManagerSubGateFlags & 0x1001U) != 0U) {
            SetManagerSlotEntityStopWord(game_manager, 6U, 0U);
            switch (cursor[0]) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
                ReserveContextChannel(
                    reinterpret_cast<void *>(0x00492590U), 0xaU, 0U);
                SetManagerSlotEntityStopWord(game_manager, 90U, 7U);
                SetManagerSlotEntityStopWord(game_manager, 91U, 7U);
                SetGameManagerSubState(game_manager, 4U);
                break;
            case 5:
                ReserveContextChannel(
                    reinterpret_cast<void *>(0x00492590U), 0xaU, 0U);
                SetManagerSlotEntityStopWord(game_manager, 90U, 7U);
                SetManagerSlotEntityStopWord(game_manager, 91U, 7U);
                SetGameManagerSubState(game_manager, 4U);
                ReserveContextChannel(
                    reinterpret_cast<void *>(0x00492590U), 0xaU, 0U);
                break;
            case 6:
                ReserveContextChannel(
                    reinterpret_cast<void *>(0x00492590U), 0xaU, 0U);
                SetGameManagerSubState(game_manager, 4U);
                Call42C750(game_manager, 90U);
                Call42C750(game_manager, 91U);
                break;
            case 7:
                ReserveContextChannel(
                    reinterpret_cast<void *>(0x00492590U), 0xbU, 0U);
                SetGameManagerSubState(game_manager, 4U);
                break;
            default:
                break;
            }
        }
        return 1;
    }
    case 4: {
        if (static_cast<i32>(words[0x2b4 / 4]) < 20)
            return 1;

        switch (cursor[0]) {
        case 0:
            g_GlobalModeFlags &= ~0x10U;
            // Common rows 0/2 tail below.
            ReleaseManagerSlotEntity(game_manager, 0x58U);
            SetGameManagerState(game_manager, 6U);
            RunManagerCursorHandle(cursor);
            {
                u32 difficulty = g_CurrentDifficulty;
                if (difficulty >= 4U) {
                    difficulty = 1U;
                    g_CurrentDifficulty = 1U;
                }
                Call40AD20(difficulty, cursor);
            }
            return 1;
        case 1:
            g_GlobalModeFlags &= ~0x10U;
            ReleaseManagerSlotEntity(game_manager, 0x58U);
            SetGameManagerState(game_manager, 6U);
            RunManagerCursorHandle(cursor);
            words[0x58f0 / 4] = g_CurrentDifficulty;
            g_CurrentDifficulty = 4U;
            // Re-clamp the cursor from the (unchanged) maximum with the
            // native sign-split idiom (positive or zero maximum -> 0,
            // negative maximum -> maximum - 1).
            {
                const i32 maximum = static_cast<i32>(cursor[2]);
                if (maximum >= 0)
                    cursor[0] = 0U;
                else
                    cursor[0] = static_cast<u32>(maximum - 1);
            }
            return 1;
        case 2:
            g_GlobalModeFlags |= 0x10U;
            ReleaseManagerSlotEntity(game_manager, 0x58U);
            SetGameManagerState(game_manager, 6U);
            RunManagerCursorHandle(cursor);
            {
                u32 difficulty = g_CurrentDifficulty;
                if (difficulty >= 4U) {
                    difficulty = 1U;
                    g_CurrentDifficulty = 1U;
                }
                Call40AD20(difficulty, cursor);
            }
            return 1;
        case 3:
            ReleaseManagerSlotEntity(game_manager, 0x58U);
            SetGameManagerState(game_manager, 12U);
            RunManagerCursorHandle(cursor);
            return 1;
        case 4:
            ReleaseManagerSlotEntity(game_manager, 0x58U);
            SetGameManagerState(game_manager, 11U);
            RunManagerCursorHandle(cursor);
            return 1;
        case 5:
            ReleaseManagerSlotEntity(game_manager, 0x58U);
            SetGameManagerState(game_manager, 14U);
            RunManagerCursorHandle(cursor);
            return 1;
        case 6:
            SetGameManagerState(game_manager, 4U);
            RunManagerCursorHandle(cursor);
            return 1;
        case 7:
            SetGameManagerState(game_manager, 3U);
            return 1;
        default:
            return 1;
        }
    }
    default:
        return 1;
    }
}
