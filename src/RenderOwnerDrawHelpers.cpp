// TH10 render-owner draw helpers (0x004415c0-0x004458b0 cluster): camera
// tween arming, small vector stores, the per-frame blend/filter state
// applier, the vertex free-list reset and the ribbon quad builders/draw.
#include <cmath>
#include <string.h>

#include "RenderOwnerDrawHelpers.hpp"
#include "Th10Platform.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "MainChainRender.hpp"
#include "MainChainRuntime.hpp"
#include "PlayerShotData.hpp"

namespace th10 {

namespace {

extern D3D9Device *g_MainChainD3DDevice;   // TH10 DAT_00491c30
extern void *g_MainChainRenderOwner;       // TH10 DAT_00491c10

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

// TH10 0x00442f50 (registered): flush pending owner vertices.
void FlushOwnerVertices(void *owner)
{
    FlushRenderOwnerPendingVerticesEsiAbi(
        reinterpret_cast<RenderOwnerPartial *>(owner));
}

void WriteU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

u32 ReadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

} // namespace

// TH10 0x004415c0. Native EAX = count, EDX = row base, ECX = VM-work
// cursor, stack (ret 4) = argument forwarded to 0x00404f30. The native
// advances the forwarded cursor by one byte per row (quirk preserved).
u16 InitializePlayerVmRowBatchEaxEdxCcxAbi(u32 count, u8 *rows,
                                           u32 vm_cursor, i32 anm_argument)
{
    u32 cursor = vm_cursor;
    u8 *row = rows;
    while (count != 0U) {
        InitializePlayerMainVmEsiStackAbi(row,
            reinterpret_cast<void *>(cursor), anm_argument);
        const u16 word = *reinterpret_cast<const u16 *>(row + 0x384U);
        *reinterpret_cast<u16 *>(row + 0x388U) = word;
        ++cursor;                       // native byte-increment quirk
        row += 0x3acU;
        --count;
    }
    (void)rows;
    return 0;
}

// TH10 0x00441e40. Native EAX = target, EDX/ECX = first two dwords,
// stack = third.
void StoreInterpolationTripleEaxEdxCcxAbi(u32 *target, u32 value_edx,
                                          u32 value_ecx, u32 value_stack)
{
    target[0] = value_ecx;
    target[1] = value_edx;
    target[2] = value_stack;
}

// TH10 0x00441e50. Native EDI = source, ESI = target, stack = scale.
// Per-dword truncating float multiply (x87 fistp).
void ScaleInterpolationTripleEdiEsiAbi(const u32 *source, u32 *target,
                                       float scale)
{
    for (u32 index = 0; index < 3U; ++index) {
        const float scaled = static_cast<float>(source[index]) * scale;
        target[index] = static_cast<u32>(static_cast<i32>(scaled));
    }
}

// TH10 0x00441ef0. Native ECX = float[2] target, stack = angle, radius.
void PolarToCartesianEcxAbi(float *target, float angle, float radius)
{
    target[0] = cosf(angle) * radius;
    target[1] = sinf(angle) * radius;
}

// Shared eased-interpolation priming used by 0x00441f50 / 0x00442050 /
// 0x00434a80: flag word, -999999 prime, easing table, then -1 target.
static void PrimeEasedInterpolation(u32 *interp)
{
    if ((interp[4] & 1U) == 0U) {
        interp[1] = 0;
        interp[0] = 0xfff0bdc1U;
        interp[2] = 0;
        interp[3] = 0x00476f78U;
        interp[4] |= 1U;
    }
    interp[1] = 0;
    interp[2] = 0;
    interp[0] = 0xffffffffU;
}

// TH10 0x00441f50. Native EAX = record, DL = easing kind, ECX = duration,
// stack (ret 8) = two bytes for +0x208/+0x20c.
void BeginCameraTweenEaxStackAbi(void *record, u8 easing_kind, u32 duration,
                                 u8 arg_208, u8 arg_20c)
{
    WriteU32At(record, 0x230U, easing_kind);
    WriteU32At(record, 0x22cU, duration);
    WriteU32At(record, 0x20cU, arg_20c);
    WriteU32At(record, 0x208U, arg_208);
    u32 *const interp = reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + 0x218U);
    PrimeEasedInterpolation(interp);
}

