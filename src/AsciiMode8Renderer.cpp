#include "AsciiMode8Renderer.hpp"

#include <math.h>
#include <string.h>

#include "LargeRenderOwnerLayout.hpp"
#include "MainChainRender.hpp"
#include "Th10Platform.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

typedef i32 (TH10_STDCALL *D3DSetTransformFn)(D3D9Device *, u32, const D3DMatrix *);
typedef i32 (TH10_STDCALL *D3DSetTextureFn)(D3D9Device *, u32, void *);
typedef i32 (TH10_STDCALL *D3DSetStreamSourceFn)(D3D9Device *, u32, void *,
                                                   u32, u32);
typedef i32 (TH10_STDCALL *D3DSetFVFFn)(D3D9Device *, u32);
typedef i32 (TH10_STDCALL *D3DSetTextureStageStateFn)(D3D9Device *, u32, u32,
                                                        u32);
typedef i32 (TH10_STDCALL *D3DSetRenderStateFn)(D3D9Device *, u32, u32);
typedef i32 (TH10_STDCALL *D3DDrawPrimitiveFn)(D3D9Device *, u32, u32, u32);

extern D3D9Device *g_MainChainD3D9Device;
extern void D3dxMatrixRotationX(D3DMatrix *out, float angle);
extern void D3dxMatrixRotationY(D3DMatrix *out, float angle);
extern void D3dxMatrixRotationZ(D3DMatrix *out, float angle);
extern void D3dxMatrixMultiply(D3DMatrix *out, const D3DMatrix *left,
                               const D3DMatrix *right);

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

u32 ReadU32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

u32 ModulateColor(u32 color, const LargeRenderOwnerLayout &owner)
{
    // Byte-wise reads of the +0x732458 packed clear/modulation color.
    const u8 *const clear_color_bytes =
        reinterpret_cast<const u8 *>(&owner.clear_color);
    u32 output = 0;
    for (u32 index = 0; index != 4; ++index) {
        u32 channel = ((color >> (index * 8)) & 0xffU) *
            clear_color_bytes[index] >> 7;
        if (channel > 0xffU)
            channel = 0xffU;
        output |= channel << (index * 8);
    }
    return output;
}

void RebuildMode8MatrixIfDirty(u8 *vm)
{
    VmRecord &vm_record = *reinterpret_cast<VmRecord *>(vm);
    u32 flags = vm_record.flags;
    if ((flags & 0x4000U) != 0 || (flags & 0x0cU) == 0)
        return;
    D3DMatrix *const matrix =
        reinterpret_cast<D3DMatrix *>(vm_record.world_matrix);
    memcpy(matrix, vm_record.base_matrix, sizeof(*matrix));
    matrix->values[0] *= vm_record.scale_x;
    matrix->values[5] *= vm_record.scale_y;
    vm_record.flags = flags & ~0x08U;
    D3DMatrix rotation;
    const float angles[] = {vm_record.rotation_x, vm_record.rotation_y,
                            vm_record.rotation_z};
    for (u32 index = 0; index != 3; ++index) {
        if (angles[index] == 0.0f || angles[index] != angles[index])
            continue;
        if (index == 0)
            D3dxMatrixRotationX(&rotation, angles[index]);
        else if (index == 1)
            D3dxMatrixRotationY(&rotation, angles[index]);
        else
            D3dxMatrixRotationZ(&rotation, angles[index]);
        D3dxMatrixMultiply(matrix, matrix, &rotation);
    }
    vm_record.flags &= ~0x04U;
}

void UpdateMode8SharedState(void *owner, const u8 *vm)
{
    const VmRecord &vm_record = *reinterpret_cast<const VmRecord *>(vm);
    LargeRenderOwnerLayout &owner_ref = *static_cast<LargeRenderOwnerLayout *>(owner);
    const u32 flags = vm_record.flags;
    const u8 blend = static_cast<u8>((flags >> 4) & 3);
    if (owner_ref.blend_mode_cache != blend) {
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
        owner_ref.blend_mode_cache = blend;
        if (blend == 0 || blend == 1 || blend == 2) {
            const u32 value = blend == 0 ? 6 : 2;
            (void)reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
                g_MainChainD3D9Device, 57))(g_MainChainD3D9Device, 0x14, value);
        }
    }
    u32 color = (flags & 0x8000U) != 0 ? vm_record.secondary_color :
        vm_record.primary_color;
    if (owner_ref.custom_color_gate != 0)
        color = ModulateColor(color, owner_ref);
    if (owner_ref.modulation_color != color) {
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
        owner_ref.modulation_color = color;
        (void)reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
            g_MainChainD3D9Device, 57))(g_MainChainD3D9Device, 0x3c, color);
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

