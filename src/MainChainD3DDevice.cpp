#include <math.h>
#include <string.h>

#include "MainChainRender.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

struct D3DDisplayMode {
    u32 width;
    u32 height;
    u32 format;
    u32 refresh_rate;
};

struct D3DPresentationParameters {
    u32 back_buffer_width;
    u32 back_buffer_height;
    u32 back_buffer_format;
    u32 back_buffer_count;
    u32 multisample_type;
    u32 multisample_quality;
    u32 swap_effect;
    void *device_window;
    u32 windowed;
    u32 enable_auto_depth_stencil;
    u32 auto_depth_stencil_format;
    u32 flags;
    u32 fullscreen_refresh_rate;
    u32 presentation_interval;
};

struct MainChainD3DCaps {
    u8 unknown_0000[0x58];
    u32 max_texture_width;
    u8 unknown_005c[0x34];
    u32 texture_op_caps;
};

typedef char AssertD3DPresentationParametersSize[
    sizeof(D3DPresentationParameters) == 0x38 ? 1 : -1];
typedef char AssertD3DCapsMaxTextureWidthOffset[
    offsetof(MainChainD3DCaps, max_texture_width) == 0x58 ? 1 : -1];
typedef char AssertD3DCapsTextureOpCapsOffset[
    offsetof(MainChainD3DCaps, texture_op_caps) == 0x90 ? 1 : -1];

typedef i32 (TH10_STDCALL *D3DGetAdapterDisplayModeFn)(D3D9Device *, u32,
                                                        D3DDisplayMode *);
typedef i32 (TH10_STDCALL *D3DCreateDeviceFn)(D3D9Device *, u32, u32, void *,
                                               u32, D3DPresentationParameters *,
                                               D3D9Device **);
typedef i32 (TH10_STDCALL *D3DCheckDeviceFormatFn)(D3D9Device *, u32, u32,
                                                    u32, u32, u32, u32);
typedef i32 (TH10_STDCALL *D3DSetTransformFn)(D3D9Device *, u32,
                                               const D3DMatrix *);
typedef i32 (TH10_STDCALL *D3DGetViewportFn)(D3D9Device *, D3DViewport *);
typedef i32 (TH10_STDCALL *D3DGetDeviceCapsFn)(D3D9Device *,
                                                MainChainD3DCaps *);
typedef u32 (TH10_STDCALL *D3DReleaseFn)(D3D9Device *);
typedef i32 (TH10_STDCALL *D3DSetViewportFn)(D3D9Device *,
                                              const D3DViewport *);
typedef i32 (TH10_STDCALL *D3DClearFn)(D3D9Device *, u32, const void *, u32,
                                        u32, float, u32);
typedef i32 (TH10_STDCALL *D3DPresentFn)(D3D9Device *, const void *,
                                          const void *, void *, const void *);
typedef i32 (TH10_STDCALL *D3DResetFn)(D3D9Device *, void *);

extern D3D9Device *g_Direct3D9; // DAT_00491c2c
extern D3D9Device *g_MainChainD3D9Device; // DAT_00491c30
extern D3DPresentationParameters g_D3D9PresentationParameters; // DAT_00491d0c
extern D3DMatrix g_MainChainInitialView; // DAT_00491c74
extern D3DMatrix g_MainChainInitialProjection; // DAT_00491cb4
extern D3DViewport g_MainChainInitialViewport; // DAT_00491cf4
extern MainChainD3DCaps g_MainChainD3DCaps; // DAT_00492000
extern void *g_MainChainWindow; // DAT_004924f0
extern u8 g_MainChainFallbackWindowMode; // DAT_00491d65
extern u8 g_MainChainBackBufferFormatOption; // DAT_00491d62
extern u8 g_MainChainAlternateLaunchPath; // DAT_00492518
extern u32 g_MainChainRuntimeOptions; // DAT_00491d78
extern u32 g_MainChainRuntimeFlags; // DAT_00491ff4
extern i32 g_MainChainDeviceFallbackMode; // DAT_00491fdc
extern i32 g_MainChainDeviceRefreshFallback; // DAT_00491fe0
extern u32 g_MainChainExitRequested; // DAT_004924f4
extern u32 g_MainChainDeviceInitState; // DAT_00491fe4
extern MainChainRenderOwnerFrameState *g_MainChainRenderOwner; // DAT_00491c10

