#include "MainChainRender.hpp"
#include "MainChainContext.hpp"
#include "MainChainRuntime.hpp"

#include <math.h>

namespace th10 {

namespace {

struct D3DRect;

typedef i32 (TH10_STDCALL *D3DSetViewportFn)(D3D9Device *, const D3DViewport *);
typedef i32 (TH10_STDCALL *D3DClearFn)(D3D9Device *, u32, const void *, u32,
                                        u32, float, u32);
typedef i32 (TH10_STDCALL *D3DSetTextureStageStateFn)(D3D9Device *, u32, u32, u32);
typedef i32 (TH10_STDCALL *D3DSetFVFFn)(D3D9Device *, u32);
typedef i32 (TH10_STDCALL *D3DDrawPrimitiveUPFn)(D3D9Device *, u32, u32,
                                                   const void *, u32);
typedef i32 (TH10_STDCALL *D3DSetTransformFn)(D3D9Device *, u32,
                                                const D3DMatrix *);
typedef i32 (TH10_STDCALL *D3DSetRenderStateFn)(D3D9Device *, u32, u32);
typedef i32 (TH10_STDCALL *D3DGetBackBufferFn)(D3D9Device *, u32, u32, u32,
                                                 void **);
typedef i32 (TH10_STDCALL *D3DCreateRenderTargetFn)(D3D9Device *, u32, u32,
                                                      u32, u32, u32, i32,
                                                      void **, void *);
typedef i32 (TH10_STDCALL *D3DCreateOffscreenPlainSurfaceFn)(D3D9Device *,
                                                               u32, u32, u32,
                                                               u32, void **,
                                                               void *);
typedef i32 (TH10_STDCALL *D3DTextureGetSurfaceLevelFn)(void *, u32, void **);
typedef i32 (TH10_STDCALL *D3DUpdateSurfaceFn)(D3D9Device *, void *,
                                                const D3DRect *, void *,
                                                const void *);

struct D3DRect {
    i32 left;
    i32 top;
    i32 right;
    i32 bottom;
};

extern D3D9Device *g_D3D9ClearDevice; // TH10 DAT_00491c30
extern MainChainRenderOwnerFrameState *g_MainChainRenderOwner; // 0x491c10
extern u32 g_MainChainBackBufferFormat; // TH10 DAT_00491d14
extern void D3dxMatrixLookAtLH(D3DMatrix *out, const D3DVector3 *eye,
                               const D3DVector3 *target,
                               const D3DVector3 *up);
extern void D3dxMatrixPerspectiveFovLH(D3DMatrix *out, float fov,
                                       float aspect, float near_z, float far_z);
extern void D3dxVec3Cross(D3DVector3 *out, const D3DVector3 *left,
                          const D3DVector3 *right);
extern void D3dxVec3Normalize(D3DVector3 *out, const D3DVector3 *input);
extern void ReleaseLargeRenderSurface(void *surface);
extern void FreeLargeRenderOwnerCachedBuffer(void *buffer); // TH10 0x00452422
extern i32 D3dxLoadSurfaceFromSurface(void *destination, const void *palette,
                                      const D3DRect *destination_rect,
                                      void *source, const void *source_palette,
                                      const D3DRect *source_rect, u32 filter,
                                      u32 color_key);

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

u32 ReadU32(const void *address)
{
    return *static_cast<const u32 *>(address);
}

void WriteU32(void *address, u32 value)
{
    *static_cast<u32 *>(address) = value;
}

void *ResolveLargeRenderOwnerSourceSurface(void *owner, i32 group, u32 slot)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    void *const source_group = *reinterpret_cast<void **>(bytes + 0x3ad06c +
        static_cast<u32>(group) * sizeof(void *));
    u8 *const source_slots = *reinterpret_cast<u8 **>(
        static_cast<u8 *>(source_group) + 0x120);
    return *reinterpret_cast<void **>(source_slots + slot * 0x10);
}

i32 GetMainChainD3DBackBuffer(void **out_back_buffer)
{
    return reinterpret_cast<D3DGetBackBufferFn>(GetD3DSlot(
        g_D3D9ClearDevice, 18))(g_D3D9ClearDevice, 0, 0, 0, out_back_buffer);
}

bool RebuildLargeRenderOwnerPrimaryFromShadow(void *owner, u32 cache_slot)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    void **const primary_slots = reinterpret_cast<void **>(bytes + 0x3ad4e0);
    void **const shadow_slots = reinterpret_cast<void **>(bytes + 0x3ad560);
    CachedRenderSurfaceDescriptor *const descriptors =
        reinterpret_cast<CachedRenderSurfaceDescriptor *>(bytes + 0x3ad6e0);
    if (primary_slots[cache_slot] != 0)
        return true;