// TH10 0x00442050. Native EAX = record, EDX = from-RGB, ECX = to-RGB,
// stack (ret 8) = duration + flag byte.
void BeginCameraColorTweenEaxAbi(void *record, const u8 *from_rgb,
                                 const u8 *to_rgb, u32 duration, u8 flag)
{
    WriteU32At(record, 0x200U, duration);
    u32 *const zero_a = reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + 0x1d4U);
    zero_a[0] = 0;
    zero_a[1] = 0;
    zero_a[2] = 0;
    u32 *const zero_b = reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + 0x1e0U);
    zero_b[0] = 0;
    zero_b[1] = 0;
    zero_b[2] = 0;
    WriteU32At(record, 0x204U, flag);

    u32 *const from = reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + 0x1bcU);
    from[0] = from_rgb[0];
    from[1] = from_rgb[1];
    from[2] = from_rgb[2];
    u32 *const to = reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + 0x1c8U);
    to[0] = to_rgb[0];
    to[1] = to_rgb[1];
    to[2] = to_rgb[2];

    u32 *const interp = reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + 0x1ecU);
    PrimeEasedInterpolation(interp);
}

// TH10 0x00442130. Copies three dwords to record+0x18.
void StoreOwnerVectorAt24EaxCcxAbi(const u32 *source, void *record)
{
    u32 *const target = reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + 0x18U);
    target[0] = source[0];
    target[1] = source[1];
    target[2] = source[2];
}

// TH10 0x00442150. Copies three dwords to record+0x24.
void StoreOwnerVectorAt36EaxCcxAbi(const u32 *source, void *record)
{
    u32 *const target = reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + 0x24U);
    target[0] = source[0];
    target[1] = source[1];
    target[2] = source[2];
}

// TH10 0x004421a0. Plain three-dword copy.
void CopyVector3EaxCcxAbi(const u32 *source, u32 *target)
{
    target[0] = source[0];
    target[1] = source[1];
    target[2] = source[2];
}

// TH10 0x004421c0. Copies three dwords to record+0x0c.
void StoreOwnerVectorAt12EaxCcxAbi(const u32 *source, void *record)
{
    u32 *const target = reinterpret_cast<u32 *>(
        static_cast<u8 *>(record) + 0x0cU);
    target[0] = source[0];
    target[1] = source[1];
    target[2] = source[2];
}

// TH10 0x004422f0. Native EAX = target, DL = second byte, CL = first byte,
// stack = third byte.
void StoreRgbTripleEaxAbi(u8 *target, u8 second, u8 first, u8 third)
{
    target[0] = first;
    target[1] = second;
    target[2] = third;
}

// TH10 0x004423c0. Native AL = multiplier, CL = value.
u32 ScaleBrightnessClamp255Abi(u8 multiplier, u8 value)
{
    const u32 scaled = (static_cast<u32>(value) * multiplier) >> 7;
    return (scaled >= 0x100U) ? 255U : scaled;
}

// TH10 0x004425a0. Native EAX = owner, EDI = entity. Returns the trailing
// sampler-state call result (or the sign bit when no change was needed).
u32 ApplyOwnerRenderModesEaxEdiAbi(void *owner, void *entity)
{
    LargeRenderOwnerLayout &owner_state =
        *static_cast<LargeRenderOwnerLayout *>(owner);
    const u32 state_word = ReadU32At(entity, 0x35cU);

    const u32 blend = (state_word >> 4) & 3U;
    if (owner_state.blend_mode_cache != static_cast<u8>(blend)) {
        FlushOwnerVertices(owner);
        owner_state.blend_mode_cache = static_cast<u8>(blend);
        if (blend == 0U) {
            (void)reinterpret_cast<i32 (TH10_STDCALL *)(D3D9Device *, u32,
                u32)>(GetD3DSlot(g_MainChainD3DDevice, 0xe4 / 4))(
                g_MainChainD3DDevice, 0x14U, 6U);
        } else if (blend == 1U) {
            (void)reinterpret_cast<i32 (TH10_STDCALL *)(D3D9Device *, u32,
                u32)>(GetD3DSlot(g_MainChainD3DDevice, 0xe4 / 4))(
                g_MainChainD3DDevice, 0x14U, 2U);
        }
    }

    const u32 negative = state_word >> 31;
    u32 result = negative;
    if (owner_state.sampler_filter_cache != static_cast<u8>(negative)) {
        FlushOwnerVertices(owner);
        owner_state.sampler_filter_cache = static_cast<u8>(negative);
        typedef i32 (TH10_STDCALL *SetSamplerFn)(D3D9Device *, u32, u32,
                                                 u32);
        const SetSamplerFn set_sampler =
            reinterpret_cast<SetSamplerFn>(
                GetD3DSlot(g_MainChainD3DDevice, 0x114 / 4));
        const u32 filter = (negative != 0U) ? 2U : 1U;
        (void)set_sampler(g_MainChainD3DDevice, 0U, 5U, filter);
        result = static_cast<u32>(set_sampler(g_MainChainD3DDevice, 0U, 6U,
                                              filter));
    }

    owner_state.render_mode_counter = owner_state.render_mode_counter + 1U;
    return result;
}

