// TH10 0x004236f0 — post-run replay-save menu state machine.
//
// Native switch on record+4 - 6 over modes 6,7,8,10,11,12,13 (mode 9 and any
// other value fall through to the bare epilogue). The record is the same
// 0x3ac-byte VM/scheduler record family used by the HUD batch pools; the
// embedded fields used here are:
//   +0x04 mode                     +0x10..+0x20 five-field scaled timer
//   +0x24 menu cursor record A     (+0x24 value, +0x28 saved, +0x2c max,
//                                   +0xf4 wrap-style flag)
//   +0xfc menu cursor record B     (+0xfc value, +0x100 saved, +0x104 max,
//                                   +0x1cc wrap-style flag)
//   +0x1d4 entity handle slot A    +0x1d8 entity handle slot B
//   +0x1e0 typed-name length       +0x1e4 extra-stage flag
//   +0x1e8 replay-present gate     +0x1ec parsed replay files[25]
//   +0x2b4 name buffer (>=10 bytes) +0x2c0 float saved to DAT_00476f78
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "AsciiHudOwner.hpp"
#include "EntityHelpers.hpp"
#include "GameManagerState.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "PlayerTimerHelpers.hpp"
#include "PostRunReplaySaveMenu.hpp"
#include "ReplaySave.hpp"
#include "SaveRunHighScoreEntry.hpp"
#include "Th10Types.hpp"

