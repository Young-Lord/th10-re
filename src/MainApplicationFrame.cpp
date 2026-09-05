#include "MainApplicationFrame.hpp"

#include <stdio.h>

#include "BgmRuntime.hpp"
#include "CallbackScheduler.hpp"
#include "MainChainContext.hpp"
#include "MainChainFrameTime.hpp"
#include "MainChainRender.hpp"
#include "ThreadControl.hpp"

namespace th10 {

namespace {

typedef i32 (TH10_STDCALL *D3DSetViewportFn)(D3D9Device *, const void *);
typedef i32 (TH10_STDCALL *D3DBeginSceneFn)(D3D9Device *);
typedef i32 (TH10_STDCALL *D3DEndSceneFn)(D3D9Device *);
typedef i32 (TH10_STDCALL *D3DSetTextureFn)(D3D9Device *, u32, void *);
typedef i32 (TH10_STDCALL *D3DPresentFn)(D3D9Device *, const void *,
                                          const void *, void *, const void *);
typedef i32 (TH10_STDCALL *D3DResetFn)(D3D9Device *, void *);

extern double g_MainChainFrameStep; // TH10 DAT_00470c90
extern void SleepMilliseconds(u32 milliseconds);
extern D3D9Device *g_MainChainD3D9Device; // TH10 DAT_00491c30
extern void *g_MainChainFrameViewportBlock; // TH10 DAT_00491fac
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590
extern ThreadControl g_MainChainSecondaryControl; // TH10 DAT_00492254
extern MainChainRenderOwnerFrameState *g_MainChainRenderOwner; // 0x491c10
extern u8 g_RegistrationFrameIncrement; // TH10 DAT_00491d66
extern u32 g_MainChainPresentColor; // TH10 DAT_00492378
extern double g_MainChainFrameOverrun; // TH10 DAT_004923a0
extern MainChainContext g_MainChainContext; // TH10 DAT_00491c28
extern void *g_D3D9PresentationParameters; // TH10 DAT_00491d0c
extern i32 g_MainChainDeviceRecoveryState; // TH10 DAT_00491fd4
extern u32 g_MainChainPresentDiagnosticOptions; // TH10 DAT_00474e36
extern i32 CreateMainChainDirectory(const char *path); // TH10 0x00452abc
extern i32 DoesMainChainFileExist(const char *path); // TH10 0x0044b4d0
// TH10 0x00420670 has ESI=context and EAX=output path. This normal C++
// boundary retains that nonstandard entry ABI outside the semantic caller.
extern i32 CaptureMainChainSnapshot(MainChainContext *context,
                                    const char *output_path);

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

void ProcessMainChainPresentDiagnostics()
{
    if ((g_MainChainPresentDiagnosticOptions & 0x800U) == 0)
        return;

    (void)CreateMainChainDirectory("snapshot");
    for (i32 index = 0; index < 1000; ++index) {
        char output_path[0x100];
        sprintf(output_path, "snapshot/th%.3d.bmp", index);
        if (DoesMainChainFileExist(output_path) == 0) {
            (void)CaptureMainChainSnapshot(&g_MainChainContext, output_path);
            return;
        }
    }
}

} // namespace

i32 RunMainChainFrame(MainApplicationFrameState *state)
{
    const double now = GetMainChainFrameTime();
    state->frame_now_0038 = now;
    if (!(now < state->previous_now_0040))
        state->scheduled_now_0048 = now;
    state->previous_now_0040 = now;

    if (!(now > state->scheduled_now_0048)) {
        SleepMilliseconds(0);
        return 0;
    }
    while (state->scheduled_now_0048 < state->frame_now_0038)
        state->scheduled_now_0048 += g_MainChainFrameStep;

    FlushRenderOwnerPendingVertices(
        reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
    MainChainCameraWork *const frame_work =
        static_cast<MainChainCameraWork *>(g_MainChainFrameViewportBlock);
    g_MainChainFrameViewportBlock = frame_work;
    (void)UpdateMainChainD3DFrameStateEdiAbi(frame_work);
    (void)reinterpret_cast<D3DSetViewportFn>(GetD3DSlot(
        g_MainChainD3D9Device, 47))(
            g_MainChainD3D9Device,
            static_cast<u8 *>(g_MainChainFrameViewportBlock) + 0xcc);

    const i32 calculation_result =
        CallbackSchedulerApi::DispatchCalculation(g_CallbackScheduler);
    (void)AdvanceBgmRuntime(&g_TransitionRoot);
    if (calculation_result == 0) {
        StopThreadControl(&g_MainChainSecondaryControl);
        return 1;
    }
    if (calculation_result == -1) {
        StopThreadControl(&g_MainChainSecondaryControl);
        return 2;
    }

    ++state->present_counter_0014;
    if (static_cast<i32>(static_cast<signed char>(
            state->present_counter_0014)) >=
        static_cast<i32>(g_RegistrationFrameIncrement) + 1) {
        (void)reinterpret_cast<D3DBeginSceneFn>(GetD3DSlot(
            g_MainChainD3D9Device, 41))(g_MainChainD3D9Device);
        u8 *const owner = static_cast<u8 *>(
            static_cast<void *>(g_MainChainRenderOwner));
        *reinterpret_cast<u32 *>(owner + 0x3adac8) = 0;
        *reinterpret_cast<void **>(owner + 0x72dacc) = owner + 0x3adacc;
        *reinterpret_cast<void **>(owner + 0x72dad0) = owner + 0x3adacc;
        g_MainChainPresentColor = 0xff;
        (void)DisableMainChainFogIfNeeded(&g_MainChainContext);
        (void)CallbackSchedulerApi::DispatchDraw(g_CallbackScheduler);
        FlushRenderOwnerPendingVertices(
            reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
        (void)reinterpret_cast<D3DSetTextureFn>(GetD3DSlot(
            g_MainChainD3D9Device, 65))(
                g_MainChainD3D9Device, 0, 0);
        (void)reinterpret_cast<D3DEndSceneFn>(GetD3DSlot(
            g_MainChainD3D9Device, 42))(g_MainChainD3D9Device);
        state->present_counter_0014 = 0;
        PresentAndRecoverMainChainDevice();
    }

    g_MainChainFrameOverrun = GetMainChainFrameTime() - state->frame_now_0038;
    return 0;
}

void PresentAndRecoverMainChainDevice()
{
    if (reinterpret_cast<D3DPresentFn>(GetD3DSlot(
            g_MainChainD3D9Device, 17))(
                g_MainChainD3D9Device, 0, 0, 0, 0) < 0) {
        ReleaseResetSensitiveRenderSlots(g_MainChainRenderOwner);
        (void)reinterpret_cast<D3DResetFn>(GetD3DSlot(
            g_MainChainD3D9Device, 16))(
                g_MainChainD3D9Device, g_D3D9PresentationParameters);
        RestoreMainChainD3DRenderStates();
        g_MainChainDeviceRecoveryState = 2;
    }

    FlushRenderOwnerPendingRenderBatches(g_MainChainRenderOwner);
    ProcessMainChainPresentDiagnostics();
}

} // namespace th10
