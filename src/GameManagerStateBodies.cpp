// Manager per-state calculation bodies. State 1 (0x0042d300) drives the
// loading/entry gate for the demo, stage-select and practice paths; the
// switch reads the sub-state at manager + 0x20.
#include <cstdio>
#include <string.h>

#include "EntityHelpers.hpp"
#include "GameManagerState.hpp"
#include "ReplaySave.hpp"
#include "Th10Types.hpp"

namespace th10 {

namespace {

// Main-chain render owner (TH10 DAT_00491c10) whose entity lists the state
// bodies resolve.
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

// Diagnostic/present flags twin at DAT_00474e36 used by state 1 sub-state 2
// (only read, so a fixed global address suffices).
extern u32 g_ManagerSubGateFlags; // TH10 DAT_00474e36

// TH10 0x0042c620 is implemented as SetGameManagerSubState in
// GameManagerState.cpp (sub-state write + rate-tracker reset).

// TH10 0x00448d00. Resolves a cell inside the manager work record
// (native: three stack arguments: work record from manager + 0x18, 0, 0xf).
extern u32 *ResolveManagerWorkCell(void *work_record, u32 zero, u32 fifteen); // TH10 0x00448d00

// TH10 0x00409e50. Releases the handle stored at a target address
// (native EAX = target address).
extern void ReleaseHandleTargetByPointer(void *target); // TH10 0x00409e50

// Fixed menu globals shared by the select bodies.
extern u32 g_StageTextSprites[2]; // TH10 DAT_00474c68/6c (menu stage value/floor)
extern u32 g_StageScoreSelector[2]; // TH10 DAT_00474c74/78 (menu selector/prev)
extern void *g_AsciiManagerHost; // TH10 DAT_004776e0
extern u8 g_MenuInputFlagsByte; // TH10 DAT_00474e34 (byte gate flags)
extern u32 g_SharedStatusGate; // TH10 DAT_00491fb8

// TH10 0x0044bea0. Cursor shift with disabled-slot skipping is implemented
// and exported as ShiftManagerSelector in GameManagerState.cpp.

// TH10 0x0042c6d0 / 0x0042c750 / 0x0040ace0 / 0x0044be70 / 0x0044be20 /
// 0x0040ad20 are implemented and exported from GameManagerState.cpp as
// ReleaseManagerSlotEntity / Call42C750 / PollMenuInputState / Call44BE70 /
// RunManagerCursorHandle / Call40AD20.

// TH10 0x0040c540 (extra difficulty-select entrance effect; native takes
// two stack floats, called here as 480.0 and 392.0).
extern void Call40C540(float x_arg, float y_arg); // TH10 0x0040c540

// TH10 0x0043c8b0 (extra entrance animation/voice helper; five stack args).
extern void Call43C8B0(i32 a1, i32 a2, i32 a3, i32 a4, i32 a5); // TH10 0x0043c8b0

// TH10 0x00420c30 (stage-entry setup helper taking one stack float, called
// here as 6.0).
extern void Call420C30(float value); // TH10 0x00420c30

// TH10 0x0044b010. Extra/replay-mode presence check returning nonzero when
// the alternate save bank is active.
extern i32 Call44B010(void); // TH10 0x0044b010

// Stage-select memory (DAT_00491c08) and the clear-scan result used by the
// state-9 stage select (DAT_00474cac).
extern u32 g_StageSelectMemory; // TH10 DAT_00491c08
extern u32 g_ClearFlagScan; // TH10 DAT_00474cac

// State-B menu boundaries. These retain their native responsibilities until
// their individual leaf bodies are reconstructed.
extern i32 GetManagerDifficultyValue(void *owner, i32 selector); // TH10 0x004088c0
extern void RefreshStateBSelection(void *game_manager); // TH10 0x00432690
extern void ResetStateBCompletionFlags(void); // TH10 0x0042c8c0
extern void *FindStateBEntityByHandle(void *owner, u32 handle); // TH10 0x004491c0
extern void *ParseDemoRecord(char *source); // TH10 0x004296f0
extern void DestroyDemoParseObject(void *parsed); // TH10 0x004294a0
extern void StartBgmQueue(u32 channel, const char *name); // TH10 0x00420a90
extern void ResetBgmQueue(u32 channel, u32 value); // TH10 0x00420b10
// TH10 0x00421fa0 (native EDI ABI, implemented in ScoreFileFormats.cpp):
// inserts the finished run into one score-save slot's high-score table and
// returns the insertion rank, or -1 when the run did not place. The native
// callers derive EDI from the score-save state pointer variable and the
// stage/difficulty slot globals.
extern i32 InsertScoreRecordEdi(void *score_table);
extern void ResetNameInputPresentation(u32 value); // TH10 0x00405410
extern void SaveReplayNameInput(void); // TH10 0x004297b0

// Replay save context (TH10 DAT_00477838 game-mode object) passed to the
// replay-save writer 0x00429b60.
extern void *g_GameModeObject; // TH10 DAT_00477838
extern i32 LoadScoreDisplayRecords(void **out_records, u32 unused); // TH10 0x0044b360
extern void ReleaseScoreDisplayRecord(void *record); // TH10 0x00434a20
extern void ReleaseScoreDisplayText(void *text); // TH10 0x004349e0
extern void SpawnScoreDisplayText(void); // TH10 0x00449450
extern void UpdateScoreDisplayEntity(void *entity, u32 style); // TH10 0x00434a80
extern void RefreshAudioOptionPresentation(void); // TH10 0x0042e5a0
extern void SetAudioOptionEntityState(u32 value); // TH10 0x00449670
extern i32 QueryAudioOptionAvailability(u32 option); // TH10 0x00427c70

// Append `row` to the disabled-row list of the cursor record embedded at
// manager + 0x24: the list lives at manager + 0xb4 and its index/count word
// at manager + 0xf8 (native increments that same word in place).
inline void AppendDisabledMenuRow(u32 *words, u32 row)
{
    const u32 index = words[0xf8 / 4];
    words[(0xb4 + 4 * index) / 4] = row;
    words[0xf8 / 4] = index + 1;
}

} // namespace

// TH10 0x0042d300. Game-manager state-1 calculation body (manager in EBX,
// dispatched with ECX set to the state value by the calculation controller).
// The + 0x20 sub-state gates entry slots 0x58/0x5a, a voice reset boundary
// and practice-entry thresholds, all against the + 0x2b4 frame counter.
i32 RunManagerStateBody1(void *game_manager)
{
    u32 *const words = static_cast<u32 *>(game_manager);
    switch (words[0x20 / 4]) {
    case 0: {
        u8 *const entity = FindEntityEdxStackAbi(
            g_MainChainRenderOwner, words[0x424 / 4]);
        if (entity == 0) {
            SpawnManagerEntityFromScript(game_manager, 0x58);
            SpawnManagerEntityFromScript(game_manager, 0x5a);
            u32 *const cell = ResolveManagerWorkCell(
                reinterpret_cast<void *>(words[0x18 / 4]), 0, 0xf);
            words[0x684 / 4] = *cell;
        }
        SetGameManagerSubState(game_manager, 1);
        const i32 slot_count = static_cast<i32>(words[0x2c / 4]);
        if (slot_count == 0 || slot_count > 0)
            words[0x24 / 4] = 0;
        else
            words[0x24 / 4] = static_cast<u32>(slot_count - 1);
        break;
    }
    case 1:
        break;
    case 2:
        if ((g_ManagerSubGateFlags & 0x160bU) == 0)
            return 1;
        SetManagerSlotEntityStopWord(game_manager, 0x5a, 6);
        SetGameManagerSubState(game_manager, 4);
        ReserveContextChannel(
            reinterpret_cast<void *>(0x00492590), 0x20, 0);
        ReleaseHandleTargetByPointer(
            reinterpret_cast<u8 *>(game_manager) + 0x684);
        return 1;
    case 4:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 0x1e)
            SetGameManagerState(game_manager, 2);
        return 1;
    default:
        return 1;
    }

    if (static_cast<i32>(words[0x2b4 / 4]) > 10)
        SetGameManagerSubState(game_manager, 2);
    return 1;
}