namespace th10 {

namespace {

// ---------------------------------------------------------------- globals

extern u32 g_ManagerSubGateFlags;    // TH10 DAT_00474e36
extern u8 g_MenuInputFlagsByte;      // TH10 DAT_00474e34
extern u32 g_GameRunFlags;           // TH10 DAT_00474ca0 (byte 0x10 = practice)
extern u32 g_MaximumScore;           // TH10 DAT_00474c40
extern u32 g_CurrentRunScore;        // TH10 DAT_00474c44
extern u32 g_RunChara;               // TH10 DAT_00474c68
extern u32 g_RunCharaSlot;           // TH10 DAT_00474c6c
extern u32 g_RunShot;                // TH10 DAT_00474c74
extern u32 g_RunStage;               // TH10 DAT_00474c7c
extern void *g_ScoreSaveState;       // TH10 DAT_0047783c (save data)
extern void *g_AsciiHudOverlayState; // TH10 DAT_0047770c
extern void *g_GameModeObject;       // TH10 DAT_00477838
extern u32 g_SharedStatusGate;       // TH10 DAT_00491fb8
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern float g_FrameTimeScale;       // TH10 DAT_00476f78

const u32 kMenuSoundContext = 0x492590U;   // ReserveContextChannel context
const u32 kGameStateManager = 0x491C28U;   // RunGameOverPathBStackAbi target
const char *const kCharset = reinterpret_cast<const char *>(0x4746D8U);
const char *const kNineSpaces = reinterpret_cast<const char *>(0x46E354U);
const char *const kSavedReplayName =
    reinterpret_cast<const char *>(0x47783CU + 0x1D878U);

// Record field offsets.
const u32 kMode = 0x04U;
const u32 kTimerPrev = 0x10U;
const u32 kTimerCount = 0x14U;
const u32 kTimerRate = 0x1CU;
const u32 kCursorA = 0x24U;   // value; +4 saved; +8 max; +0xd0 wrap flag
const u32 kCursorAWrap = 0xF4U;
const u32 kCursorB = 0xFCU;
const u32 kCursorBWrap = 0x1CCU;
const u32 kHandleA = 0x1D4U;
const u32 kHandleB = 0x1D8U;
const u32 kNameLength = 0x1E0U;
const u32 kExtraStageFlag = 0x1E4U;
const u32 kReplayGate = 0x1E8U;
const u32 kFiles = 0x1ECU;
const u32 kNameBuffer = 0x2B4U;
const u32 kSavedFloat = 0x2C0U;
const u32 kFileCount = 25U;

// ------------------------------------------------------- access helpers

inline u32 &W(void *record, u32 offset)
{
    return *reinterpret_cast<u32 *>(static_cast<u8 *>(record) + offset);
}

inline i32 &I(void *record, u32 offset)
{
    return *reinterpret_cast<i32 *>(static_cast<u8 *>(record) + offset);
}

inline u8 *B(void *record, u32 offset)
{
    return static_cast<u8 *>(record) + offset;
}

inline char *Name(void *record)
{
    return reinterpret_cast<char *>(B(record, kNameBuffer));
}

// State-word argument for 0x00449470. Two native encodings exist:
//  - zero-extended 16-bit load (`xor esi,esi; mov si,word; add si,7`)
//  - sign-extended load (`movsx esi,ax; add si,7`, high word keeps sign)
inline i32 StateWordArgZeroExt(u32 value)
{
    return static_cast<i32>(static_cast<u16>(value + 7U));
}

inline i32 StateWordArgSignExt(i32 value)
{
    const i32 s = static_cast<i16>(static_cast<u16>(value));
    return static_cast<i32>((static_cast<u32>(s) & 0xFFFF0000U)
                            | static_cast<u32>(static_cast<u16>(s + 7)));
}

// The recurring "clamp a cursor value against its own maximum" idiom.
// Mode 7/8 variant: 0 -> 2, > 2 -> 2, otherwise v-1 (negative falls through).
inline i32 ClampSelectorTwoOrDecrement(i32 value)
{
    if (value == 0) {
        return 2;
    }
    if (value > 2) {
        return 2;
    }
    return value - 1;
}

// Mode 12 variant: 0 or positive -> 0, negative -> v-1.
inline i32 ClampSelectorZeroOrDecrement(i32 value)
{
    if (value >= 0) {
        return 0;
    }
    return value - 1;
}

// Max-aware store used when a cursor maximum exists: max < 0 stores max-1,
// otherwise 0 (native `test/jg/dec` chain, preserved verbatim).
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
    ReserveContextChannel(reinterpret_cast<void *>(kMenuSoundContext), kind, 0U);
}

inline void ArmRunTimer(void *record)
{
    TickPlayerTimerEaxStackAbi(B(record, kTimerPrev), 0);
}

inline u32 StrLen(const char *text)
{
    return static_cast<u32>(strlen(text));
}

// ------------------------------------------------- reconstructed leaves

// TH10 0x004296f0 (parse one replay file into a display record; boundary
// implemented elsewhere). Native stack ABI, char* source.
extern void *ParseDemoRecord(char *source);

// TH10 0x004294a0 (destroy a parsed replay record; boundary).
extern void DestroyDemoParseObject(void *parsed);

// TH10 0x00423570 (post-run high-score entry wrapper around
// InsertScoreRecordEdi with the stage-7 extra-stage swap quirk) is now the
// semantic body in SaveRunHighScoreEntry.cpp.

// TH10 0x00421f60. Publish the finished-run score into the HUD overlay owner
// at +0x9e78 (displayed_score) and raise the all-time maximum at DAT_00474c40.
void PublishHudFinalScore()
{
    const u32 score = g_CurrentRunScore;
    AsciiHudOwner &hud =
        *reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOverlayState);
    hud.displayed_score = score;
    if (static_cast<i32>(g_MaximumScore) < static_cast<i32>(score)) {
        g_MaximumScore = score;
    }
}

// TH10 0x004243f0. Set the u16 stop word 6 at entity+0x304 for the entity
// resolved from *handle_slot, propagating to the +0x14 child chain when the
// +0x18 count is zero.
void SetReplayWatchStopWord(u32 *handle_slot)
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
        // The child chain holds {entity, next} nodes; the stop word goes
        // to the entity's +0x304, not the node's.
        u8 *child = *reinterpret_cast<u8 **>(node);
        *reinterpret_cast<u16 *>(child + 0x304) = 6;
        node = *reinterpret_cast<u32 *>(node + 4);
    }
}

// TH10 0x00449590. Set-twin of ClearEntityFlag2ByHandleSlot: resolve the
// handle, set bit 2 of the +0x35c flag word, propagate while +0x18 is zero.
void SetEntityFlagWord2ByHandleSlot(u32 *handle_slot)
{
    u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner, *handle_slot);
    if (entity == 0) {
        return;
    }
    *reinterpret_cast<u32 *>(entity + 0x35C) |= 2U;
    if (*reinterpret_cast<u32 *>(entity + 0x18) != 0) {
        return;
    }
    u32 node = *reinterpret_cast<u32 *>(entity + 0x14);
    while (node != 0) {
        u8 *child = reinterpret_cast<u8 *>(node);
        *reinterpret_cast<u32 *>(child + 0x35C) |= 2U;
        node = *reinterpret_cast<u32 *>(child + 4);
    }
}

