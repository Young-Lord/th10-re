#include <string.h>

#include "GlobalLifecycleManager.hpp"
#include "ThreadControl.hpp"

namespace th10 {

namespace {

extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern GlobalLifecycleManager *g_GlobalLifecycleManager; // TH10 DAT_00477820

extern GlobalLifecycleManager *AllocateGlobalLifecycleManager(u32 bytes);
extern void FreeGlobalLifecycleManager(void *pointer);
extern void FreeGlobalLifecycleOwnedBuffer(void *pointer);
extern void *BeginGlobalLifecycleThread(GlobalLifecycleManager *manager,
                                        u32 *thread_id);
extern void ReleaseGlobalLifecycleCoordinatedGlobals();

extern i32 TH10_FASTCALL DisableGlobalManagerChainsOnFlag(
    GlobalLifecycleManager *manager);
extern i32 TH10_FASTCALL InvokeGlobalManagerFrameHandler(
    GlobalLifecycleManager *manager);

void ConstructGlobalLifecycleManager(GlobalLifecycleManager *manager)
{
    memset(manager, 0, sizeof(*manager));
    manager->flags = 2;
    g_GlobalLifecycleManager = manager;
}

i32 InitializeGlobalLifecycleManager(GlobalLifecycleManager *manager)
{
    ChainElem *calculation = CallbackSchedulerApi::Create(
        reinterpret_cast<ChainCallback>(DisableGlobalManagerChainsOnFlag));
    calculation->flags &= ~ChainElemFlag_Enabled;
    calculation->arg = manager;
    manager->calc_callback = calculation;
    CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler,
                                                 calculation, 3);

    ChainElem *draw = CallbackSchedulerApi::Create(
        reinterpret_cast<ChainCallback>(InvokeGlobalManagerFrameHandler));
    draw->flags &= ~ChainElemFlag_Enabled;
    draw->arg = manager;
    manager->draw_callback = draw;
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw, 2);

    StopThreadControl(reinterpret_cast<ThreadControl *>(
        &manager->thread_control_0010));
    manager->startup_thread_proc =
        reinterpret_cast<void *>(GlobalLifecycleStartupThread);
    manager->thread_running = 1;
    manager->close_requested = 0;
    manager->thread_handle = BeginGlobalLifecycleThread(manager,
                                                         &manager->thread_id);
    return 0;
}

void TeardownGlobalLifecycleManagerInPlace(GlobalLifecycleManager *manager)
{
    StopThreadControl(reinterpret_cast<ThreadControl *>(
        &manager->thread_control_0010));

    if (manager->calc_callback != 0)
        CallbackSchedulerApi::Remove(g_CallbackScheduler,
                                     manager->calc_callback);
    if (manager->draw_callback != 0)
        CallbackSchedulerApi::Remove(g_CallbackScheduler,
                                     manager->draw_callback);

    ReleaseGlobalLifecycleCoordinatedGlobals();

    if (manager->owned_buffer != 0) {
        FreeGlobalLifecycleOwnedBuffer(manager->owned_buffer);
        manager->owned_buffer = 0;
    }

    manager->thread_control_0010 = reinterpret_cast<void *>(0x004703e4);
    StopThreadControl(reinterpret_cast<ThreadControl *>(
        &manager->thread_control_0010));
}

} // namespace

// TH10 0x0041fd00, expressed as a source-level factory. The original's ESI
// constructor and EBX initializer conventions are intentionally contained
// within this implementation instead of exposed to callers.
GlobalLifecycleManager *CreateGlobalLifecycleManager()
{
    GlobalLifecycleManager *manager = AllocateGlobalLifecycleManager(
        sizeof(GlobalLifecycleManager));
    if (manager != 0)
        ConstructGlobalLifecycleManager(manager);

    // The original invokes its initializer even after an allocation failure.
    // This semantic boundary avoids dereferencing null while preserving the
    // observable normal and initializer-failure ownership paths.
    if (manager == 0)
        return 0;

    if (InitializeGlobalLifecycleManager(manager) != 0) {
        TeardownGlobalLifecycleManagerInPlace(manager);
        FreeGlobalLifecycleManager(manager);
        return 0;
    }

    return manager;
}

// Semantic ownership wrapper: TH10 0x0041fb50 tears down in place; its callers
// separately free the allocation. This wrapper represents the combined owner.
void DestroyGlobalLifecycleManager(GlobalLifecycleManager *manager)
{
    if (manager == 0)
        return;

    TeardownGlobalLifecycleManagerInPlace(manager);
    FreeGlobalLifecycleManager(manager);
}

} // namespace th10