extern void D3dxMatrixLookAtLH(D3DMatrix *out, const D3DVector3 *eye,
                               const D3DVector3 *target,
                               const D3DVector3 *up);
extern void D3dxMatrixPerspectiveFovLH(D3DMatrix *out, float fov,
                                       float aspect, float near_z, float far_z);
extern void AppendMainChainD3DMessage(u32 message_id);
extern void AppendMainChainD3DFatalMessage(u32 message_id);

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

i32 TryCreateMainChainD3DDevice(u32 device_type, u32 behavior_flags,
                                D3DPresentationParameters *parameters)
{
    return reinterpret_cast<D3DCreateDeviceFn>(GetD3DSlot(g_Direct3D9, 16))(
        g_Direct3D9, 0, device_type, g_MainChainWindow, behavior_flags,
        parameters, &g_MainChainD3D9Device);
}

bool CreateMainChainD3DDeviceWithFallbacks(D3DPresentationParameters *parameters)
{
    bool retried_after_refresh_change = false;
    bool used_hardware_device = false;
    for (;;) {
        if ((g_MainChainRuntimeOptions & 2U) == 0) {
            if (TryCreateMainChainD3DDevice(1, 0x40, parameters) >= 0) {
                AppendMainChainD3DMessage(0);
                g_MainChainRuntimeFlags |= 1;
                return true;
            }
            if (retried_after_refresh_change)
                AppendMainChainD3DMessage(1);

            if (TryCreateMainChainD3DDevice(1, 0x20, parameters) >= 0) {
                AppendMainChainD3DMessage(2);
                g_MainChainRuntimeFlags &= ~1U;
                return true;
            }
            if (retried_after_refresh_change)
                AppendMainChainD3DMessage(3);
        }

        if (TryCreateMainChainD3DDevice(2, 0x20, parameters) >= 0) {
            AppendMainChainD3DMessage(4);
            g_MainChainRuntimeFlags &= ~1U;
            return false;
        }

        if (g_MainChainDeviceFallbackMode == 0) {
            AppendMainChainD3DMessage(5);
            parameters->fullscreen_refresh_rate = 0;
            g_MainChainDeviceRefreshFallback = 0;
            retried_after_refresh_change = true;
            continue;
        }
        if (parameters->presentation_interval == 0x80000000U) {
            AppendMainChainD3DMessage(6);
            parameters->presentation_interval = 1;
            parameters->swap_effect = 3;
            continue;
        }

        AppendMainChainD3DFatalMessage(0);
        if (g_Direct3D9 != 0) {
            (void)reinterpret_cast<D3DReleaseFn>(GetD3DSlot(g_Direct3D9, 2))(
                g_Direct3D9);
            g_Direct3D9 = 0;
        }
        return used_hardware_device;
    }
}

} // namespace

void InitializeMainChainViewportAndClearBlack(u32 color)
{
    if (g_MainChainRenderOwner != 0)
        FlushRenderOwnerPendingVertices(
            reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));

    g_MainChainInitialViewport.x = 0;
    g_MainChainInitialViewport.y = 0;
    g_MainChainInitialViewport.width = 0x280;
    g_MainChainInitialViewport.height = 0x1e0;
    g_MainChainInitialViewport.min_z = 0.0f;
    g_MainChainInitialViewport.max_z = 1.0f;
    (void)reinterpret_cast<D3DSetViewportFn>(GetD3DSlot(
        g_MainChainD3D9Device, 47))(g_MainChainD3D9Device,
        &g_MainChainInitialViewport);

    for (u32 attempt = 0; attempt != 2; ++attempt) {
        (void)reinterpret_cast<D3DClearFn>(GetD3DSlot(
            g_MainChainD3D9Device, 43))(g_MainChainD3D9Device, 0, 0, 3,
                color, 1.0f, 0);
        if (reinterpret_cast<D3DPresentFn>(GetD3DSlot(
                g_MainChainD3D9Device, 17))(g_MainChainD3D9Device,
                    0, 0, 0, 0) < 0) {
            (void)reinterpret_cast<D3DResetFn>(GetD3DSlot(
                g_MainChainD3D9Device, 16))(g_MainChainD3D9Device,
                    &g_D3D9PresentationParameters);
        }
    }
}

