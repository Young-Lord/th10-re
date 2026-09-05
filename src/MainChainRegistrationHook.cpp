#include "MainChainContext.hpp"
#include "MainChainRuntime.hpp"
#include "MainChainStartupGlobals.hpp"
#include "RegistrationDrawOwner.hpp"
#include "GeneratedFontTable.hpp"
#include "VersionData.hpp"
#include "MainChainBackgroundThread.hpp"
#include "MainChainThreadCreation.hpp"
#include "ThreadControl.hpp"
#include "LargeRenderOwnerFrameLoop.hpp"

namespace th10 {

namespace {

extern float g_MainChainStartupScale; // TH10 DAT_00476f78
extern u32 g_MainChainClearColor; // TH10 DAT_004923a8
extern u32 g_MainChainStartupTick; // TH10 DAT_00491ff8
extern u32 g_MainChainStartupTickLow0; // TH10 DAT_004918b0
extern u32 g_MainChainStartupTickLow1; // TH10 DAT_004918a8
extern void *g_MainChainResourceThread; // TH10 DAT_004977ac
extern ThreadControl g_MainChainBackgroundControl; // 0x474dd4
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern MainChainContext g_MainChainContext; // TH10 DAT_00491c28
extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590

extern u32 GetMainChainStartupTick(); // imported timeGetTime
#include "LargeRenderOwnerFrameLoop.hpp"

} // namespace

// TH10 0x004201b0. The scheduler supplies context in ECX, but the original
// never dereferences it. CreateThread overwrites its temporary ECX save with a
// thread ID; that post-call register value is unused by the scheduler.
i32 TH10_FASTCALL InitializeMainChainCalculationCallback(void *)
{
    (void)InitializeVersionData();
    g_MainChainStartupScale = 1.0f;
    g_MainChainClearColor = 0xff000000;
    ResetMainChainStartupGlobals();

    const u32 tick = GetMainChainStartupTick();
    g_MainChainStartupTick = tick;
    g_MainChainStartupTickLow0 = tick & 0xffff;
    g_MainChainStartupTickLow1 = tick & 0xffff;

    u32 resource_thread_id = 0;
    g_MainChainResourceThread = StartMainChainResourceThread(
        &g_TransitionRoot, &resource_thread_id);

    CreateRegistrationDrawOwner();

    StopThreadControl(&g_MainChainBackgroundControl);
    g_MainChainBackgroundControl.stop_requested = 1;
    g_MainChainBackgroundControl.field_0010 = 0;
    g_MainChainBackgroundControl.thread_entry =
        reinterpret_cast<void *>(MainChainBackgroundThread);
    g_MainChainBackgroundControl.thread_handle = StartMainChainBackgroundThread(
        &g_MainChainContext, &g_MainChainBackgroundControl.thread_id);

    InitializeLargeRenderOwner(g_MainChainRenderOwner);
    InitializeGeneratedTableAndFonts();
    return 0;
}

} // namespace th10