// TH10 0x004297b0. Release one parsed replay record (destroy + free).
void ReleaseParsedDemoRecord(void *parsed)
{
    if (parsed == 0) {
        return;
    }
    DestroyDemoParseObject(parsed);
    free(parsed);
}

// TH10 0x00423510. Mode-13 preparation: re-arm the timer, expire both entity
// handles and copy the record float into the global frame-time scale.
void ResetReplayMenuEntities(void *record)
{
    I(record, kMode) = 0xD;
    W(record, 0x20) |= 1U; // only reached when bit 0 was clear natively
    W(record, kTimerPrev) = 0xFFF0BDC1U;
    W(record, kTimerRate) = reinterpret_cast<u32>(&g_FrameTimeScale);
    W(record, kTimerCount) = 0;
    W(record, kTimerPrev) = 0xFFFFFFFFU; // native overwrites unconditionally
    ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(B(record, kHandleB)));
    ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(B(record, kHandleA)));
    g_FrameTimeScale =
        *reinterpret_cast<float *>(B(record, kSavedFloat));
}

// Release every parsed replay slot (mode 10 epilogue): destroy + free each
// non-null pointer, then clear the slot (count 25, native order preserved).
void FreeParsedFileList(void *record)
{
    u32 *slot = reinterpret_cast<u32 *>(B(record, kFiles));
    for (u32 i = kFileCount; i != 0U; --i) {
        const u32 parsed = *slot;
        if (parsed != 0U) {
            DestroyDemoParseObject(reinterpret_cast<void *>(parsed));
            free(reinterpret_cast<void *>(parsed));
        }
        *slot = 0;
        ++slot;
    }
}

} // namespace

