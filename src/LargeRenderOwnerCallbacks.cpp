#include "LargeRenderOwnerFrameLoop.hpp"

#include "CallbackScheduler.hpp"
#include "MainChainRender.hpp"
#include "MainChainStartupGlobals.hpp"

namespace th10 {

namespace {

extern MainChainStartupGlobalStorage g_MainChainStartupGlobals;
extern void *g_MainChainActiveCameraWork;
extern u32 g_AsciiActiveViewIsDefault;
extern u32 g_AsciiFogEnableCache;
extern float g_AsciiOverlayRenderOffsetX;
extern float g_AsciiOverlayRenderOffsetY;
extern D3D9Device *g_MainChainD3D9Device;

typedef i32 (TH10_STDCALL *D3DSetTransformFn)(D3D9Device *, u32, const D3DMatrix *);
typedef i32 (TH10_STDCALL *D3DSetViewportFn)(D3D9Device *, const D3DViewport *);

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

i32 TH10_FASTCALL CalcUpdateListA(void *owner)
{
    UpdateLargeRenderOwnerListA(owner);
    return 1;
}

i32 TH10_FASTCALL CalcUpdateListB(void *owner)
{
    UpdateLargeRenderOwnerListB(owner);
    return 1;
}

i32 TH10_FASTCALL DrawKind0(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 0); }
i32 TH10_FASTCALL DrawKind1(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 1); }
i32 TH10_FASTCALL DrawKind2(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 2); }

i32 TH10_FASTCALL DrawKind3WithD3DSetup(void *owner)
{
    g_MainChainActiveCameraWork = &g_MainChainStartupGlobals;
    UpdateMainChainD3DFrameStateEdiAbi(
        reinterpret_cast<MainChainCameraWork *>(g_MainChainActiveCameraWork));
    (void)reinterpret_cast<D3DSetViewportFn>(GetD3DSlot(
        g_MainChainD3D9Device, 47))(
        g_MainChainD3D9Device,
        reinterpret_cast<const D3DViewport *>(
            static_cast<u8 *>(g_MainChainActiveCameraWork) + 0xcc));

    if (g_AsciiFogEnableCache != 0) {
        FlushRenderOwnerPendingVertices(
            reinterpret_cast<RenderOwnerPartial *>(owner));
        (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(
            g_MainChainD3D9Device, 57))(
            g_MainChainD3D9Device, 0x1c, 0);
        g_AsciiFogEnableCache = 0;
    }

    return DrawLargeRenderOwnerKindChain(owner, 3);
}

i32 TH10_FASTCALL DrawKind4(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 4); }
i32 TH10_FASTCALL DrawKind5(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 5); }
i32 TH10_FASTCALL DrawKind6(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 6); }
i32 TH10_FASTCALL DrawKind7(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 7); }
i32 TH10_FASTCALL DrawKind8(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 8); }
i32 TH10_FASTCALL DrawKind9(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 9); }
i32 TH10_FASTCALL DrawKind10(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 10); }
i32 TH10_FASTCALL DrawKind11(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 11); }
i32 TH10_FASTCALL DrawKind12(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 12); }
i32 TH10_FASTCALL DrawKind13(void *owner) { return DrawLargeRenderOwnerKindChain(owner, 13); }

i32 TH10_FASTCALL DrawKind14ClearGlobals(void *owner)
{
    g_AsciiOverlayRenderOffsetX = 0.0f;
    g_AsciiOverlayRenderOffsetY = 0.0f;
    u8 *const owner_bytes = reinterpret_cast<u8 *>(owner);
    *reinterpret_cast<u32 *>(owner_bytes + 0x5c) = 0;
    *reinterpret_cast<u32 *>(owner_bytes + 0x60) = 0;
    return DrawLargeRenderOwnerKindChain(owner, 14);
}

i32 TH10_FASTCALL DrawKind15WithMatrix(void *owner)
{
    g_MainChainActiveCameraWork = &g_MainChainStartupGlobals.block_b;
    UpdateMainChainD3DFrameStateEdiAbi(
        reinterpret_cast<MainChainCameraWork *>(g_MainChainActiveCameraWork));
    (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(
        g_MainChainD3D9Device, 47))(
        g_MainChainD3D9Device, 2,
        &reinterpret_cast<MainChainCameraWork *>(g_MainChainActiveCameraWork)->view);
    g_AsciiActiveViewIsDefault = 1;
    return DrawLargeRenderOwnerKindChain(owner, 15);
}

i32 TH10_FASTCALL DrawKind16WithMatrix(void *owner)
{
    g_MainChainActiveCameraWork = &g_MainChainStartupGlobals.block_b;
    UpdateMainChainD3DFrameStateEdiAbi(
        reinterpret_cast<MainChainCameraWork *>(g_MainChainActiveCameraWork));
    (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(
        g_MainChainD3D9Device, 47))(
        g_MainChainD3D9Device, 2,
        &reinterpret_cast<MainChainCameraWork *>(g_MainChainActiveCameraWork)->view);
    g_AsciiActiveViewIsDefault = 1;
    return DrawLargeRenderOwnerKindChain(owner, 16);
}

i32 TH10_FASTCALL DrawKind19WithMatrix(void *owner)
{
    g_MainChainActiveCameraWork = &g_MainChainStartupGlobals.block_b;
    UpdateMainChainD3DFrameStateEdiAbi(
        reinterpret_cast<MainChainCameraWork *>(g_MainChainActiveCameraWork));
    (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(
        g_MainChainD3D9Device, 47))(
        g_MainChainD3D9Device, 2,
        &reinterpret_cast<MainChainCameraWork *>(g_MainChainActiveCameraWork)->view);
    g_AsciiActiveViewIsDefault = 1;
    return DrawLargeRenderOwnerKindChain(owner, 19);
}

const ChainCallback kLargeRenderOwnerCallbacks[20] = {
    CalcUpdateListA,
    CalcUpdateListB,
    DrawKind0,
    DrawKind1,
    DrawKind2,
    DrawKind3WithD3DSetup,
    DrawKind4,
    DrawKind5,
    DrawKind6,
    DrawKind7,
    DrawKind8,
    DrawKind9,
    DrawKind10,
    DrawKind11,
    DrawKind12,
    DrawKind13,
    DrawKind14ClearGlobals,
    DrawKind15WithMatrix,
    DrawKind16WithMatrix,
    DrawKind19WithMatrix,
};

} // namespace

ChainCallback GetLargeRenderOwnerCallback(u32 index)
{
    return kLargeRenderOwnerCallbacks[index];
}

} // namespace th10
