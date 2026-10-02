// TH10 0x00422c80 — pause menu state machine (modes 1..5 of the
// calculation-record dispatcher 0x004223f0), plus its resume helper
// 0x00422c30. Mode 1 arms the item cursor, mode 2 is the item list
// (accept splits into the terminal mode 3 and the confirm submenu mode 4,
// with the 0x75/0x74 child sprites highlighted through 0x004243f0), mode 3
// is the terminal dispatcher (resume / state transition / shared status
// gate), mode 4 the confirm submenu (kinds 0x77/0x78) and mode 5 its
// commit handler. Modes 2 and 4 share the cancel tail at 0x423059
// (gate byte 0x474e36 bit 3).
#include <stdlib.h>

#include "BgmRuntime.hpp"
#include "EntityHelpers.hpp"
#include "GameManagerState.hpp"
#include "GameStateManagerObject.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "PlayerTimerHelpers.hpp"
#include "PauseMenuModes.hpp"
#include "Th10Types.hpp"
#include "TitleScreenObject.hpp"

namespace th10 {

namespace {

// ---------------------------------------------------------------- globals

extern void *g_TitleScreen;          // TH10 DAT_00477810 (pause/game state)
extern u32 g_ManagerSubGateFlags;    // TH10 DAT_00474e36
extern u8 g_MenuInputFlagsByte;      // TH10 DAT_00474e34
extern u32 g_SharedStatusGate;       // TH10 DAT_00491fb8
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern float g_FrameTimeScale;       // TH10 DAT_00476f78

const u32 kGameStateManager = 0x491C28U; // RequestGameStateTransition target

// The record is the 0x2c8 game-state manager (TH10 DAT_00477830), accessed
// through the typed GameStateManager view (src/GameStateManagerObject.hpp):
// mode +0x04, frame-timer count +0x14, cursor record +0x24 (value +0x24 /
// previous +0x28 / maximum +0x2c / wrap flag +0xf4), overlay handles
// +0x1d4 / +0x1d8 / +0x1dc, saved time scale +0x2c0.

// ------------------------------------------------------- access helpers

// Zero-extended 16-bit state word argument (`xor esi,esi; mov si,value;
// add si,7` / `add si,0xf`).
inline i32 StateWordArgZeroExt(u32 value, u32 bias)
{
    return static_cast<i32>(static_cast<u16>(value + bias));
}

// Sign-extended state word argument (`movsx esi,ax; add si,7`).
inline i32 StateWordArgSignExt(i32 value, u32 bias)
{
    const i32 s = static_cast<i16>(static_cast<u16>(value));
    return static_cast<i32>((static_cast<u32>(s) & 0xFFFF0000U)
                            | static_cast<u32>(static_cast<u16>(s + bias)));
}

// Max-aware store: max < 0 stores max - 1, otherwise 0 (native `test/jg/dec`
// chain, preserved verbatim).
inline void StoreClampedAgainstMax(i32 &value, const i32 &max)
{
    if (max < 0) {
        value = max - 1;
    } else {
        value = 0;
    }
}

inline void PlayMenuSound(u32 kind)
{
    ReserveContextChannel(reinterpret_cast<void *>(0x492590U), kind, 0U);
}

inline void ArmRunTimer(GameStateManager &mgr)
{
    TickPlayerTimerEaxStackAbi(&mgr.frame_timer, 0);
}

// TH10 0x004243f0. Set the u16 stop word 6 at entity+0x304 for the entity
// resolved from *handle_slot, propagating to the +0x14 child chain when the
// +0x18 count is zero. (Duplicate of the anonymous-namespace helper in
// PostRunReplaySaveMenu.cpp; see the reconciliation note in
// docs/evidence/replay-context-and-player-damage.md.)
void SetResultEntityStateSixByHandleSlot(u32 *handle_slot)
{
    u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner, *handle_slot);
    if (entity == 0) {
        return;
    }
    *reinterpret_cast<u16 *>(entity + 0x304) = 6;
    if (*reinterpret_cast<u32 *>(entity + 0x18) != 0) {
        return;
    }
    u32 node = *reinterpret_cast<u32 *>(entity + 0x14);
    while (node != 0) {
        u8 *child = *reinterpret_cast<u8 **>(node);
        *reinterpret_cast<u16 *>(child + 0x304) = 6;
        node = *reinterpret_cast<u32 *>(node + 4);
    }
}

// Highlight a child of `kind` under the record's handle A and stop it.
void HighlightChildByKind(GameStateManager &mgr, i32 kind, u32 *out_handle)
{
    ResolveChildEntityByKind(&mgr.handle_a_01d4, kind, out_handle);
    SetResultEntityStateSixByHandleSlot(out_handle);
}

// Shared cancel tail at 0x423059 (modes 2 and 4): gate byte 0x474e36 bit 3
// clamps the cursor against its maximum, expires all three handles, moves
// to mode 3 and re-arms the timer.
void CancelTail(GameStateManager &mgr)
{
    if ((g_ManagerSubGateFlags & 0x8U) == 0U) {
        return;
    }
    StoreClampedAgainstMax(mgr.cursor_a.value, mgr.cursor_a.maximum);
    ExpireEntityHandleEaxAbi(&mgr.handle_b_01d8);
    ExpireEntityHandleEaxAbi(&mgr.handle_a_01d4);
    ExpireEntityHandleEaxAbi(&mgr.handle_c_01dc);
    mgr.mode_0004 = 3;
    ArmRunTimer(mgr);
}

} // namespace

