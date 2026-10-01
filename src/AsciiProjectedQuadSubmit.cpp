// TH10 0x442ad0 — projected glyph quad submission (the body behind the
// 0x443670 wrapper; operates on the shared glyph scratch quad at 0x4978c0
// and the render owner's caches).
//
// Steps (native order preserved):
//   1. every scratch-quad x += owner+0x5c and y += owner+0x60 (the frame
//      camera offsets mirrored into the owner),
//   2. flags bit 0: snap the quad to the pixel grid — round-half-even each
//      edge (frndint with the game's control word) and subtract 0.5f
//      (flt_4700e8), assigning left/right and top/bottom pairs,
//   3. UVs from the VM's texture-source record ([vm+0x394]):
//        u_left  = record+0x28 + vm+0x54 -> v0.u, v2.u
//        u_right = record+0x20 + vm+0x54 -> v1.u, v3.u
//        v_top   = record+0x24 + vm+0x58 -> v0.v, v1.v
//        v_bot   = record+0x2c + vm+0x58 -> v2.v, v3.v
//   4. viewport cull against DAT_00491fac's D3DViewport (+0xcc x, +0xd0 y,
//      +0xd4 width, +0xd8 height; coordinates converted as unsigned with
//      the fild+2^32 fixup). Native comparison quirks preserved: the x
//      edges reject only when strictly outside (unordered/NaN passes), the
//      top edge additionally rejects equality,
//   5. texture bind: when the resource's +0x04 texture differs from the
//      owner's +0x3ada64 cache, flush, cache and SetTexture(0, tex)
//      (device vtable 65); the +0x3ada6a texture-active flag is flushed and
//      set to 1 when not already 1,
//   6. flags bit 1 clear: color = (vm+0x35c bit 15 set ? +0x300 : +0x2fc),
//      modulated per channel through the 0x4423c0 (a*b)>>7 clamp helper
//      with the owner's +0x732458..b factors when owner+0x73245c is
//      nonzero, published to all four vertex colors,
//   7. glyph render-state update (0x4425a0 semantic) and the quad appended
//      to the owner's vertex buffer as two triangles (0x442fe0 semantic).
#include "AsciiProjectedQuadSubmit.hpp"

#include <cmath>

#include "MainChainRender.hpp"
#include "Th10Platform.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

extern D3D9Device *g_D3D9ClearDevice;       // TH10 DAT_00491c30
extern void *g_MainChainActiveCameraWork;   // TH10 DAT_00491fac

// Shared glyph scratch quad (TH10 0x4978c0): four 0x1c-byte
// {x, y, z, rhw, color, u, v} vertices.
struct GlyphVertex {
    float x;
    float y;
    float z;
    float rhw;
    u32 color;
    float u;
    float v;
};

extern GlyphVertex g_AsciiGlyphScratchQuad[4]; // defined by the glyph modules

const float kHalfPixel = 0.5f; // TH10 flt_4700e8

typedef i32 (TH10_STDCALL *D3DSetTextureFn)(D3D9Device *, u32, void *);
typedef i32 (TH10_STDCALL *D3DSetRenderStateFn)(D3D9Device *, u32, u32);
typedef i32 (TH10_STDCALL *D3DSetSamplerStateFn)(D3D9Device *, u32, u32, u32);

inline void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

// TH10 0x4423c0: (a * b) >> 7 clamped to 0xff.
u32 MultiplyColorChannel(u32 channel, u32 factor)
{
    u32 result = (channel & 0xffU) * (factor & 0xffU) >> 7;
    if (result > 0xffU) {
        result = 0xffU;
    }
    return result;
}

u32 ModulateColor(u32 color, const u8 *owner)
{
    const u32 blue = MultiplyColorChannel(color, owner[0x732458U]);
    const u32 green = MultiplyColorChannel(color >> 8, owner[0x732459U]);
    const u32 red = MultiplyColorChannel(color >> 16, owner[0x73245aU]);
    const u32 alpha = MultiplyColorChannel(color >> 24, owner[0x73245bU]);
    return blue | (green << 8) | (red << 16) | (alpha << 24);
}

// The game's x87 control word uses round-to-nearest/even for frndint here.
float RoundGlyphCoordinate(float value)
{
    const float lower = static_cast<float>(std::floor(static_cast<double>(value)));
    const float fraction = value - lower;
    if (fraction < 0.5f) {
        return lower;
    }
    if (fraction > 0.5f) {
        return lower + 1.0f;
    }
    return (static_cast<i32>(lower) & 1) == 0 ? lower : lower + 1.0f;
}

// Unsigned dword -> float with the fild + 2^32 fixup the native uses for
// the viewport fields.
float LoadUnsignedAsFloat(const void *base, u32 offset)
{
    const u32 value = *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
    return static_cast<float>(value);
}

