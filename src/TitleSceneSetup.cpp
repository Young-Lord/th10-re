// Title -> game-scene startup orchestration (TH10 0x00417870). Called by the
// title callback worker (0x00417c70) with the published title-screen state.
// The manager-creation leaves (0x414830 / 0x425020 / 0x406060) are the
// semantic bodies in ManagerCreation.cpp; the scheduler record registration
// uses the CallbackSchedulerApi modeling of 0x00449ed0/0x00449ae0/0x00449b70
// (calc priority 10, draw priority 4, both records created disabled and
// carrying the title state as their argument). Remaining mode-worker leaves
// stay explicit boundaries below. The native body uses forward jumps into
// one shared failure tail; this reconstruction keeps the tail linear by
// funneling every failure through the `failed` flag in source order.
#include "TitleSceneSetup.hpp"

#include "CallbackScheduler.hpp"
#include "GameManagerGateVms.hpp"
#include "ManagerCreation.hpp"
#include "ReplaySceneReuse.hpp"
#include "Th10Platform.hpp"
#include "Th10Types.hpp"
#include "TitleScreenObject.hpp"

namespace th10 {

namespace {

// ---- shared globals ----------------------------------------------------

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10 (manager object)
extern u32 g_InputModeFlags; // TH10 DAT_00491ff4 (bit 0x80 = abort gate)
extern u32 g_GlobalModeFlags; // TH10 DAT_00474ca0
extern u32 g_CurrentDifficulty; // TH10 DAT_00474c74
extern u32 g_SceneModeSelector; // TH10 DAT_00474c7c (7 = spell practice)
extern u32 g_SceneStageIndexA; // TH10 DAT_00474c68
extern u32 g_SceneStageIndexB; // TH10 DAT_00474c6c
extern void *g_ScoreSaveState; // TH10 DAT_0047783c
extern u32 g_HighScoreValue; // TH10 DAT_00474c40
extern u32 g_SceneSubTimer; // TH10 DAT_00474c44
extern u32 g_PracticeScoreSeed; // TH10 DAT_00474c90
extern u32 g_SceneVoiceId; // TH10 DAT_00474c94
extern u32 g_ScenePlayCountSeed; // TH10 DAT_00474c9c
extern u32 g_SceneRankValue; // TH10 DAT_00474c98
extern u32 g_SceneFlag84; // TH10 DAT_00474c84
extern u32 g_SceneFlag88; // TH10 DAT_00474c88
extern u32 g_SceneFlag8c; // TH10 DAT_00474c8c
extern u32 g_SceneWord48; // TH10 DAT_00474c48 (u16)
extern u32 g_SceneSelector50; // TH10 DAT_00474c50
extern u32 g_PracticeStartIndex; // TH10 DAT_00474c70
extern u32 g_PracticeStartRequest; // TH10 DAT_00474cac
extern float g_SceneFadeScale; // TH10 DAT_00476f78
extern u32 g_GameActiveFlag; // TH10 DAT_00491fc4
extern void *g_PublishedModeRecord; // TH10 DAT_00477848
extern void *g_GameModeObject; // TH10 DAT_00477838 (replay context pointer)
extern u32 g_TitleStateDefaults[13]; // TH10 DAT_00491d48 (52 bytes)
extern void *g_AsciiHudOwner; // TH10 DAT_0047770c
extern u8 g_SceneNameBuffer; // TH10 DAT_00477710 (byte buffer)
extern u8 g_SceneTimeSource; // TH10 DAT_00477708 (doubles at +36/+44)
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern void *g_GameManagerSlot; // TH10 DAT_00491c28
extern u32 g_ManagerGateReady; // TH10 DAT_00492264
extern u32 g_ManagerGateRun; // TH10 DAT_00492260
extern u32 g_ModeWorkerBusy; // TH10 DAT_00494518
extern u32 g_SceneWord4918a4; // TH10 DAT_004918a4

// ---- boundary leaves (native register ABIs noted at each site) ----------

extern void ResetMainChainFrameStateBlock(u32 value); // TH10 0x00418b80
extern i32 TH10_FASTCALL TitleScreenCalcCallback(void *title_screen); // TH10 0x004187c0
extern i32 TH10_FASTCALL TitleScreenDrawCallback(void *title_screen); // TH10 0x004187d0
extern i32 OpenSceneScriptResource(void *scene_owner); // TH10 0x00413a20
// (semantic body in StageScriptOpen.cpp; the earlier
// "ReleaseAsciiHudOwnerRecords" reading was wrong — the native opens the
// mode record's stage script into the scene owner, return ignored here)
extern i32 OpenModeRecordBank(void *record, u32 flag); // TH10 0x00402640
extern i32 CopyReplayStageName(u32 slot, void *name_buffer); // TH10 0x00429610
extern i32 RunModeWorkerSetupA(); // TH10 0x0041aed0
extern i32 RunModeWorkerSetupB(); // TH10 0x0041c290
extern i32 RunModeWorkerSetupC(); // TH10 0x00422360
extern i32 RunModeWorkerSetupD(); // TH10 0x00419090
extern i32 RunModeWorkerFinish(); // TH10 0x0042b660
extern void EnterGameManagerGate(void *slot); // TH10 0x00421300
extern void LeaveGameManagerGate(void *slot); // TH10 0x00421070
// (both now resolve to the semantic bodies EnterGameManagerGateStackAbi /
// LeaveGameManagerGateStackAbi in GameManagerGateVms.cpp — they stop the
// three 0x491c28-slot background VMs, not just a gate toggle)
extern void RunSceneIdleKick(); // TH10 0x00417800
extern i32 LoadStageEnemyDefinition(void *record); // TH10 0x0040d6b0
extern i32 RunSceneAudioSetup(); // TH10 0x0040af90
extern i32 RunSceneEffectSetup(); // TH10 0x004056b0
extern i32 RunSceneTextSetup(); // TH10 0x00408c90
extern void RunModeStopGate(); // TH10 0x00420c00
extern void StartBgmQueue(u32 channel, const char *name); // TH10 0x00420a90
extern void ResetSceneTimer(u32 value); // TH10 0x00405410
extern void *EnableManagerSchedulerRecordsEaxAbi(void *manager); // TH10 0x00409e20
extern void SleepMilliseconds(u32 milliseconds); // Win32 Sleep boundary

} // namespace

// TH10 0x00417870. Native stdcall with one stack argument.
int SetupGameSceneFromTitle(void *title_state)
{
    TitleScreen &ts = *reinterpret_cast<TitleScreen *>(title_state);

    ts.flags |= 4U; // +0x58 scene-setup busy flag

    // Wait until the render owner reports the handover: loop while either
    // of its first two signed words is still non-negative. The 0x491ff4
    // bit 0x80 aborts the wait into the failure tail.
    bool failed = false;
    i32 *const owner = static_cast<i32 *>(g_MainChainRenderOwner);
    for (;;) {
        if ((g_InputModeFlags & 0x80U) != 0U) {
            failed = true;
            break;
        }
        if (owner[0] < 0 && owner[1] < 0)
            break;
        SleepMilliseconds(1U);
    }

    if (!failed) {
        g_SceneFadeScale = 1.0f;
        g_SceneFlag88 = 0U;
        g_SceneFlag8c = 0U;

        if (g_GameActiveFlag != 0U) {
            i32 difficulty_block = static_cast<i32>(g_CurrentDifficulty);
            if (g_SceneModeSelector == 7U) {
                difficulty_block = 4;
                g_CurrentDifficulty = 4U;
            }
            const u32 row_offset = 240U * static_cast<u32>(difficulty_block);
            u8 *const save = static_cast<u8 *>(g_ScoreSaveState);
            // 34552 * A + 17276 * (A + B): the stage-record stride layout
            // of the save bank.
            u8 *const stage_row = save + 34552U * g_SceneStageIndexA
                + 17276U * g_SceneStageIndexA
                + 17276U * g_SceneStageIndexB;
            g_HighScoreValue =
                *reinterpret_cast<u32 *>(stage_row + row_offset + 24U);
            g_SceneVoiceId = stage_row[row_offset + 29U];
            if ((g_GlobalModeFlags & 8U) == 0U)
                g_PracticeScoreSeed = 0U;
            g_SceneSubTimer = 0U;
            ResetMainChainFrameStateBlock(50000U);
            if ((g_GlobalModeFlags & 0x10U) != 0U) {
                if (g_PracticeStartRequest != 0U)
                    g_PracticeStartIndex = g_PracticeStartRequest - 1U;
                else
                    g_PracticeStartIndex = 9U;
            } else {
                g_PracticeStartIndex = 2U;
            }
            g_SceneWord48 = (g_SceneModeSelector != 1U) ? 0x50U : 0U;
            g_GlobalModeFlags &= ~4U;
            if (ts.mode == 0U) {
                u8 *const play_count_row = save
                    + 17276U * (g_SceneStageIndexA + g_SceneStageIndexB
                                + 2U * g_SceneStageIndexA);
                u32 play_count =
                    *reinterpret_cast<u32 *>(play_count_row + 1224U);
                if (play_count < 99999U) {
                    ++play_count;
                    *reinterpret_cast<u32 *>(play_count_row + 1224U) =
                        play_count;
                }
            }
            g_ScenePlayCountSeed = 0U;
            g_SceneRankValue =
                (g_GlobalModeFlags & 8U) != 0U ? 0xFFFFFE00U : 0U;
        } else if (g_SceneSubTimer > g_HighScoreValue) {
            g_HighScoreValue = g_SceneSubTimer;
        }

        g_SceneSelector50 = 9U;

        // Register the scene calc record (priority 10) and draw record
        // (priority 4); both are created with the enabled bit cleared and
        // the title state as their callback argument.
        ChainElem *calc =
            CallbackSchedulerApi::Create(TitleScreenCalcCallback);
        calc->flags &= ~ChainElemFlag_Enabled;
        calc->arg = title_state;
        CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler,
                                                    calc, 10);
        ts.calc_element = calc;

        ChainElem *draw =
            CallbackSchedulerApi::Create(TitleScreenDrawCallback);
        draw->flags &= ~ChainElemFlag_Enabled;
        draw->arg = title_state;
        CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw, 4);
        ts.draw_element = draw;

