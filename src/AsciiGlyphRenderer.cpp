#include "AsciiGlyphRenderer.hpp"
#include "AsciiProjectedRenderer.hpp"

#include <math.h>

#include "Th10Types.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

struct AsciiGlyphScratchVertex {
    float x;
    float y;
    float z;
    float rhw;
    u32 color;
    float u;
    float v;
};

extern void *g_MainChainRenderOwner;
extern AsciiGlyphScratchVertex g_AsciiGlyphScratchQuad[4]; // TH10 0x4978c0
extern void SubmitAsciiGlyphScratchQuad(void *vm, void *owner,
                                        u32 pixel_snap); // TH10 0x442670

float ReadFloat(const u8 *record, u32 offset)
{
    return *reinterpret_cast<const float *>(record + offset);
}

u32 ReadWord(const u8 *record, u32 offset)
{
    return *reinterpret_cast<const u32 *>(record + offset);
}

void WriteHorizontalRange(float base, float extent, u32 anchor, bool floor_center)
{
    float first;
    switch (anchor) {
    case 0:
        first = base - extent * 0.5f;
        if (floor_center)
            first = static_cast<float>(floor(static_cast<double>(first)));
        g_AsciiGlyphScratchQuad[0].x = first;
        g_AsciiGlyphScratchQuad[2].x = first;
        g_AsciiGlyphScratchQuad[1].x = first + extent;
        g_AsciiGlyphScratchQuad[3].x = first + extent;
        break;
    case 1:
        g_AsciiGlyphScratchQuad[0].x = base;
        g_AsciiGlyphScratchQuad[2].x = base;
        g_AsciiGlyphScratchQuad[1].x = base + extent;
        g_AsciiGlyphScratchQuad[3].x = base + extent;
        break;
    case 2:
        g_AsciiGlyphScratchQuad[0].x = base - extent;
        g_AsciiGlyphScratchQuad[2].x = base - extent;
        g_AsciiGlyphScratchQuad[1].x = base;
        g_AsciiGlyphScratchQuad[3].x = base;
        break;
    }
}

void WriteVerticalRange(float base, float extent, u32 anchor, bool floor_center)
{
    float first;
    switch (anchor) {
    case 0:
        first = base - extent * 0.5f;
        if (floor_center)
            first = static_cast<float>(floor(static_cast<double>(first)));
        g_AsciiGlyphScratchQuad[0].y = first;
        g_AsciiGlyphScratchQuad[1].y = first;
        g_AsciiGlyphScratchQuad[2].y = first + extent;
        g_AsciiGlyphScratchQuad[3].y = first + extent;
        break;
    case 1:
        g_AsciiGlyphScratchQuad[0].y = base;
        g_AsciiGlyphScratchQuad[1].y = base;
        g_AsciiGlyphScratchQuad[2].y = base + extent;
        g_AsciiGlyphScratchQuad[3].y = base + extent;
        break;
    case 2:
        g_AsciiGlyphScratchQuad[0].y = base - extent;
        g_AsciiGlyphScratchQuad[1].y = base - extent;
        g_AsciiGlyphScratchQuad[2].y = base;
        g_AsciiGlyphScratchQuad[3].y = base;
        break;
    }
}

void DrawAsciiAnimationVmQuad(void *vm_memory, bool scaled, void *owner)
{
    const u8 *const vm = static_cast<const u8 *>(vm_memory);
    const VmRecord &vm_record = *reinterpret_cast<const VmRecord *>(vm);
    const float width = vm_record.scale_x * ReadFloat(vm, 0x4c);
    float height = vm_record.scale_y * ReadFloat(vm, 0x50);
    if (scaled)
        height *= 0.5f;

    const float axis_x = vm_record.base_pos_x + vm_record.delta_pos_x +
        vm_record.alt_pos_x;
    const float axis_y = vm_record.base_pos_y + vm_record.delta_pos_y +
        vm_record.alt_pos_y;
    const float depth = vm_record.base_pos_z + vm_record.delta_pos_z +
        vm_record.alt_pos_z;
    const u32 flags = vm_record.flags;

    WriteHorizontalRange(axis_x, width, (flags >> 18) & 3, !scaled);
    WriteVerticalRange(axis_y, height, (flags >> 20) & 3, !scaled);
    for (u32 index = 0; index != 4; ++index)
        g_AsciiGlyphScratchQuad[index].z = depth;

    SubmitAsciiGlyphScratchQuad(vm_memory, owner, scaled ? 0 : 1);
}

