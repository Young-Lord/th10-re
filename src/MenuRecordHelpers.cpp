#include "MenuRecordHelpers.hpp"

#include "GameModeTeardown.hpp"
#include "GameStateManagerObject.hpp"
#include "PauseEnterSetup.hpp"
#include "PauseMenuModes.hpp"
#include "PostRunReplaySaveMenu.hpp"
#include "TimelineRenderObjects.hpp"
#include "TitleScreenObject.hpp"

#include <string.h>

namespace th10 {

namespace {

// TH10 0x452493 / 0x4524a1: operator new / delete.
void *AllocateHeapBlock(u32 bytes);
void FreeHeapBlock(void *pointer);

// TH10 0x00449ed0 / 0x00449ae0 / 0x00449b70: scheduler node alloc
// and the calc/draw registration pair.
void *AllocSchedulerCallbackNode(void *callback);
void RegisterSchedulerCalcCallback(void *node, void *heap, u32 slot);
void RegisterSchedulerDrawCallback(void *node, void *heap, u32 slot);

// Bound callbacks owned by other translation units.
void ScoreNameEntryCalcCallback();   // TH10 0x00422a90
void ScoreNameEntryDrawCallback();   // TH10 0x00422aa0

extern u32 g_SchedulerHeap;          // TH10 DAT_00491be4
extern void *g_ScoreRecordOwner;     // TH10 DAT_00477830
extern float g_FrameTimeScale;       // TH10 DAT_00476f78
extern u32 g_ScoreNameStage;         // TH10 DAT_00474ca0
extern u32 g_ScoreNameTrigger;       // TH10 DAT_00474e36
extern u32 g_ScoreNamePending;       // TH10 DAT_00491ff4
extern void *g_MainChainContext;     // TH10 DAT_00477810

} // namespace

void *ResetScoreRecordDefaultsEdxAbi(void *record)
{
    u8 *bytes = static_cast<u8 *>(record);
    GameStateManager &mgr = *reinterpret_cast<GameStateManager *>(record);
    mgr.frame_timer.flags &= ~1U;      // +0x20
    mgr.cursor_a.step_count = 0;       // +0xb0
    mgr.cursor_a.value = 0;            // +0x24
    mgr.cursor_a.disabled_count = 0;   // +0xf8
    mgr.cursor_a.wrap_flag = 1;        // +0xf4
    mgr.cursor_a.maximum = 999;        // +0x2c
    mgr.cursor_b.maximum = 999;        // +0x104
    mgr.cursor_b.step_count = 0;       // +0x188
    mgr.cursor_b.value = 0;            // +0xfc
    mgr.cursor_b.disabled_count = 0;   // +0x1d0
    mgr.cursor_b.wrap_flag = 1;        // +0x1cc
    // The native zeroes the whole 0x2c8 record after the defaults,
    // erasing them; preserved verbatim.
    memset(bytes, 0, 0x2c8);
    mgr.flags_0000 |= 2U;
    g_ScoreRecordOwner = bytes;
    return bytes;
}

i32 InstallReplayNameEntryCallbacksEbxAbi(void *owner)
{
    GameStateManager &mgr = *reinterpret_cast<GameStateManager *>(owner);

    u8 *calc_node = static_cast<u8 *>(AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&ScoreNameEntryCalcCallback)));
    *reinterpret_cast<u32 *>(calc_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(calc_node + 0x20) =
        reinterpret_cast<u32>(&mgr);
    RegisterSchedulerCalcCallback(calc_node, &g_SchedulerHeap, 0);
    mgr.calc_element = reinterpret_cast<ChainElem *>(calc_node); // +0x8

    u8 *draw_node = static_cast<u8 *>(AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&ScoreNameEntryDrawCallback)));
    *reinterpret_cast<u32 *>(draw_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(draw_node + 0x20) =
        reinterpret_cast<u32>(&mgr);
    RegisterSchedulerDrawCallback(draw_node, &g_SchedulerHeap, 0);
    mgr.draw_element = reinterpret_cast<ChainElem *>(draw_node); // +0xc

    u32 flags = mgr.frame_timer.flags; // +0x20
    if ((flags & 1U) == 0U) {
        mgr.frame_timer.count = 0;     // +0x14
        mgr.frame_timer.prev = static_cast<i32>(0xfff0bdc1U); // +0x10
        mgr.frame_timer.accum = 0;     // +0x18
        mgr.frame_timer.rate = &g_FrameTimeScale; // +0x1c
        mgr.frame_timer.flags = flags | 1U;
    }
    mgr.frame_timer.count = 0;         // +0x14
    mgr.frame_timer.accum = 0;         // +0x18
    mgr.frame_timer.prev = -1;         // +0x10
    return 0;
}