        // 52-byte manager-state defaults from DAT_00491d48 into +0x24, and
        // the published mode record pointer into +0x04.
        const u8 *const defaults =
            reinterpret_cast<const u8 *>(g_TitleStateDefaults);
        u8 *const target = ts.sub_object;
        for (u32 i = 0; i < 52U; ++i)
            target[i] = defaults[i];
        ts.mode_record = *static_cast<void **>(g_PublishedModeRecord);

        // Scene build: the replay/continue reuse path (mode flags bit 1)
        // skips the manager creation cascade.
        i32 stage_open = 0;
        if ((g_GlobalModeFlags & 2U) != 0U) {
            PrepareReplaySceneReuse(g_GameModeObject);
            (void)OpenSceneScriptResource(g_AsciiHudOwner);
            stage_open = OpenModeRecordBank(
                *reinterpret_cast<void **>(
                    static_cast<u8 *>(g_PublishedModeRecord) + 4),
                0U);
        } else if (CopyReplayStageName(ts.mode, &g_SceneNameBuffer) != 0
                   && OpenModeRecordBank(
                          *reinterpret_cast<void **>(
                              static_cast<u8 *>(g_PublishedModeRecord) + 4),
                          0U) != 0
                   && CreateAsciiHudOwner() != 0
                   && CreatePlayerStateBlock() != 0
                   && CreateEffectManagerRoot() != 0
                   && RunModeWorkerSetupA() != 0
                   && RunModeWorkerSetupB() != 0
                   && RunModeWorkerSetupC() != 0
                   && RunModeWorkerSetupD() != 0) {
            stage_open = RunModeWorkerFinish();
        }