    if (reinterpret_cast<D3DCreateRenderTargetFn>(GetD3DSlot(
            g_D3D9ClearDevice, 28))(g_D3D9ClearDevice,
            descriptors[cache_slot].width, descriptors[cache_slot].height,
            g_MainChainBackBufferFormat, 0, 0, 1, &primary_slots[cache_slot],
            0) != 0 &&
        reinterpret_cast<D3DCreateOffscreenPlainSurfaceFn>(GetD3DSlot(
            g_D3D9ClearDevice, 36))(g_D3D9ClearDevice,
            descriptors[cache_slot].width, descriptors[cache_slot].height,
            g_MainChainBackBufferFormat, 3, &primary_slots[cache_slot], 0) != 0) {
        return false;
    }

    return D3dxLoadSurfaceFromSurface(primary_slots[cache_slot], 0, 0,
        shadow_slots[cache_slot], 0, 0, 1, 0) == 0;
}

} // namespace

void SetD3D9Viewport(D3D9Device *device, const D3DViewport *viewport)
{
    (void)reinterpret_cast<D3DSetViewportFn>(GetD3DSlot(device, 47))(device, viewport);
}

void ClearD3D9Target(D3D9Device *device, u32 color)
{
    (void)reinterpret_cast<D3DClearFn>(GetD3DSlot(device, 43))(
        device, 0, 0, 1, color, 1.0f, 0);
}

i32 EnableMainChainFogIfNeeded(MainChainContext *context)
{
    if (context->fog_enabled_cache == 1)
        return 0;

    FlushRenderOwnerPendingVertices(
        reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
    context->fog_enabled_cache = 1;
    return reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
        context->draw_target, 57))(context->draw_target, 0x1c, 1);
}

i32 DisableMainChainFogIfNeeded(MainChainContext *context)
{
    if (context->fog_enabled_cache == 0)
        return 0;

    FlushRenderOwnerPendingVertices(
        reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
    context->fog_enabled_cache = 0;
    return reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
        context->draw_target, 57))(context->draw_target, 0x1c, 0);
}

i32 EnableMainChainZWriteIfNeeded(MainChainContext *context)
{
    if (context->z_write_enabled_cache == 1)
        return 0;

    FlushRenderOwnerPendingVertices(
        reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
    context->z_write_enabled_cache = 1;
    return reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
        context->draw_target, 57))(context->draw_target, 0x0e, 1);
}

i32 DisableMainChainZWriteIfNeeded(MainChainContext *context)
{
    if (context->z_write_enabled_cache == 0)
        return 0;

    FlushRenderOwnerPendingVertices(
        reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
    context->z_write_enabled_cache = 0;
    return reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
        context->draw_target, 57))(context->draw_target, 0x0e, 0);
}

i32 FlushThenSetMainChainRenderState(MainChainContext *context, u32 state,
                                      u32 value)
{
    FlushRenderOwnerPendingVertices(
        reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
    return reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
        context->draw_target, 57))(context->draw_target, state, value);
}

void UpdateMainChainCameraWorkEdiAbi(MainChainCameraWork *work)
{
    if (g_MainChainRenderOwner != 0)
        FlushRenderOwnerPendingVertices(
            reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));

    D3DVector3 eye = work->eye;
    D3DVector3 target = work->target;
    eye.x += work->translation.x;
    eye.y += work->translation.y;
    eye.z += work->translation.z;
    target.x += work->translation.x;
    target.y += work->translation.y;
    target.z += work->translation.z;
    D3dxMatrixLookAtLH(&work->view, &eye, &target, work->up);
    D3dxMatrixPerspectiveFovLH(&work->projection, work->fov,
        static_cast<float>(static_cast<i32>(work->viewport.width)) /
        static_cast<float>(static_cast<i32>(work->viewport.height)),
        20.0f, 1800.0f);
    (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(g_D3D9ClearDevice, 44))(
        g_D3D9ClearDevice, 2, &work->view);
    (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(g_D3D9ClearDevice, 44))(
        g_D3D9ClearDevice, 3, &work->projection);
    D3dxVec3Cross(&work->normalized_cross, work->up, &work->eye);
    D3dxVec3Normalize(&work->normalized_cross, &work->normalized_cross);

    if (g_MainChainRenderOwner != 0) {
        g_MainChainRenderOwner->camera_value_005c = work->owner_value_00e8;
        g_MainChainRenderOwner->camera_value_0060 = work->owner_value_00ec;
    }
}

