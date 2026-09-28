// TH10 render-owner frame-loop entries (0x004485f0-0x00448810): the five
// "draw the owner kind chain" wrappers the scene states dispatch through.
// Each binds a camera-work base (DAT_00491fac pointer), applies the D3D
// frame state (0x00421480), re-materializes the material block through
// vtable +0xbc (SetMaterial at +0xcc) and dispatches the chain with a
// fixed kind constant (0 / 3 / 0xe / 0xf).
#include "Th10Types.hpp"
#include "Th10Platform.hpp"
#include "MainChainRender.hpp"

namespace th10 {

namespace {

void WriteOwnerU32(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}


extern D3D9Device *g_MainChainD3DDevice; // TH10 DAT_00491c30
extern void *g_MainChainRenderOwner;     // TH10 DAT_00491c10

// Camera-work bases the wrappers bind (material block follows at +0xcc).
extern u8 g_CameraWorkBaseA; // TH10 flt_491d7c
extern u8 g_CameraWorkBaseB; // TH10 dword_491e94

extern u32 *g_OwnerCameraWorkPtr;   // TH10 dword_491fac
extern u32 g_OwnerDrawMode;         // TH10 dword_491fb0
extern u32 g_OwnerClearPending;     // TH10 dword_492378
extern u32 g_OwnerSentinelA;        // TH10 dword_491e64
extern u32 g_OwnerSentinelB;        // TH10 dword_491e68

// TH10 0x00421480 (registered).
extern void *UpdateMainChainD3DFrameStateEdiAbi(void *work);

// TH10 0x00448980 (registered): kind chain dispatch (EAX = kind,
// EDI = owner).
extern i32 DrawLargeRenderOwnerKindChain(void *owner, u32 kind);

// Shared tail of 0x00448770/0x004487c0/0x00448810: bind camera work B and
// draw kind 15 with draw-mode 1.
i32 DrawOwnerChainCameraWorkB()
{
    g_OwnerCameraWorkPtr = reinterpret_cast<u32 *>(&g_CameraWorkBaseB);
    (void)UpdateMainChainD3DFrameStateEdiAbi(&g_CameraWorkBaseB);

    D3D9Device *device = g_MainChainD3DDevice;
    typedef void (TH10_STDCALL *SetMaterialFn)(D3D9Device *,
                                               const void *);
    reinterpret_cast<SetMaterialFn>(device->vtable[0xbc / 4])(
        device, reinterpret_cast<const u8 *>(*g_OwnerCameraWorkPtr) + 0xccU);
    g_OwnerDrawMode = 1U;
    return DrawLargeRenderOwnerKindChain(g_MainChainRenderOwner, 0xfU);
}

} // namespace

// TH10 0x004485f0. Native ECX = owner; plain kind-0 dispatch.
i32 DrawOwnerChainPlainEcxAbi(void *owner)
{
    return DrawLargeRenderOwnerKindChain(owner, 0U);
}

// TH10 0x00448620. Native ECX = owner. Binds camera work A (the shared
// flt_491d7c block), resets the draw-mode sentinel and, when a background
// clear is pending, flushes and clears render state 28 before the kind-3
// dispatch.
i32 DrawOwnerChainWithClearEcxAbi(void *owner)
{
    g_OwnerCameraWorkPtr = reinterpret_cast<u32 *>(&g_CameraWorkBaseA);
    (void)UpdateMainChainD3DFrameStateEdiAbi(&g_CameraWorkBaseA);

    D3D9Device *device = g_MainChainD3DDevice;
    typedef void (TH10_STDCALL *SetMaterialFn)(D3D9Device *,
                                               const void *);
    reinterpret_cast<SetMaterialFn>(device->vtable[0xbc / 4])(
        device, reinterpret_cast<const u8 *>(*g_OwnerCameraWorkPtr) + 0xccU);
    g_OwnerDrawMode = 0U;
    if (g_OwnerClearPending != 0U) {
        extern void FlushRenderOwnerPendingVerticesEsiAbi(
            RenderOwnerPartial * owner);
        FlushRenderOwnerPendingVerticesEsiAbi(
            reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
        typedef i32 (TH10_STDCALL *SetRenderStateFn)(D3D9Device *, u32,
                                                     u32);
        reinterpret_cast<SetRenderStateFn>(device->vtable[0xe4 / 4])(
            device, 0x1cU, 0U);
        g_OwnerClearPending = 0U;
    }
    return DrawLargeRenderOwnerKindChain(owner, 3U);
}

// TH10 0x00448740. Native ECX = owner. Resets the two camera sentinels and
// the owner camera pair (+0x5c/+0x60), then dispatches kind 0xe.
i32 DrawOwnerChainAfterResetEcxAbi(void *owner)
{
    g_OwnerSentinelA = 0;
    g_OwnerSentinelB = 0;
    WriteOwnerU32(owner, 0x5cU, 0U);
    WriteOwnerU32(owner, 0x60U, 0U);
    return DrawLargeRenderOwnerKindChain(owner, 0xeU);
}

// TH10 0x00448770. Native ECX = owner. Camera work B variant of the
// kind-15 dispatch.
i32 DrawOwnerChainNoClearAEcxAbi(void *owner)
{
    (void)owner;
    return DrawOwnerChainCameraWorkB();
}

// TH10 0x004487c0. Identical body to 0x00448770 (second call site).
i32 DrawOwnerChainNoClearBEcxAbi(void *owner)
{
    (void)owner;
    return DrawOwnerChainCameraWorkB();
}

// TH10 0x00448810. Identical body to 0x00448770 (third call site).
i32 DrawOwnerChainNoClearCEcxAbi(void *owner)
{
    (void)owner;
    return DrawOwnerChainCameraWorkB();
}

} // namespace th10