// TH10 0x0042d920. Game-manager state-4 calculation body: audio options.
// Cursor rows 0..5 select BGM volume, SE volume, input/listen mode, reset,
// and exit commands. The three byte settings are TH10 DAT_00491d68..6a.
i32 RunManagerStateBody4(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    u8 *const music_volume = reinterpret_cast<u8 *>(0x00491d68U);
    u8 *const sound_volume = reinterpret_cast<u8 *>(0x00491d69U);
    u8 *const presentation_mode = reinterpret_cast<u8 *>(0x00491d6aU);

    switch (words[8]) {
    case 0:
        words[0x2c / 4] = 6;
        Call40AD20(0, bytes + 0x24);
        SpawnManagerEntityFromScript(game_manager, 1);
        RefreshAudioOptionPresentation();
        SetGameManagerSubState(game_manager, 1);
        break;
    case 1:
        if (static_cast<i32>(words[0x2b4 / 4]) <= 6)
            return 1;
        SetGameManagerSubState(game_manager, 2);
        SetEntityStopWordByIdAndRun(words[178], 3);
        SetManagerSlotEntityStopWord(game_manager, 1,
            static_cast<u16>(words[0x24 / 4]) + 17);
        break;
    case 2: {
        const u32 previous = words[0x24 / 4];
        words[0x28 / 4] = previous;
        if ((g_ManagerSubGateFlags & 0x10U) != 0 ||
            (g_MenuInputFlagsByte & 0x10U) != 0)
            ShiftManagerSelector(bytes + 0x24, -1);
        if ((g_ManagerSubGateFlags & 0x20U) != 0 ||
            (g_MenuInputFlagsByte & 0x20U) != 0)
            ShiftManagerSelector(bytes + 0x24, 1);
        const u32 selection = words[0x24 / 4];
        if (previous != selection) {
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);
            SetEntityStopWordByIdAndRun(words[178], 3);
            SetManagerSlotEntityStopWord(game_manager, 1,
                static_cast<u16>(selection) + 7);
        }
        if ((g_ManagerSubGateFlags & 0xaU) != 0) {
            if (selection == 5) {
                SetManagerSlotEntityStopWord(game_manager, 1, 6);
                ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xb, 0);
                SetGameManagerSubState(game_manager, 4);
            } else {
                Call40AD20(5, bytes + 0x24);
                Call42C750(game_manager, 1);
                SetManagerSlotEntityStopWord(game_manager, 1, 12);
                ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xb, 0);
            }
            return 1;
        }
        if (selection == 2 && QueryAudioOptionAvailability(60) != 0)
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);
        if (PollMenuInputState(0x40)) {
            if (selection == 0) {
                *presentation_mode = (*presentation_mode == 0) ? 2 :
                    static_cast<u8>(*presentation_mode - 1);
                ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);
            } else if (selection == 1) {
                *music_volume = (*music_volume >= 5) ?
                    static_cast<u8>(*music_volume - 5) : 0;
                RefreshAudioOptionPresentation();
            } else if (selection == 2) {
                *sound_volume = (*sound_volume >= 5) ?
                    static_cast<u8>(*sound_volume - 5) : 0;
                RefreshAudioOptionPresentation();
            }
        }
        if (PollMenuInputState(0x80)) {
            if (selection == 0) {
                *presentation_mode = (*presentation_mode >= 2) ? 0 :
                    static_cast<u8>(*presentation_mode + 1);
                ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);
            } else if (selection == 1) {
                *music_volume = (*music_volume <= 95) ?
                    static_cast<u8>(*music_volume + 5) : 100;
                RefreshAudioOptionPresentation();
            } else if (selection == 2) {
                *sound_volume = (*sound_volume <= 95) ?
                    static_cast<u8>(*sound_volume + 5) : 100;
                RefreshAudioOptionPresentation();
            }
        }
        if ((g_ManagerSubGateFlags & 0x1001U) == 0)
            return 1;
        if (selection == 4) {
            *music_volume = 100;
            *sound_volume = 80;
            *presentation_mode = 0;
            RefreshAudioOptionPresentation();
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);
        } else if (selection == 3 || selection == 5) {
            SetManagerSlotEntityStopWord(game_manager, 1, 6);
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xa, 0);
            SetGameManagerSubState(game_manager, 4);
        }
        break;
    }
    case 4:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 6) {
            ReleaseManagerSlotEntity(game_manager, 1);
            SetManagerSlotEntityStopWord(game_manager, 0x5a, 8);
            SetManagerSlotEntityStopWord(game_manager, 0x5b, 8);
            SetGameManagerState(game_manager, 2);
        }
        break;
    default:
        break;
    }
    return 1;
}

// TH10 0x00430320. Game-manager state-6 calculation body: the character /
// extra-character select menu. The +0x20 sub-state drives entry spawns, the
// cursor loop, the confirm/focus branches and the exit cleanup; the entity
// slots for the menu scripts live at +0x2c4 + 4*s and the previous selector
// copy at +0x58f0 (extra) or +0x24 (main).
i32 RunManagerStateBody6(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    u32 *const selector = g_StageScoreSelector; // DAT_00474c74

    const u32 sub_state = words[0x20 / 4];
    if (sub_state > 4)
        return 1;

    // Native precomputes slot 0x77 (main) / 0x78 (extra) in EBX before the
    // jump table; sub-state 0 never needs v3 again after setup, so a local
    // is only kept where the later branches use it.
    const u32 char_slot = (selector[0] >= 4) ? 0x78 : 0x77;

    switch (sub_state) {
    case 0: {
        u8 *const entity = FindEntityEdxStackAbi(
            g_MainChainRenderOwner, words[0x43c / 4]);
        if (entity == 0) {
            SpawnManagerEntityFromScript(game_manager, 0x5e);
            u32 *const cell = ResolveManagerWorkCell(
                static_cast<u8 *>(g_AsciiManagerHost) + 0x8994, 8, 0xf);
            words[0x5d0 / 4] = *cell;
        }
        words[0x2c / 4] = (selector[0] >= 4) ? 1U : 4U;
        ReleaseManagerSlotEntity(game_manager, char_slot);
        SpawnManagerEntityFromScript(game_manager, char_slot);
        SetEntityStopWordByIdAndRun(
            words[(0x2c4 + 4 * char_slot) / 4], 3);
        SetManagerSlotEntityStopWord(
            game_manager, char_slot,
            static_cast<u16>(words[0x24 / 4]) + 0x11);
        SpawnManagerEntityFromScript(game_manager, 0x62);
        SetGameManagerSubState(game_manager, 1);
        if (static_cast<i32>(words[0x2b4 / 4]) > 6)
            SetGameManagerSubState(game_manager, 2);
        break;
    }
    case 1:
        if (static_cast<i32>(words[0x2b4 / 4]) > 6)
            SetGameManagerSubState(game_manager, 2);
        break;
    case 2:
        if (selector[0] < 4) {
            const u32 previous = words[0x24 / 4];
            words[0x28 / 4] = previous;
            if (PollMenuInputState(0x10) != 0)
                ShiftManagerSelector(bytes + 0x24, -1);
            if (PollMenuInputState(0x20) != 0)
                ShiftManagerSelector(bytes + 0x24, 1);
            if (words[0x28 / 4] != words[0x24 / 4]) {
                ReserveContextChannel(
                    reinterpret_cast<void *>(0x00492590), 0xc, 0);
                Call42C750(game_manager, char_slot);
                SetManagerSlotEntityStopWord(
                    game_manager, char_slot,
                    static_cast<u16>(words[0x24 / 4]) + 7);
            }
        }
        if ((g_ManagerSubGateFlags & 0xaU) != 0) {
            SetGameManagerSubState(game_manager, 4);
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0xb, 0);
            ReleaseManagerSlotEntity(game_manager, char_slot);
            return 1;
        }
        if ((g_ManagerSubGateFlags & 0x1001U) != 0) {
            u32 chosen_handle = 0;
            SetManagerSlotEntityStopWord(game_manager, char_slot, 6);
            if (selector[0] >= 4) {
                u32 *const slot_id = &words[(0x2c4 + 4 * char_slot) / 4];
                u8 *const focus_entity =
                    FindEntityEdxStackAbi(g_MainChainRenderOwner, *slot_id);
                if (focus_entity == 0)
                    *slot_id = 0;
                u8 *node = focus_entity + 0x10;
                while (node != 0) {
                    u8 *const child = *reinterpret_cast<u8 **>(node);
                    const i32 child_kind = static_cast<i32>(
                        *reinterpret_cast<const short *>(child + 0x38a));
                    if (child_kind == 0x71) {
                        chosen_handle = *reinterpret_cast<u32 *>(child);
                        break;
                    }
                    node = *reinterpret_cast<u8 **>(node + 4);
                }
            } else {
                ResolveChildEntityByKind(
                    &words[(0x2c4 + 4 * char_slot) / 4],
                    static_cast<i32>(words[0x24 / 4]) + 0x6d,
                    &chosen_handle);
            }
            SetEntityStateWordByHandleSlot(&chosen_handle, 2);
            SetGameManagerSubState(game_manager, 3);
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0xa, 0);
            return 1;
        }
        break;
    case 3:
        if (static_cast<i32>(words[0x2b4 / 4]) < 14)
            return 1;
        ReleaseManagerSlotEntity(game_manager, 0x62);
        SetGameManagerState(game_manager, 7);
        if (selector[0] < 4)
            selector[0] = words[0x24 / 4];
        {
            void *const handle = RunManagerCursorHandle(bytes + 0x24);
            words[0x2c / 4] = 2;
            Call40AD20(g_StageTextSprites[0], handle);
        }
        return 1;
    case 4:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 6) {
            ReleaseManagerSlotEntity(game_manager, 0x62);
            SetManagerSlotEntityStopWord(game_manager, 0x5a, 8);
            SetManagerSlotEntityStopWord(game_manager, 0x5b, 8);
            ReleaseManagerSlotEntity(game_manager, 0x5e);
            ReleaseHandleTargetByPointer(bytes + 0x5d0);
            SetGameManagerState(game_manager, 2);
            selector[0] = (selector[0] >= 4) ? words[0x58f0 / 4]
                                              : words[0x24 / 4];
            Call44BE70(bytes + 0x24);
        }
        break;
    default:
        break;
    }
    return 1;
}