        if (stage_open == 0) {
            failed = true;
        } else if ((g_GlobalModeFlags & 9U) != 0U) {
            RunSceneIdleKick();
        } else if (LoadStageEnemyDefinition(
                       *reinterpret_cast<void **>(
                           static_cast<u8 *>(g_PublishedModeRecord) + 12))
                   == 0) {
            failed = true;
        }

        if (!failed
            && (RunSceneAudioSetup() == 0 || RunSceneEffectSetup() == 0
                || RunSceneTextSetup() == 0))
            failed = true;

        if (!failed) {
            if ((g_GlobalModeFlags & 0x20U) == 0U) {
                RunModeStopGate();
                StartBgmQueue(
                    0U,
                    *reinterpret_cast<const char **>(
                        static_cast<u8 *>(g_PublishedModeRecord) + 16));
                StartBgmQueue(
                    1U,
                    *reinterpret_cast<const char **>(
                        static_cast<u8 *>(g_PublishedModeRecord) + 20));
            }

            *reinterpret_cast<double *>(
                &g_SceneTimeSource + 44) = 0.0;
            *reinterpret_cast<double *>(
                &g_SceneTimeSource + 36) = 0.0;
            ResetSceneTimer(0U);
            while (g_ModeWorkerBusy != 0U)
                SleepMilliseconds(16U);

            if (g_SceneFlag84 != 0U)
                g_SceneFlag8c = 0U;
            g_SceneFlag84 = 0U;
        }
    }

    if (failed) {
        // Native failure tail (0x00417b3e..0x00417b7e).
        ts.flags |= 8U;
        EnterGameManagerGateStackAbi(&g_GameManagerSlot);
        g_ManagerGateReady = 0U;
        g_ManagerGateRun = 1U;
        if (ts.calc_element != 0)
            ts.calc_element->flags |= ChainElemFlag_Enabled;
        if (ts.draw_element != 0)
            ts.draw_element->flags |= ChainElemFlag_Enabled;
        return -1;
    }

    // Native success tail (0x00417c2b..0x00417b77).
    LeaveGameManagerGateStackAbi(&g_GameManagerSlot);
    ts.flags &= ~4U;
    g_GlobalModeFlags &= 0xFFFFFFF4U; // clear bits 0x4 and 0x8
    g_ManagerGateReady = 0U;
    g_ManagerGateRun = 1U;
    g_SceneWord4918a4 = 0U;
    EnableManagerSchedulerRecordsEaxAbi(g_MainChainRenderOwner);
    return 0;
}

} // namespace th10