i32 CreateMainChainD3D9Device()
{
    D3DPresentationParameters parameters;
    D3DDisplayMode display_mode;
    memset(&parameters, 0, sizeof(parameters));
    (void)reinterpret_cast<D3DGetAdapterDisplayModeFn>(GetD3DSlot(
        g_Direct3D9, 8))(g_Direct3D9, 0, &display_mode);

    if (g_MainChainFallbackWindowMode == 0) {
        if ((g_MainChainRuntimeOptions & 1U) != 0) {
            parameters.back_buffer_format = 0x17;
            g_MainChainBackBufferFormatOption = 1;
        } else if (g_MainChainBackBufferFormatOption == 0xff) {
            parameters.back_buffer_format = 0x16;
            g_MainChainBackBufferFormatOption = 0;
            AppendMainChainD3DMessage(7);
        } else {
            parameters.back_buffer_format =
                g_MainChainBackBufferFormatOption != 0 ? 0x17 : 0x16;
        }

        if (g_MainChainAlternateLaunchPath != 0)
            g_MainChainDeviceFallbackMode = 1;
        if (g_MainChainDeviceFallbackMode == 0) {
            parameters.fullscreen_refresh_rate = 60;
            parameters.presentation_interval = 1;
            AppendMainChainD3DMessage(8);
        } else {
            parameters.fullscreen_refresh_rate = 0;
            parameters.presentation_interval = 0x80000000U;
            AppendMainChainD3DMessage(9);
        }
    } else {
        parameters.back_buffer_format = display_mode.format;
        parameters.windowed = 1;
    }

    parameters.back_buffer_width = 0x280;
    parameters.back_buffer_height = 0x1e0;
    parameters.swap_effect = 1;
    parameters.enable_auto_depth_stencil = 1;
    parameters.auto_depth_stencil_format = 0x50;
    parameters.flags = 1;
    g_MainChainDeviceRefreshFallback = 1;

    const bool used_hardware_device =
        CreateMainChainD3DDeviceWithFallbacks(&parameters);
    if (g_MainChainD3D9Device == 0)
        return 1;

    g_D3D9PresentationParameters = parameters;
    const D3DVector3 eye = { 320.0f, -240.0f,
        -240.0f / static_cast<float>(tan(0.261799388f)) };
    const D3DVector3 target = { 320.0f, -240.0f, 0.0f };
    const D3DVector3 up = { 0.0f, 1.0f, 0.0f };
    D3dxMatrixLookAtLH(&g_MainChainInitialView, &eye, &target, &up);
    D3dxMatrixPerspectiveFovLH(&g_MainChainInitialProjection, 0.52359879f,
        4.0f / 3.0f, 100.0f, 10000.0f);
    (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(
        g_MainChainD3D9Device, 44))(g_MainChainD3D9Device, 2,
        &g_MainChainInitialView);
    (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(
        g_MainChainD3D9Device, 44))(g_MainChainD3D9Device, 3,
        &g_MainChainInitialProjection);
    (void)reinterpret_cast<D3DGetViewportFn>(GetD3DSlot(
        g_MainChainD3D9Device, 48))(g_MainChainD3D9Device,
        &g_MainChainInitialViewport);
    (void)reinterpret_cast<D3DGetDeviceCapsFn>(GetD3DSlot(
        g_MainChainD3D9Device, 7))(g_MainChainD3D9Device, &g_MainChainD3DCaps);
    if ((g_MainChainD3DCaps.texture_op_caps & 0x40U) == 0)
        AppendMainChainD3DMessage(10);
    if (g_MainChainD3DCaps.max_texture_width <= 0x100U)
        AppendMainChainD3DMessage(11);

    if ((g_MainChainRuntimeOptions & 1U) == 0 && used_hardware_device) {
        const i32 format_result = reinterpret_cast<D3DCheckDeviceFormatFn>(
            GetD3DSlot(g_Direct3D9, 10))(g_Direct3D9, 0, 1,
                parameters.back_buffer_format, 0, 3, 0x15);
        if (format_result == 0) {
            g_MainChainRuntimeFlags |= 4;
        } else {
            g_MainChainRuntimeOptions |= 1;
            g_MainChainRuntimeFlags &= ~4U;
            AppendMainChainD3DMessage(12);
        }
    }

    RestoreMainChainD3DRenderStates();
    InitializeMainChainViewportAndClearBlack(0xff000000U);
    g_MainChainExitRequested = 0;
    g_MainChainDeviceInitState = 0;
    return 0;
}

} // namespace th10