// TH10 0x00430a60. Game-manager state-8 calculation body: the difficulty
// select. Manager + 0x24 holds the cursor record (current + 0x24, previous
// + 0x28, maximum + 0x2c, disabled list at + 0xb4 with the count at + 0xf8);
// the difficulty rows are child entities (kind keyed off the stage) under
// the stage + 0x96 script entity, whose id slot is at + 0x51c + 4*stage.
// Extra mode (selector DAT_00474c74 == 4) trims locked rows out of the
// disabled list using the unlock bytes at DAT_00477c3c + 3*stage + 0x1d888.
i32 RunManagerStateBody8(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    const u32 stage = g_StageTextSprites[0];
    const u32 selector = g_StageScoreSelector[0];
    u32 *const slot_word = &words[(0x51c + 4 * stage) / 4];
    const u32 extra_slot = stage + 0x96;

    const u32 sub_state = words[0x20 / 4];
    if (sub_state > 4)
        return 1;

    switch (sub_state) {
    case 0: {
        words[0x2c / 4] = 3;
        if (selector == 4) {
            u8 *const player = reinterpret_cast<u8 *>(0x47783cU);
            const i32 step = static_cast<i32>(words[0x2c / 4]);
            if (player[3 * stage + 0x1d888] == 0) {
                if (words[0x24 / 4] == 0) {
                    if (player[3 * stage + 0x1d889] != 0) {
                        words[0x24 / 4] = (step != 0)
                            ? ((step > 1) ? 1U
                                          : static_cast<u32>(step - 1))
                            : 1U;
                    } else {
                        words[0x24 / 4] = (step != 0)
                            ? ((step > 2) ? 2U
                                          : static_cast<u32>(step - 1))
                            : 2U;
                    }
                }
                AppendDisabledMenuRow(words, 0);
            }
            if (player[3 * stage + 0x1d889] == 0) {
                if (words[0x24 / 4] == 1) {
                    if (player[3 * stage + 0x1d888] == 0) {
                        words[0x24 / 4] = (step != 0)
                            ? ((step > 2) ? 2U
                                          : static_cast<u32>(step - 1))
                            : 2U;
                    } else {
                        words[0x24 / 4] = (step != 0)
                            ? ((step > 0) ? 0U
                                          : static_cast<u32>(step - 1))
                            : 0U;
                    }
                }
                AppendDisabledMenuRow(words, 1);
            }
            if (player[3 * stage + 0x1d88a] == 0) {
                if (words[0x24 / 4] == 2) {
                    if (player[3 * stage + 0x1d888] == 0) {
                        words[0x24 / 4] = (step != 0)
                            ? ((step > 1) ? 1U
                                          : static_cast<u32>(step - 1))
                            : 1U;
                    } else {
                        words[0x24 / 4] = (step != 0)
                            ? ((step > 0) ? 0U
                                          : static_cast<u32>(step - 1))
                            : 0U;
                    }
                }
                AppendDisabledMenuRow(words, 2);
            }
        }
        SpawnManagerEntityFromScript(game_manager, 0x64);
        ReleaseManagerSlotEntity(game_manager, extra_slot);
        SpawnManagerEntityFromScript(game_manager, extra_slot);
        SetEntityStopWordByIdAndRun(*slot_word, 3);
        SetManagerSlotEntityStopWord(
            game_manager, extra_slot,
            static_cast<u16>(words[0x24 / 4]) + 0x11);
        SetGameManagerSubState(game_manager, 1);

        u32 *const player_words = reinterpret_cast<u32 *>(0x47783cU);
        const u32 record_index = stage * 0x329d + selector;
        u32 child_handle = 0;
        if (player_words[record_index + 308] == 0) {
            ResolveChildEntityByKind(
                slot_word, static_cast<i32>(stage * 3) + 0x8a,
                &child_handle);
            ClearEntityFlag2ByHandleSlot(&child_handle);
        }
        if (player_words[record_index + 4627] == 0) {
            ResolveChildEntityByKind(
                slot_word, static_cast<i32>(stage * 3) + 0x8b,
                &child_handle);
            ClearEntityFlag2ByHandleSlot(&child_handle);
        }
        if (player_words[record_index + 8946] == 0) {
            ResolveChildEntityByKind(
                slot_word, static_cast<i32>(stage * 3) + 0x8c,
                &child_handle);
            ClearEntityFlag2ByHandleSlot(&child_handle);
        }
        if (static_cast<i32>(words[0x2b4 / 4]) > 6)
            SetGameManagerSubState(game_manager, 2);
        break;
    }
    case 1:
        if (static_cast<i32>(words[0x2b4 / 4]) > 6)
            SetGameManagerSubState(game_manager, 2);
        break;
    case 2: {
        words[0x28 / 4] = words[0x24 / 4];
        if ((g_ManagerSubGateFlags & 0x10U) != 0 ||
            (g_MenuInputFlagsByte & 0x10U) != 0)
            ShiftManagerSelector(bytes + 0x24, -1);
        if ((g_ManagerSubGateFlags & 0x20U) != 0 ||
            (g_MenuInputFlagsByte & 0x20U) != 0)
            ShiftManagerSelector(bytes + 0x24, 1);
        if (words[0x28 / 4] != words[0x24 / 4]) {
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0xc, 0);
            Call42C750(game_manager, extra_slot);
            SetManagerSlotEntityStopWord(
                game_manager, extra_slot,
                static_cast<u16>(words[0x24 / 4]) + 7);
        }
        if ((g_ManagerSubGateFlags & 0xaU) != 0) {
            SetGameManagerSubState(game_manager, 4);
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0xb, 0);
            return 1;
        }
        if ((g_ManagerSubGateFlags & 0x1001U) != 0) {
            u32 child_handle = 0;
            SetGameManagerSubState(game_manager, 3);
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0xa, 0);
            ResolveChildEntityByKind(
                slot_word,
                static_cast<i32>(words[0x24 / 4]) + 6 * stage + 0x7e,
                &child_handle);
            SetEntityStateWordByHandleSlot(&child_handle, 6);
            return 1;
        }
        break;
    }
    case 3:
        if (static_cast<i32>(words[0x2b4 / 4]) == 10) {
            if ((*reinterpret_cast<u8 *>(0x474ca0U) & 0x10) != 0) {
                g_StageTextSprites[1] = words[0x24 / 4];
                RunManagerCursorHandle(bytes + 0x24);
                ReleaseManagerSlotEntity(game_manager, 0x64);
                SetGameManagerState(game_manager, 9);
            } else {
                Call40C540(480.0f, 392.0f);
                Call43C8B0(5, 0x20, 0, 0, 0);
            }
        }
        if (static_cast<i32>(words[0x2b4 / 4]) < 40)
            return 1;
        g_StageTextSprites[1] = words[0x24 / 4];
        RunManagerCursorHandle(bytes + 0x24);
        if ((*reinterpret_cast<u8 *>(0x474ca0U) & 0x10) != 0) {
            ReleaseManagerSlotEntity(game_manager, 0x64);
            SetGameManagerState(game_manager, 9);
            return 1;
        }
        SetGameManagerState(game_manager, 3);
        if (selector >= 4) {
            *reinterpret_cast<u32 *>(0x477848U) = 0x4748d8U;
            *reinterpret_cast<u32 *>(0x474c7cU) = 7;
            *reinterpret_cast<u32 *>(0x474c80U) = 7;
        } else {
            *reinterpret_cast<u32 *>(0x477848U) = 0x4747b8U;
            *reinterpret_cast<u32 *>(0x474c7cU) = 1;
            *reinterpret_cast<u32 *>(0x474c80U) = 1;
        }
        g_SharedStatusGate = 7;
        Call420C30(6.0f);
        return 1;
    case 4:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 6) {
            ReleaseManagerSlotEntity(game_manager, extra_slot);
            ReleaseManagerSlotEntity(game_manager, 0x64);
            SetGameManagerState(game_manager, 7);
            Call44BE70(bytes + 0x24);
        }
        break;
    default:
        break;
    }
    return 1;
}

