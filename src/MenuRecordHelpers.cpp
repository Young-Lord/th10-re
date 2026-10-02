#include "MenuRecordHelpers.hpp"

#include "GameModeTeardown.hpp"
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
    *reinterpret_cast<u32 *>(bytes + 0x20) &= ~1U;
    *reinterpret_cast<u32 *>(bytes + 0xb0) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x24) = 0;
    *reinterpret_cast<u32 *>(bytes + 0xf8) = 0;
    *reinterpret_cast<u32 *>(bytes + 0xf4) = 1;
    *reinterpret_cast<u32 *>(bytes + 0x2c) = 999;
    *reinterpret_cast<u32 *>(bytes + 0x104) = 999;
    *reinterpret_cast<u32 *>(bytes + 0x188) = 0;
    *reinterpret_cast<u32 *>(bytes + 0xfc) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x1d0) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x1cc) = 1;
    // The native zeroes the whole 0x2c8 record after the defaults,
    // erasing them; preserved verbatim.
    memset(bytes, 0, 0x2c8);
    *reinterpret_cast<u32 *>(bytes) |= 2U;
    g_ScoreRecordOwner = bytes;
    return bytes;
}

i32 InstallReplayNameEntryCallbacksEbxAbi(void *owner)
{
    u8 *bytes = static_cast<u8 *>(owner);

    u8 *calc_node = static_cast<u8 *>(AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&ScoreNameEntryCalcCallback)));
    *reinterpret_cast<u32 *>(calc_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(calc_node + 0x20) =
        reinterpret_cast<u32>(bytes);
    RegisterSchedulerCalcCallback(calc_node, &g_SchedulerHeap, 0);
    *reinterpret_cast<u32 *>(bytes + 8) =
        reinterpret_cast<u32>(calc_node);

    u8 *draw_node = static_cast<u8 *>(AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&ScoreNameEntryDrawCallback)));
    *reinterpret_cast<u32 *>(draw_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(draw_node + 0x20) =
        reinterpret_cast<u32>(bytes);
    RegisterSchedulerDrawCallback(draw_node, &g_SchedulerHeap, 0);
    *reinterpret_cast<u32 *>(bytes + 0xc) =
        reinterpret_cast<u32>(draw_node);

    u32 flags = *reinterpret_cast<u32 *>(bytes + 0x20);
    if ((flags & 1U) == 0U) {
        *reinterpret_cast<u32 *>(bytes + 0x14) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x10) = 0xfff0bdc1U;
        *reinterpret_cast<u32 *>(bytes + 0x18) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x1c) =
            reinterpret_cast<u32>(&g_FrameTimeScale);
        *reinterpret_cast<u32 *>(bytes + 0x20) = flags | 1U;
    }
    *reinterpret_cast<u32 *>(bytes + 0x14) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x18) = 0;
    *reinterpret_cast<i32 *>(bytes + 0x10) = -1;
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
    u8 *bytes = static_cast<u8 *>(record);
    switch (*reinterpret_cast<u32 *>(bytes + 4)) {
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

    const u32 current = *reinterpret_cast<u32 *>(bytes + 0x14);
    const float *rate = *reinterpret_cast<float *const *>(bytes + 0x1c);
    *reinterpret_cast<u32 *>(bytes + 0x10) = current;
    if (*rate <= 0.99f || *rate >= 1.01f) {
        const float accumulated =
            *rate + *reinterpret_cast<float *>(bytes + 0x18);
        *reinterpret_cast<float *>(bytes + 0x18) = accumulated;
        *reinterpret_cast<u32 *>(bytes + 0x14) =
            static_cast<u32>(accumulated);
    } else {
        *reinterpret_cast<float *>(bytes + 0x18) += 1.0f;
        *reinterpret_cast<u32 *>(bytes + 0x14) = current + 1U;
    }
    return 1;
}

void SeedPostRunReplaySaveModeEsiAbi(void *record)
{
    u8 *bytes = static_cast<u8 *>(record);
    *reinterpret_cast<u32 *>(bytes + 4) = 13;
    u32 flags = *reinterpret_cast<u32 *>(bytes + 0x20);
    if ((flags & 1U) == 0U) {
        flags |= 1U;
        *reinterpret_cast<u32 *>(bytes + 0x14) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x10) = 0xfff0bdc1U;
        *reinterpret_cast<u32 *>(bytes + 0x18) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x1c) =
            reinterpret_cast<u32>(&g_FrameTimeScale);
        *reinterpret_cast<u32 *>(bytes + 0x20) = flags;
    }
    *reinterpret_cast<u32 *>(bytes + 0x14) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x18) = 0;
    *reinterpret_cast<i32 *>(bytes + 0x10) = -1;
    ReleaseTimelineContinuationHandle(
        reinterpret_cast<i32 *>(bytes + 0x1d8));
    ReleaseTimelineContinuationHandle(
        reinterpret_cast<i32 *>(bytes + 0x1d4));
    // Native quirk: the frame-time scale dword is republished as a
    // raw copy from the record's +0x2c0 slot.
    *reinterpret_cast<u32 *>(&g_FrameTimeScale) =
        *reinterpret_cast<u32 *>(bytes + 0x2c0);
}

} // namespace th10
