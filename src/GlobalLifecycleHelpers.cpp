#include "GlobalLifecycleHelpers.hpp"

#include "Th10Platform.hpp"
#include "ThreadControl.hpp"

namespace th10 {

namespace {

// TH10 0x00449ed0 / 0x00449ae0 / 0x00449b70: scheduler node alloc and
// the calc/draw registration pair.
void *AllocSchedulerCallbackNode(void *callback);
void RegisterSchedulerCalcCallback(void *node, void *heap, u32 slot);
void RegisterSchedulerDrawCallback(void *node, void *heap, u32 slot);

// Bound callbacks owned by other translation units.
void GlobalLifecycleCalcCallback();   // TH10 0x0041feb0
void GlobalLifecycleDrawCallback();   // TH10 0x0041fef0

// TH10 0x00453683: _beginthreadex(security, stack, start, arg,
// initflag, thrdaddr). Native MSVC CRT entry.
u32 BeginThreadExNative(void *security, u32 stack_size,
                        u32 (TH10_STDCALL *start)(void *), void *arg,
                        u32 initflag, u32 *thrdaddr);

// TH10 0x0041f990: the startup thread body.
u32 TH10_STDCALL GlobalLifecycleStartupThreadBody(void *argument);

extern u32 g_SchedulerHeap;       // TH10 DAT_00491be4

// Loading-screen entity helpers.
void *CreateTimelinePresetTextSlotNodeNative(void);        // 0x00449950
void ApplyTimelineRenderObjectPresetCloneEcxEcxAbi(        // 0x00449870
    void *node, u32 preset);
void LinkEntityAndAssignIdEaxEdxAbi(void *slot, void *owner); // 0x004489d0
void *CreateTimelineContinuationRenderObjectNative(        // 0x00448d50
    void *base, void *argument, i32 kind);

extern void *g_LoadingScreenContext;  // TH10 DAT_004776e0
extern void *g_RenderOwner;           // TH10 DAT_00491c10

} // namespace

i32 InstallGlobalLifecycleCallbacksEbxAbi(void *manager)
{
    u8 *bytes = static_cast<u8 *>(manager);

    u8 *calc_node = static_cast<u8 *>(AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&GlobalLifecycleCalcCallback)));
    *reinterpret_cast<u32 *>(calc_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(calc_node + 0x20) =
        reinterpret_cast<u32>(bytes);
    RegisterSchedulerCalcCallback(calc_node, &g_SchedulerHeap, 3);
    *reinterpret_cast<u32 *>(bytes + 8) =
        reinterpret_cast<u32>(calc_node);

    u8 *draw_node = static_cast<u8 *>(AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&GlobalLifecycleDrawCallback)));
    *reinterpret_cast<u32 *>(draw_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(draw_node + 0x20) =
        reinterpret_cast<u32>(bytes);
    RegisterSchedulerDrawCallback(draw_node, &g_SchedulerHeap, 2);
    *reinterpret_cast<u32 *>(bytes + 0xc) =
        reinterpret_cast<u32>(draw_node);

    StopThreadControl(reinterpret_cast<ThreadControl *>(bytes + 0x10));

    *reinterpret_cast<u32 *>(bytes + 0x28) =
        reinterpret_cast<u32>(&GlobalLifecycleStartupThreadBody);
    *reinterpret_cast<u32 *>(bytes + 0x20) = 1;
    *reinterpret_cast<u32 *>(bytes + 0x1c) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x14) = BeginThreadExNative(
        0, 0, &GlobalLifecycleStartupThreadBody, bytes, 0,
        reinterpret_cast<u32 *>(bytes + 0x18));
    return 0;
}

i32 CreateLoadingScreenEntitiesStackAbi(void *manager)
{
    u8 *bytes = static_cast<u8 *>(manager);
    if (*reinterpret_cast<u32 *>(bytes + 0x3e4) == 1U) {
        void *node = CreateTimelinePresetTextSlotNodeNative();
        u32 state = *reinterpret_cast<u32 *>(
            static_cast<u8 *>(node) + 0x35c) | 0x40000000U;
        *reinterpret_cast<u32 *>(static_cast<u8 *>(node) + 0x20) = 15;
        *reinterpret_cast<u32 *>(static_cast<u8 *>(node) + 0x35c) =
            state;
        ApplyTimelineRenderObjectPresetCloneEcxEcxAbi(node, 0);
        u32 slot = 0;
        LinkEntityAndAssignIdEaxEdxAbi(&slot, g_RenderOwner);
        *reinterpret_cast<u32 *>(bytes + 0x3dc) = slot;
        ++(*reinterpret_cast<u32 *>(bytes + 0x3e4));
    }
    if (*reinterpret_cast<u32 *>(bytes + 0x3e8) == 1U) {
        u8 *context = static_cast<u8 *>(g_LoadingScreenContext);
        if (*reinterpret_cast<u32 *>(context + 0x89a4) == 0U) {
            *reinterpret_cast<u32 *>(context + 0x89a4) =
                reinterpret_cast<u32>(CreateTimelineContinuationRenderObjectNative(
                    context + 0x8994, bytes, 6));
        }
        ++(*reinterpret_cast<u32 *>(bytes + 0x3e8));
    }
    ++(*reinterpret_cast<u32 *>(bytes + 0x3ec));
    return 1;
}

void MarkPauseChainFlagsUncheckedEaxAbi(void *owner)
{
    u8 *bytes = static_cast<u8 *>(owner);
    // All three dereferences are unchecked in the native; the records
    // are guaranteed live on the paths that reach this callback.
    (*reinterpret_cast<u32 **>(bytes + 0xc))[1] |= 2U;
    (*reinterpret_cast<u32 **>(bytes + 0x10))[1] |= 2U;
    (*reinterpret_cast<u32 **>(bytes + 0x89a8))[1] |= 2U;
}

} // namespace th10
