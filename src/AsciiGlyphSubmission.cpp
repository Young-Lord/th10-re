#include <math.h>

#include "LargeRenderOwnerLayout.hpp"
#include "MainChainRender.hpp"
#include "Th10Platform.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

struct AsciiGlyphVertex {
    float x;
    float y;
    float z;
    float rhw;
    u32 color;
    float u;
    float v;
};

typedef i32 (TH10_STDCALL *D3DSetTextureFn)(D3D9Device *, u32, void *);
typedef i32 (TH10_STDCALL *D3DSetRenderStateFn)(D3D9Device *, u32, u32);
typedef i32 (TH10_STDCALL *D3DSetSamplerStateFn)(D3D9Device *, u32, u32, u32);

extern D3D9Device *g_MainChainD3D9Device; // TH10 DAT_00491c30
extern void *g_MainChainActiveCameraWork; // TH10 DAT_00491fac
extern AsciiGlyphVertex g_AsciiGlyphScratchQuad[4]; // TH10 0x4978c0

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

u32 MultiplyColorChannel(u32 channel, u32 factor)
{
    u32 result = (channel & 0xffU) * (factor & 0xffU) >> 7;
    if (result > 0xffU)
        result = 0xffU;
    return result;
}

// The program's x87 control word uses round-to-nearest/even at this callsite.
// Keep the semantic operation in C++ rather than exposing the FRNDINT ABI.
float RoundGlyphCoordinate(float value)
{
    const float lower = static_cast<float>(floor(static_cast<double>(value)));
    const float fraction = value - lower;
    if (fraction < 0.5f)
        return lower;
    if (fraction > 0.5f)
        return lower + 1.0f;
    return (static_cast<i32>(lower) & 1) == 0 ? lower : lower + 1.0f;
}

u32 ModulateColor(u32 color, const LargeRenderOwnerLayout &owner)
{
    // Byte-wise reads of the +0x732458 packed clear/modulation color.
    const u8 *const clear_color_bytes =
        reinterpret_cast<const u8 *>(&owner.clear_color);
    const u32 blue = MultiplyColorChannel(color, clear_color_bytes[0]);
    const u32 green = MultiplyColorChannel(color >> 8, clear_color_bytes[1]);
    const u32 red = MultiplyColorChannel(color >> 16, clear_color_bytes[2]);
    const u32 alpha = MultiplyColorChannel(color >> 24, clear_color_bytes[3]);
    return blue | (green << 8) | (red << 16) | (alpha << 24);
}

void FlushGlyphOwnerVertices(void *owner)
{
    FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
}

void UpdateGlyphRenderStates(void *vm, void *owner)
{
    const VmRecord &vm_record = *reinterpret_cast<const VmRecord *>(vm);
    LargeRenderOwnerLayout &owner_ref = *static_cast<LargeRenderOwnerLayout *>(owner);
    const u32 flags = vm_record.flags;
    const u8 blend_mode = static_cast<u8>((flags >> 4) & 3);
    if (owner_ref.blend_mode_cache != blend_mode) {
        FlushGlyphOwnerVertices(owner);
        owner_ref.blend_mode_cache = blend_mode;
        if (blend_mode == 0) {
            (void)reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
                g_MainChainD3D9Device, 57))(g_MainChainD3D9Device, 0x14, 6);
        } else if (blend_mode == 1) {
            (void)reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
                g_MainChainD3D9Device, 57))(g_MainChainD3D9Device, 0x14, 2);
        }
    }

    const u8 sampler_mode = static_cast<u8>(flags >> 31);
    if (owner_ref.sampler_filter_cache != sampler_mode) {
        FlushGlyphOwnerVertices(owner);
        owner_ref.sampler_filter_cache = sampler_mode;
        const u32 value = sampler_mode == 0 ? 2 : 1;
        (void)reinterpret_cast<D3DSetSamplerStateFn>(GetD3DSlot(
            g_MainChainD3D9Device, 69))(g_MainChainD3D9Device, 0, 5, value);
        (void)reinterpret_cast<D3DSetSamplerStateFn>(GetD3DSlot(
            g_MainChainD3D9Device, 69))(g_MainChainD3D9Device, 0, 6, value);
    }
    ++owner_ref.render_mode_counter;
}

bool IntersectsActiveViewport()
{
    float min_x = g_AsciiGlyphScratchQuad[0].x;
    float max_x = min_x;
    float min_y = g_AsciiGlyphScratchQuad[0].y;
    float max_y = min_y;
    for (u32 index = 1; index != 4; ++index) {
        const AsciiGlyphVertex &vertex = g_AsciiGlyphScratchQuad[index];
        if (vertex.x < min_x)
            min_x = vertex.x;
        if (max_x < vertex.x)
            max_x = vertex.x;
        if (vertex.y < min_y)
            min_y = vertex.y;
        if (max_y < vertex.y)
            max_y = vertex.y;
    }

    const u8 *const view = static_cast<const u8 *>(g_MainChainActiveCameraWork);
    const float left = static_cast<float>(*reinterpret_cast<const i32 *>(view + 0xcc));
    const float top = static_cast<float>(*reinterpret_cast<const i32 *>(view + 0xd0));
    const float right = left + static_cast<float>(*reinterpret_cast<const i32 *>(view + 0xd4));
    const float bottom = top + static_cast<float>(*reinterpret_cast<const i32 *>(view + 0xd8));
    return !(max_x < left || max_y < top || right < min_x || bottom < min_y);
}