void *UpdateMainChainD3DFrameStateEdiAbi(MainChainCameraWork *work)
{
    if (g_MainChainRenderOwner != 0)
        FlushRenderOwnerPendingVertices(
            reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));

    const float width = static_cast<float>(work->viewport.width);
    const float height = static_cast<float>(work->viewport.height);
    const float center_x = width * 0.5f;
    const float center_y = height * 0.5f;
    const D3DVector3 eye = {
        center_x, center_y, height / static_cast<float>(tan(0.157079637f))
    };
    const D3DVector3 target = { center_x, center_y, 0.0f };
    const D3DVector3 up = { 0.0f, -1.0f, 0.0f };

    D3dxMatrixLookAtLH(&work->view, &eye, &target, &up);
    D3dxMatrixPerspectiveFovLH(&work->projection, 0.313159257f,
        width / height, 1.0f, 10000.0f);
    (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(
        g_D3D9ClearDevice, 44))(g_D3D9ClearDevice, 2, &work->view);
    (void)reinterpret_cast<D3DSetTransformFn>(GetD3DSlot(
        g_D3D9ClearDevice, 44))(g_D3D9ClearDevice, 3, &work->projection);

    MainChainRenderOwnerFrameState *const owner = g_MainChainRenderOwner;
    if (owner != 0) {
        owner->camera_value_005c = work->owner_value_00e8;
        owner->camera_value_0060 = work->owner_value_00ec;
    }
    return owner;
}

void FlushRenderOwnerPendingVertices(RenderOwnerPartial *owner)
{
    u8 *const bytes = reinterpret_cast<u8 *>(owner);
    const u32 count = ReadU32(bytes + 0x3adac8);
    if (count == 0)
        return;

    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        g_D3D9ClearDevice, 67))(g_D3D9ClearDevice, 0, 6, 0);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        g_D3D9ClearDevice, 67))(g_D3D9ClearDevice, 0, 3, 0);
    (void)reinterpret_cast<D3DSetFVFFn>(GetD3DSlot(g_D3D9ClearDevice, 89))(
        g_D3D9ClearDevice, 0x144);
    (void)reinterpret_cast<D3DDrawPrimitiveUPFn>(GetD3DSlot(
        g_D3D9ClearDevice, 83))(g_D3D9ClearDevice, 4, count << 1,
        *reinterpret_cast<const void *const *>(bytes + 0x72dad0), 0x1c);

    WriteU32(bytes + 0x72dad0, ReadU32(bytes + 0x72dacc));
    WriteU32(bytes + 0x3adac8, 0);
    WriteU32(bytes + 0x58, ReadU32(bytes + 0x58) + 1);
}

void FlushRenderOwnerPendingRenderBatches(void *owner)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    i32 *const second_batch_index = reinterpret_cast<i32 *>(bytes + 0x4);
    if (*second_batch_index >= 0) {
        SubmitLargeRenderOwnerSecondDeferredBatch(owner);
        *second_batch_index = -1;
    }

    i32 *const first_batch_index = reinterpret_cast<i32 *>(bytes);
    if (*first_batch_index >= 0) {
        SubmitLargeRenderOwnerFirstDeferredBatch(owner);
        *first_batch_index = -1;
    }
}