void DrawRotatedAsciiAnimationVmQuad(void *vm_memory, void *owner,
                                     bool mode1_nan_fallback)
{
    const u8 *const vm = static_cast<const u8 *>(vm_memory);
    const VmRecord &vm_record = *reinterpret_cast<const VmRecord *>(vm);
    const float angle = vm_record.rotation_z;
    if (angle == 0.0f || (mode1_nan_fallback && angle != angle)) {
        DrawAsciiAnimationVmQuad(vm_memory, true, owner);
        return;
    }

    const float width = vm_record.scale_x * ReadFloat(vm, 0x4c);
    const float height = vm_record.scale_y * ReadFloat(vm, 0x50);
    const float axis_x = vm_record.base_pos_x + vm_record.delta_pos_x +
        vm_record.alt_pos_x;
    const float axis_y = vm_record.base_pos_y + vm_record.delta_pos_y +
        vm_record.alt_pos_y;
    const float depth = vm_record.base_pos_z + vm_record.delta_pos_z +
        vm_record.alt_pos_z;
    const u32 flags = vm_record.flags;
    float left;
    float right;
    float top;
    float bottom;
    switch ((flags >> 18) & 3) {
    case 0: left = -width * 0.5f; right = width * 0.5f; break;
    case 1: left = 0.0f; right = width; break;
    case 2: left = -width; right = 0.0f; break;
    }
    switch ((flags >> 20) & 3) {
    case 0: top = -height * 0.5f; bottom = height * 0.5f; break;
    case 1: top = 0.0f; bottom = height; break;
    case 2: top = -height; bottom = 0.0f; break;
    }
    const float cosine = static_cast<float>(cos(static_cast<double>(angle)));
    const float sine = static_cast<float>(sin(static_cast<double>(angle)));
    const float input_x[] = {left, right, left, right};
    const float input_y[] = {top, top, bottom, bottom};
    for (u32 index = 0; index != 4; ++index) {
        g_AsciiGlyphScratchQuad[index].x = input_x[index] * cosine -
            input_y[index] * sine + axis_x;
        g_AsciiGlyphScratchQuad[index].y = input_x[index] * sine +
            input_y[index] * cosine + axis_y;
        g_AsciiGlyphScratchQuad[index].z = depth;
    }
    SubmitAsciiGlyphScratchQuad(vm_memory, owner, 0);
}

} // namespace

// TH10 0x00443080 semantic body. Native input is ESI plus one stack owner.
void DrawAsciiAnimationVmUnscaled(void *vm)
{
    DrawAsciiAnimationVmUnscaledToOwner(vm, g_MainChainRenderOwner);
}

// TH10 0x00443290 semantic body. Native input is EAX plus one stack owner.
void DrawAsciiAnimationVmScaled(void *vm)
{
    DrawAsciiAnimationVmScaledToOwner(vm, g_MainChainRenderOwner);
}

void DrawAsciiAnimationVmUnscaledToOwner(void *vm, void *owner)
{
    DrawAsciiAnimationVmQuad(vm, false, owner);
}

void DrawAsciiAnimationVmScaledToOwner(void *vm, void *owner)
{
    DrawAsciiAnimationVmQuad(vm, true, owner);
}

// TH10 0x004436c0 semantic body. Native input is EDX=vm, ECX=owner.
void DrawAsciiAnimationVmRotatedMode1(void *vm, void *owner)
{
    DrawRotatedAsciiAnimationVmQuad(vm, owner, true);
}

// TH10 0x00443910 semantic body. Native input is EDX=vm, ECX=owner.
void DrawAsciiAnimationVmRotatedMode3(void *vm, void *owner)
{
    DrawRotatedAsciiAnimationVmQuad(vm, owner, false);
}

// TH10 0x00443f80 semantic body. Native input is ESI=vm plus stack owner.
i32 DrawAsciiAnimationVmProjectedMode4(void *vm, void *owner)
{
    if (BuildPerspectiveAsciiGlyphQuad(vm) != 0)
        return -1;
    SubmitAsciiGlyphScratchQuad(vm, owner, 0);
    return 0;
}

} // namespace th10