// TH10 0x004236f0 (native `retn 4`; the record arrives as the stack argument).
void RunPostRunReplaySaveMenuStackAbi(void *record)
{
    char file_name[0x40];
    switch (I(record, kMode) - 6) {
    case 0: // ------------------------------------------------- mode 6
        if (I(record, kTimerCount) < 10) {
            return;
        }
        if ((g_ManagerSubGateFlags & 0x1001U) == 0U) {
            return;
        }
        PlayMenuSound(10U); // native edi = 0xa across the call
        I(record, kMode) = 7;
        SetEntityStateWordEaxEsiAbi(reinterpret_cast<u32 *>(B(record, kHandleA)),
                                    2);
        ArmRunTimer(record);
        return;

    case 1: { // ------------------------------------------------ mode 7
        if (I(record, kTimerCount) < 10) {
            return;
        }
        PublishHudFinalScore();
        if ((g_GameRunFlags & 0x10U) != 0U) {
            // Practice: merge the run score into the per-stage best table at
            // save data + (charaSlot + 3*chara)*0x437c + (stage+6*shot)*8
            // + 0x4dc, then fall into the mode-8 setup.
            u32 *best =
                reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(g_ScoreSaveState)
                    + (g_RunCharaSlot + 3U * g_RunChara) * 0x437CU
                    + (g_RunStage + 6U * g_RunShot) * 8U + 0x4DCU);
            if (static_cast<i32>(g_CurrentRunScore)
                > static_cast<i32>(*best)) {
                *best = g_CurrentRunScore;
            }
            I(record, kMode) = 8;
            I(record, 0x2C) = 3;
            I(record, kCursorAWrap) = 1;
            I(record, kCursorA) =
                ClampSelectorTwoOrDecrement(I(record, 0x2C));
            FireEntityHandleEaxAbi(
                reinterpret_cast<u32 *>(B(record, kHandleA)));
            SetEntityStateWordEaxEsiAbi(
                reinterpret_cast<u32 *>(B(record, kHandleA)),
                StateWordArgZeroExt(
                    static_cast<u32>(I(record, kCursorA))));
            return;
        }
        SaveRunHighScoreEntry(record); // TH10 0x00423570 boundary
        ArmRunTimer(record);
        if (I(record, kReplayGate) != 0) {
            I(record, kMode) = 8;
            I(record, 0x2C) = 3;
            I(record, kCursorAWrap) = 1;
            I(record, kCursorA) =
                ClampSelectorTwoOrDecrement(I(record, 0x2C));
            FireEntityHandleEaxAbi(
                reinterpret_cast<u32 *>(B(record, kHandleA)));
            SetEntityStateWordEaxEsiAbi(
                reinterpret_cast<u32 *>(B(record, kHandleA)),
                StateWordArgZeroExt(
                    static_cast<u32>(I(record, kCursorA))));
            return;
        }
        I(record, kMode) = 0xC;
        ClearEntityFlag2ByHandleSlot(
            reinterpret_cast<u32 *>(B(record, kHandleA)));
        return;
    }

    case 2: { // ------------------------------------------------ mode 8
        W(record, kCursorA + 4U) = W(record, kCursorA);
        if ((g_ManagerSubGateFlags & 0x10U) != 0U
            || (g_MenuInputFlagsByte & 0x10U) != 0U) {
            ShiftManagerSelector(B(record, kCursorA), -1);
        }
        if ((g_ManagerSubGateFlags & 0x20U) != 0U
            || (g_MenuInputFlagsByte & 0x20U) != 0U) {
            ShiftManagerSelector(B(record, kCursorA), 1);
        }
        if (I(record, kCursorA + 4U) != I(record, kCursorA)) {
            SetEntityStateWordEaxEsiAbi(
                reinterpret_cast<u32 *>(B(record, kHandleA)),
                StateWordArgSignExt(I(record, kCursorA)));
            PlayMenuSound(12U);
        }
        if ((g_ManagerSubGateFlags & 0x1001U) != 0U) {
            u32 highlight = 0;
            ResolveChildEntityByKind(
                reinterpret_cast<u32 *>(B(record, kHandleA)),
                I(record, kCursorA) + 0x7C, &highlight);
            SetReplayWatchStopWord(&highlight);
            PlayMenuSound(10U);
            ArmRunTimer(record);
            I(record, kMode) = 0xD;
            PublishHudFinalScore();
            const i32 selected = I(record, kCursorA);
            if (selected == 1) {
                ArmRunTimer(record);
                I(record, kMode) = 0xA;
                ClearEntityFlag2ByHandleSlot(
                    reinterpret_cast<u32 *>(B(record, kHandleA)));
                RunManagerCursorHandle(B(record, kCursorA));
                I(record, 0x2C) = 0x19;
                I(record, kCursorAWrap) = 1;
                StoreClampedAgainstMax(record, kCursorA, 0x2C);
                for (i32 index = 1; index <= 0x19; ++index) {
                    sprintf(file_name, "th10_%.2d.rpy", index);
                    W(record, kFiles + 4U * (index - 1)) =
                        reinterpret_cast<u32>(ParseDemoRecord(file_name));
                }
            } else if (selected == 2) {
                ArmRunTimer(record);
                I(record, kMode) = 0xD;
                PlayMenuSound(10U);
            }
        }
        if ((g_ManagerSubGateFlags & 0xAU) == 0U) {
            return;
        }
        PlayMenuSound(11U);
        if (I(record, kCursorA) != 2) {
            I(record, kCursorA) =
                ClampSelectorTwoOrDecrement(I(record, 0x2C));
            SetEntityStateWordEaxEsiAbi(
                reinterpret_cast<u32 *>(B(record, kHandleA)),
                StateWordArgZeroExt(
                    static_cast<u32>(I(record, kCursorA))));
        }
        return;
    }

    case 3: { // ------------------------------------------------ mode 10
        if (I(record, kTimerCount) < 10) {
            return;
        }
        W(record, kCursorA + 4U) = W(record, kCursorA);
        if (PollMenuInputState(0x10U)) {
            ShiftManagerSelector(B(record, kCursorA), -1);
        }
        if (PollMenuInputState(0x20U)) {
            ShiftManagerSelector(B(record, kCursorA), 1);
        }
        if (I(record, kCursorA + 4U) != I(record, kCursorA)) {
            PlayMenuSound(12U);
        }
        if ((g_ManagerSubGateFlags & 0x1001U) != 0U) {
            ArmRunTimer(record);
            I(record, kMode) = 0xB;
            {
                const i32 countdown = I(record, 0x104);
                I(record, kCursorB) =
                    (countdown < 0) ? countdown - 1 : 0;
            }
            I(record, 0x104) = static_cast<i32>(StrLen(kCharset));
            I(record, kCursorBWrap) = 1;
            time(reinterpret_cast<time_t *>(
                reinterpret_cast<u8 *>(*reinterpret_cast<void **>(
                                           static_cast<u8 *>(g_GameModeObject)
                                           + 0x18U))
                + 0xCU));
            u32 *header = reinterpret_cast<u32 *>(
                reinterpret_cast<u8 *>(*reinterpret_cast<void **>(
                                           static_cast<u8 *>(g_GameModeObject)
                                           + 0x18U))
                + 0x5CU);
            if (I(record, kExtraStageFlag) != 0
                && (g_GameRunFlags & 0x10U) == 0U) {
                *header = 8;
            } else {
                *header = g_RunStage;
            }
            strcpy(Name(record), kSavedReplayName);
            if (memcmp(Name(record), kNineSpaces, 9) == 0) {
                ShiftManagerSelector(B(record, kCursorB), -1);
            }
            i32 length = 8;
            while (length > 0
                   && Name(record)[length - 1] == ' ') {
                --length;
            }
            I(record, kNameLength) = length;
            PlayMenuSound(10U);
            return;
        }
        if ((g_ManagerSubGateFlags & 0xAU) == 0U) {
            return;
        }
        // 0x423c20 epilogue: return to the slot selector (mode 8).
        ArmRunTimer(record);
        I(record, kMode) = 8;
        SetEntityFlagWord2ByHandleSlot(
            reinterpret_cast<u32 *>(B(record, kHandleA)));
        Call44BE70(B(record, kCursorA));
        SetEntityStateWordEaxEsiAbi(
            reinterpret_cast<u32 *>(B(record, kHandleA)),
            StateWordArgSignExt(I(record, kCursorA)));
        I(record, 0x2C) = 3;
        I(record, kCursorAWrap) = 1;
        FreeParsedFileList(record);
        ArmRunTimer(record);
        I(record, kMode) = 8;
        PlayMenuSound(11U);
        return;
    }

    case 4: { // ------------------------------------------------ mode 11
        if (I(record, kTimerCount) < 10) {
            return;
        }
        W(record, kCursorB + 4U) = W(record, kCursorB);
        if (PollMenuInputState(0x10U)) {
            ShiftManagerSelector(B(record, kCursorB), -13);
        }
        if (PollMenuInputState(0x20U)) {
            ShiftManagerSelector(B(record, kCursorB), 13);
        }
        if (PollMenuInputState(0x40U)) {
            ShiftManagerSelector(
                B(record, kCursorB),
                (I(record, kCursorB) % 13 == 0) ? 12 : -1);
        }
        if (PollMenuInputState(0x80U)) {
            ShiftManagerSelector(
                B(record, kCursorB),
                (I(record, kCursorB) % 13 == 12) ? -12 : 1);
        }
        if (I(record, kCursorB + 4U) != I(record, kCursorB)) {
            PlayMenuSound(12U);
        }
        if ((g_ManagerSubGateFlags & 0x1001U) == 0U) {
            // Shared 0x423e07 tail below handles the 0xa keys.
        } else {
            const i32 length = static_cast<i32>(StrLen(kCharset));
            const i32 index = I(record, kCursorB);
            char *name = Name(record);
            if (index < length - 3) {
                // Type the selected character at the name cursor.
                const i32 pos = I(record, kNameLength);
                if (pos < 8) {
                    name[pos] = kCharset[index];
                } else {
                    name[pos - 1] = kCharset[index];
                }
                const i32 advanced = pos + 1;
                I(record, kNameLength) = advanced;
                if (advanced >= 8) {
                    // Name full: park the selector on the END row and let the
                    // 0x40ad20 clamp behave natively.
                    Call40AD20(static_cast<u32>(length - 1),
                               B(record, kCursorB));
                }
                PlayMenuSound(10U);
            } else if (index == length - 3) {
                // Space insert + single advance.
                const i32 pos = I(record, kNameLength);
                if (pos < 8) {
                    name[pos] = ' ';
                } else {
                    name[pos - 1] = ' ';
                    PlayMenuSound(10U);
                    // Shared tail.
                    if ((g_ManagerSubGateFlags & 0xAU) == 0U) {
                        return;
                    }
                    PlayMenuSound(11U);
                    if (I(record, kNameLength) != 0) {
                        const i32 back = I(record, kNameLength) - 1;
                        I(record, kNameLength) = back;
                        name[back] = ' ';
                        return;
                    }
                    ArmRunTimer(record);
                    I(record, kMode) = 0xA;
                    return;
                }
                const i32 advanced = pos + 1;
                I(record, kNameLength) = advanced;
                if (advanced >= 8) {
                    Call40AD20(static_cast<u32>(length - 1),
                               B(record, kCursorB));
                }
                PlayMenuSound(10U);
            } else if (index == length - 2) {
                // Delete: one backspace, then stop for this frame.
                const i32 pos = I(record, kNameLength);
                if (pos == 0) {
                    return;
                }
                I(record, kNameLength) = pos - 1;
                name[pos - 1] = ' ';
                PlayMenuSound(11U);
                return;
            } else if (index == length - 1) {
                // END: commit the replay file for the selected slot.
                PlayMenuSound(0x2CU);
                sprintf(file_name, "th10_%.2d.rpy",
                        I(record, kCursorA) + 1);
                const u32 previous =
                    W(record, kFiles + 4U * I(record, kCursorA));
                ReleaseParsedDemoRecord(reinterpret_cast<void *>(previous));
                CommitReplaySave(g_GameModeObject, file_name, name);
                const u32 slot = static_cast<u32>(I(record, kCursorA));
                W(record, kFiles + 4U * slot) =
                    reinterpret_cast<u32>(ParseDemoRecord(file_name));
                ArmRunTimer(record);
                I(record, kMode) = 0xA;
                strcpy(const_cast<char *>(kSavedReplayName), file_name);
                return;
            } else {
                PlayMenuSound(10U);
            }
        }
        // Shared 0x423e07 tail (mode 11): 0xa keys.
        if ((g_ManagerSubGateFlags & 0xAU) == 0U) {
            return;
        }
        PlayMenuSound(11U);
        if (I(record, kNameLength) != 0) {
            const i32 back = I(record, kNameLength) - 1;
            I(record, kNameLength) = back;
            Name(record)[back] = ' ';
            return;
        }
        ArmRunTimer(record);
        I(record, kMode) = 0xA;
        return;
    }

    case 5: { // ------------------------------------------------ mode 12
        if (I(record, kTimerCount) < 10) {
            return;
        }
        if (I(record, kReplayGate) == 0) {
            W(record, kCursorB + 4U) = W(record, kCursorB);
            if (PollMenuInputState(0x10U)) {
                ShiftManagerSelector(B(record, kCursorB), -13);
            }
            if (PollMenuInputState(0x20U)) {
                ShiftManagerSelector(B(record, kCursorB), 13);
            }
            if (PollMenuInputState(0x40U)) {
                ShiftManagerSelector(
                    B(record, kCursorB),
                    (I(record, kCursorB) % 13 == 0) ? 12 : -1);
            }
            if (PollMenuInputState(0x80U)) {
                ShiftManagerSelector(
                    B(record, kCursorB),
                    (I(record, kCursorB) % 13 == 12) ? -12 : 1);
            }
            if (I(record, kCursorB + 4U) != I(record, kCursorB)) {
                PlayMenuSound(12U);
            }
        }
        if ((g_ManagerSubGateFlags & 0x1001U) == 0U) {
            // 0x42426d tail only.
        } else if (I(record, kReplayGate) != 0) {
            // 0x424217 epilogue: return to the slot selector.
            SetEntityFlagWord2ByHandleSlot(
                reinterpret_cast<u32 *>(B(record, kHandleA)));
            I(record, kMode) = 8;
            I(record, 0x2C) = 3;
            I(record, kCursorAWrap) = 1;
            I(record, kCursorA) =
                ClampSelectorZeroOrDecrement(I(record, 0x2C));
            FireEntityHandleEaxAbi(
                reinterpret_cast<u32 *>(B(record, kHandleA)));
            SetEntityStateWordEaxEsiAbi(
                reinterpret_cast<u32 *>(B(record, kHandleA)),
                StateWordArgZeroExt(
                    static_cast<u32>(I(record, kCursorA))));
        } else {
            const i32 length = static_cast<i32>(StrLen(kCharset));
            const i32 index = I(record, kCursorB);
            char *name = Name(record);
            if (index < length - 3) {
                // Native quirk: the mode-12 type path writes the character
                // but never advances the name cursor (unlike mode 11).
                const i32 pos = I(record, kNameLength);
                if (pos < 8) {
                    name[pos] = kCharset[index];
                } else {
                    name[pos - 1] = kCharset[index];
                }
                PlayMenuSound(10U);
            } else if (index == length - 3) {
                const i32 pos = I(record, kNameLength);
                if (pos < 8) {
                    name[pos] = ' ';
                } else {
                    name[pos - 1] = ' ';
                }
                const i32 advanced = pos + 1;
                I(record, kNameLength) = advanced;
                if (advanced >= 8) {
                    Call40AD20(static_cast<u32>(length - 1),
                               B(record, kCursorB));
                }
                PlayMenuSound(10U);
            } else if (index == length - 2) {
                const i32 pos = I(record, kNameLength);
                if (pos == 0) {
                    return;
                }
                I(record, kNameLength) = pos - 1;
                name[pos - 1] = ' ';
                PlayMenuSound(10U);
            } else if (index == length - 1) {
                // END: store the typed name into the score-save slot named
                // by the mode-8 selection. Native address formula (kept
                // verbatim, including the chara/slot asymmetry against the
                // mode-7 best-score target):
                //   base + (chara + 3*slot474c6c)*0x437c
                //       + 3*(cursor + 10*shot)*8 + 0x1e
                u8 *dst = reinterpret_cast<u8 *>(g_ScoreSaveState)
                    + (g_RunChara + 3U * g_RunCharaSlot) * 0x437CU
                    + static_cast<u32>(3 * (I(record, kCursorA)
                                            + 10 * static_cast<i32>(g_RunShot))) * 8U
                    + 0x1EU;
                strcpy(reinterpret_cast<char *>(dst), name);
                strcpy(const_cast<char *>(kSavedReplayName), name);
                SetEntityFlagWord2ByHandleSlot(
                    reinterpret_cast<u32 *>(B(record, kHandleA)));
                I(record, kMode) = 8;
                I(record, 0x2C) = 3;
                I(record, kCursorAWrap) = 1;
                I(record, kCursorA) =
                    ClampSelectorZeroOrDecrement(I(record, 0x2C));
                FireEntityHandleEaxAbi(
                    reinterpret_cast<u32 *>(B(record, kHandleA)));
                SetEntityStateWordEaxEsiAbi(
                    reinterpret_cast<u32 *>(B(record, kHandleA)),
                    StateWordArgZeroExt(
                        static_cast<u32>(I(record, kCursorA))));
                PlayMenuSound(10U);
            } else {
                PlayMenuSound(10U);
            }
        }
        // Shared 0x42426d tail (mode 12): 0xa keys.
        if ((g_ManagerSubGateFlags & 0xAU) == 0U) {
            return;
        }
        if (I(record, kCursorA) >= 0) {
            if (I(record, kNameLength) == 0) {
                return;
            }
            PlayMenuSound(11U);
            const i32 back = I(record, kNameLength) - 1;
            I(record, kNameLength) = back;
            Name(record)[back] = ' ';
            return;
        }
        I(record, kMode) = 8;
        I(record, 0x2C) = 3;
        I(record, kCursorA) =
            ClampSelectorZeroOrDecrement(I(record, 0x2C));
        ArmRunTimer(record);
        PlayMenuSound(10U);
        return;
    }

    case 6: { // ------------------------------------------------ mode 13
        if (I(record, kTimerCount) < 0xC) {
            return;
        }
        ResetReplayMenuEntities(record);
        switch (I(record, kCursorA)) {
        case 0:
            g_SharedStatusGate = (I(record, kExtraStageFlag) != 0) ? 10U : 13U;
            return;
        case 1:
        case 2:
            RunGameOverPathBStackAbi(reinterpret_cast<void *>(kGameStateManager),
                                     4);
            return;
        default:
            return;
        }
    }

    default: // mode 9 / out of range: bare epilogue
        return;
    }
}

} // namespace th10