// TH10 0x004306a0. Game-manager state-7 calculation body: the shot-type
// select reached from the character select (confirm in state 6). The same
// cursor record idiom as the other menus applies (current +0x24, previous
// +0x28, maximum +0x2c, disabled rows at +0xb4 with count +0xf8); the two
// shot scripts live in slots 0x63 (99) and 0x7d (125, id at word 302 =
// +0x4b8). Extra mode (DAT_00474c74 == 4) trims whole shot rows using the
// six unlock bytes at DAT_00477c3c + 0x1d888.
i32 RunManagerStateBody7(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    const u32 selector = g_StageScoreSelector[0];
    u32 *const slot_word = &words[(0x4b8 / 4)]; // slot 0x7d id

    const u32 sub_state = words[0x20 / 4];
    if (sub_state > 4)
        return 1;

    switch (sub_state) {
    case 0: {
        words[0x2c / 4] = 2;
        if (selector == 4) {
            u8 *const player = reinterpret_cast<u8 *>(0x47783cU);
            const i32 step = static_cast<i32>(words[0x2c / 4]);
            if (player[0x1d888] == 0 && player[0x1d889] == 0 &&
                player[0x1d88a] == 0) {
                if (words[0x24 / 4] == 0) {
                    words[0x24 / 4] = (step != 0)
                        ? ((step > 1) ? 1U
                                      : static_cast<u32>(step - 1))
                        : 1U;
                }
                AppendDisabledMenuRow(words, 0);
            }
            if (player[0x1d88b] == 0 && player[0x1d88c] == 0 &&
                player[0x1d88d] == 0) {
                if (words[0x24 / 4] == 1) {
                    words[0x24 / 4] = (step != 0)
                        ? ((step > 0) ? 0U
                                      : static_cast<u32>(step - 1))
                        : 0U;
                }
                AppendDisabledMenuRow(words, 1);
            }
        }
        SpawnManagerEntityFromScript(game_manager, 0x63);
        ReleaseManagerSlotEntity(game_manager, 0x7d);
        SpawnManagerEntityFromScript(game_manager, 0x7d);
        SetEntityStopWordByIdAndRun(*slot_word, 3);
        SetManagerSlotEntityStopWord(
            game_manager, 0x7d,
            static_cast<u16>(words[0x24 / 4]) + 0x11);
        SetGameManagerSubState(game_manager, 1);
        if (static_cast<i32>(words[0x2b4 / 4]) > 6)
            SetGameManagerSubState(game_manager, 2);
        break;
    }
    case 1:
        if (static_cast<i32>(words[0x2b4 / 4]) > 6)
            SetGameManagerSubState(game_manager, 2);
        break;
    case 2: {
        words[0x28 / 4] = words[0x24 / 4];
        if ((g_ManagerSubGateFlags & 0x40U) != 0 ||
            (g_MenuInputFlagsByte & 0x40U) != 0)
            ShiftManagerSelector(bytes + 0x24, -1);
        if ((g_ManagerSubGateFlags & 0x80U) != 0 ||
            (g_MenuInputFlagsByte & 0x80U) != 0)
            ShiftManagerSelector(bytes + 0x24, 1);
        if (words[0x28 / 4] != words[0x24 / 4]) {
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0xc, 0);
            Call42C750(game_manager, 0x7d);
            SetManagerSlotEntityStopWord(
                game_manager, 0x7d,
                static_cast<u16>(words[0x24 / 4]) + 7);
        }
        if ((g_ManagerSubGateFlags & 0xaU) != 0) {
            SetGameManagerSubState(game_manager, 4);
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0xb, 0);
            return 1;
        }
        if ((g_ManagerSubGateFlags & 0x1001U) != 0) {
            const u32 cursor = words[0x24 / 4];
            u32 child_handle = 0;
            ResolveChildEntityByKind(
                slot_word, static_cast<i32>(cursor) + 0x79,
                &child_handle);
            SetEntityStateWordByHandleSlot(&child_handle, 6);
            ResolveChildEntityByKind(
                slot_word, static_cast<i32>(cursor) + 0x7b,
                &child_handle);
            SetEntityStateWordByHandleSlot(&child_handle, 6);
            ResolveChildEntityByKind(
                slot_word, 0x7a - static_cast<i32>(cursor),
                &child_handle);
            SetEntityStateWordByHandleSlot(&child_handle, 1);
            ResolveChildEntityByKind(
                slot_word, 0x7c - static_cast<i32>(cursor),
                &child_handle);
            SetEntityStateWordByHandleSlot(&child_handle, 1);
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0xa, 0);
            SetGameManagerSubState(game_manager, 3);
            return 1;
        }
        break;
    }
    case 3:
        if (static_cast<i32>(words[0x2b4 / 4]) < 14)
            return 1;
        ReleaseManagerSlotEntity(game_manager, 0x63);
        SetGameManagerState(game_manager, 8);
        {
            const u32 previous_stage = g_StageTextSprites[0];
            const u32 new_stage = words[0x24 / 4];
            g_StageTextSprites[0] = new_stage;
            void *const handle = RunManagerCursorHandle(bytes + 0x24);
            words[0x2c / 4] = 3;
            if (previous_stage == new_stage) {
                Call40AD20(g_StageTextSprites[1], handle);
                return 1;
            }
            i32 *const handle_words = static_cast<i32 *>(handle);
            const i32 previous_value = handle_words[2];
            handle_words[0] =
                (previous_value > 0) ? 0 : previous_value - 1;
            return 1;
        }
    case 4:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 6) {
            ReleaseManagerSlotEntity(game_manager, 0x7d);
            ReleaseManagerSlotEntity(game_manager, 0x63);
            SetGameManagerState(game_manager, 6);
            g_StageTextSprites[0] = words[0x24 / 4];
            Call44BE70(bytes + 0x24);
        }
        break;
    default:
        break;
    }
    return 1;
}

// ---- Extra-unlock code listener (native 0x432396..0x00432ce tail) ------

// Keyboard-state bank: previous/current 0x100-byte key tables (GetKeyboardState
// or the DirectInput buffer) and the interleaved four-lane pressed-edge
// buffer computed each frame (cur & (cur ^ prev) per lane byte).
extern u8 g_KeyboardStatePrevious[0x100]; // TH10 DAT_00497d90
extern u8 g_KeyboardStateCurrent[0x100]; // TH10 DAT_00497e90
extern u8 g_KeyboardPressedEdge[0x400]; // TH10 DAT_004979a8

// Sequence progress (TH10 DAT_004979a4, 0..22) and idle timeout
// (TH10 DAT_004979a0, resets the sequence after 300 frames).
extern u32 g_ExtraCodeSequenceIndex; // TH10 DAT_004979a4
extern u32 g_ExtraCodeIdleTimer; // TH10 DAT_004979a0

// The 22-entry expected-key table (TH10 DAT_0046ef10); entries index the
// pressed-edge buffer and a key is "pressed" when its byte is negative.
extern const u32 g_ExtraUnlockCodeSequence[22]; // TH10 DAT_0046ef10

