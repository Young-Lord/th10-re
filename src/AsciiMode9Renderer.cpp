#include "AsciiMode9Renderer.hpp"

#include "MainChainRender.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

typedef i32 (TH10_STDCALL *D3DSetTextureFn)(D3D9Device *, u32, void *);
typedef i32 (TH10_STDCALL *D3DSetFVFFn)(D3D9Device *, u32);
typedef i32 (TH10_STDCALL *D3DSetTextureStageStateFn)(D3D9Device *, u32, u32,
                                                        u32);
typedef i32 (TH10_STDCALL *D3DSetRenderStateFn)(D3D9Device *, u32, u32);
typedef i32 (TH10_STDCALL *D3DDrawPrimitiveUPFn)(D3D9Device *, u32, u32,
                                                  const void *, u32);

extern D3D9Device *g_MainChainD3D9Device;

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

void UpdateMode9RenderState(void *owner, const u8 *vm)
{
    u8 *const owner_bytes = static_cast<u8 *>(owner);
    const u32 flags = *reinterpret_cast<const u32 *>(vm + 0x35c);
    const u8 blend = static_cast<u8>((flags >> 4) & 3);
    if (owner_bytes[0x3ada68] != blend) {
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
        owner_bytes[0x3ada68] = blend;
        if (blend == 0 || blend == 1) {
            (void)reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
                g_MainChainD3D9Device, 57))(g_MainChainD3D9Device, 0x14,
                                             blend == 0 ? 6 : 2);
        }
    }
    const u8 sampler = static_cast<u8>(flags >> 31);
    if (owner_bytes[0x3ada6e] != sampler) {
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
        owner_bytes[0x3ada6e] = sampler;
        const u32 value = sampler == 0 ? 2 : 1;
        (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
            g_MainChainD3D9Device, 67))(g_MainChainD3D9Device, 0, 5, value);
        (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
            g_MainChainD3D9Device, 67))(g_MainChainD3D9Device, 0, 6, value);
    }
    ++*reinterpret_cast<u32 *>(owner_bytes + 0x54);
}

} // namespace

i32 DrawAsciiAnimationVmMode9(void *vm_memory, void *owner,
                               const void *vertices, u32 vertex_count)
{
    const u8 *const vm = static_cast<const u8 *>(vm_memory);
    u8 *const owner_bytes = static_cast<u8 *>(owner);
    const u32 flags = *reinterpret_cast<const u32 *>(vm + 0x35c);
    if ((flags & 3U) != 3U || *(vm + 0x2ff) == 0)
        return -1;
    if (*reinterpret_cast<u32 *>(owner_bytes + 0x3adac8) != 0)
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
    u8 *const glyph = *reinterpret_cast<u8 *const *>(vm + 0x394);
    void *const texture = *reinterpret_cast<void **>(glyph + 4);
    if (*reinterpret_cast<void **>(owner_bytes + 0x3ada64) != texture) {
        *reinterpret_cast<void **>(owner_bytes + 0x3ada64) = texture;
        (void)reinterpret_cast<D3DSetTextureFn>(GetD3DSlot(
            g_MainChainD3D9Device, 65))(g_MainChainD3D9Device, 0, texture);
    }
    if (owner_bytes[0x3ada6a] != 3) {
        (void)reinterpret_cast<D3DSetFVFFn>(GetD3DSlot(
            g_MainChainD3D9Device, 89))(g_MainChainD3D9Device, 0x144);
        owner_bytes[0x3ada6a] = 3;
    }
    UpdateMode9RenderState(owner, vm);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        g_MainChainD3D9Device, 67))(g_MainChainD3D9Device, 0, 6, 0);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        g_MainChainD3D9Device, 67))(g_MainChainD3D9Device, 0, 3, 0);
    (void)reinterpret_cast<D3DDrawPrimitiveUPFn>(GetD3DSlot(
        g_MainChainD3D9Device, 83))(g_MainChainD3D9Device, 5, vertex_count - 2,
                                     vertices, 0x1c);
    return 0;
}

} // namespace th10