void SubmitLargeRenderOwnerSecondDeferredBatch(void *owner)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    const i32 group = *reinterpret_cast<const i32 *>(bytes + 0x4);
    const u32 source_slot = *reinterpret_cast<const u32 *>(bytes + 0x28);
    const u32 *const rectangles = reinterpret_cast<const u32 *>(bytes + 0x8);
    void *const source = ResolveLargeRenderOwnerSourceSurface(owner, group,
                                                               source_slot);
    if (source == 0)
        return;

    FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
    void *back_buffer = 0;
    if (GetMainChainD3DBackBuffer(&back_buffer) != 0)
        return;

    // Native re-resolves the borrowed texture after GetBackBuffer, obtains its
    // level-zero surface, then releases that temporary surface after D3DX.
    void *const source_texture = ResolveLargeRenderOwnerSourceSurface(owner,
        group, source_slot);
    void *source_surface = 0;
    if (reinterpret_cast<D3DTextureGetSurfaceLevelFn>(GetD3DSlot(
            static_cast<D3D9Device *>(source_texture), 18))(
            source_texture, 0, &source_surface) == 0) {
        const D3DRect source_rect = {
            static_cast<i32>(rectangles[0]), static_cast<i32>(rectangles[1]),
            static_cast<i32>(rectangles[0] + rectangles[2]),
            static_cast<i32>(rectangles[1] + rectangles[3])
        };
        const D3DRect destination_rect = {
            static_cast<i32>(rectangles[4]), static_cast<i32>(rectangles[5]),
            static_cast<i32>(rectangles[4] + rectangles[6]),
            static_cast<i32>(rectangles[5] + rectangles[7])
        };
        (void)D3dxLoadSurfaceFromSurface(back_buffer, 0, &destination_rect,
            source_surface, 0, &source_rect, 2, 0);
        ReleaseLargeRenderSurface(source_surface);
    }
    ReleaseLargeRenderSurface(back_buffer);
}

void ReleaseLargeRenderOwnerCachedSurfacePair(void *owner, u32 cache_slot)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    void **const primary_slots = reinterpret_cast<void **>(bytes + 0x3ad4e0);
    void **const shadow_slots = reinterpret_cast<void **>(bytes + 0x3ad560);
    void **const buffers = reinterpret_cast<void **>(bytes + 0x3ad5e0);
    if (primary_slots[cache_slot] != 0) {
        ReleaseLargeRenderSurface(primary_slots[cache_slot]);
        primary_slots[cache_slot] = 0;
    }
    if (shadow_slots[cache_slot] != 0) {
        ReleaseLargeRenderSurface(shadow_slots[cache_slot]);
        shadow_slots[cache_slot] = 0;
    }
    if (buffers[cache_slot] != 0) {
        FreeLargeRenderOwnerCachedBuffer(buffers[cache_slot]);
        buffers[cache_slot] = 0;
    }
    buffers[cache_slot] = 0;
}

void UpdateLargeRenderOwnerCachedSurfaceRegion(void *owner, u32 cache_slot,
                                               i32 source_left, i32 source_top,
                                               i32 destination_x,
                                               i32 destination_y)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    void **const primary_slots = reinterpret_cast<void **>(bytes + 0x3ad4e0);
    void **const shadow_slots = reinterpret_cast<void **>(bytes + 0x3ad560);
    CachedRenderSurfaceDescriptor *const descriptors =
        reinterpret_cast<CachedRenderSurfaceDescriptor *>(bytes + 0x3ad6e0);
    if (shadow_slots[cache_slot] == 0)
        return;

    void *back_buffer = 0;
    if (GetMainChainD3DBackBuffer(&back_buffer) != 0)
        return;
    if (!RebuildLargeRenderOwnerPrimaryFromShadow(owner, cache_slot)) {
        ReleaseLargeRenderSurface(back_buffer);
        return;
    }

    // Native writes descriptor dimensions directly to RECT.right/bottom.
    const D3DRect source_rect = { source_left, source_top,
        static_cast<i32>(descriptors[cache_slot].width),
        static_cast<i32>(descriptors[cache_slot].height) };
    const i32 destination_point[2] = { destination_x, destination_y };
    (void)reinterpret_cast<D3DUpdateSurfaceFn>(GetD3DSlot(
        g_D3D9ClearDevice, 30))(g_D3D9ClearDevice, primary_slots[cache_slot],
                                 &source_rect, back_buffer, destination_point);
    ReleaseLargeRenderSurface(back_buffer);
}