// One frame of the 22-key unlock sequence listener. Runs only while the
// menu sits on difficulty row 4 / shot-type row 2; on completion it unlocks
// all spell cards and reserves boundary channel 0x2c.
void RunExtraUnlockCodeListener()
{
    if ((g_ManagerSubGateFlags & 0x160bU) != 0U) {
        g_ExtraCodeSequenceIndex = 0U;
        g_ExtraCodeIdleTimer = 0U;
    }

    // Shift current -> previous, then sample the keyboard.
    for (u32 i = 0; i < 0x100U; ++i)
        g_KeyboardStatePrevious[i] = g_KeyboardStateCurrent[i];

    if (Call44B010() != 0) {
        // Pressed-edge recomputation over the four interleaved lanes.
        for (u32 j = 0; j < 0x100U; j += 4U) {
            for (u32 lane = 0; lane < 4U; ++lane) {
                const u8 current = g_KeyboardStateCurrent[j + lane];
                const u8 previous = g_KeyboardStatePrevious[j + lane];
                g_KeyboardPressedEdge[j + lane] =
                    static_cast<u8>(current & (current ^ previous));
            }
        }

        if (g_ExtraCodeSequenceIndex >= 22U) {
            // Full sequence entered: unlock all spell cards.
            ResetStateBCompletionFlags();
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590),
                                  0x2c, 0);
            g_ExtraCodeSequenceIndex = 0U;
        } else if (static_cast<signed char>(
                       g_KeyboardPressedEdge
                           [g_ExtraUnlockCodeSequence
                                [g_ExtraCodeSequenceIndex]]) < 0) {
            ++g_ExtraCodeSequenceIndex;
            g_ExtraCodeIdleTimer = 0U;
        } else {
            // Any press among every third triple of edge bytes (indices
            // 0..56 step 3, each ORing bytes k, k+1, k+2 — the native
            // address arithmetic at 0x4979a6/eax resolves to the same
            // three lanes) restarts the sequence.
            u8 any_pressed = 0;
            for (u32 k = 0; k < 57U; k += 3U) {
                any_pressed = static_cast<u8>(
                    any_pressed
                    | g_KeyboardPressedEdge[k]
                    | g_KeyboardPressedEdge[k + 1]
                    | g_KeyboardPressedEdge[k + 2]);
            }
            if (static_cast<signed char>(any_pressed) < 0)
                g_ExtraCodeSequenceIndex = 0U;
        }
    }

    if (++g_ExtraCodeIdleTimer > 300U) {
        g_ExtraCodeSequenceIndex = 0U;
        g_ExtraCodeIdleTimer = 0U;
    }
}

// TH10 0x00431ee0. Game-manager state-B calculation body: extra-mode
// difficulty/shot-type selection. The native routine uses two cursor records
// in the manager (+0x24 and +0xfc), with ten dynamically loaded option
// handles at +0x5d4; the opaque boundary calls remain explicit below.
i32 RunManagerStateBodyB(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    u32 *const primary_cursor = words + 9;
    u32 *const secondary_cursor = words + 63;
    const u32 sub_state = words[8];

    switch (sub_state) {
    case 0: {
        words[0x2c / 4] = 6;
        primary_cursor[0] = 0;
        words[0x104 / 4] = 5;
        secondary_cursor[0] = (words[0x104 / 4] > 1)
            ? 1U : words[0x104 / 4] - 1U;
        words[0x1cc / 4] = 1;
        const i32 difficulty = GetManagerDifficultyValue(
            g_MainChainRenderOwner, static_cast<i32>(secondary_cursor[0]));
        const i32 option_count = (difficulty + 9) / 10 + 1;
        words[0x1dc / 4] = static_cast<u32>(option_count);
        words[0x1d4 / 4] = (option_count > 0) ? 0U : static_cast<u32>(
            (difficulty + 9) / 10);
        words[0x2a4 / 4] = 1;
        if (FindStateBEntityByHandle(g_MainChainRenderOwner,
                                     words[0x43c / 4]) == 0) {
            SpawnManagerEntityFromScript(game_manager, 0x5e);
            u32 *const work_cell = ResolveManagerWorkCell(
                static_cast<u8 *>(g_AsciiManagerHost) + 0x8994, 8, 0xf);
            words[0x5d0 / 4] = *work_cell;
        }
        SpawnManagerEntityFromScript(game_manager, 0x66);
        SetGameManagerSubState(game_manager, 1);
        SpawnManagerEntityFromScript(game_manager,
                                     primary_cursor[0] / 3 + 152);
        SpawnManagerEntityFromScript(game_manager, primary_cursor[0] + 154);
        SpawnManagerEntityFromScript(game_manager,
                                     secondary_cursor[0] + 160);
        for (u32 script_id = 168; script_id <= 172; ++script_id)
            SpawnManagerEntityFromScript(game_manager, script_id);
        for (u32 script_id = 165; script_id <= 167; ++script_id)
            SpawnManagerEntityFromScript(game_manager, script_id);
        // Native case 0 falls through into the case 1 timer check.
        // fallthrough
    }
    case 1:
        if (static_cast<i32>(words[0x2b4 / 4]) > 6)
            SetGameManagerSubState(game_manager, 2);
        break;
    case 2: {
        const u32 old_secondary = secondary_cursor[0];
        const u32 old_primary = primary_cursor[0];
        secondary_cursor[1] = old_secondary;
        words[0x1d8 / 4] = words[0x1d4 / 4];
        if ((g_ManagerSubGateFlags & 0x10U) != 0 ||
            (g_MenuInputFlagsByte & 0x10U) != 0) {
            ShiftManagerSelector(secondary_cursor, -1);
            SetEntityStopWordByIdAndRun(words[0x56c / 4], 2);
        }
        if ((g_ManagerSubGateFlags & 0x20U) != 0 ||
            (g_MenuInputFlagsByte & 0x20U) != 0) {
            ShiftManagerSelector(secondary_cursor, 1);
            SetEntityStopWordByIdAndRun(words[0x570 / 4], 2);
        }
        if (old_secondary != secondary_cursor[0]) {
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);
            ReleaseManagerSlotEntity(game_manager, old_secondary + 160);
            SpawnManagerEntityFromScript(game_manager,
                                         secondary_cursor[0] + 160);
            // Reload the page cursor toward one (native sign-split on the
            // +0x1dc page count: zero or > 1 -> 1, == 1 -> 0).
            if (words[0x1d4 / 4] > 0) {
                const u32 page_count = words[0x1dc / 4];
                words[0x1d4 / 4] =
                    (page_count == 0 || page_count > 1)
                        ? 1U : page_count - 1U;
                RefreshStateBSelection(game_manager);
            }
            words[0x1dc / 4] = static_cast<u32>(
                (GetManagerDifficultyValue(g_MainChainRenderOwner,
                                            static_cast<i32>(secondary_cursor[0])) + 9) / 10 + 1);
        }
        if ((g_ManagerSubGateFlags & 0x40U) != 0 ||
            (g_MenuInputFlagsByte & 0x40U) != 0) {
            ShiftManagerSelector(primary_cursor, -1);
            SetEntityStopWordByIdAndRun(words[0x564 / 4], 2);
        }
        if ((g_ManagerSubGateFlags & 0x80U) != 0 ||
            (g_MenuInputFlagsByte & 0x80U) != 0) {
            ShiftManagerSelector(primary_cursor, 1);
            SetEntityStopWordByIdAndRun(words[0x568 / 4], 2);
        }
        if (old_primary != primary_cursor[0]) {
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);
            if (old_primary / 3 != primary_cursor[0] / 3) {
                ReleaseManagerSlotEntity(game_manager,
                                         old_primary / 3 + 152);
                SpawnManagerEntityFromScript(game_manager,
                                             primary_cursor[0] / 3 + 152);
            }
            ReleaseManagerSlotEntity(game_manager, old_primary + 154);
            SpawnManagerEntityFromScript(game_manager,
                                         primary_cursor[0] + 154);
            if (words[0x1d4 / 4] > 0)
                RefreshStateBSelection(game_manager);
        }
        if ((g_ManagerSubGateFlags & 0x1001U) != 0) {
            if (words[0x1d4 / 4] == 0) {
                for (u32 option_index = 0; option_index < 10; ++option_index)
                    words[(0x5d4 + 4 * option_index) / 4] =
                        *ResolveManagerWorkCell(
                            static_cast<u8 *>(g_AsciiManagerHost) + 0x899c,
                            option_index + 23, 15);
            }
            ShiftManagerSelector(words + 117, 1);
            // On a page above zero the list render is refreshed; on the
            // first page the ten just-loaded option handles expire instead
            // (native 0x00409e50 loop over +0x5d4).
            if (words[0x1d4 / 4] != 0) {
                RefreshStateBSelection(game_manager);
            } else {
                for (u32 option_index = 0; option_index < 10; ++option_index)
                    ReleaseHandleTargetByPointer(
                        bytes + 0x5d4 + 4 * option_index);
            }
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xa, 0);
        }
        // Extra-unlock code listener: active only while the difficulty
        // cursor sits on row 4 and the shot-type cursor on row 2 (native
        // 0x43237f..0x4324ce).
        if (secondary_cursor[0] == 4 && primary_cursor[0] == 2)
            RunExtraUnlockCodeListener();
        if ((g_ManagerSubGateFlags & 0xaU) != 0) {
            SetGameManagerSubState(game_manager, 3);
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xb, 0);
            for (u32 script_id = 152; script_id <= 172; ++script_id)
                ReleaseManagerSlotEntity(game_manager, script_id);
            for (u32 option_index = 0; option_index < 10; ++option_index)
                ReleaseHandleTargetByPointer(bytes + 0x5d4 + 4 * option_index);
        }
        break;
    }
    case 3:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 6) {
            ReleaseManagerSlotEntity(game_manager, 0x66);
            SetManagerSlotEntityStopWord(game_manager, 0x5a, 8);
            SetManagerSlotEntityStopWord(game_manager, 0x5b, 8);
            ReleaseManagerSlotEntity(game_manager, 0x5e);
            ReleaseHandleTargetByPointer(bytes + 0x5d0);
            SetGameManagerState(game_manager, 2);
            Call44BE70(bytes + 0x24);
        }
        break;
    default:
        break;
    }
    return 1;
}