// TH10 0x4425a0 semantic (glyph render-state transition): keeps the
// owner's blend-mode (+0x3ada68) and sampler-mode (+0x3ada6e) caches in
// sync with the VM flag word, flushing pending vertices on changes.
void UpdateGlyphRenderStatesLocal(void *vm, void *owner)
{
    const VmRecord &vm_record = *reinterpret_cast<const VmRecord *>(vm);
    u8 *const owner_bytes = static_cast<u8 *>(owner);
    const u32 flags = vm_record.flags;
    const u8 blend_mode = static_cast<u8>((flags >> 4) & 3U);
    if (owner_bytes[0x3ada68U] != blend_mode) {
        FlushRenderOwnerPendingVertices(
            reinterpret_cast<RenderOwnerPartial *>(owner));
        owner_bytes[0x3ada68U] = blend_mode;
        if (blend_mode == 0) {
            (void)reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
                g_D3D9ClearDevice, 57))(g_D3D9ClearDevice, 0x14U, 6U);
        } else if (blend_mode == 1) {
            (void)reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
                g_D3D9ClearDevice, 57))(g_D3D9ClearDevice, 0x14U, 2U);
        }
    }

    const u8 sampler_mode = static_cast<u8>(flags >> 31);
    if (owner_bytes[0x3ada6eU] != sampler_mode) {
        FlushRenderOwnerPendingVertices(
            reinterpret_cast<RenderOwnerPartial *>(owner));
        owner_bytes[0x3ada6eU] = sampler_mode;
        const u32 value = sampler_mode == 0 ? 2U : 1U;
        (void)reinterpret_cast<D3DSetSamplerStateFn>(GetD3DSlot(
            g_D3D9ClearDevice, 69))(g_D3D9ClearDevice, 0U, 5U, value);
        (void)reinterpret_cast<D3DSetSamplerStateFn>(GetD3DSlot(
            g_D3D9ClearDevice, 69))(g_D3D9ClearDevice, 0U, 6U, value);
    }
    ++*reinterpret_cast<u32 *>(owner_bytes + 0x54U);
}

// TH10 0x442fe0 semantic: append the scratch quad to the owner's vertex
// buffer as two triangles (6 vertices) and advance the cursor/counters.
void AppendScratchQuadAsTriangles(void *owner)
{
    u8 *const owner_bytes = static_cast<u8 *>(owner);
    GlyphVertex *const destination = *reinterpret_cast<GlyphVertex **>(
        owner_bytes + 0x72daccU);
    destination[0] = g_AsciiGlyphScratchQuad[0];
    destination[1] = g_AsciiGlyphScratchQuad[1];
    destination[2] = g_AsciiGlyphScratchQuad[2];
    destination[3] = g_AsciiGlyphScratchQuad[1];
    destination[4] = g_AsciiGlyphScratchQuad[2];
    destination[5] = g_AsciiGlyphScratchQuad[3];
    *reinterpret_cast<GlyphVertex **>(owner_bytes + 0x72daccU) =
        destination + 6;
    ++*reinterpret_cast<u32 *>(owner_bytes + 0x3adac8U);
}

} // namespace

