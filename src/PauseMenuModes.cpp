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
#include "PlayerFrameworkHelpers.hpp"
#include "PlayerTimerHelpers.hpp"
#include "PauseMenuModes.hpp"
#include "Th10Types.hpp"

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

// Record field offsets.
const u32 kMode = 0x04U;
const u32 kTimerCount = 0x14U;
const u32 kCursor = 0x24U;    // value; +4 copy; +8 max; +0xd0 wrap flag
const u32 kCursorWrap = 0xF4U;
const u32 kHandleA = 0x1D4U;
const u32 kHandleB = 0x1D8U;
const u32 kHandleC = 0x1DCU;
const u32 kSavedFloat = 0x2C0U;

// ------------------------------------------------------- access helpers

inline u32 &W(void *record, u32 offset)
{
    return *reinterpret_cast<u32 *>(static_cast<u8 *>(record) + offset);
}

inline i32 &I(void *record, u32 offset)
{
    return *reinterpret_cast<i32 *>(static_cast<u8 *>(record) + offset);
}

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
inline void StoreClampedAgainstMax(void *record, u32 value_offset,
                                   u32 max_offset)
{
    const i32 max = I(record, max_offset);
    if (max < 0) {
        I(record, value_offset) = max - 1;
    } else {
        I(record, value_offset) = 0;
    }
}

inline void PlayMenuSound(u32 kind)
{
    ReserveContextChannel(reinterpret_cast<void *>(0x492590U), kind, 0U);
}

inline void ArmRunTimer(void *record)
{
    TickPlayerTimerEaxStackAbi(reinterpret_cast<u8 *>(record) + 0x10U, 0);
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
void HighlightChildByKind(void *record, i32 kind, u32 *out_handle)
{
    ResolveChildEntityByKind(reinterpret_cast<u32 *>(
                                 static_cast<u8 *>(record) + kHandleA),
                             kind, out_handle);
    SetResultEntityStateSixByHandleSlot(out_handle);
}

// Shared cancel tail at 0x423059 (modes 2 and 4): gate byte 0x474e36 bit 3
// clamps the cursor against its maximum, expires all three handles, moves
// to mode 3 and re-arms the timer.
void CancelTail(void *record)
{
    if ((g_ManagerSubGateFlags & 0x8U) == 0U) {
        return;
    }
    StoreClampedAgainstMax(record, kCursor, 0x2CU);
    ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + kHandleB));
    ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + kHandleA));
    ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + kHandleC));
    I(record, kMode) = 3;
    ArmRunTimer(record);
}

} // namespace

// TH10 0x00422c30. Native ESI = record.
void ResumeGameFromPauseEsiAbi(void *record)
{
    // Clear the pause bit 0x10 of the 0x477810 state object's +0x58 word.
    u8 *const state = static_cast<u8 *>(g_TitleScreen);
    *reinterpret_cast<u32 *>(state + 0x58) &= ~0x10U;

    // Queue the "UnPause" BGM command (native string at 0x46e0b8; the
    // adjacent 0x46e0bc holds "Pause").
    QueueBgmCommand(reinterpret_cast<TransitionRootPartial *>(0x492590U),
                    "UnPause", 7, 0);

    // Soft-release the entity bound to the +0x1dc handle slot.
    u32 *const handle_slot = reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + kHandleC);
    if (*handle_slot != 0U) {
        ReleaseEntityById(g_MainChainRenderOwner, *handle_slot);
    }
    *handle_slot = 0;

    // Restore the saved frame-time scale.
    g_FrameTimeScale =
        *reinterpret_cast<float *>(static_cast<u8 *>(record) + kSavedFloat);
}