// TH10 0x004315c0. Replay-select calculation body. The file-system scan is
// represented by the same parser boundary used by the native replay loader;
// the manager keeps fifty parser handles at +0x59d4, with the first twenty-five
// reserved for the numbered replay names.
i32 RunManagerStateBodyC(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    u32 *const cursor = words + 9;
    const u32 sub_state = words[8];

    switch (sub_state) {
    case 0: {
        words[0x2c / 4] = 25;
        words[0x2c / 4] = 25;
        Call40AD20(0, cursor);
        SetGameManagerSubState(game_manager, 1);
        for (u32 replay_index = 1; replay_index <= 25; ++replay_index) {
            char replay_name[32];
            sprintf(replay_name, "th10_%.2d.rpy", replay_index);
            words[(0x59d4 + 4 * (replay_index - 1)) / 4] =
                reinterpret_cast<u32>(ParseDemoRecord(replay_name));
        }
        words[5750] = 0;
        break;
    }
    case 1:
        if (static_cast<i32>(words[0x2b4 / 4]) > 6) {
            SetGameManagerSubState(game_manager, 2);
            words[0x2b4 / 4] = 0;
            words[0x2ac / 4] = static_cast<u32>(-1);
            words[0x2b8 / 4] = 0;
        }
        break;
    case 2: {
        cursor[1] = cursor[0];
        if ((g_ManagerSubGateFlags & 0x10U) != 0 ||
            (g_MenuInputFlagsByte & 0x10U) != 0)
            ShiftManagerSelector(cursor, -1);
        if ((g_ManagerSubGateFlags & 0x20U) != 0 ||
            (g_MenuInputFlagsByte & 0x20U) != 0)
            ShiftManagerSelector(cursor, 1);
        if (cursor[1] != cursor[0])
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);
        if ((g_ManagerSubGateFlags & 0xaU) != 0) {
            SetGameManagerSubState(game_manager, 5);
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xb, 0);
        } else if ((g_ManagerSubGateFlags & 0x1001U) != 0 &&
                   words[(0x59d4 + 4 * cursor[0]) / 4] != 0) {
            SetGameManagerSubState(game_manager, 4);
            words[5751] = cursor[0];
            RunManagerCursorHandle(cursor);
            words[0x2c / 4] = 7;
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xa, 0);
        }
        break;
    }
    case 3:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 32) {
            SetGameManagerState(game_manager, 3);
            *reinterpret_cast<u32 *>(0x00477848U) =
                0x474788U + 48 * (words[5751] + 1);
            *reinterpret_cast<u32 *>(0x474c7cU) = words[5751] + 1;
            *reinterpret_cast<u32 *>(0x474c80U) = words[5751] + 1;
            Call420C30(6.0f);
            g_SharedStatusGate = 12;
        }
        break;
    case 4:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 15) {
            SetGameManagerSubState(game_manager, 2);
            Call44BE70(cursor);
        }
        break;
    case 5:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 6) {
            for (u32 replay_index = 0; replay_index < 50; ++replay_index) {
                void *const replay = reinterpret_cast<void *>(
                    words[(0x59d4 + 4 * replay_index) / 4]);
                if (replay != 0)
                    DestroyDemoParseObject(replay);
                words[(0x59d4 + 4 * replay_index) / 4] = 0;
            }
            SetGameManagerState(game_manager, 2);
            Call44BE70(cursor);
        }
        break;
    default:
        break;
    }
    return 1;
}

// TH10 0x00432cb0. Game-manager state-F calculation body: post-run name
// entry. It provides a 13-column character grid and an eight-character text
// buffer at manager+0x58dc; the active grid cursor lives at +0x58f4.
i32 RunManagerStateBodyF(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    u32 *const primary_cursor = words + 9;
    u32 *const grid_cursor = words + (0x58f4 / 4);
    char *const name_buffer = reinterpret_cast<char *>(bytes + 0x58dc);
    const char *const alphabet = reinterpret_cast<const char *>(0x4746d8U);

    switch (words[8]) {
    case 0: {
        words[0x2c / 4] = 30;
        StartBgmQueue(0, "bgm/th10_17.wav");
        ResetBgmQueue(0, 17);
        if (FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                  words[0x43c / 4]) == 0) {
            SpawnManagerEntityFromScript(game_manager, 0x5e);
            u32 *const work_cell = ResolveManagerWorkCell(
                static_cast<u8 *>(g_AsciiManagerHost) + 0x8994, 8, 0xf);
            words[0x5d0 / 4] = *work_cell;
        }
        SpawnManagerEntityFromScript(game_manager, 0x68);
        SetGameManagerSubState(game_manager, 1);
        SpawnManagerEntityFromScript(game_manager, g_StageTextSprites[0] + 152);
        SpawnManagerEntityFromScript(game_manager,
            g_StageTextSprites[0] + g_StageTextSprites[1] +
            2 * g_StageTextSprites[0] + 154);
        SpawnManagerEntityFromScript(game_manager, g_StageScoreSelector[0] + 160);
        // Native 0x00432db9: EDI = (value of DAT_0047783c) + 8 +
        // 0x437c * (DAT_00474C6C + 3 * DAT_00474C68) — the stage slot whose
        // ten 24-byte records form the character's high-score table.
        const u32 score_slot =
            *reinterpret_cast<const u32 *>(0x474C6CU) +
            3U * *reinterpret_cast<const u32 *>(0x474C68U);
        void *const score_table =
            reinterpret_cast<u8 *>(
                *reinterpret_cast<const u32 *>(0x47783CU)) +
            8U + 0x437CU * score_slot;
        const i32 initial_cursor = InsertScoreRecordEdi(score_table);
        if (initial_cursor < 0) {
            Call40AD20(0, primary_cursor);
            words[0x58ec / 4] = 1;
        } else {
            ResetNameInputPresentation(0);
            Call40AD20(static_cast<u32>(initial_cursor), primary_cursor);
            words[0x590c / 4] = static_cast<u32>(strlen(alphabet));
            strncpy(name_buffer,
                    reinterpret_cast<const char *>(0x47783cU + 120952), 8);
            name_buffer[8] = '\0';
            words[0x58f0 / 4] = static_cast<u32>(strlen(name_buffer));
            words[0x58ec / 4] = 0;
        }
        break;
    }
    case 1:
        if (static_cast<i32>(words[0x2b4 / 4]) > 6)
            SetGameManagerSubState(game_manager, 2);
        break;
    case 2: {
        if (words[0x58ec / 4] == 0) {
            grid_cursor[1] = grid_cursor[0];
            if (PollMenuInputState(0x10))
                ShiftManagerSelector(grid_cursor, -13);
            if (PollMenuInputState(0x20))
                ShiftManagerSelector(grid_cursor, 13);
            if (PollMenuInputState(0x40))
                ShiftManagerSelector(grid_cursor,
                    (grid_cursor[0] % 13 != 0) ? -1 : 12);
            if (PollMenuInputState(0x80))
                ShiftManagerSelector(grid_cursor,
                    (grid_cursor[0] % 13 == 12) ? -12 : 1);
            if (grid_cursor[1] != grid_cursor[0])
                ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);
        }

        if ((g_ManagerSubGateFlags & 0x1001U) != 0) {
            const u32 alphabet_length = static_cast<u32>(strlen(alphabet));
            const u32 selection = grid_cursor[0];
            u32 name_length = words[0x58f0 / 4];
            if (words[0x58ec / 4] != 0) {
                SetGameManagerSubState(game_manager, 3);
            } else if (selection < alphabet_length - 3) {
                if (name_length < 8) {
                    name_buffer[name_length] = alphabet[selection];
                    ++name_length;
                    name_buffer[name_length] = '\0';
                    words[0x58f0 / 4] = name_length;
                    if (name_length == 8)
                        Call40AD20(alphabet_length - 1, grid_cursor);
                }
            } else if (selection == alphabet_length - 3) {
                if (name_length < 8) {
                    name_buffer[name_length] = ' ';
                    ++name_length;
                    name_buffer[name_length] = '\0';
                    words[0x58f0 / 4] = name_length;
                }
            } else if (selection == alphabet_length - 2) {
                if (name_length != 0) {
                    --name_length;
                    name_buffer[name_length] = '\0';
                    words[0x58f0 / 4] = name_length;
                }
            } else if (selection == alphabet_length - 1) {
                SetGameManagerSubState(game_manager, 3);
            }
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xa, 0);
        }

        if ((g_ManagerSubGateFlags & 0xaU) != 0) {
            if (words[0x58ec / 4] != 0) {
                SetGameManagerSubState(game_manager, 3);
                ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xa, 0);
            } else if (words[0x58f0 / 4] != 0) {
                const u32 name_length = words[0x58f0 / 4] - 1;
                words[0x58f0 / 4] = name_length;
                name_buffer[name_length] = '\0';
                ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xb, 0);
            }
        }
        break;
    }
    case 3:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 6) {
            ReleaseManagerSlotEntity(game_manager, 0x68);
            ReleaseManagerSlotEntity(game_manager, g_StageTextSprites[0] + 152);
            ReleaseManagerSlotEntity(game_manager,
                g_StageTextSprites[0] + g_StageTextSprites[1] +
                2 * g_StageTextSprites[0] + 154);
            ReleaseManagerSlotEntity(game_manager, g_StageScoreSelector[0] + 160);
            SetGameManagerState(game_manager, 0x10);
        }
        break;
    default:
        break;
    }
    return 1;
}

