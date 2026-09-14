// TH10 0x43bfa0 — ending overlay quad submission (ending.cpp cluster, the
// entry right before the full-screen rectangle callbacks 0x43c1a0+).
//
// Native EDI = four-dword record, stack = four scalars {arg1..arg4}; ret
// 0x10. The body flushes the render-owner vertex buffer, assembles the
// 0x50-byte triangle-strip vertex block on the stack, runs the ending's
// texture-stage state block, draws two triangle-strip primitives with FVF
// XYZRHW|DIFFUSE (0x44, stride 0x14), resets the render-owner draw caches
// and restores the texture-stage state.
//
// The vertex block is reproduced cell-by-cell from the compiler-shuffled
// store sequence (0x43bfaf..0x43c097) — several cells receive values that
// later writes overwrite (arg2 is dead, arg4 lands past the drawn vertices),
// which is preserved literally.
#include "EndingOverlayQuad.hpp"

#include "MainChainRender.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

extern D3D9Device *g_D3D9ClearDevice;   // TH10 DAT_00491c30
extern void *g_MainChainRenderOwner;    // TH10 DAT_00491c10

// FVF 0x44 vertex (XYZRHW | DIFFUSE), stride 0x14.
struct OverlayQuadVertex {
    float x;
    float y;
    float z;
    float rhw;
    u32 color;
};

typedef i32 (TH10_STDCALL *D3DSetTextureStageStateFn)(D3D9Device *, u32,
                                                      u32, u32);
typedef i32 (TH10_STDCALL *D3DSetRenderStateFn)(D3D9Device *, u32, u32);
typedef i32 (TH10_STDCALL *D3DSetFvFFn)(D3D9Device *, u32);
typedef i32 (TH10_STDCALL *D3DDrawPrimitiveUPFn)(D3D9Device *, u32, u32,
                                                 const void *, u32);
// Native quirk: one post-epilogue stage-state call pushes only the stage
// argument; modeled with its own single-argument slot type.
typedef i32 (TH10_STDCALL *D3DSetTextureStageStageOnlyFn)(D3D9Device *, u32);

inline void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

inline u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

inline void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

const float kOne = 1.0f;

} // namespace

void DrawEndingOverlayQuadStackAbi(const void *record, u32 arg1, u32 arg2,
                                   u32 arg3, u32 arg4)
{
    (void)arg2; // written to a cell that 1.0f overwrites (native quirk)
    (void)arg4; // written past the drawn vertex data (native quirk)

    FlushRenderOwnerPendingVertices(
        reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));

    const u32 rec0 = LoadU32At(record, 0U);
    const u32 rec1 = LoadU32At(record, 4U);
    const u32 rec2 = LoadU32At(record, 8U);
    const u32 rec3 = LoadU32At(record, 0xcU);

    OverlayQuadVertex vertices[5]; // 0x14 bytes of tail locals follow
    OverlayQuadVertex &v0 = vertices[0];
    OverlayQuadVertex &v1 = vertices[1];
    OverlayQuadVertex &v2 = vertices[2];
    OverlayQuadVertex &v3 = vertices[3];
    u32 *const tail = reinterpret_cast<u32 *>(vertices + 4);

    v0.x = reinterpret_cast<const float &>(rec2);
    v0.y = reinterpret_cast<const float &>(rec3);
    v0.z = 0.0f;
    v0.rhw = reinterpret_cast<const float &>(rec0);
    v0.color = rec1;

    v1.x = 0.0f;
    v1.y = kOne;
    v1.z = reinterpret_cast<const float &>(arg1);
    v1.rhw = reinterpret_cast<const float &>(rec2);
    v1.color = rec1;

    v2.x = 0.0f;
    v2.y = kOne;
    v2.z = kOne;
    v2.rhw = reinterpret_cast<const float &>(rec0);
    v2.color = rec3;

    v3.x = 0.0f;
    v3.y = kOne;
    v3.z = reinterpret_cast<const float &>(arg3);
    v3.rhw = reinterpret_cast<const float &>(rec2);
    v3.color = 0x3f800000U; // 1.0f bit pattern (native quirk)

    // Tail locals below the vertex block (E-0xc..E-0x4).
    tail[0] = 0U;
    tail[1] = 0x3f800000U;
    tail[2] = arg4;

    D3D9Device *const device = g_D3D9ClearDevice;

    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(device, 67))(
        device, 0, 4, 2);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(device, 67))(
        device, 0, 1, 2);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(device, 67))(
        device, 0, 5, 0);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(device, 67))(
        device, 0, 2, 0);
    (void)reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(device, 57))(
        device, 0x14, 6);
    (void)reinterpret_cast<D3DSetFvFFn>(GetD3DSlot(device, 89))(device, 0x44);
    (void)reinterpret_cast<D3DDrawPrimitiveUPFn>(GetD3DSlot(device, 83))(
        device, 5, 2, vertices, 0x14);

    // Reset the render-owner draw caches (native order preserved).
    StoreU32At(g_MainChainRenderOwner, 0x3ada6aU, 0xffU);
    StoreU32At(g_MainChainRenderOwner, 0x3ada70U, 0U);
    StoreU32At(g_MainChainRenderOwner, 0x3ada64U, 0U);
    StoreU32At(g_MainChainRenderOwner, 0x3ada69U, 0xffU);
    StoreU32At(g_MainChainRenderOwner, 0x3ada68U, 3U);
    StoreU32At(g_MainChainRenderOwner, 0x3ada6bU, 0xffU);

    // Native quirk: this stage-state call pushes only the stage argument.
    (void)reinterpret_cast<D3DSetTextureStageStageOnlyFn>(GetD3DSlot(
        device, 67))(device, 0);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(device, 67))(
        device, 0, 1, 4);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(device, 67))(
        device, 0, 5, 2);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(device, 67))(
        device, 0, 2, 2);
}

} // namespace th10
