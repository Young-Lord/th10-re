#include "AsciiMode9Renderer.hpp"

#include "LargeRenderOwnerLayout.hpp"
#include "MainChainRender.hpp"
#include "Th10Platform.hpp"
#include "VmRecord.hpp"

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
    const VmRecord &vm_record = *reinterpret_cast<const VmRecord *>(vm);
    LargeRenderOwnerLayout &owner_ref = *static_cast<LargeRenderOwnerLayout *>(owner);
    const u32 flags = vm_record.flags;
    const u8 blend = static_cast<u8>((flags >> 4) & 3);
    if (owner_ref.blend_mode_cache != blend) {
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
        owner_ref.blend_mode_cache = blend;
        if (blend == 0 || blend == 1) {
            (void)reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
                g_MainChainD3D9Device, 57))(g_MainChainD3D9Device, 0x14,
                                             blend == 0 ? 6 : 2);
        }
    }
    const u8 sampler = static_cast<u8>(flags >> 31);
    if (owner_ref.sampler_filter_cache != sampler) {
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
        owner_ref.sampler_filter_cache = sampler;
        const u32 value = sampler == 0 ? 2 : 1;
        (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
            g_MainChainD3D9Device, 67))(g_MainChainD3D9Device, 0, 5, value);
        (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
            g_MainChainD3D9Device, 67))(g_MainChainD3D9Device, 0, 6, value);
    }
    ++owner_ref.render_mode_counter;
}

} // namespace

i32 DrawAsciiAnimationVmMode9(void *vm_memory, void *owner,
                               const void *vertices, u32 vertex_count)
{
    const u8 *const vm = static_cast<const u8 *>(vm_memory);
    const VmRecord &vm_record = *reinterpret_cast<const VmRecord *>(vm);
    LargeRenderOwnerLayout &owner_ref = *static_cast<LargeRenderOwnerLayout *>(owner);
    const u32 flags = vm_record.flags;
    if ((flags & 3U) != 3U || *(vm + 0x2ff) == 0)
        return -1;
    if (owner_ref.pending_quad_count != 0)
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
    u8 *const glyph = static_cast<u8 *>(vm_record.anim_entry);
    void *const texture = *reinterpret_cast<void **>(glyph + 4);
    if (owner_ref.bound_texture != texture) {
        owner_ref.bound_texture = texture;
        (void)reinterpret_cast<D3DSetTextureFn>(GetD3DSlot(
            g_MainChainD3D9Device, 65))(g_MainChainD3D9Device, 0, texture);
    }
    if (owner_ref.fvf_active_cache != 3) {
        (void)reinterpret_cast<D3DSetFVFFn>(GetD3DSlot(
            g_MainChainD3D9Device, 89))(g_MainChainD3D9Device, 0x144);
        owner_ref.fvf_active_cache = 3;
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