// TH10 0x00433ef0. Game-manager state-E calculation body: score-record
// list. The record loader determines how many visible rows exist; this body
// owns the cursor/window state and the presentation entity lifecycle while
// leaving record decoding and text rasterization at their dedicated leaves.
i32 RunManagerStateBodyE(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    u32 *const cursor = words + 9;
    u32 *const row_entities = words + (0x614 / 4);
    u32 *const header_entities = words + (0x664 / 4);
    const u32 sub_state = words[8];

    switch (sub_state) {
    case 0:
        if (words[0x2b4 / 4] == 1) {
            words[0x2c / 4] = 6;
            Call40AD20(0, cursor);
            if (FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                      words[0x440 / 4]) == 0) {
                SpawnManagerEntityFromScript(game_manager, 0x5f);
                u32 *const work_cell = ResolveManagerWorkCell(
                    static_cast<u8 *>(g_AsciiManagerHost) + 0x8994, 8, 0xf);
                words[0x5d0 / 4] = *work_cell;
            }
            SpawnManagerEntityFromScript(game_manager, 0x67);
            void *score_records = 0;
            const i32 record_count = LoadScoreDisplayRecords(&score_records, 0);
            words[0x5aac / 4] = reinterpret_cast<u32>(score_records);
            words[0x2c / 4] = (record_count > 0) ?
                static_cast<u32>(record_count) : 0;
            words[0x594 / 4] = words[0x2c / 4];
            words[0x58d8 / 4] = 0;
            for (u32 header_index = 0; header_index < 8; ++header_index) {
                SpawnManagerEntityFromScript(game_manager, header_index + 39);
                header_entities[header_index] =
                    words[(0x2c4 + 4 * (header_index + 39)) / 4];
            }
            SetGameManagerSubState(game_manager, 1);
        }
        break;
    case 1:
        if ((words[0x2b4 / 4] & 1U) == 0 && words[0x68c / 4] < 8) {
            SpawnScoreDisplayText();
            ++words[0x68c / 4];
        }
        if (static_cast<i32>(words[0x2b4 / 4]) > 4)
            SetGameManagerSubState(game_manager, 2);
        break;
    case 2: {
        if ((words[0x2b4 / 4] & 1U) == 0 && words[0x68c / 4] < 8) {
            SpawnScoreDisplayText();
            ++words[0x68c / 4];
        }
        const u32 previous = cursor[0];
        cursor[1] = previous;
        if ((g_ManagerSubGateFlags & 0x10U) != 0 ||
            (g_MenuInputFlagsByte & 0x10U) != 0)
            ShiftManagerSelector(cursor, -1);
        if ((g_ManagerSubGateFlags & 0x20U) != 0 ||
            (g_MenuInputFlagsByte & 0x20U) != 0)
            ShiftManagerSelector(cursor, 1);
        if (previous != cursor[0]) {
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);
            u32 window_start = words[0x58d8 / 4];
            if (cursor[0] < window_start)
                window_start = cursor[0];
            else if (cursor[0] >= window_start + 10)
                window_start = cursor[0] - 9;
            words[0x58d8 / 4] = window_start;
            for (u32 row_index = 0; row_index < words[0x594 / 4]; ++row_index) {
                u8 *const entity = FindEntityEdxStackAbi(
                    g_MainChainRenderOwner, row_entities[row_index]);
                if (entity != 0)
                    UpdateScoreDisplayEntity(entity,
                        (row_index == cursor[0]) ? 2U : 3U);
            }
        }
        if ((g_ManagerSubGateFlags & 0xaU) != 0) {
            void *const score_records = reinterpret_cast<void *>(words[0x5aac / 4]);
            if (score_records != 0)
                ReleaseScoreDisplayRecord(score_records);
            words[0x5aac / 4] = 0;
            Call44BE70(cursor);
            for (u32 row_index = 0; row_index < words[0x594 / 4]; ++row_index)
                ReleaseManagerSlotEntity(game_manager, row_index + 173);
            for (u32 header_index = 0; header_index < 8; ++header_index)
                ReleaseManagerSlotEntity(game_manager, header_index + 39);
            SetGameManagerState(game_manager, 2);
        }
        break;
    }
    default:
        break;
    }
    return 1;
}

