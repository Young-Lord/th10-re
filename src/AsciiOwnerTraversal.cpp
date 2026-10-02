#include "AsciiOwnerTraversal.hpp"

#include <math.h>

#include "AsciiRenderModeDispatcher.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "MainChainRender.hpp"
#include "MainChainRuntime.hpp"
#include "TitleScreenState.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

struct OuterTraversalRecord {
    short child_index;
    u16 unknown_0002;
    D3DVector3 translation;
};

struct CullingRecord {
    u32 unknown_0000;
    D3DVector3 local_center;
    D3DVector3 full_extent;
};

typedef D3DVector3 *(*D3dxVec3ProjectArrayFn)(D3DVector3 *, u32,
                                                const D3DVector3 *, u32,
                                                const D3DViewport *,
                                                const D3DMatrix *,
                                                const D3DMatrix *,
                                                const D3DMatrix *);

extern void D3dxMatrixTranslation(D3DMatrix *out, float x, float y, float z);
extern D3DVector3 *D3dxVec3ProjectArray(D3DVector3 *out, u32 out_stride,
                                         const D3DVector3 *source,
                                         u32 source_stride,
                                         const D3DViewport *viewport,
                                         const D3DMatrix *projection,
                                         const D3DMatrix *view,
                                         const D3DMatrix *world);
extern void *g_MainChainActiveCameraWork; // TH10 DAT_00491fac
extern MainChainCameraWork g_AsciiCameraWork; // TH10 DAT_00491d7c
extern D3D9Device *g_MainChainD3DDevice; // TH10 DAT_00491c30
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern u32 g_AsciiFogEnableCache; // TH10 DAT_00492378
extern u32 g_MainChainActiveView; // TH10 DAT_00491fb0

inline short ReadI16(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const short *>(bytes + offset);
}

inline float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void WriteFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

void SetFogEnabled(u32 enabled)
{
    if (g_AsciiFogEnableCache == enabled)
        return;
    FlushRenderOwnerPendingVertices(
        reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
    g_AsciiFogEnableCache = enabled;
    typedef i32 (*SetRenderStateFn)(D3D9Device *, u32, u32);
    const SetRenderStateFn set_render_state =
        reinterpret_cast<SetRenderStateFn>(g_MainChainD3DDevice->vtable[0x39]);
    (void)set_render_state(g_MainChainD3DDevice, 28, enabled);
}

bool IsNativeNonZero(float value)
{
    // FUCOMPP against 0: NaN, unlike ordered zero, reaches the division.
    return value != 0.0f || value != value;
}

} // namespace

i32 CullAsciiOwnerChild(void *projection_memory, void *child_memory,
                        const void *outer_translation_memory,
                        float squared_distance_limit)
{
    const MainChainCameraWork *const projection =
        static_cast<const MainChainCameraWork *>(projection_memory);
    const CullingRecord *const child = static_cast<const CullingRecord *>(child_memory);
    const D3DVector3 *const outer =
        static_cast<const D3DVector3 *>(outer_translation_memory);
    const D3DVector3 center = {
        child->local_center.x + outer->x,
        child->local_center.y + outer->y,
        child->local_center.z + outer->z
    };
    const float dx = center.x - (projection->translation.x + projection->target.x);
    const float dy = center.y - (projection->translation.y + projection->target.y);
    const float dz = center.z - (projection->translation.z + projection->target.z);
    if (!(dx * dx + dy * dy + dz * dz <= squared_distance_limit))
        return 1;

    const D3DVector3 half_extent = {
        child->full_extent.x * 0.5f,
        child->full_extent.y * 0.5f,
        child->full_extent.z * 0.5f
    };
    const D3DVector3 corners[8] = {
        {-half_extent.x, -half_extent.y, -half_extent.z},
        { half_extent.x, -half_extent.y, -half_extent.z},
        {-half_extent.x,  half_extent.y, -half_extent.z},
        { half_extent.x,  half_extent.y, -half_extent.z},
        {-half_extent.x, -half_extent.y,  half_extent.z},
        { half_extent.x, -half_extent.y,  half_extent.z},
        {-half_extent.x,  half_extent.y,  half_extent.z},
        { half_extent.x,  half_extent.y,  half_extent.z}
    };
    D3DMatrix world;
    D3DVector3 projected[8];
    D3dxMatrixTranslation(&world, center.x, center.y, center.z);
    (void)D3dxVec3ProjectArray(projected, sizeof(D3DVector3), corners,
        sizeof(D3DVector3), &projection->viewport, &projection->projection,
        &projection->view, &world);

    float max_x = 24.0f;
    float min_x = 424.0f;
    float max_y = 8.0f;
    float min_y = 472.0f;
    for (u32 index = 0; index != 8; ++index) {
        const D3DVector3 point = projected[index];
        if (point.z != point.z || point.z < 0.0f || point.z > 1.0f)
            continue;
        if (point.x != point.x || min_x != min_x || point.x < min_x)
            min_x = point.x;
        else if (point.x > max_x)
            max_x = point.x;
        if (point.y != point.y || min_y != min_y || point.y < min_y)
            min_y = point.y;
        else if (point.y > max_y)
            max_y = point.y;
    }
    return max_x >= 32.0f && min_x == min_x && min_x <= 416.0f &&
           max_y >= 16.0f && min_y == min_y && min_y <= 464.0f ? 0 : 1;
}