// TH10 0x00442f30. Native EAX = owner. Resets the free-list head dword at
// owner+0x3adac8 and links the two sentinel nodes (0x72d54c/0x72d550) to
// the address right behind the head.
void InitializeOwnerVertexFreeListEaxAbi(void *owner)
{
    LargeRenderOwnerLayout &owner_state =
        *static_cast<LargeRenderOwnerLayout *>(owner);
    u8 *const base = static_cast<u8 *>(owner);
    owner_state.pending_quad_count = 0U;
    const u32 node_value = reinterpret_cast<u32>(base) + 964275U * 4U;
    WriteU32At(base, 1881779U * 4U, node_value);
    WriteU32At(base, 1881780U * 4U, node_value);
}

// TH10 0x00444b10. Native EAX = count, ECX = entity, EBX = vertex buffer.
// Writes two interleaved vertex columns (stride 14 floats, layout
// rhw/z/x/y) descending from uv[10]; second row bound is uv[11].
i32 BuildHorizontalRibbonVerticesEaxCcxAbi(i32 count, void *entity,
                                           u8 *vertex_buffer)
{
    if (count < 3)
        return -1;
    const u8 *const ent = static_cast<const u8 *>(entity);
    const float *const uv = reinterpret_cast<const float *>(
        ReadU32At(ent, 0x394U));
    const float start_x =
        uv[10] + *reinterpret_cast<const float *>(ent + 84U);
    const float row0_y = uv[9] + *reinterpret_cast<const float *>(ent + 88U);
    const float row1_y =
        uv[11] + *reinterpret_cast<const float *>(ent + 88U);
    const float z = *reinterpret_cast<const float *>(ent + 0x2fcU);
    const float step = static_cast<float>((uv[10] - uv[8]) /
        static_cast<double>((count + 1) / 2 - 1));

    float x = start_x;
    float *out = reinterpret_cast<float *>(vertex_buffer + 24U);
    u32 pairs = ((static_cast<u32>(count) - 1U) >> 1) + 1U;
    while (pairs != 0U) {
        out[-1] = x;
        x -= step;
        out[0] = row0_y;
        out[-2] = z;
        out[-3] = 1.0f;
        out += 14;
        --pairs;
    }

    float *out2 = reinterpret_cast<float *>(vertex_buffer + 52U);
    pairs = ((static_cast<u32>(count) - 2U) >> 1) + 1U;
    x = start_x;
    while (pairs != 0U) {
        out2[-1] = x;
        x -= step;
        out2[0] = row1_y;
        out2[-2] = z;
        out2[-3] = 1.0f;
        out2 += 14;
        --pairs;
    }
    return 0;
}

// TH10 0x00444be0. Vertical twin of 0x00444b10: descending from uv[11]
// with rows bound at uv[8] / uv[10].
i32 BuildVerticalRibbonVerticesEaxCcxAbi(i32 count, void *entity,
                                         u8 *vertex_buffer)
{
    if (count < 3)
        return -1;
    const u8 *const ent = static_cast<const u8 *>(entity);
    const float *const uv = reinterpret_cast<const float *>(
        ReadU32At(ent, 0x394U));
    const float start_x =
        uv[11] + *reinterpret_cast<const float *>(ent + 88U);
    const float row0_x = uv[8] + *reinterpret_cast<const float *>(ent + 84U);
    const float row1_x =
        uv[10] + *reinterpret_cast<const float *>(ent + 84U);
    const float z = *reinterpret_cast<const float *>(ent + 0x2fcU);
    const float step = static_cast<float>((uv[11] - uv[9]) /
        static_cast<double>((count + 1) / 2 - 1));

    float x = start_x;
    float *out = reinterpret_cast<float *>(vertex_buffer + 20U);
    u32 pairs = ((static_cast<u32>(count) - 1U) >> 1) + 1U;
    while (pairs != 0U) {
        out[1] = x;
        x -= step;
        out[0] = row0_x;
        out[-1] = z;
        out[-2] = 1.0f;
        out += 14;
        --pairs;
    }

    float *out2 = reinterpret_cast<float *>(vertex_buffer + 48U);
    pairs = ((static_cast<u32>(count) - 2U) >> 1) + 1U;
    x = start_x;
    while (pairs != 0U) {
        out2[1] = x;
        x -= step;
        out2[0] = row1_x;
        out2[-1] = z;
        out2[-2] = 1.0f;
        out2 += 14;
        --pairs;
    }
    return 0;
}