// TH10 0x00433570. Game-manager state-10 calculation body: replay-save
// naming. It selects one of twenty-five replay slots, edits the same
// 13-column alphabet grid used by state-F, writes the name to the replay,
// reparses its slot, and then returns to replay selection.
i32 RunManagerStateBody10(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    u32 *const replay_cursor = words + 9;
    u32 *const grid_cursor = words + (0x58f4 / 4);
    char *const name_buffer = reinterpret_cast<char *>(bytes + 0x58dc);
    const char *const alphabet = reinterpret_cast<const char *>(0x4746d8U);

    switch (words[8]) {
    case 0:
        words[0x2c / 4] = 25;
        words[0xf4 / 4] = 1;
        Call40AD20(0, replay_cursor);
        *reinterpret_cast<u32 *>(0x477848U) = 0x474908U;
        *reinterpret_cast<u32 *>(0x474c7cU) = 8;
        *reinterpret_cast<u32 *>(0x474c80U) = 8;
        for (u32 replay_index = 1; replay_index <= 25; ++replay_index) {
            char replay_name[32];
            sprintf(replay_name, "th10_%.2d.rpy", replay_index);
            words[(0x59e4 + 4 * (replay_index - 1)) / 4] =
                reinterpret_cast<u32>(ParseDemoRecord(replay_name));
        }
        SpawnManagerEntityFromScript(game_manager, 0x69);
        SetGameManagerSubState(game_manager, 1);
        break;
    case 1:
        if (static_cast<i32>(words[0x2b4 / 4]) > 6)
            SetGameManagerSubState(game_manager, 2);
        break;
    case 2:
        replay_cursor[1] = replay_cursor[0];
        if ((g_ManagerSubGateFlags & 0x10U) != 0 ||
            (g_MenuInputFlagsByte & 0x10U) != 0)
            ShiftManagerSelector(replay_cursor, -1);
        if ((g_ManagerSubGateFlags & 0x20U) != 0 ||
            (g_MenuInputFlagsByte & 0x20U) != 0)
            ShiftManagerSelector(replay_cursor, 1);
        if (replay_cursor[1] != replay_cursor[0])
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);
        if ((g_ManagerSubGateFlags & 0xaU) != 0) {
            SetGameManagerSubState(game_manager, 4);
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xb, 0);
        } else if ((g_ManagerSubGateFlags & 0x1001U) != 0) {
            words[0x59c4 / 4] = replay_cursor[0];
            Call40AD20(0, grid_cursor);
            words[0x590c / 4] = static_cast<u32>(strlen(alphabet));
            strncpy(name_buffer,
                    reinterpret_cast<const char *>(0x47783cU + 120952), 8);
            name_buffer[8] = '\0';
            words[0x58f0 / 4] = static_cast<u32>(strlen(name_buffer));
            SetGameManagerSubState(game_manager, 3);
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xa, 0);
        }
        break;
    case 3: {
        grid_cursor[1] = grid_cursor[0];
        if ((g_ManagerSubGateFlags & 0x10U) != 0 ||
            (g_MenuInputFlagsByte & 0x10U) != 0)
            ShiftManagerSelector(grid_cursor, -13);
        if ((g_ManagerSubGateFlags & 0x20U) != 0 ||
            (g_MenuInputFlagsByte & 0x20U) != 0)
            ShiftManagerSelector(grid_cursor, 13);
        if ((g_ManagerSubGateFlags & 0x40U) != 0 ||
            (g_MenuInputFlagsByte & 0x40U) != 0)
            ShiftManagerSelector(grid_cursor,
                (grid_cursor[0] % 13 != 0) ? -1 : 12);
        if ((g_ManagerSubGateFlags & 0x80U) != 0 ||
            (g_MenuInputFlagsByte & 0x80U) != 0)
            ShiftManagerSelector(grid_cursor,
                (grid_cursor[0] % 13 == 12) ? -12 : 1);
        if (grid_cursor[1] != grid_cursor[0])
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xc, 0);

        const u32 alphabet_length = static_cast<u32>(strlen(alphabet));
        const u32 selection = grid_cursor[0];
        u32 name_length = words[0x58f0 / 4];
        if ((g_ManagerSubGateFlags & 0x1001U) != 0) {
            if (selection < alphabet_length - 3 && name_length < 8) {
                name_buffer[name_length++] = alphabet[selection];
                name_buffer[name_length] = '\0';
            } else if (selection == alphabet_length - 3 && name_length < 8) {
                name_buffer[name_length++] = ' ';
                name_buffer[name_length] = '\0';
            } else if (selection == alphabet_length - 2 && name_length != 0) {
                name_buffer[--name_length] = '\0';
            } else if (selection == alphabet_length - 1) {
                char replay_name[32];
                sprintf(replay_name, "th10_%.2d.rpy", words[0x59c4 / 4] + 1);
                ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0x2c, 0);
                SaveReplayNameInput();
                // TH10 0x00429b60 (native ECX = DAT_00477838 game-mode
                // object, EDX = file name, stack = player name).
                CommitReplaySave(g_GameModeObject, replay_name, name_buffer);
                DestroyDemoParseObject(reinterpret_cast<void *>(
                    words[(0x59e4 + 4 * words[0x59c4 / 4]) / 4]));
                words[(0x59e4 + 4 * words[0x59c4 / 4]) / 4] =
                    reinterpret_cast<u32>(ParseDemoRecord(replay_name));
                SetGameManagerSubState(game_manager, 2);
            }
            words[0x58f0 / 4] = name_length;
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xa, 0);
        }
        if ((g_ManagerSubGateFlags & 0xaU) != 0) {
            if (name_length != 0) {
                name_buffer[--name_length] = '\0';
                words[0x58f0 / 4] = name_length;
                ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xb, 0);
            } else {
                SetGameManagerSubState(game_manager, 2);
            }
        }
        break;
    }
    case 4:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 6) {
            ReleaseManagerSlotEntity(game_manager, 0x69);
            SetManagerSlotEntityStopWord(game_manager, 0x5a, 8);
            SetManagerSlotEntityStopWord(game_manager, 0x5b, 8);
            ReleaseManagerSlotEntity(game_manager, 0x5e);
            ReleaseHandleTargetByPointer(bytes + 0x5d0);
            SetGameManagerState(game_manager, 2);
            Call44BE70(replay_cursor);
            StartBgmQueue(0, "bgm/th10_02.wav");
            ResetBgmQueue(0, 0);
            for (u32 replay_index = 0; replay_index < 25; ++replay_index) {
                void *const replay = reinterpret_cast<void *>(
                    words[(0x59e4 + 4 * replay_index) / 4]);
                if (replay != 0)
                    DestroyDemoParseObject(replay);
                words[(0x59e4 + 4 * replay_index) / 4] = 0;
            }
        }
        break;
    default:
        break;
    }
    return 1;
}

// TH10 0x00430ff0. Game-manager state-9 calculation body: the six-row stage
// select reached after the difficulty menu (extra / practice paths).  The
// cursor record at manager + 0x24 is reused; row scripts 106 and
// DAT_00474c68 + 107 are spawned.  Confirm gates on the per-record clear
// flag at DAT_00477c3c[0x437c*(3*DAT_00474c68 + DAT_00474c6c) +
// 0x30*DAT_00474c74 + 8*cursor + 0x4e9], queues the feedback channel, then
// scans the nine clear bytes of the active save bank (DAT_00477c3c + 0x24d6a
// or the alternate bank) into DAT_00474cac.
i32 RunManagerStateBody9(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    const u32 stage_script = g_StageTextSprites[0];
    const u32 difficulty = g_StageTextSprites[1];
    const u32 selector = g_StageScoreSelector[0];

    switch (words[0x20 / 4]) {
    case 0: {
        const u32 maximum = 6;
        words[0x2c / 4] = maximum;
        const i32 saved = static_cast<i32>(g_StageSelectMemory);
        if (maximum != 0) {
            words[0x24 / 4] = (saved < static_cast<i32>(maximum))
                ? ((saved < 0) ? 0U : static_cast<u32>(saved))
                : maximum - 1;
        } else {
            words[0x24 / 4] = static_cast<u32>(saved);
        }
        SpawnManagerEntityFromScript(game_manager, 0x6a);
        SpawnManagerEntityFromScript(game_manager, stage_script + 0x6b);
        SetGameManagerSubState(game_manager, 1);
        if (static_cast<i32>(words[0x2b4 / 4]) > 10)
            SetGameManagerSubState(game_manager, 2);
        break;
    }
    case 1:
        if (static_cast<i32>(words[0x2b4 / 4]) > 10)
            SetGameManagerSubState(game_manager, 2);
        break;
    case 2: {
        words[0x28 / 4] = words[0x24 / 4];
        if ((g_ManagerSubGateFlags & 0x10U) != 0 ||
            (g_MenuInputFlagsByte & 0x10U) != 0)
            ShiftManagerSelector(bytes + 0x24, -1);
        if ((g_ManagerSubGateFlags & 0x20U) != 0 ||
            (g_MenuInputFlagsByte & 0x20U) != 0)
            ShiftManagerSelector(bytes + 0x24, 1);
        if (words[0x28 / 4] != words[0x24 / 4])
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0xc, 0);
        if ((g_ManagerSubGateFlags & 0xaU) != 0) {
            SetGameManagerSubState(game_manager, 4);
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0xb, 0);
            g_StageSelectMemory = words[0x24 / 4];
            return 1;
        }
        if ((g_ManagerSubGateFlags & 0x1001U) == 0)
            return 1;

        const u32 cursor = words[0x24 / 4];
        const u8 *const player = reinterpret_cast<const u8 *>(0x47783cU);
        const u32 combo_index = 3 * stage_script + difficulty;
        const u8 clear_flag = player[0x437c * combo_index + 8 * cursor +
                                     0x30 * selector + 0x4e9];
        if (clear_flag != 0) {
            SetGameManagerSubState(game_manager, 3);
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0xa, 0);
        } else {
            ReserveContextChannel(
                reinterpret_cast<void *>(0x00492590), 0x25, 0);
        }
        g_StageSelectMemory = cursor;
        g_ClearFlagScan = 0;
        const u8 *const clear_flags = (Call44B010() != 0)
            ? reinterpret_cast<const u8 *>(0x497aaaU)
            : reinterpret_cast<const u8 *>(0x497ad9U);
        const signed char *const signed_flags =
            reinterpret_cast<const signed char *>(clear_flags);
        u32 index = 0;
        for (; index < 8; ++index) {
            if (signed_flags[index] < 0) {
                g_ClearFlagScan = index + 1;
                return 1;
            }
        }
        if (signed_flags[8] < 0)
            g_ClearFlagScan = 9;
        return 1;
    }
    case 3:
        if (static_cast<i32>(words[0x2b4 / 4]) == 10) {
            Call40C540(480.0f, 392.0f);
            Call43C8B0(5, 0x20, 0, 0, 0);
        }
        if (static_cast<i32>(words[0x2b4 / 4]) < 40)
            return 1;
        RunManagerCursorHandle(bytes + 0x24);
        SetGameManagerState(game_manager, 3);
        {
            const u32 next_stage = words[0x24 / 4] + 1;
            *reinterpret_cast<u32 *>(0x477848U) =
                0x474788U + 0x30 * next_stage;
            *reinterpret_cast<u32 *>(0x474c7cU) = next_stage;
            *reinterpret_cast<u32 *>(0x474c80U) = next_stage;
            Call420C30(6.0f);
            g_SharedStatusGate = 7;
        }
        return 1;
    case 4:
        if (static_cast<i32>(words[0x2b4 / 4]) >= 6) {
            ReleaseManagerSlotEntity(game_manager, 0x6a);
            ReleaseManagerSlotEntity(game_manager, stage_script + 0x6b);
            SetGameManagerState(game_manager, 8);
            Call44BE70(bytes + 0x24);
        }
        break;
    default:
        break;
    }
    return 1;
}

} // namespace th10