void *CreateScoreRecordOwner()
{
    void *record = AllocateHeapBlock(0x2c8);
    if (record != 0)
        ResetScoreRecordDefaultsEdxAbi(record);
    if (InstallReplayNameEntryCallbacksEbxAbi(record) == 0)
        return record;
    if (record != 0) {
        DestroyGameStateObjectInPlace(record);
        FreeHeapBlock(record);
    }
    return 0;
}

i32 TickPauseMenuSequencerEsiAbi(void *record)
{
    GameStateManager &mgr = *reinterpret_cast<GameStateManager *>(record);
    switch (mgr.mode_0004) {
    case 0:
        if ((g_ScoreNameStage & 0x20U) == 0U &&
            ((g_ScoreNameTrigger & 8U) != 0U ||
             (g_ScoreNamePending & 0x10U) != 0U)) {
            // Native: mov ecx, g_TitleScreen; mov eax,[ecx+8] (calc
            // element); test byte [eax+4], 2; cmp dword [ecx+0x14], 0x1e.
            const TitleScreen &ts =
                *reinterpret_cast<const TitleScreen *>(g_MainChainContext);
            if (ts.calc_element != 0 &&
                (*reinterpret_cast<const u8 *>(&ts.calc_element->flags)
                 & 2U) != 0U &&
                ts.timer.count >= 30)
                RunPauseEnterSetupStackAbi(record);
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        RunPauseMenuModesStackAbi(record);
        break;
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xa:
    case 0xb:
    case 0xc:
    case 0xd:
        RunPostRunReplaySaveMenuStackAbi(record);
        break;
    default:
        break;
    }

    const u32 current = static_cast<u32>(mgr.frame_timer.count); // +0x14
    const float *rate = mgr.frame_timer.rate;                    // +0x1c
    mgr.frame_timer.prev = static_cast<i32>(current);            // +0x10
    if (*rate <= 0.99f || *rate >= 1.01f) {
        const float accumulated =
            *rate + *reinterpret_cast<const float *>(&mgr.frame_timer.accum);
        *reinterpret_cast<float *>(&mgr.frame_timer.accum) = accumulated;
        mgr.frame_timer.count = static_cast<i32>(static_cast<u32>(accumulated));
    } else {
        *reinterpret_cast<float *>(&mgr.frame_timer.accum) += 1.0f;
        mgr.frame_timer.count = static_cast<i32>(current + 1U);
    }
    return 1;
}

void SeedPostRunReplaySaveModeEsiAbi(void *record)
{
    GameStateManager &mgr = *reinterpret_cast<GameStateManager *>(record);
    mgr.mode_0004 = 13; // +0x4
    u32 flags = mgr.frame_timer.flags; // +0x20
    if ((flags & 1U) == 0U) {
        flags |= 1U;
        mgr.frame_timer.count = 0;     // +0x14
        mgr.frame_timer.prev = static_cast<i32>(0xfff0bdc1U); // +0x10
        mgr.frame_timer.accum = 0;     // +0x18
        mgr.frame_timer.rate = &g_FrameTimeScale; // +0x1c
        mgr.frame_timer.flags = flags;
    }
    mgr.frame_timer.count = 0;         // +0x14
    mgr.frame_timer.accum = 0;         // +0x18
    mgr.frame_timer.prev = -1;         // +0x10
    ReleaseTimelineContinuationHandle(
        reinterpret_cast<i32 *>(&mgr.handle_b_01d8)); // +0x1d8
    ReleaseTimelineContinuationHandle(
        reinterpret_cast<i32 *>(&mgr.handle_a_01d4)); // +0x1d4
    // Native quirk: the frame-time scale dword is republished as a
    // raw copy from the record's +0x2c0 slot.
    *reinterpret_cast<u32 *>(&g_FrameTimeScale) =
        *reinterpret_cast<u32 *>(&mgr.saved_time_scale_02c0);
}

} // namespace th10