// TH10 0x004450e0. Native EAX = render object, ECX = owner, stack (ret 8)
// = two vertex words. Applies state 3, binds the texture and issues the
// 28-vertex triangle-fan draw through the raw device vtable. The native
// reads the StartIndex/PrimCount words from stale stack (quirk); modeled
// as zero here.
i32 DrawRibbonStripIndexedEcxAbi(void *render_object, void *owner,
                                 u32 vertex_word_a, u32 vertex_word_b)
{
    LargeRenderOwnerLayout &owner_state =
        *static_cast<LargeRenderOwnerLayout *>(owner);
    if (owner_state.pending_quad_count != 0U)
        FlushOwnerVertices(owner);

    if (owner_state.fvf_active_cache != 3U) {
        (void)reinterpret_cast<i32 (TH10_STDCALL *)(D3D9Device *, u32)>(
            GetD3DSlot(g_MainChainD3DDevice, 0x164 / 4))(
            g_MainChainD3DDevice, 0x144U);
        owner_state.fvf_active_cache = 3U;
    }

    (void)ApplyOwnerRenderModesEaxEdiAbi(owner, render_object);

    const u32 texture_work = ReadU32At(render_object, 0x394U);
    const u32 texture = ReadU32At(
        reinterpret_cast<const void *>(texture_work), 4U);
    if (reinterpret_cast<u32>(owner_state.bound_texture) != texture) {
        owner_state.bound_texture = reinterpret_cast<void *>(texture);
        (void)reinterpret_cast<i32 (TH10_STDCALL *)(D3D9Device *, u32,
            u32)>(GetD3DSlot(g_MainChainD3DDevice, 0x104 / 4))(
            g_MainChainD3DDevice, 0U, texture);
    }

    FlushOwnerVertices(g_MainChainRenderOwner);
    (void)reinterpret_cast<i32 (TH10_STDCALL *)(D3D9Device *, u32, u32)>(
        GetD3DSlot(g_MainChainD3DDevice, 0xe4 / 4))(
        g_MainChainD3DDevice, 0x0eU, 0U);
    typedef i32 (TH10_STDCALL *StageStateFn)(D3D9Device *, u32, u32, u32);
    const StageStateFn stage_state = reinterpret_cast<StageStateFn>(
        GetD3DSlot(g_MainChainD3DDevice, 0x10c / 4));
    (void)stage_state(g_MainChainD3DDevice, 0U, 6U, 0U);
    (void)stage_state(g_MainChainD3DDevice, 0U, 3U, 0U);

    typedef i32 (TH10_STDCALL *DrawIndexedFn)(D3D9Device *, u32, i32, u32,
                                              u32, u32, u32);
    (void)reinterpret_cast<DrawIndexedFn>(
        GetD3DSlot(g_MainChainD3DDevice, 0x14c / 4))(
        g_MainChainD3DDevice, 6U,
        static_cast<i32>(vertex_word_b) - 2, vertex_word_a, 0x1cU, 0U, 0U);
    return 0;
}

// TH10 0x00445880. Native thiscall (record): dispatches the ribbon strip
// draw against the DAT_00491c10 owner singleton with the fixed 33-word
// vertex budget.
i32 DrawRibbonStripFromWorkEcxAbi(void *record)
{
    extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
    const u32 vertex_word_a = ReadU32At(record, 0x358U);
    (void)DrawRibbonStripIndexedEcxAbi(record, g_MainChainRenderOwner,
                                       vertex_word_a, 33U);
    return 0;
}

// TH10 0x004458b0. Native ECX = float[2] target, stack = angle, radius.
void PolarToCartesianStripEcxAbi(float *target, float angle, float radius)
{
    target[0] = cosf(angle) * radius;
    target[1] = sinf(angle) * radius;
}

} // namespace th10