i32 SubmitAsciiProjectedQuadEaxEcxStackAbi(void *vm, void *owner, u32 flags)
{
    const VmRecord &vm_record = *reinterpret_cast<const VmRecord *>(vm);
    u8 *const owner_bytes = static_cast<u8 *>(owner);

    // 1. camera offset accumulation over the four vertex positions.
    for (u32 index = 0; index != 4; ++index) {
        GlyphVertex &vertex = g_AsciiGlyphScratchQuad[index];
        vertex.x += *reinterpret_cast<const float *>(owner_bytes + 0x5cU);
        vertex.y += *reinterpret_cast<const float *>(owner_bytes + 0x60U);
    }

    // 2. optional pixel snap (flags bit 0).
    if ((flags & 1U) != 0U) {
        const float left = RoundGlyphCoordinate(
            g_AsciiGlyphScratchQuad[0].x) - kHalfPixel;
        const float right = RoundGlyphCoordinate(
            g_AsciiGlyphScratchQuad[2].x) - kHalfPixel;
        const float top = RoundGlyphCoordinate(
            g_AsciiGlyphScratchQuad[0].y) - kHalfPixel;
        const float bottom = RoundGlyphCoordinate(
            g_AsciiGlyphScratchQuad[3].y) - kHalfPixel;
        g_AsciiGlyphScratchQuad[3].y = bottom;
        g_AsciiGlyphScratchQuad[2].y = bottom;
        g_AsciiGlyphScratchQuad[0].y = top;
        g_AsciiGlyphScratchQuad[1].y = top;
        g_AsciiGlyphScratchQuad[2].x = right;
        g_AsciiGlyphScratchQuad[3].x = right;
        g_AsciiGlyphScratchQuad[0].x = left;
        g_AsciiGlyphScratchQuad[1].x = left;
    }

    // 3. UV re-derivation from the VM's texture-source record.
    const u8 *const source = static_cast<const u8 *>(vm_record.anim_entry);
    const float u_left = *reinterpret_cast<const float *>(source + 0x28U) +
                         vm_record.texture_u;
    const float u_right = *reinterpret_cast<const float *>(source + 0x20U) +
                          vm_record.texture_u;
    const float v_top = *reinterpret_cast<const float *>(source + 0x24U) +
                        vm_record.texture_v;
    const float v_bottom = *reinterpret_cast<const float *>(source + 0x2cU) +
                           vm_record.texture_v;
    g_AsciiGlyphScratchQuad[0].u = u_left;
    g_AsciiGlyphScratchQuad[2].u = u_left;
    g_AsciiGlyphScratchQuad[1].u = u_right;
    g_AsciiGlyphScratchQuad[3].u = u_right;
    g_AsciiGlyphScratchQuad[0].v = v_top;
    g_AsciiGlyphScratchQuad[1].v = v_top;
    g_AsciiGlyphScratchQuad[2].v = v_bottom;
    g_AsciiGlyphScratchQuad[3].v = v_bottom;

    // 4. viewport cull (NaN behavior documented above).
    float max_x = g_AsciiGlyphScratchQuad[0].x;
    float min_x = max_x;
    float max_y = g_AsciiGlyphScratchQuad[0].y;
    float min_y = max_y;
    for (u32 index = 1; index != 4; ++index) {
        const GlyphVertex &vertex = g_AsciiGlyphScratchQuad[index];
        if (vertex.x > max_x) {
            max_x = vertex.x;
        }
        if (vertex.x < min_x) {
            min_x = vertex.x;
        }
        if (vertex.y > max_y) {
            max_y = vertex.y;
        }
        if (vertex.y < min_y) {
            min_y = vertex.y;
        }
    }

    const u8 *const camera =
        static_cast<const u8 *>(g_MainChainActiveCameraWork);
    const float viewport_x = LoadUnsignedAsFloat(camera, 0xccU);
    const float viewport_y = LoadUnsignedAsFloat(camera, 0xd0U);
    // Right/bottom are summed in integers before the (unsigned) fild.
    const u32 right_dword =
        *reinterpret_cast<const u32 *>(camera + 0xd4U) +
        *reinterpret_cast<const u32 *>(camera + 0xccU);
    const u32 bottom_dword =
        *reinterpret_cast<const u32 *>(camera + 0xd8U) +
        *reinterpret_cast<const u32 *>(camera + 0xd0U);
    const float viewport_right = static_cast<float>(right_dword);
    const float viewport_bottom = static_cast<float>(bottom_dword);

    // The x edges reject only when strictly outside (unordered/NaN passes);
    // the top edge additionally rejects equality (native fnstsw mask quirk).
    if (max_x == max_x && viewport_x == viewport_x && max_x < viewport_x) {
        return 0;
    }
    {
        const bool unordered = max_y != max_y || viewport_y != viewport_y;
        if (!unordered && !(max_y > viewport_y)) {
            return 0;
        }
    }
    if (viewport_right == viewport_right && viewport_right < min_x) {
        return 0;
    }
    if (viewport_bottom == viewport_bottom && viewport_bottom < min_y) {
        return 0;
    }

    // 5. texture bind through the owner cache.
    const u8 *const source_record = static_cast<const u8 *>(vm_record.anim_entry);
    const u32 texture = *reinterpret_cast<const u32 *>(source_record + 4U);
    if (*reinterpret_cast<u32 *>(owner_bytes + 0x3ada64U) != texture) {
        *reinterpret_cast<u32 *>(owner_bytes + 0x3ada64U) = texture;
        FlushRenderOwnerPendingVertices(
            reinterpret_cast<RenderOwnerPartial *>(owner));
        (void)reinterpret_cast<D3DSetTextureFn>(GetD3DSlot(
            g_D3D9ClearDevice, 65))(g_D3D9ClearDevice, 0U,
                                    reinterpret_cast<void *>(texture));
    }
    if (owner_bytes[0x3ada6aU] != 1U) {
        FlushRenderOwnerPendingVertices(
            reinterpret_cast<RenderOwnerPartial *>(owner));
        owner_bytes[0x3ada6aU] = 1U;
    }

    // 6. color update (skipped with flags bit 1).
    if ((flags & 2U) == 0U) {
        const u32 vm_flags = vm_record.flags;
        u32 color = (vm_flags & 0x8000U) != 0U
                        ? vm_record.secondary_color
                        : vm_record.primary_color;
        if (*reinterpret_cast<const u32 *>(owner_bytes + 0x73245cU) != 0U) {
            color = ModulateColor(color, owner_bytes);
        }
        g_AsciiGlyphScratchQuad[0].color = color;
        g_AsciiGlyphScratchQuad[1].color = color;
        g_AsciiGlyphScratchQuad[2].color = color;
        g_AsciiGlyphScratchQuad[3].color = color;
    }

    // 7. render-state transition + vertex submission.
    UpdateGlyphRenderStatesLocal(vm, owner);
    AppendScratchQuadAsTriangles(owner);
    return 0;
}

} // namespace th10