i32 RenderAsciiSceneChannel(void *scene_memory, i32 channel)
{
    TitleScreenState &state =
        *reinterpret_cast<TitleScreenState *>(scene_memory);
    g_MainChainActiveCameraWork = &g_AsciiCameraWork;
    UpdateMainChainCameraWorkEdiAbi(&g_AsciiCameraWork);
    SetD3D9Viewport(g_MainChainD3DDevice, &g_AsciiCameraWork.viewport);
    g_MainChainActiveView = 0;
    LargeRenderOwnerLayout &render_owner =
        *static_cast<LargeRenderOwnerLayout *>(g_MainChainRenderOwner);
    render_owner.ascii_scene_active = 1;

    // The outer descriptor stream pointer is read from the +0x18 script_base
    // field (TH10 0x403a39: mov ebx,[ebp+18h]).
    const OuterTraversalRecord *outer =
        reinterpret_cast<const OuterTraversalRecord *>(state.script_base);
    while (outer->child_index >= 0) {
        // TH10 0x403a90: mov edx,[ebp+14h]; mov esi,[edx+ecx*4] — the child
        // pointer is selected through the +0x14 script_pointer_table (double
        // dereference through the table pointer).
        u8 *const child = reinterpret_cast<u8 *const *>(
            state.script_pointer_table)[
                static_cast<i32>(outer->child_index)];
        if (*(reinterpret_cast<const signed char *>(child + 2)) == channel) {
            if (CullAsciiOwnerChild(&g_AsciiCameraWork, child,
                                    &outer->translation,
                                    state.background_fade) == 0) {
                child[3] |= 2;
                u8 *operation = child + 0x1c;
                while (ReadI16(operation, 0) >= 0) {
                    u8 *const vm = state.vm_heap_array +
                        static_cast<i32>(ReadI16(operation, 6)) * 0x3ac;
                    VmRecord &vm_record = *reinterpret_cast<VmRecord *>(vm);
                    if (ReadI16(operation, 0) == 0 &&
                        (vm_record.flags & 0x03c00000U) >= 0x01000000U) {
                        vm_record.delta_pos_x = ReadFloat(operation, 8) + outer->translation.x;
                        vm_record.delta_pos_y = ReadFloat(operation, 0xc) + outer->translation.y;
                        vm_record.delta_pos_z = ReadFloat(operation, 0x10) + outer->translation.z;
                        const float scale_x = ReadFloat(operation, 0x14);
                        if (IsNativeNonZero(scale_x)) {
                            vm_record.flags |= 8;
                            vm_record.scale_x = scale_x /
                                ReadFloat(static_cast<u8 *>(vm_record.anim_entry), 0x34);
                        }
                        const float scale_y = ReadFloat(operation, 0x18);
                        if (IsNativeNonZero(scale_y)) {
                            vm_record.flags |= 8;
                            vm_record.scale_y = scale_y /
                                ReadFloat(static_cast<u8 *>(vm_record.anim_entry), 0x30);
                        }
                    }
                    SetFogEnabled((vm_record.flags &
                        0x03c00000U) == 0x02000000U ? 1 : 0);
                    (void)DispatchAsciiAnimationVmRenderMode(vm,
                        g_MainChainRenderOwner);
                    ++state.op_counter; // +0x2a14
                    operation += ReadI16(operation, 2);
                }
                ++state.scene_counter; // +0x2a0c
            } else {
                ++state.scene_counter_clear; // +0x2a10
            }
        }
        ++outer;
    }
    return 0;
}

} // namespace th10
