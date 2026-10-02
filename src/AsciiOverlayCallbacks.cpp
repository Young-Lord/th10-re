#include "AsciiOverlayCallbacks.hpp"

#include "AsciiHudRenderer.hpp"
#include "MainChainRender.hpp"
#include "TitleScreenObject.hpp"

namespace th10 {

namespace {

extern D3D9Device *g_MainChainD3D9Device; // TH11 DAT_00491c30
extern void *g_MainChainRenderOwner; // TH11 DAT_00491c10
extern D3DViewport g_AsciiOverlayViewport; // TH11 DAT_00491cf4
extern i32 g_AsciiOverlayUpdateSuspended; // TH11 DAT_004918a4
extern void *g_TitleScreen; // TH11 DAT_00477810
extern i32 ConvertFloatToI32TowardZeroX87(float value);
extern u8 g_AsciiOverlayRandomState[]; // TH11 DAT_004918b0
extern float g_AsciiOverlayRenderOffsetX; // TH11 DAT_00491e64
extern float g_AsciiOverlayRenderOffsetY; // TH11 DAT_00491e68
extern const float g_AsciiOverlayInitialRate; // TH11 DAT_00476f78

u32 GetOverlayColor(const u8 *context)
{
    return (*reinterpret_cast<const u32 *>(context + 0x18) << 24) |
        *reinterpret_cast<const u32 *>(context + 0x20);
}

i32 WrapAdd(i32 left, i32 right)
{
    return static_cast<i32>(static_cast<u32>(left) + static_cast<u32>(right));
}

void AdvanceAsciiOverlayTime(u8 *context)
{
    const i32 tick = *reinterpret_cast<const i32 *>(context + 0x34);
    *reinterpret_cast<i32 *>(context + 0x30) = tick;
    const float rate = **reinterpret_cast<const float *const *>(context + 0x3c);
    float *const accumulated_time = reinterpret_cast<float *>(context + 0x38);
    if (rate > 0.99f && rate < 1.01f) {
        *reinterpret_cast<i32 *>(context + 0x34) = WrapAdd(tick, 1);
        *accumulated_time += 1.0f;
    } else {
        *accumulated_time += rate;
        *reinterpret_cast<i32 *>(context + 0x34) =
            ConvertFloatToI32TowardZeroX87(*accumulated_time);
    }
}

void ResetAsciiOverlayTimeForCountdown(u8 *context)
{
    if ((*reinterpret_cast<const u32 *>(context + 0x40) & 1U) == 0) {
        *reinterpret_cast<u32 *>(context + 0x40) |= 1U;
        *reinterpret_cast<i32 *>(context + 0x34) = 0;
        *reinterpret_cast<i32 *>(context + 0x30) =
            static_cast<i32>(0xfff0bdc1U);
        *reinterpret_cast<float *>(context + 0x38) = 0.0f;
        *reinterpret_cast<const float **>(context + 0x3c) =
            &g_AsciiOverlayInitialRate;
    }
    *reinterpret_cast<i32 *>(context + 0x34) = 0;
    *reinterpret_cast<i32 *>(context + 0x30) = -1;
    *reinterpret_cast<float *>(context + 0x38) = 0.0f;
}

u32 NextAsciiOverlayRandomValue()
{
    u16 *const seed = reinterpret_cast<u16 *>(g_AsciiOverlayRandomState);
    u32 first = static_cast<u32>(static_cast<u16>(*seed ^ 0x9630U));
    first = static_cast<u32>(static_cast<u16>(first - 0x6553U));
    const u32 high = (first << 2) + (first >> 14);
    const u32 second = (high ^ 0x9630U) - 0x6553U;
    *seed = static_cast<u16>(high);
    const u16 low = static_cast<u16>((second << 2) + (second >> 14));
    *reinterpret_cast<u32 *>(g_AsciiOverlayRandomState + 4) += 2;
    *seed = low;
    return (static_cast<u32>(static_cast<u16>(high)) << 16) | low;
}

u32 NextAsciiOverlayRandomModulo(u32 modulus)
{
    if (modulus == 0)
        return 0;
    return NextAsciiOverlayRandomValue() % modulus;
}

u32 NextAsciiOverlayKindOneRandomModulo()
{
    const u32 low = NextAsciiOverlayRandomValue() & 0xffffU;
    return ((low << 16) | low) % 3U;
}

float SelectAsciiOverlayOffset(float magnitude)
{
    switch (NextAsciiOverlayRandomModulo(3)) {
    case 1:
        return magnitude;
    case 2:
        return -magnitude;
    default:
        return 0.0f;
    }
}

float SelectAsciiOverlayKindOneOffset(float magnitude)
{
    switch (NextAsciiOverlayKindOneRandomModulo()) {
    case 1:
        return magnitude;
    case 2:
        return -magnitude;
    default:
        return 0.0f;
    }
}

} // namespace

i32 DrawAsciiFullScreenOverlayWithViewport(void *context_memory)
{
    const u8 *const context = static_cast<const u8 *>(context_memory);
    FlushRenderOwnerPendingVertices(
        reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
    g_AsciiOverlayViewport.x = 0;
    g_AsciiOverlayViewport.y = 0;
    g_AsciiOverlayViewport.width = 640;
    g_AsciiOverlayViewport.height = 480;
    SetD3D9Viewport(g_MainChainD3D9Device, &g_AsciiOverlayViewport);
    const float rectangle[] = {0.0f, 0.0f, 640.0f, 480.0f};
    DrawImmediateAsciiColoredRectangle(rectangle, GetOverlayColor(context));
    return 1;
}

i32 DrawAsciiInsetOverlayA(void *context_memory)
{
    const u8 *const context = static_cast<const u8 *>(context_memory);
    const float rectangle[] = {32.0f, 16.0f, 416.0f, 464.0f};
    DrawImmediateAsciiColoredRectangle(rectangle, GetOverlayColor(context));
    return 1;
}

i32 DrawAsciiInsetOverlayB(void *context_memory)
{
    const u8 *const context = static_cast<const u8 *>(context_memory);
    const float rectangle[] = {32.0f, 16.0f, 416.0f, 464.0f};
    DrawImmediateAsciiColoredRectangle(rectangle, GetOverlayColor(context));
    return 1;
}

i32 DrawAsciiInsetOverlayMaskedRgb(void *context_memory)
{
    const u8 *const context = static_cast<const u8 *>(context_memory);
    const u32 color = (*reinterpret_cast<const u32 *>(context + 0x18) << 24) |
        (*reinterpret_cast<const u32 *>(context + 0x24) & 0x00ffffffU);
    const float rectangle[] = {32.0f, 16.0f, 416.0f, 464.0f};
    DrawImmediateAsciiColoredRectangle(rectangle, color);
    return 1;
}

i32 UpdateAsciiOverlayFadeIn(void *context_memory)
{
    u8 *const context = static_cast<u8 *>(context_memory);
    if (g_AsciiOverlayUpdateSuspended != 0)
        return 7;
    const i32 duration = *reinterpret_cast<const i32 *>(context + 0x1c);
    const i32 tick = *reinterpret_cast<const i32 *>(context + 0x34);
    if (duration != 0) {
        i32 alpha;
        if (tick >= duration) {
            alpha = 255;
        } else {
            alpha = ConvertFloatToI32TowardZeroX87(
                *reinterpret_cast<const float *>(context + 0x38) * 255.0f /
                static_cast<float>(duration));
            if (alpha < 0)
                alpha = 0;
        }
        *reinterpret_cast<i32 *>(context + 0x18) = alpha;
    }
    if (tick >= WrapAdd(duration, 2))
        return 7;
    const TitleScreen *const ts = static_cast<const TitleScreen *>(g_TitleScreen);
    if (ts == 0 || (ts->flags & 5U) == 0)
        AdvanceAsciiOverlayTime(context);
    return 1;
}

i32 UpdateAsciiOverlayHalfAlpha(void *context_memory)
{
    u8 *const context = static_cast<u8 *>(context_memory);
    const i32 tick = *reinterpret_cast<const i32 *>(context + 0x34);
    if (*reinterpret_cast<const u32 *>(context + 0x2c) != 0) {
        if (tick > 8)
            return 7;
        *reinterpret_cast<i32 *>(context + 0x18) = 128 -
            ConvertFloatToI32TowardZeroX87(
                *reinterpret_cast<const float *>(context + 0x38) * 16.0f);
    } else {
        const i32 duration = *reinterpret_cast<const i32 *>(context + 0x1c);
        if (duration != 0 && tick <= duration)
            *reinterpret_cast<i32 *>(context + 0x18) =
                ConvertFloatToI32TowardZeroX87(
                    *reinterpret_cast<const float *>(context + 0x38) * 128.0f /
                    static_cast<float>(duration));
    }
    AdvanceAsciiOverlayTime(context);
    return 1;
}

i32 DrawAsciiOverlayKindSix(void *context_memory)
{
    const u8 *const context = static_cast<const u8 *>(context_memory);
    const float rectangle[] = {0.0f, 0.0f, 640.0f, 480.0f};
    DrawImmediateAsciiColoredRectangle(rectangle, GetOverlayColor(context));
    return 1;
}

i32 UpdateAsciiOverlayFullFade(void *context_memory)
{
    u8 *const context = static_cast<u8 *>(context_memory);
    if (g_AsciiOverlayUpdateSuspended != 0)
        return 7;

    const i32 duration = *reinterpret_cast<const i32 *>(context + 0x1c);
    if (duration != 0) {
        const i32 alpha = ConvertFloatToI32TowardZeroX87(
            255.0f - *reinterpret_cast<const float *>(context + 0x38) *
                255.0f / static_cast<float>(duration));
        *reinterpret_cast<i32 *>(context + 0x18) = alpha < 0 ? 0 : alpha;
    }
    if (*reinterpret_cast<const i32 *>(context + 0x34) >= duration)
        return 7;
    AdvanceAsciiOverlayTime(context);
    return 1;
}

i32 UpdateAsciiOverlayKindFive(void *context_memory)
{
    u8 *const context = static_cast<u8 *>(context_memory);
    if (g_AsciiOverlayUpdateSuspended != 0)
        return 0;

    const i32 duration = *reinterpret_cast<const i32 *>(context + 0x1c);
    const i32 tick = *reinterpret_cast<const i32 *>(context + 0x34);
    if (tick < duration) {
        const i32 target_alpha = static_cast<i32>(context[0x27]);
        const i32 decrease = ConvertFloatToI32TowardZeroX87(
            static_cast<float>(target_alpha) *
            *reinterpret_cast<const float *>(context + 0x38) /
            static_cast<float>(duration));
        const i32 alpha = static_cast<i32>(
            static_cast<u32>(target_alpha) - static_cast<u32>(decrease));
        *reinterpret_cast<i32 *>(context + 0x18) = alpha < 0 ? 0 : alpha;
        AdvanceAsciiOverlayTime(context);
        return 1;
    }

    const i32 count = WrapAdd(*reinterpret_cast<const i32 *>(context + 0x20),
                              -1);
    *reinterpret_cast<i32 *>(context + 0x18) = 0;
    *reinterpret_cast<i32 *>(context + 0x20) = count;
    if (count <= 0)
        return 0;
    ResetAsciiOverlayTimeForCountdown(context);
    AdvanceAsciiOverlayTime(context);
    return 1;
}

i32 UpdateAsciiOverlayKindOne(void *context_memory)
{
    u8 *const context = static_cast<u8 *>(context_memory);
    if (g_AsciiOverlayUpdateSuspended != 0)
        return 7;

    AdvanceAsciiOverlayTime(context);
    const i32 duration = *reinterpret_cast<const i32 *>(context + 0x1c);
    if (*reinterpret_cast<const i32 *>(context + 0x34) >= duration)
        return 7;

    const i32 initial_magnitude =
        *reinterpret_cast<const i32 *>(context + 0x20);
    const i32 magnitude_delta = static_cast<i32>(
        static_cast<u32>(*reinterpret_cast<const i32 *>(context + 0x24)) -
        static_cast<u32>(initial_magnitude));
    const float magnitude = static_cast<float>(initial_magnitude) +
        static_cast<float>(magnitude_delta) *
        *reinterpret_cast<const float *>(context + 0x38) /
        static_cast<float>(duration);
    g_AsciiOverlayRenderOffsetX = SelectAsciiOverlayKindOneOffset(magnitude);
    g_AsciiOverlayRenderOffsetY = SelectAsciiOverlayKindOneOffset(magnitude);
    return 1;
}

i32 UpdateAsciiOverlayKindEight(void *context_memory)
{
    u8 *const context = static_cast<u8 *>(context_memory);
    if (g_AsciiOverlayUpdateSuspended != 0)
        return 7;
    const TitleScreen *const ts = static_cast<const TitleScreen *>(g_TitleScreen);
    if (ts == 0 || (ts->flags & 0x77U) != 0)
        return 1;

    AdvanceAsciiOverlayTime(context);
    const i32 tick = *reinterpret_cast<const i32 *>(context + 0x34);
    const i32 ramp_in = *reinterpret_cast<const i32 *>(context + 0x20);
    const i32 hold = *reinterpret_cast<const i32 *>(context + 0x24);
    const i32 ramp_out = *reinterpret_cast<const i32 *>(context + 0x28);
    const u32 hold_end_bits = static_cast<u32>(ramp_in) +
        static_cast<u32>(hold);
    const u32 finish_bits = hold_end_bits + static_cast<u32>(ramp_out);
    float factor;
    if (tick < ramp_in) {
        factor = *reinterpret_cast<const float *>(context + 0x38) /
            static_cast<float>(ramp_in);
    } else if (tick < static_cast<i32>(hold_end_bits)) {
        factor = 1.0f;
    } else if (tick < static_cast<i32>(finish_bits)) {
        factor = (static_cast<float>(finish_bits) -
            *reinterpret_cast<const float *>(context + 0x38)) /
            static_cast<float>(static_cast<u32>(ramp_out));
    } else {
        return 7;
    }
    const float magnitude = static_cast<float>(
        *reinterpret_cast<const i32 *>(context + 0x1c)) * factor;
    g_AsciiOverlayRenderOffsetX = SelectAsciiOverlayOffset(magnitude);
    g_AsciiOverlayRenderOffsetY = SelectAsciiOverlayOffset(magnitude);
    return 1;
}

} // namespace th10
