#include "AsciiHudRenderer.hpp"

#include "AsciiHudOwner.hpp"
#include "AsciiRenderModeDispatcher.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "MainChainRender.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

struct ImmediateColorVertex {
    float x;
    float y;
    float z;
    float rhw;
    u32 color;
};

typedef i32 (TH10_STDCALL *D3DSetTextureStageStateFn)(D3D9Device *, u32, u32,
                                                        u32);
typedef i32 (TH10_STDCALL *D3DSetFVFFn)(D3D9Device *, u32);
typedef i32 (TH10_STDCALL *D3DSetRenderStateFn)(D3D9Device *, u32, u32);
typedef i32 (TH10_STDCALL *D3DDrawPrimitiveUPFn)(D3D9Device *, u32, u32,
                                                  const void *, u32);

extern D3D9Device *g_MainChainD3D9Device; // TH10 DAT_00491c30
extern MainChainRenderOwnerFrameState *g_MainChainRenderOwner; // DAT_00491c10
extern u32 g_AsciiHudBarValue; // TH10 DAT_00474c58
extern void *g_AsciiHudConditionalState; // TH10 DAT_00477704
extern float g_AsciiHudBarScale; // TH10 DAT_00470cb8
extern float g_AsciiHudBarOffset; // TH10 DAT_00470d2c
extern float g_AsciiHudProgressScale; // TH10 DAT_00470d28
extern float g_AsciiHudProgressOffset; // TH10 DAT_00470d24
extern float g_AsciiHudEntryOffset; // TH10 DAT_00470c64

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

void DispatchVmRange(u8 *first, u32 count)
{
    for (u32 index = 0; index != count; ++index)
        (void)DispatchAsciiAnimationVmRenderMode(first + index * 0x3ac,
            g_MainChainRenderOwner);
}

bool IsNativeNonZero(float value)
{
    return value != 0.0f || value != value;
}

float NativeOrderedMinimum(float left, float right)
{
    // The x87 branch selects left only for an ordered left < right.
    return left == left && left < right ? left : right;
}

} // namespace

void DrawImmediateAsciiColoredRectangle(const float rectangle[4], u32 color)
{
    FlushRenderOwnerPendingVertices(
        reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));

    const float left = rectangle[0];
    const float top = rectangle[1];
    const float right = rectangle[2];
    const float bottom = rectangle[3];
    const ImmediateColorVertex vertices[4] = {
        {left, top, 0.0f, 1.0f, color},
        {right, top, 0.0f, 1.0f, color},
        {left, bottom, 0.0f, 1.0f, color},
        {right, bottom, 0.0f, 1.0f, color}
    };
    D3D9Device *const device = g_MainChainD3D9Device;
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        device, 67))(device, 0, 4, 2);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        device, 67))(device, 0, 1, 2);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        device, 67))(device, 0, 5, 0);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        device, 67))(device, 0, 2, 0);
    (void)reinterpret_cast<D3DSetRenderStateFn>(GetD3DSlot(
        device, 57))(device, 0x14, 6);
    (void)reinterpret_cast<D3DSetFVFFn>(GetD3DSlot(device, 89))(device, 0x44);
    (void)reinterpret_cast<D3DDrawPrimitiveUPFn>(GetD3DSlot(
        device, 83))(device, 5, 2, vertices, sizeof(ImmediateColorVertex));

    LargeRenderOwnerLayout &owner =
        *reinterpret_cast<LargeRenderOwnerLayout *>(g_MainChainRenderOwner);
    owner.fvf_active_cache = 0xff;
    owner.glyph_texture_cache = 0;
    owner.bound_texture = 0;
    owner.state_cache_3ada69 = 0xff;
    owner.blend_mode_cache = 3;
    owner.state_cache_3ada6b = 0xff;
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        device, 67))(device, 0, 4, 4);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        device, 67))(device, 0, 1, 4);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        device, 67))(device, 0, 5, 2);
    (void)reinterpret_cast<D3DSetTextureStageStateFn>(GetD3DSlot(
        device, 67))(device, 0, 2, 2);
}

i32 RenderAsciiHudBatch(void *owner_memory)
{
    AsciiHudOwner &hud = *reinterpret_cast<AsciiHudOwner *>(owner_memory);
    // Raw view for the deliberately unconverted +0x8393 alpha byte (the
    // record-relative +0x2ff output byte has no named VmRecord field).
    u8 *const owner = reinterpret_cast<u8 *>(&hud);
    for (u32 index = 0; index != 10; ++index) {
        (void)DispatchAsciiAnimationVmRenderMode(&hud.pool_a[index],
            g_MainChainRenderOwner);
        (void)DispatchAsciiAnimationVmRenderMode(&hud.pool_b[index],
            g_MainChainRenderOwner);
    }
    DispatchVmRange(reinterpret_cast<u8 *>(hud.pool_c), 9);
    DispatchVmRange(reinterpret_cast<u8 *>(hud.pool_d), 4);
    DispatchVmRange(reinterpret_cast<u8 *>(hud.pool_f), 7);

    if (g_AsciiHudBarValue != 0) {
        const float y = static_cast<float>(static_cast<i32>(g_AsciiHudBarValue)) *
            g_AsciiHudBarScale + g_AsciiHudBarOffset;
        const u32 alpha = static_cast<u32>(owner[0x8393]) << 24;
        const float first[] = {51.0f, 461.0f, y, 463.0f};
        DrawImmediateAsciiColoredRectangle(first, alpha);
        const float second[] = {50.0f, 460.0f, y - 1.0f, 462.0f};
        DrawImmediateAsciiColoredRectangle(second, alpha | 0x00ffffffU);
    }

    const float progress = hud.boss_hp_fill;
    if (progress == progress && progress > 0.0f) {
        const float y = progress * g_AsciiHudProgressScale +
            g_AsciiHudProgressOffset;
        const float first[] = {41.0f, 23.0f, y, 25.0f};
        DrawImmediateAsciiColoredRectangle(first, 0xff000000U);
        const float second[] = {40.0f, 22.0f, y - 1.0f, 24.0f};
        DrawImmediateAsciiColoredRectangle(second, 0xffffffffU);
        for (u32 index = 0; index != 4; ++index) {
            const AsciiHudSpellBarEntry &entry = hud.spell_bars[index];
            const float value = entry.value;
            if (IsNativeNonZero(value)) {
                const float rectangle[] = {40.0f, 22.0f,
                    NativeOrderedMinimum(value, progress) *
                        g_AsciiHudProgressScale + g_AsciiHudEntryOffset,
                    24.0f};
                DrawImmediateAsciiColoredRectangle(rectangle, entry.color);
            }
        }
    }

    u8 *const conditional = static_cast<u8 *>(g_AsciiHudConditionalState);
    if (conditional != 0) {
        u8 *const state = *reinterpret_cast<u8 **>(conditional + 0x10);
        if (state != 0 && (*reinterpret_cast<const u32 *>(state + 0x2480) &
                           0x11U) == 0)
            (void)DispatchAsciiAnimationVmRenderMode(&hud.aux_vm,
                g_MainChainRenderOwner);
        if (hud.spell_countdown >= 0 &&
            state != 0 && hud.result_script_state == 0)
            DispatchVmRange(reinterpret_cast<u8 *>(hud.pool_e), 2);
    }
    return 1;
}

} // namespace th10