void AppendAsciiGlyphScratchQuadAsTriangles(void *owner)
{
    LargeRenderOwnerLayout &owner_ref = *static_cast<LargeRenderOwnerLayout *>(owner);
    AsciiGlyphVertex *const destination =
        static_cast<AsciiGlyphVertex *>(owner_ref.vertex_write_cursor);
    destination[0] = g_AsciiGlyphScratchQuad[0];
    destination[1] = g_AsciiGlyphScratchQuad[1];
    destination[2] = g_AsciiGlyphScratchQuad[2];
    destination[3] = g_AsciiGlyphScratchQuad[1];
    destination[4] = g_AsciiGlyphScratchQuad[2];
    destination[5] = g_AsciiGlyphScratchQuad[3];
    owner_ref.vertex_write_cursor = destination + 6;
    ++owner_ref.pending_quad_count;
}

} // namespace

// TH10 0x00442670 semantic body. The original receives EAX=vm, ECX=owner,
// and one stack flag; typed callers leave that mixed ABI to a future thunk.
void SubmitAsciiGlyphScratchQuad(void *vm, void *owner, u32 flags)
{
    const VmRecord &vm_record = *reinterpret_cast<const VmRecord *>(vm);
    LargeRenderOwnerLayout &owner_ref = *static_cast<LargeRenderOwnerLayout *>(owner);
    const float offset_x =
        *reinterpret_cast<const float *>(&owner_ref.camera_value_005c);
    const float offset_y =
        *reinterpret_cast<const float *>(&owner_ref.camera_value_0060);
    for (u32 index = 0; index != 4; ++index) {
        g_AsciiGlyphScratchQuad[index].x += offset_x;
        g_AsciiGlyphScratchQuad[index].y += offset_y;
    }
    if ((flags & 1) != 0) {
        const float snapped_x0 = RoundGlyphCoordinate(
            g_AsciiGlyphScratchQuad[0].x) - 0.5f;
        const float snapped_x1 = RoundGlyphCoordinate(
            g_AsciiGlyphScratchQuad[1].x) - 0.5f;
        const float snapped_y0 = RoundGlyphCoordinate(
            g_AsciiGlyphScratchQuad[0].y) - 0.5f;
        const float snapped_y1 = RoundGlyphCoordinate(
            g_AsciiGlyphScratchQuad[2].y) - 0.5f;
        g_AsciiGlyphScratchQuad[0].x = g_AsciiGlyphScratchQuad[2].x = snapped_x0;
        g_AsciiGlyphScratchQuad[1].x = g_AsciiGlyphScratchQuad[3].x = snapped_x1;
        g_AsciiGlyphScratchQuad[0].y = g_AsciiGlyphScratchQuad[1].y = snapped_y0;
        g_AsciiGlyphScratchQuad[2].y = g_AsciiGlyphScratchQuad[3].y = snapped_y1;
    }

    const u8 *const glyph = static_cast<const u8 *>(vm_record.anim_entry);
    const float u_offset = vm_record.texture_u;
    const float v_offset = vm_record.texture_v;
    g_AsciiGlyphScratchQuad[0].u = g_AsciiGlyphScratchQuad[2].u =
        *reinterpret_cast<const float *>(glyph + 0x20) + u_offset;
    g_AsciiGlyphScratchQuad[1].u = g_AsciiGlyphScratchQuad[3].u =
        *reinterpret_cast<const float *>(glyph + 0x28) + u_offset;
    g_AsciiGlyphScratchQuad[0].v = g_AsciiGlyphScratchQuad[1].v =
        *reinterpret_cast<const float *>(glyph + 0x24) + v_offset;
    g_AsciiGlyphScratchQuad[2].v = g_AsciiGlyphScratchQuad[3].v =
        *reinterpret_cast<const float *>(glyph + 0x2c) + v_offset;
    if (!IntersectsActiveViewport())
        return;

    void *const texture = *reinterpret_cast<void *const *>(glyph + 4);
    if (owner_ref.bound_texture != texture) {
        owner_ref.bound_texture = texture;
        FlushGlyphOwnerVertices(owner);
        (void)reinterpret_cast<D3DSetTextureFn>(GetD3DSlot(
            g_MainChainD3D9Device, 65))(g_MainChainD3D9Device, 0, texture);
    }
    if (owner_ref.fvf_active_cache != 1) {
        FlushGlyphOwnerVertices(owner);
        owner_ref.fvf_active_cache = 1;
    }
    if ((flags & 2) == 0) {
        const u32 vm_flags = vm_record.flags;
        u32 color = (vm_flags & 0x8000U) != 0 ? vm_record.secondary_color :
            vm_record.primary_color;
        if (owner_ref.custom_color_gate != 0)
            color = ModulateColor(color, owner_ref);
        for (u32 index = 0; index != 4; ++index)
            g_AsciiGlyphScratchQuad[index].color = color;
    }
    UpdateGlyphRenderStates(vm, owner);
    AppendAsciiGlyphScratchQuadAsTriangles(owner);
}

} // namespace th10
