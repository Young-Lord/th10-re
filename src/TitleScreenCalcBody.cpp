// TH10 0x00418190. Title-screen calculation-record body (the target of the
// 0x004187c0 ECX forwarder). Semantics are described in the header; every
// fixed global address, struct offset and flag bit below is taken from the
// native disassembly at 0x418190..0x418795.
#include <string.h>

#include "AsciiHudOverlayUpdate.hpp"
#include "AsciiHudOwner.hpp"
#include "AsciiOverlayFactory.hpp"
#include "BgmRuntime.hpp"
#include "EclScriptLibrary.hpp"
#include "EntityHelpers.hpp"
#include "MainChainRuntime.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "PlayerOptionRecords.hpp"
#include "PlayerTimerHelpers.hpp"
#include "Th10Platform.hpp"
#include "TimelineAudioActions.hpp"
#include "Th10Types.hpp"
#include "TitleScreenObject.hpp"
#include "ThreadControl.hpp"
#include "TitleCalcCluster.hpp"
#include "TitleScreenCalcBody.hpp"
#include "TitleScoreAnimTriggers.hpp"

namespace th10 {

namespace {

// --- Shared globals -----------------------------------------------------
extern void *g_TitleScreenStatePrimary;   // TH10 DAT_004776e4
extern void *g_TitleScreenStateSecondary; // TH10 DAT_004776e8
extern void *g_AsciiManagerHost;          // TH10 DAT_004776e0
extern void *g_AsciiHudConditionalState;  // TH10 DAT_00477704
extern void *g_GameStateManager;          // TH10 DAT_00477830
extern u32 g_SharedStatusGate;            // TH10 DAT_00491fb8
extern u32 g_GlobalModeFlags;             // TH10 DAT_00474ca0
extern u32 g_MainChainRuntimeOptions;     // TH10 DAT_00491d78
extern ThreadControl g_MainChainSecondaryControl; // TH10 DAT_00492254
extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590

// Native operator delete (TH10 0x004524a1 = j__free); the epilogue releases
// the primary title state with it.
extern void FreeMainChainObject(void *object); // TH10 0x004524a1

// --- Boundary set -------------------------------------------------------
// The remaining callees keep their native responsibilities as thin
// boundaries; register inputs are passed as explicit arguments.
extern void CleanupEffectManagerEsiBoundary(void *manager); // TH10 0x00405ed0, native ESI = manager
extern void DestroyTitleScreenStateBufferInPlace(void *object); // TH10 0x00402440 (TitleGameManagerLifecycle.cpp)

u32 LoadU32From(const void *address)
{
    return *static_cast<const u32 *>(address);
}

u32 LoadU32At(const void *base, u32 offset)
{
    return LoadU32From(static_cast<const u8 *const>(base) + offset);
}

void MarkSchedulerRecords(void *manager)
{
    // Sets bit 1 of the word at record+4 for the two scheduler records at
    // manager+8 and manager+0xc, skipping absent records.
    u8 *const base = static_cast<u8 *>(manager);
    for (u32 slot = 8U; slot <= 0xcU; slot += 4U) {
        const void *record =
            reinterpret_cast<const void *>(LoadU32At(base, slot));
        if (record != 0) {
            u32 *const flags =
                reinterpret_cast<u32 *>(LoadU32At(base, slot) + 4U);
            *flags |= 2U;
        }
    }
}

void MarkSchedulerRecordsUnchecked(void *manager)
{
    // Twin used for DAT_00477834 / DAT_004776fc / DAT_00477840: the native
    // dereferences both record slots without the null guard.
    u8 *const base = static_cast<u8 *>(manager);
    for (u32 slot = 8U; slot <= 0xcU; slot += 4U) {
        u32 *const flags =
            reinterpret_cast<u32 *>(LoadU32At(base, slot) + 4U);
        *flags |= 2U;
    }
}

// The shared game-start reset: manager cleanup pair, the 0x21cea0-byte wipe
// of the bullet manager payload, HUD conditional-state release, the
// 0x47781c reset, counter clears, 0x42a450, "main" ECL script creation, HUD
// overlay re-arm and the scheduler-record flag sweep.
void RunGameStartReset(u32 *descriptor)
{
    CleanupEffectManagerEsiBoundary(
        *reinterpret_cast<void **>(0x4776F0U)); // TH10 DAT_004776f0
    ResetOptionPositionRecordsEsiAbi(
        *reinterpret_cast<void **>(0x477834U)); // TH10 DAT_00477834

    void *const bullet_manager = *reinterpret_cast<void **>(0x477818U);
    memset(static_cast<u8 *>(bullet_manager) + 0x14, 0, 0x21CEA0U);

    ReleaseAsciiHudConditionalState(g_AsciiHudConditionalState);
    ReleaseOwnerRecordChainEaxAbi(
        *reinterpret_cast<void **>(0x47781CU)); // TH10 DAT_00477781c

    *reinterpret_cast<u32 *>(0x474C88U) = 0; // TH10 DAT_00474c88
    *reinterpret_cast<u32 *>(0x474C8CU) = 0; // TH10 DAT_00474c8c
    // Native EBX = DAT_00477838 (the game-mode object).
    ApplyOptionPositionStateEbxAbi(
        *reinterpret_cast<void **>(0x477838U));

    CreateEclScriptObjectEaxStackAbi(
        reinterpret_cast<const u32 *>(descriptor),
        g_AsciiHudConditionalState,
        static_cast<i32>(reinterpret_cast<u32>("main")));

    ResetAsciiHudOverlayEdiAbi(*reinterpret_cast<void **>(0x47770CU));

    MarkSchedulerRecords(g_GameStateManager);            // DAT_00477830
    MarkSchedulerRecordsUnchecked(                       // DAT_00477834
        *reinterpret_cast<void **>(0x477834U));
    RebuildPlayerOptionRecords(*reinterpret_cast<void **>(0x477834U));
    MarkSchedulerRecords(*reinterpret_cast<void **>(0x4776F0U));
    MarkSchedulerRecords(g_AsciiHudConditionalState);    // DAT_00477704
    MarkSchedulerRecords(*reinterpret_cast<void **>(0x477818U));
    MarkSchedulerRecords(*reinterpret_cast<void **>(0x47781CU));
    MarkSchedulerRecordsUnchecked(                       // DAT_004776fc
        *reinterpret_cast<void **>(0x4776FCU));
    MarkSchedulerRecords(*reinterpret_cast<void **>(0x4776ECU));
    MarkSchedulerRecordsUnchecked(                       // DAT_00477840
        *reinterpret_cast<void **>(0x477840U));
    MarkSchedulerRecords(*reinterpret_cast<void **>(0x4776F4U));
    // DAT_004776f4 also has an unchecked third record slot at +0x37ac.
    {
        u8 *const base = static_cast<u8 *>(
            *reinterpret_cast<void **>(0x4776F4U));
        u32 *const flags =
            reinterpret_cast<u32 *>(LoadU32At(base, 0x37ACU) + 4U);
        *flags |= 2U;
    }
    MarkSchedulerRecords(*reinterpret_cast<void **>(0x477814U));
}

} // namespace

i32 TH10_STDCALL RunTitleScreenCalcBodyStackAbi(void *title_screen)
{
    TitleScreen &ts = *reinterpret_cast<TitleScreen *>(title_screen);
    const u32 frame = static_cast<u32>(ts.timer.count);

    if (frame == 0U) {
        if ((ts.flags & 8U) != 0U) {
            // Shutdown frame: stop the secondary worker and publish the
            // shared-status gate ((~(DAT_00491ff4 >> 12) & 1) | 2).
            StopThreadControl(&g_MainChainSecondaryControl);
            g_SharedStatusGate =
                ((~(LoadU32From(reinterpret_cast<const void *>(0x491FF4U)) >>
                    12)) & 1U) | 2U;
            return 1;
        }

        InitializeTitleSecondaryStateStackAbi(g_TitleScreenStateSecondary);
        if (g_TitleScreenStatePrimary != 0) {            // Title screen still alive: arm both score anims, latch the
            // 0x800 flag and expire the HUD overlay handle at +0x9e14.
            TriggerTitleScoreAnim30EsiAbi(g_TitleScreenStatePrimary);
            TriggerTitleScoreAnim60EaxAbi(g_TitleScreenStateSecondary);
            ts.flags |= 0x800U;
            AsciiHudOwner &hud = *reinterpret_cast<AsciiHudOwner *>(
                *reinterpret_cast<void **>(0x47770CU));
            ExpireEntityHandleEaxAbi(&hud.first_banner_handle);
        } else {
            // First frame without the title state: run the game-start reset.
            ts.flags &= ~0x800U;
            u32 descriptor[16];
            for (u32 i = 0; i < 16U; ++i)
                descriptor[i] = 0U;
            RunGameStartReset(descriptor);

            if ((g_GlobalModeFlags & 0x20U) == 0U) {
                void *const mode_record =
                    *reinterpret_cast<void **>(0x477848U);
                SelectTimelineAudioMode(
                    0, static_cast<i32>(LoadU32At(mode_record, 0x24U)));
            }

            u8 *const host = static_cast<u8 *>(g_AsciiManagerHost);
            ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(host + 0x89A0U));
            ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(host + 0x89A4U));
            *reinterpret_cast<u32 *>(host + 0x89A4U) = 0;
        }
    } else if (frame == 30U) {
        // Restart frame, gated on the 0x800 flag.
        if ((ts.flags & 0x800U) != 0U) {
            ts.flags &= ~0x800U;
            u32 descriptor[16];
            for (u32 i = 0; i < 16U; ++i)
                descriptor[i] = 0U;
            RunGameStartReset(descriptor);

            if ((g_MainChainRuntimeOptions & 0x10U) != 0U) {
                QueueBgmCommand(&g_TransitionRoot, "dummy", 4, 0);
            } else {
                QueueBgmCommand(&g_TransitionRoot, "dummy", 3, 0);
            }
            {
                void *const mode_record =
                    *reinterpret_cast<void **>(0x477848U);
                SelectTimelineAudioMode(
                    0, static_cast<i32>(LoadU32At(mode_record, 0x24U)));
            }

            u8 *const host = static_cast<u8 *>(g_AsciiManagerHost);
            ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(host + 0x89A4U));
            *reinterpret_cast<u32 *>(host + 0x89A4U) = 0;
            TickPlayerTimerEaxStackAbi(&ts.timer, 0);
        }
    }

    // --- Common epilogue -------------------------------------------------
    // Free the primary title state when its +0x2a18 block flag bit 3 is set
    // (the native repeats the null check; behavior is identical).
    if (g_TitleScreenStatePrimary != 0 &&
        ((*reinterpret_cast<const u8 *>(g_TitleScreenStatePrimary) +
          0x2A18U) & 8U) != 0U &&
        g_TitleScreenStatePrimary != 0) {
        DestroyTitleScreenStateBufferInPlace(g_TitleScreenStatePrimary);
        FreeMainChainObject(g_TitleScreenStatePrimary);
    }

    if ((ts.flags & 4U) != 0U) {
        ts.flags |= 0x80U;
        return 1;
    }

    StopThreadControl(&g_MainChainSecondaryControl);

    if ((g_GlobalModeFlags & 0x20U) != 0U) {
        const u32 present_options =
            LoadU32From(reinterpret_cast<const void *>(0x474E30U));
        if ((present_options & 0x160BU) != 0U ||
            (ts.flags & 0x70U) != 0U) {
            // DAT_00491fb8 = (DAT_00491ff4 & 0x1000) ? 2 : 4.
            g_SharedStatusGate =
                (LoadU32From(reinterpret_cast<const void *>(0x491FF4U)) &
                 0x1000U) != 0U ? 2 : 4;
        }
        if (frame == 2940U) {
            // Native pushes (5, 0x3c, 0, 0, 0) with EBX = 0x2b priority.
            CreateAsciiOverlayContext(5U, 60U, 0U, 0U, 0U, 0x2bU);
        } else if (frame == 3000U) {
            RequestGameStateTransitionEaxStackAbi(
                reinterpret_cast<void *>(0x491C28U), 4);
        }
    }

    UpdateInGameScoreDisplayEsiAbi(*reinterpret_cast<void **>(0x47770CU));

    if ((ts.flags & (0x10U | 0x20U | 0x40U)) != 0U) {
        return 3;
    }

    // Play-time accumulator: record selected by (3 * DAT_00474c68 +
    // DAT_00474c6c) * 0x437c into the DAT_00477783c table at +0x4cc, capped
    // below 215999999 (signed compare). Skipped while the game-mode object
    // reports mode 1. The native dereferences DAT_00477838 without a null
    // check; preserved.
    if (LoadU32At(*reinterpret_cast<void **>(0x477838U), 0x10U) != 1U) {
        const u32 stage = LoadU32From(reinterpret_cast<const void *>(0x474C68U));
        const u32 floor =
            LoadU32From(reinterpret_cast<const void *>(0x474C6CU));
        const u32 record_offset = (floor + 2U * stage + stage) * 0x437CU;
        u8 *const table = static_cast<u8 *>(
            *reinterpret_cast<void **>(0x47783CU));
        u32 *const counter = reinterpret_cast<u32 *>(table + record_offset +
                                                     0x4CCU);
        if (static_cast<i32>(*counter) < 215999999) {
            *counter = *counter + 1U;
        }
    }

    // Spell-practice gate: with the HUD conditional-state mode cleared, the
    // HUD overlay owner idle and at least 90 frames elapsed, run 0x418a00.
    if (LoadU32At(g_AsciiHudConditionalState, 0x10U) == 0U &&
        reinterpret_cast<AsciiHudOwner *>(
            *reinterpret_cast<void **>(0x47770CU))
                ->result_script_state == 0 &&
        static_cast<i32>(frame) >= 0x5A) {
        TickTitleFrameStateEaxAbi(reinterpret_cast<void *>(0x474C40U));
    }

    ++*reinterpret_cast<u32 *>(0x474C88U);
    ++*reinterpret_cast<u32 *>(0x474C8CU);
    TickTimerForwardEsiAbi(&ts.timer);
    return 1;
}

} // namespace th10
