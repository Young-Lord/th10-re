#include "MainApplicationRuntime.hpp"

#include <string.h>

#include "BgmRuntime.hpp"
#include "CallbackScheduler.hpp"
#include "MainApplicationFinalCleanup.hpp"
#include "MainApplicationFrame.hpp"
#include "MainChainContext.hpp"
#include "MainChainFrameTime.hpp"
#include "MainChainPlatformCleanup.hpp"
#include "MainChainRender.hpp"
#include "MainChainRuntime.hpp"
#include "MainChainShutdown.hpp"
#include "MainChainThreadCreation.hpp"

namespace th10 {

namespace {

typedef i32 (TH10_STDCALL *D3DTestCooperativeLevelFn)(D3D9Device *device);
typedef i32 (TH10_STDCALL *D3DResetFn)(D3D9Device *device,
                                        void *presentation_parameters);

const i32 kD3dErrDeviceNotReset = static_cast<i32>(0x88760869U);

extern MainChainContext g_MainChainContext; // TH10 DAT_00491c28
extern D3D9Device *g_MainChainD3D9Device; // TH10 DAT_00491c30
extern void *g_D3D9PresentationParameters; // TH10 DAT_00491d0c
extern MainChainRenderOwnerFrameState *g_MainChainRenderOwner; // 0x491c10
extern MainApplicationFrameState g_MainChainFrameState; // TH10 DAT_004924f0
extern u32 g_MainChainRuntimeFlags; // TH10 DAT_00491ff4
extern i32 g_MainChainDeviceRecoveryState; // TH10 DAT_00491fd4
extern u32 g_MainChainExitRequested; // TH10 DAT_004924f4
extern void *g_MainChainApplicationInstance; // TH10 DAT_004924f8
extern void *g_MainChainPointerTable; // TH10 DAT_00491be0
extern Win32CriticalSection g_MainChainCriticalSections[7]; // 0x492274
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern void *g_Direct3D9; // TH10 DAT_00491c2c
extern void *g_MainChainWindow; // TH10 DAT_004924f0
extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590
extern void *g_MainChainSoundWorker; // TH10 DAT_004977a8
extern u32 g_MainChainSoundWorkerId; // TH10 DAT_004977b0
extern void *g_MainChainSoundWorkerWindow; // TH10 DAT_004977b8
extern u8 g_MainChainFallbackWindowMode; // TH10 DAT_00491d65
extern u32 g_MainChainRuntimeOptions; // TH10 DAT_00491d78
extern double g_MainChainFrameClockEpoch; // TH10 DAT_00492540
extern double g_MainChainFramePreviousTick; // TH10 DAT_00492538
extern double g_MainChainFrameCalculationTick; // TH10 DAT_00492528
extern double g_MainChainFrameDrawTick; // TH10 DAT_00492530

extern i32 PeekMainChainFrameMessage(void *message, void *window,
                                     u32 minimum, u32 maximum,
                                     u32 remove_flags);
extern void TranslateMainChainFrameMessage(const void *message);
extern void DispatchMainChainFrameMessage(const void *message);
extern void *AllocateMainChainApplicationMemory(u32 bytes); // 0x452493
extern void FreeMainChainApplicationMemory(void *pointer); // 0x4524a1
extern void InitializeMainChainApplicationLock(void *critical_section);
extern void AppendMainChainStartupMessage();
extern i32 InitializeMainChainHostEnvironment(); // TH10 0x00439ff0
extern void InitializeMainChainSystemSettings(); // TH10 0x004392e0
// TH10 0x420870 takes the config path in EBX and context on the stack. This
// normal C++ boundary makes both semantic inputs explicit.
extern i32 LoadMainChainConfiguration(MainChainContext *context,
                                      const char *path);
extern i32 PromptMainChainFallbackWindowMode();
extern i32 CalculateMainChainExecutableChecksum(); // TH10 0x0043a1c0
extern void *CreateDirect3D9(u32 sdk_version); // Direct3DCreate9
extern void AppendMainChainD3DCreateFailure();
extern i32 CreateMainChainWindow(); // TH10 0x00439730
extern i32 CreateMainChainD3D9Device(); // TH10 0x00439890
extern bool ProbeMainChainJoystickAvailability(); // TH10 0x0044a0c0
extern void ReleaseMainChainKeyboardPressedState(); // TH10 0x0044b080
extern void *AllocateLargeRenderOwner(u32 bytes); // TH10 0x00452493
extern i32 EnableMainChainIme(void *window, i32 enable); // WINNLSEnableIME
extern i32 ShowMainChainCursor(i32 show); // ShowCursor
extern void *SetMainChainCursor(void *cursor); // SetCursor

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

void RestoreMainChainNormalWindowInputUi()
{
    (void)EnableMainChainIme(0, 0);
    (void)ShowMainChainCursor(0);
    (void)SetMainChainCursor(0);
}

void InitializeMainChainFrameTiming()
{
    g_MainChainFrameClockEpoch = 0.0;
    const double now = GetMainChainFrameTime();
    g_MainChainFrameClockEpoch = now;
    g_MainChainFramePreviousTick = now;
    g_MainChainFrameCalculationTick = now;
    g_MainChainFrameDrawTick = now;
}

} // namespace

i32 RunMainChainFrameLoop()
{
    i32 local_status = 0;
    u8 message[0x1c];

    for (;;) {
        if (PeekMainChainFrameMessage(message, 0, 0, 0, 1) != 0) {
            TranslateMainChainFrameMessage(message);
            DispatchMainChainFrameMessage(message);
        } else {
            const i32 cooperative =
                reinterpret_cast<D3DTestCooperativeLevelFn>(GetD3DSlot(
                    g_MainChainD3D9Device, 3))(g_MainChainD3D9Device);

            if (cooperative == 0) {
                const i32 frame_result = RunMainChainFrame(
                    &g_MainChainFrameState);
                if (frame_result != 0) {
                    local_status = frame_result;
                    break;
                }
                g_MainChainRuntimeFlags &= ~0x10U;
            } else if (cooperative == kD3dErrDeviceNotReset) {
                ReleaseResetSensitiveRenderSlots(g_MainChainRenderOwner);
                const i32 reset_result = reinterpret_cast<D3DResetFn>(GetD3DSlot(
                    g_MainChainD3D9Device, 16))(
                        g_MainChainD3D9Device,
                        g_D3D9PresentationParameters);
                if (reset_result != 0) {
                    local_status = reset_result;
                    break;
                }
                RestoreMainChainD3DRenderStates();
                g_MainChainDeviceRecoveryState = 3;
            }

            g_MainChainRuntimeFlags |= 0x10U;
        }

        if (g_MainChainExitRequested != 0)
            break;
    }

    (void)ShutdownMainChainRuntime(&g_MainChainContext);
    return local_status;
}

i32 RunMainApplication(void *application_instance, void *, void *,
                        void *main_context_initial_word)
{
    g_MainChainApplicationInstance = application_instance;
    g_MainChainPointerTable = AllocateMainChainApplicationMemory(0xa004);
    if (g_MainChainPointerTable != 0)
        memset(g_MainChainPointerTable, 0, 0xa004);

    for (u32 index = 0; index != 7; ++index)
        InitializeMainChainApplicationLock(&g_MainChainCriticalSections[index]);
    AppendMainChainStartupMessage();

    i32 local_status = 0;
    if (InitializeMainChainHostEnvironment() != -1) {
        *reinterpret_cast<void **>(&g_MainChainContext) =
            main_context_initial_word;
        InitializeMainChainSystemSettings();
        if (LoadMainChainConfiguration(&g_MainChainContext, "th10.cfg") == 0) {
            if ((g_MainChainRuntimeOptions & 0x100U) != 0 &&
                PromptMainChainFallbackWindowMode() != 6) {
                g_MainChainFallbackWindowMode = 1;
            }
            (void)CalculateMainChainExecutableChecksum();

            for (;;) {
                void *const scheduler_storage =
                    AllocateMainChainApplicationMemory(sizeof(CallbackScheduler));
                g_CallbackScheduler = scheduler_storage == 0 ? 0 :
                    ConstructCallbackScheduler(scheduler_storage);
                g_Direct3D9 = CreateDirect3D9(0x20);
                if (g_Direct3D9 == 0) {
                    AppendMainChainD3DCreateFailure();
                    break;
                }
                if (CreateMainChainWindow() != 0)
                    break;

                memset(&g_TransitionRoot, 0, sizeof(g_TransitionRoot));
                g_MainChainSoundWorkerWindow = g_MainChainWindow;
                g_MainChainSoundWorker = CreateMainChainSoundWorkerThread();
                if (CreateMainChainD3D9Device() != 0)
                    break;

                (void)ProbeMainChainJoystickAvailability();
                ReleaseMainChainKeyboardPressedState();
                void *const render_owner = AllocateLargeRenderOwner(0x732460);
                g_MainChainRenderOwner = render_owner == 0 ? 0 :
                    static_cast<MainChainRenderOwnerFrameState *>(
                        ConstructLargeRenderOwner(render_owner));
                if (g_MainChainFallbackWindowMode == 0)
                    RestoreMainChainNormalWindowInputUi();
                InitializeMainChainFrameTiming();

                const i32 registration_result = RegisterMainChainCallbacks();
                if (registration_result == 0) {
                    local_status = RunMainChainFrameLoop();
                } else {
                    local_status = registration_result == -1 ? -1 : 2;
                    (void)ShutdownMainChainRuntime(&g_MainChainContext);
                }

                if (g_CallbackScheduler != 0) {
                    DestroyCallbackSchedulerInPlace(g_CallbackScheduler);
                    FreeMainChainApplicationMemory(g_CallbackScheduler);
                }
                g_CallbackScheduler = 0;
                while (AdvanceBgmRuntime(&g_TransitionRoot) != 0) {
                }

                ShutdownMainChainPlatformResources(&g_MainChainContext);
                if (local_status == 2) {
                    PrepareMainChainStartupRetry();
                    continue;
                }
                FinalizeMainChainApplication();
                return 0;
            }
        }
    }

    ShutdownMainChainPlatformResources(&g_MainChainContext);
    FinalizeMainChainApplication();
    return 0;
}

} // namespace th10
