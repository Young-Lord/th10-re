#include "GlobalLifecycleManager.hpp"

#include <string.h>

#include "MainChainRuntime.hpp"
#include "BgmRuntime.hpp"
#include "PackedArchive.hpp"

namespace th10 {

namespace {

extern GlobalLifecycleManager *g_GlobalLifecycleManager; // 0x477820
extern ManagerWorkOwnerPartial *g_GlobalLifecycleResourceOwner; // 0x491c10
extern void *g_GlobalLifecycleTextResource; // 0x491fec
extern void *g_GlobalLifecycleBgmFormat; // 0x494514
extern u8 g_GlobalLifecycleBgmFlags; // 0x491d78
extern i32 g_MainChainSharedStatus; // 0x491fb8
extern u8 g_BgmHeaderOverrideSource[10]; // 0x46dbe0
extern u8 g_BgmHeaderOverrideDestination[10]; // 0x497698
extern const char g_FrontDataCorruptMessage[]; // 0x46cb68
extern const char g_BulletDataMissingOrCorruptMessage[]; // 0x46cd54

extern void *CreateAsciiManager();
extern void AppendStartupLoadError(const char *message);
extern i32 DoesStartupFileExist(const char *path);
extern void CreateGlobalLifecycleOpaqueObject();

void EnableCalculationCallback(GlobalLifecycleManager *manager)
{
    manager->calc_callback->flags |= ChainElemFlag_Enabled;
}

void FailGlobalLifecycleStartup(GlobalLifecycleManager *manager)
{
    g_MainChainSharedStatus = 3;
    EnableCalculationCallback(manager);
}

} // namespace

// TH10 0x0041f990. This retains the native body\'s published-global lookup,
// ignored thread parameter, failure state, and deliberately ignored helper
// return values. Narrow adapters retain the unknown register ABIs below it.
// TH10 0x0041f8d0. Work publication and failure cleanup stay owned by the
// ManagerWork request/teardown paths; this helper neither frees nor rolls
// back a successful front request when bullet loading fails.
i32 InitializeFrontAndBulletResources()
{
    if (RequestManagerWork(
            g_GlobalLifecycleResourceOwner, 6, "front.anm") == 0) {
        AppendStartupLoadError(g_FrontDataCorruptMessage);
        return -1;
    }

    if (RequestManagerWork(
            g_GlobalLifecycleResourceOwner, 7, "bullet.anm") == 0) {
        AppendStartupLoadError(g_BulletDataMissingOrCorruptMessage);
        return -1;
    }

    return 0;
}

u32 TH10_CDECL GlobalLifecycleStartupThread(void *)
{
    GlobalLifecycleManager *const manager = g_GlobalLifecycleManager;
    manager->startup_result = RequestManagerWork(
        g_GlobalLifecycleResourceOwner, 1, "sig.anm");
    if (manager->startup_result == 0) {
        FailGlobalLifecycleStartup(manager);
        return 0;
    }

    manager->draw_callback->flags |= ChainElemFlag_Enabled;
    manager->startup_stage = 1;

    if (CreateAsciiManager() == 0) {
        AppendStartupLoadError("../../bgm/thbgm.fmt");
        FailGlobalLifecycleStartup(manager);
        return 0;
    }

    manager->draw_stage = 1;
    g_GlobalLifecycleTextResource = RequestManagerWork(
        g_GlobalLifecycleResourceOwner, 0, "text.anm");
    if (g_GlobalLifecycleTextResource == 0) {
        FailGlobalLifecycleStartup(manager);
        return 0;
    }

    g_GlobalLifecycleBgmFormat = LoadPackedResource("../../bgm/thbgm.fmt",
                                                      0, 0);
    if (g_GlobalLifecycleBgmFormat == 0)
        AppendStartupLoadError("../../bgm/thbgm.fmt");

    (void)InitializeBgmRelatedState(
        reinterpret_cast<TransitionRootPartial *>(0x00492590));
    if (DoesStartupFileExist("thbgm.dat") != 0) {
        if ((g_GlobalLifecycleBgmFlags & 0x10) == 0)
            (void)LoadBgmDataFile(
                reinterpret_cast<TransitionRootPartial *>(0x00492590),
                "thbgm.dat");
        else
            memcpy(g_BgmHeaderOverrideDestination, g_BgmHeaderOverrideSource,
                   sizeof(g_BgmHeaderOverrideSource));
    }

    (void)InitializeFrontAndBulletResources();
    EnableCalculationCallback(manager);
    CreateGlobalLifecycleOpaqueObject();
    return 0;
}

u32 TH10_STDCALL GlobalLifecycleStartupThreadAdapter(void *unused)
{
    return GlobalLifecycleStartupThread(unused);
}

} // namespace th10