// TH10 0x00422c30. Native ESI = record.
void ResumeGameFromPauseEsiAbi(void *record)
{
    GameStateManager &mgr = *reinterpret_cast<GameStateManager *>(record);

    // Clear the pause bit 0x10 of the 0x477810 state object's +0x58 word.
    TitleScreen &ts = *reinterpret_cast<TitleScreen *>(g_TitleScreen);
    ts.flags &= ~0x10U;

    // Queue the "UnPause" BGM command (native string at 0x46e0b8; the
    // adjacent 0x46e0bc holds "Pause").
    QueueBgmCommand(reinterpret_cast<TransitionRootPartial *>(0x492590U),
                    "UnPause", 7, 0);

    // Soft-release the entity bound to the +0x1dc handle slot.
    if (mgr.handle_c_01dc != 0U) {
        ReleaseEntityById(g_MainChainRenderOwner, mgr.handle_c_01dc);
    }
    mgr.handle_c_01dc = 0;

    // Restore the saved frame-time scale.
    g_FrameTimeScale = mgr.saved_time_scale_02c0;
}

// TH10 0x00422c80 (native `retn 4`; the record arrives as the stack
// argument). Switch on record+4 - 1 over modes 1..5.
void RunPauseMenuModesStackAbi(void *record)
{
    GameStateManager &mgr = *reinterpret_cast<GameStateManager *>(record);
    const TitleScreen &ts = *reinterpret_cast<const TitleScreen *>(g_TitleScreen);
    switch (mgr.mode_0004 - 1) {
    case 0: { // ------------------------------------------------- mode 1
        if (mgr.frame_timer.count < 10) {
            return;
        }
        mgr.mode_0004 = 2;
        // Maximum = ([0x477810]+0x5c != 0) ? 2 : 3 (neg/sbb/add-3 idiom).
        const i32 max = (ts.mode != 0U) ? 2 : 3;
        mgr.cursor_a.maximum = max;
        mgr.cursor_a.wrap_flag = 1;
        StoreClampedAgainstMax(mgr.cursor_a.value, mgr.cursor_a.maximum);
        SetEntityStateWordEaxEsiAbi(&mgr.handle_a_01d4,
                                    StateWordArgZeroExt(
                                        static_cast<u32>(mgr.cursor_a.value),
                                        7U));
        return;
    }

    case 1: { // ------------------------------------------------- mode 2
        mgr.cursor_a.previous = mgr.cursor_a.value;
        if ((g_ManagerSubGateFlags & 0x10U) != 0U
            || (g_MenuInputFlagsByte & 0x10U) != 0U) {
            ShiftManagerSelector(&mgr.cursor_a, -1);
        }
        if ((g_ManagerSubGateFlags & 0x20U) != 0U
            || (g_MenuInputFlagsByte & 0x20U) != 0U) {
            ShiftManagerSelector(&mgr.cursor_a, 1);
        }
        if (mgr.cursor_a.previous != mgr.cursor_a.value) {
            SetEntityStateWordEaxEsiAbi(
                &mgr.handle_a_01d4,
                StateWordArgZeroExt(static_cast<u32>(mgr.cursor_a.value), 7U));
            PlayMenuSound(0xCU);
        }
        if ((g_ManagerSubGateFlags & 0x1001U) != 0U) {
            // Accept: dispatch on the selected item.
            PlayMenuSound(0xAU);
            u32 highlight = 0;
            switch (mgr.cursor_a.value) {
            case 0: // to the terminal mode
                ExpireEntityHandleEaxAbi(&mgr.handle_b_01d8);
                ExpireEntityHandleEaxAbi(&mgr.handle_a_01d4);
                ExpireEntityHandleEaxAbi(&mgr.handle_c_01dc);
                mgr.mode_0004 = 3;
                break;
            case 1: // confirm submenu (kind 0x75 sprite)
                HighlightChildByKind(mgr, 0x75, &highlight);
                mgr.mode_0004 = 4;
                break;
            case 2: // second submenu entry (kind 0x74 sprite); the
                    // +0x5c gate picks the terminal mode directly
                HighlightChildByKind(mgr, 0x74, &highlight);
                mgr.mode_0004 = (ts.mode != 0U) ? 3 : 4;
                break;
            default:
                break;
            }
            ArmRunTimer(mgr);
        }
        if ((g_ManagerSubGateFlags & 0x4000U) != 0U) {
            PlayMenuSound(0xAU);
            u32 highlight = 0;
            HighlightChildByKind(mgr, 0x75, &highlight);
            ArmRunTimer(mgr);
            mgr.mode_0004 = 3;
            // Clamp 0 -> 2, > 2 -> 2, otherwise max - 1.
            const i32 max = mgr.cursor_a.maximum;
            if (max == 0 || max > 2) {
                mgr.cursor_a.value = 2;
            } else {
                mgr.cursor_a.value = max - 1;
            }
            ExpireEntityHandleEaxAbi(&mgr.handle_b_01d8);
        }
        if ((g_ManagerSubGateFlags & 0x200U) != 0U) {
            PlayMenuSound(0xAU);
            u32 highlight = 0;
            HighlightChildByKind(mgr, 0x74, &highlight);
            ArmRunTimer(mgr);
            mgr.mode_0004 = 3;
            // Clamp 0 -> 1, > 1 -> 1, otherwise max - 1.
            const i32 max = mgr.cursor_a.maximum;
            if (max == 0 || max > 1) {
                mgr.cursor_a.value = 1;
            } else {
                mgr.cursor_a.value = max - 1;
            }
        }
        CancelTail(mgr);
        return;
    }

    case 2: { // ------------------------------------------------- mode 3
        if (mgr.frame_timer.count < 0xC) {
            return;
        }
        mgr.mode_0004 = 0;
        switch (mgr.cursor_a.value) {
        case 0:
            ResumeGameFromPauseEsiAbi(&mgr);
            return;
        case 1:
            ExpireEntityHandleEaxAbi(&mgr.handle_a_01d4);
            RequestGameStateTransitionEaxStackAbi(
                reinterpret_cast<void *>(kGameStateManager), 4);
            return;
        case 2:
            ExpireEntityHandleEaxAbi(&mgr.handle_a_01d4);
            g_SharedStatusGate = 0xAU;
            return;
        default:
            return;
        }
    }

    case 3: { // ------------------------------------------------- mode 4
        if (mgr.frame_timer.count < 0x14) {
            return;
        }
        if (mgr.frame_timer.count == 0x14) {
            RunManagerCursorHandle(&mgr.cursor_a);
            mgr.cursor_a.maximum = 2;
            mgr.cursor_a.wrap_flag = 1;
            const i32 max = mgr.cursor_a.maximum;
            if (max == 0 || max > 1) {
                mgr.cursor_a.value = 1;
            } else {
                mgr.cursor_a.value = max - 1;
            }
            SetEntityStateWordEaxEsiAbi(&mgr.handle_a_01d4, 0xE);
        }
        if (mgr.frame_timer.count < 0x1E) {
            return;
        }
        if (mgr.frame_timer.count == 0x1E) {
            SetEntityStateWordEaxEsiAbi(
                &mgr.handle_a_01d4,
                StateWordArgZeroExt(static_cast<u32>(mgr.cursor_a.value),
                                    0xFU));
        }
        mgr.cursor_a.previous = mgr.cursor_a.value;
        if (PollMenuInputState(0x10U)) {
            ShiftManagerSelector(&mgr.cursor_a, -1);
        }
        if (PollMenuInputState(0x20U)) {
            ShiftManagerSelector(&mgr.cursor_a, 1);
        }
        if (mgr.cursor_a.previous != mgr.cursor_a.value) {
            SetEntityStateWordEaxEsiAbi(
                &mgr.handle_a_01d4,
                StateWordArgZeroExt(static_cast<u32>(mgr.cursor_a.value),
                                    0xFU));
            PlayMenuSound(0xCU);
        }
        if ((g_ManagerSubGateFlags & 0x1001U) != 0U) {
            PlayMenuSound(0xAU);
            u32 highlight = 0;
            if (mgr.cursor_a.value == 0) {
                HighlightChildByKind(mgr, 0x77, &highlight);
            } else if (mgr.cursor_a.value == 1) {
                HighlightChildByKind(mgr, 0x78, &highlight);
            }
            mgr.mode_0004 = 5;
            ArmRunTimer(mgr);
        }
        CancelTail(mgr);
        return;
    }

    case 4: { // ------------------------------------------------- mode 5
        if (mgr.frame_timer.count < 0x14) {
            return;
        }
        switch (mgr.cursor_a.value) {
        case 0:
            ExpireEntityHandleEaxAbi(&mgr.handle_b_01d8);
            ExpireEntityHandleEaxAbi(&mgr.handle_a_01d4);
            mgr.mode_0004 = 3;
            Call44BE70(&mgr.cursor_a);
            break;
        case 1:
            Call44BE70(&mgr.cursor_a);
            SetEntityStateWordEaxEsiAbi(
                &mgr.handle_a_01d4,
                StateWordArgSignExt(mgr.cursor_a.value, 7U));
            mgr.mode_0004 = 2;
            ArmRunTimer(mgr);
            return;
        default:
            break;
        }
        ArmRunTimer(mgr);
        return;
    }

    default: // out of range: bare epilogue
        return;
    }
}

} // namespace th10