i32 DrawAsciiAnimationVmMode8(void *vm_memory, void *owner)
{
    u8 *const vm = static_cast<u8 *>(vm_memory);
    const VmRecord &vm_record = *reinterpret_cast<const VmRecord *>(vm);
    LargeRenderOwnerLayout &owner_ref = *static_cast<LargeRenderOwnerLayout *>(owner);
    if ((vm_record.flags & 3U) != 3U || *(vm + 0x2ff) == 0)
        return -1;
    if (owner_ref.pending_quad_count != 0)
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
    RebuildMode8MatrixIfDirty(vm);

    D3DMatrix world =
        *reinterpret_cast<const D3DMatrix *>(vm_record.world_matrix);
    const u32 flags = vm_record.flags;
    const float x = vm_record.base_pos_x + vm_record.delta_pos_x +
        vm_record.alt_pos_x;
    const float y = vm_record.base_pos_y + vm_record.delta_pos_y +
        vm_record.alt_pos_y;
    const float z = vm_record.base_pos_z + vm_record.delta_pos_z +
        vm_record.alt_pos_z;
    const float half_width = static_cast<float>(fabs(static_cast<double>(
        ReadFloat(vm, 0x4c) * ReadFloat(vm, 0x3c) * 0.5f)));
    const float half_height = static_cast<float>(fabs(static_cast<double>(
        ReadFloat(vm, 0x50) * ReadFloat(vm, 0x40) * 0.5f)));
    switch ((flags >> 18) & 3) {
    case 0: world.values[12] = x; break;
    case 1: world.values[12] = x - half_width; break;
    case 2: world.values[12] = x + half_width; break;
    }
    switch ((flags >> 20) & 3) {
    case 0: world.values[13] = y; break;
    case 1: world.values[13] = y - half_height; break;
    case 2: world.values[13] = y + half_height; break;
    }
    world.values[14] = z;
    UpdateMode8SharedState(owner, vm);
    (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(
        g_MainChainD3D9Device, 44))(g_MainChainD3D9Device, 0x100, &world);

    u8 *const glyph = static_cast<u8 *>(vm_record.anim_entry);
    void *const texture = *reinterpret_cast<void **>(glyph + 4);
    if (owner_ref.bound_texture != texture) {
        owner_ref.bound_texture = texture;
        (void)reinterpret_cast<D3DSetTextureFn>(GetD3DSlot(
            g_MainChainD3D9Device, 65))(g_MainChainD3D9Device, 0, texture);
    }
    const float u_offset = vm_record.texture_u;
    if (owner_ref.glyph_texture_cache != glyph ||
        (u_offset == u_offset && u_offset != 0.0f)) {
        owner_ref.glyph_texture_cache = glyph;
        D3DMatrix texture_matrix =
            *reinterpret_cast<const D3DMatrix *>(vm_record.texture_matrix);
        texture_matrix.values[8] = ReadFloat(glyph, 0x20) + u_offset;
        texture_matrix.values[9] = ReadFloat(glyph, 0x24) + vm_record.texture_v;
        (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(
            g_MainChainD3D9Device, 44))(g_MainChainD3D9Device, 0x10,
                                        &texture_matrix);
    }
    if (owner_ref.fvf_active_cache != 2) {
        void *const vertex_buffer = owner_ref.com_viewport_interface;
        (void)reinterpret_cast<D3DSetStreamSourceFn>(GetD3DSlot(
            g_MainChainD3D9Device, 100))(g_MainChainD3D9Device, 0,
                                          vertex_buffer, 0, 0x14);
        (void)reinterpret_cast<D3DSetFVFFn>(GetD3DSlot(
            g_MainChainD3D9Device, 89))(g_MainChainD3D9Device, 0x102);
        (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
            g_MainChainD3D9Device, 67))(g_MainChainD3D9Device, 0, 6, 3);
        (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
            g_MainChainD3D9Device, 67))(g_MainChainD3D9Device, 0, 3, 3);
        owner_ref.fvf_active_cache = 2;
    }
    (void)reinterpret_cast<D3DDrawPrimitiveFn>(GetD3DSlot(
        g_MainChainD3D9Device, 81))(g_MainChainD3D9Device, 5, 0, 2);
    return 0;
}

} // namespace th10