void UpdateLargeRenderOwnerCachedSurfaceRectangle(void *owner, u32 cache_slot,
                                                  i32 destination_x,
                                                  i32 destination_y,
                                                  i32 source_x, i32 source_y,
                                                  i32 source_width,
                                                  i32 source_height)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    void **const shadow_slots = reinterpret_cast<void **>(bytes + 0x3ad560);
    void **const primary_slots = reinterpret_cast<void **>(bytes + 0x3ad4e0);
    if (shadow_slots[cache_slot] == 0)
        return;

    void *back_buffer = 0;
    if (GetMainChainD3DBackBuffer(&back_buffer) != 0)
        return;
    if (!RebuildLargeRenderOwnerPrimaryFromShadow(owner, cache_slot)) {
        ReleaseLargeRenderSurface(back_buffer);
        return;
    }

    const D3DRect source_rect = { source_x, source_y,
        source_x + source_width, source_y + source_height };
    const i32 destination_point[2] = { destination_x, destination_y };
    (void)reinterpret_cast<D3DUpdateSurfaceFn>(GetD3DSlot(
        g_D3D9ClearDevice, 30))(g_D3D9ClearDevice, primary_slots[cache_slot],
                                 &source_rect, back_buffer, destination_point);
    ReleaseLargeRenderSurface(back_buffer);
}

void SubmitLargeRenderOwnerFirstDeferredBatch(void *owner)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    const u32 cache_slot = *reinterpret_cast<const u32 *>(bytes);
    const u32 *const rectangles = reinterpret_cast<const u32 *>(bytes + 0x2c);
    void **const primary_slots = reinterpret_cast<void **>(bytes + 0x3ad4e0);
    void **const shadow_slots = reinterpret_cast<void **>(bytes + 0x3ad560);
    CachedRenderSurfaceDescriptor *const descriptors =
        reinterpret_cast<CachedRenderSurfaceDescriptor *>(bytes + 0x3ad6e0);

    FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(owner));
    if (primary_slots[cache_slot] != 0)
        ReleaseLargeRenderOwnerCachedSurfacePair(owner, cache_slot);

    void *back_buffer = 0;
    if (GetMainChainD3DBackBuffer(&back_buffer) != 0)
        return;

    // Inputs are two {left, top, width, height} groups. Native stores the
    // second group's width/height even when a following create call fails.
    descriptors[cache_slot].width = rectangles[6];
    descriptors[cache_slot].height = rectangles[7];

    if (reinterpret_cast<D3DCreateRenderTargetFn>(GetD3DSlot(
            g_D3D9ClearDevice, 28))(g_D3D9ClearDevice,
            rectangles[6], rectangles[7],
            g_MainChainBackBufferFormat, 0, 0, 1, &primary_slots[cache_slot],
            0) != 0 &&
        reinterpret_cast<D3DCreateOffscreenPlainSurfaceFn>(GetD3DSlot(
            g_D3D9ClearDevice, 36))(g_D3D9ClearDevice,
            rectangles[6], rectangles[7],
            g_MainChainBackBufferFormat, 3, &primary_slots[cache_slot], 0) != 0) {
        ReleaseLargeRenderSurface(back_buffer);
        return;
    }
    if (reinterpret_cast<D3DCreateOffscreenPlainSurfaceFn>(GetD3DSlot(
            g_D3D9ClearDevice, 36))(g_D3D9ClearDevice,
            rectangles[6], rectangles[7],
            g_MainChainBackBufferFormat, 3, &shadow_slots[cache_slot], 0) != 0) {
        ReleaseLargeRenderSurface(back_buffer);
        return;
    }

    const D3DRect source_rect = {
        static_cast<i32>(rectangles[0]), static_cast<i32>(rectangles[1]),
        static_cast<i32>(rectangles[0] + rectangles[2]),
        static_cast<i32>(rectangles[1] + rectangles[3])
    };
    const D3DRect destination_rect = {
        static_cast<i32>(rectangles[4]), static_cast<i32>(rectangles[5]),
        static_cast<i32>(rectangles[4] + rectangles[6]),
        static_cast<i32>(rectangles[5] + rectangles[7])
    };
    // The first D3DX copy is the only checked copy. Native state remains
    // published on failure, without rolling back either newly created surface.
    if (D3dxLoadSurfaceFromSurface(primary_slots[cache_slot], 0,
            &destination_rect, back_buffer, 0, &source_rect, 0xffffffffU,
            0) != 0) {
        ReleaseLargeRenderSurface(back_buffer);
        return;
    }
    (void)D3dxLoadSurfaceFromSurface(shadow_slots[cache_slot], 0, 0,
        primary_slots[cache_slot], 0, 0, 0xffffffffU, 0);
    ReleaseLargeRenderSurface(back_buffer);
}

} // namespace th10