// TH10 0x00422c80 (native `retn 4`; the record arrives as the stack
// argument). Switch on record+4 - 1 over modes 1..5.
void RunPauseMenuModesStackAbi(void *record)
{
    switch (I(record, kMode) - 1) {
    case 0: { // ------------------------------------------------- mode 1
        if (I(record, kTimerCount) < 10) {
            return;
        }
        I(record, kMode) = 2;
        // Maximum = ([0x477810]+0x5c != 0) ? 2 : 3 (neg/sbb/add-3 idiom).
        const i32 max =
            (*reinterpret_cast<u32 *>(static_cast<u8 *>(g_TitleScreen)
                                      + 0x5CU)
             != 0U)
                ? 2
                : 3;
        I(record, 0x2C) = max;
        I(record, kCursorWrap) = 1;
        StoreClampedAgainstMax(record, kCursor, 0x2CU);
        SetEntityStateWordEaxEsiAbi(reinterpret_cast<u32 *>(
                                        static_cast<u8 *>(record) + kHandleA),
                                    StateWordArgZeroExt(
                                        static_cast<u32>(I(record, kCursor)),
                                        7U));
        return;
    }

    case 1: { // ------------------------------------------------- mode 2
        W(record, kCursor + 4U) = W(record, kCursor);
        if ((g_ManagerSubGateFlags & 0x10U) != 0U
            || (g_MenuInputFlagsByte & 0x10U) != 0U) {
            ShiftManagerSelector(reinterpret_cast<u8 *>(record) + kCursor, -1);
        }
        if ((g_ManagerSubGateFlags & 0x20U) != 0U
            || (g_MenuInputFlagsByte & 0x20U) != 0U) {
            ShiftManagerSelector(reinterpret_cast<u8 *>(record) + kCursor, 1);
        }
        if (W(record, kCursor + 4U) != W(record, kCursor)) {
            SetEntityStateWordEaxEsiAbi(
                reinterpret_cast<u32 *>(static_cast<u8 *>(record) + kHandleA),
                StateWordArgZeroExt(W(record, kCursor), 7U));
            PlayMenuSound(0xCU);
        }
        if ((g_ManagerSubGateFlags & 0x1001U) != 0U) {
            // Accept: dispatch on the selected item.
            PlayMenuSound(0xAU);
            u32 highlight = 0;
            switch (W(record, kCursor)) {
            case 0: // to the terminal mode
                ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(
                    static_cast<u8 *>(record) + kHandleB));
                ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(
                    static_cast<u8 *>(record) + kHandleA));
                ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(
                    static_cast<u8 *>(record) + kHandleC));
                I(record, kMode) = 3;
                break;
            case 1: // confirm submenu (kind 0x75 sprite)
                HighlightChildByKind(record, 0x75, &highlight);
                I(record, kMode) = 4;
                break;
            case 2: // second submenu entry (kind 0x74 sprite); the
                    // +0x5c gate picks the terminal mode directly
                HighlightChildByKind(record, 0x74, &highlight);
                I(record, kMode) =
                    (*reinterpret_cast<u32 *>(
                         static_cast<u8 *>(g_TitleScreen) + 0x5CU)
                     != 0U)
                        ? 3
                        : 4;
                break;
            default:
                break;
            }
            ArmRunTimer(record);
        }
        if ((g_ManagerSubGateFlags & 0x4000U) != 0U) {
            PlayMenuSound(0xAU);
            u32 highlight = 0;
            HighlightChildByKind(record, 0x75, &highlight);
            ArmRunTimer(record);
            I(record, kMode) = 3;
            // Clamp 0 -> 2, > 2 -> 2, otherwise max - 1.
            const i32 max = I(record, 0x2C);
            if (max == 0 || max > 2) {
                I(record, kCursor) = 2;
            } else {
                I(record, kCursor) = max - 1;
            }
            ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(
                static_cast<u8 *>(record) + kHandleB));
        }
        if ((g_ManagerSubGateFlags & 0x200U) != 0U) {
            PlayMenuSound(0xAU);
            u32 highlight = 0;
            HighlightChildByKind(record, 0x74, &highlight);
            ArmRunTimer(record);
            I(record, kMode) = 3;
            // Clamp 0 -> 1, > 1 -> 1, otherwise max - 1.
            const i32 max = I(record, 0x2C);
            if (max == 0 || max > 1) {
                I(record, kCursor) = 1;
            } else {
                I(record, kCursor) = max - 1;
            }
        }
        CancelTail(record);
        return;
    }

    case 2: { // ------------------------------------------------- mode 3
        if (I(record, kTimerCount) < 0xC) {
            return;
        }
        I(record, kMode) = 0;
        switch (I(record, kCursor)) {
        case 0:
            ResumeGameFromPauseEsiAbi(record);
            return;
        case 1:
            ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(
                static_cast<u8 *>(record) + kHandleA));
            RequestGameStateTransitionEaxStackAbi(
                reinterpret_cast<void *>(kGameStateManager), 4);
            return;
        case 2:
            ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(
                static_cast<u8 *>(record) + kHandleA));
            g_SharedStatusGate = 0xAU;
            return;
        default:
            return;
        }
    }

    case 3: { // ------------------------------------------------- mode 4
        if (I(record, kTimerCount) < 0x14) {
            return;
        }
        if (I(record, kTimerCount) == 0x14) {
            RunManagerCursorHandle(reinterpret_cast<u8 *>(record) + kCursor);
            I(record, 0x2C) = 2;
            I(record, kCursorWrap) = 1;
            const i32 max = I(record, 0x2C);
            if (max == 0 || max > 1) {
                I(record, kCursor) = 1;
            } else {
                I(record, kCursor) = max - 1;
            }
            SetEntityStateWordEaxEsiAbi(reinterpret_cast<u32 *>(
                                            static_cast<u8 *>(record)
                                                + kHandleA),
                                        0xE);
        }
        if (I(record, kTimerCount) < 0x1E) {
            return;
        }
        if (I(record, kTimerCount) == 0x1E) {
            SetEntityStateWordEaxEsiAbi(
                reinterpret_cast<u32 *>(static_cast<u8 *>(record) + kHandleA),
                StateWordArgZeroExt(static_cast<u32>(I(record, kCursor)),
                                    0xFU));
        }
        W(record, kCursor + 4U) = W(record, kCursor);
        if (PollMenuInputState(0x10U)) {
            ShiftManagerSelector(reinterpret_cast<u8 *>(record) + kCursor, -1);
        }
        if (PollMenuInputState(0x20U)) {
            ShiftManagerSelector(reinterpret_cast<u8 *>(record) + kCursor, 1);
        }
        if (W(record, kCursor + 4U) != W(record, kCursor)) {
            SetEntityStateWordEaxEsiAbi(
                reinterpret_cast<u32 *>(static_cast<u8 *>(record) + kHandleA),
                StateWordArgZeroExt(W(record, kCursor), 0xFU));
            PlayMenuSound(0xCU);
        }
        if ((g_ManagerSubGateFlags & 0x1001U) != 0U) {
            PlayMenuSound(0xAU);
            u32 highlight = 0;
            if (I(record, kCursor) == 0) {
                HighlightChildByKind(record, 0x77, &highlight);
            } else if (I(record, kCursor) == 1) {
                HighlightChildByKind(record, 0x78, &highlight);
            }
            I(record, kMode) = 5;
            ArmRunTimer(record);
        }
        CancelTail(record);
        return;
    }

    case 4: { // ------------------------------------------------- mode 5
        if (I(record, kTimerCount) < 0x14) {
            return;
        }
        switch (I(record, kCursor)) {
        case 0:
            ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(
                static_cast<u8 *>(record) + kHandleB));
            ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(
                static_cast<u8 *>(record) + kHandleA));
            I(record, kMode) = 3;
            Call44BE70(reinterpret_cast<u8 *>(record) + kCursor);
            break;
        case 1:
            Call44BE70(reinterpret_cast<u8 *>(record) + kCursor);
            SetEntityStateWordEaxEsiAbi(
                reinterpret_cast<u32 *>(static_cast<u8 *>(record) + kHandleA),
                StateWordArgSignExt(I(record, kCursor), 7U));
            I(record, kMode) = 2;
            ArmRunTimer(record);
            return;
        default:
            break;
        }
        ArmRunTimer(record);
        return;
    }

    default: // out of range: bare epilogue
        return;
    }
}

} // namespace th10
