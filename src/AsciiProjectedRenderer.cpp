#include "AsciiProjectedRenderer.hpp"

#include <math.h>
#include <string.h>

#include "MainChainRender.hpp"

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

struct D3DVector4 {
    float x;
    float y;
    float z;
    float w;
};

typedef D3DVector3 *(*D3dxVec3ProjectFn)(D3DVector3 *, const D3DVector3 *,
                                          const D3DViewport *, const D3DMatrix *,
                                          const D3DMatrix *, const D3DMatrix *);

extern void D3dxMatrixRotationX(D3DMatrix *out, float angle);
extern void D3dxMatrixRotationY(D3DMatrix *out, float angle);
extern void D3dxMatrixRotationZ(D3DMatrix *out, float angle);
extern void D3dxMatrixMultiply(D3DMatrix *out, const D3DMatrix *left,
                               const D3DMatrix *right);
extern D3DVector3 *D3dxVec3Project(D3DVector3 *out, const D3DVector3 *point,
                                    const D3DViewport *viewport,
                                    const D3DMatrix *projection,
                                    const D3DMatrix *view,
                                    const D3DMatrix *world);
extern void *g_MainChainActiveCameraWork; // TH10 DAT_00491fac
extern AsciiGlyphVertex g_AsciiGlyphScratchQuad[4]; // TH10 0x4978c0
extern void SubmitAsciiGlyphScratchQuad(void *vm, void *owner, u32 flags);
extern D3DVector3 g_AsciiFogPosition; // TH10 DAT_00491d7c
extern float g_AsciiFogNearDistance; // TH10 DAT_00491e78
extern float g_AsciiFogCutoffDistance; // TH10 DAT_00491e7c
extern u8 g_AsciiFogBlue; // TH10 DAT_00491e80
extern u8 g_AsciiFogGreen; // TH10 DAT_00491e84
extern u8 g_AsciiFogRed; // TH10 DAT_00491e88
extern u32 g_AsciiFogFixedRgb; // TH10 DAT_00491e90
extern D3DVector4 *D3dxVec3Transform(D3DVector4 *out,
                                     const D3DVector3 *source,
                                     const D3DMatrix *matrix);

float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

u32 ReadU32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

u32 ModulateColor(u32 color, const u8 *owner)
{
    u32 output = 0;
    for (u32 index = 0; index != 4; ++index) {
        u32 channel = ((color >> (index * 8)) & 0xffU) *
            owner[0x732458 + index] >> 7;
        if (channel > 0xffU)
            channel = 0xffU;
        output |= channel << (index * 8);
    }
    return output;
}

void WriteAnchoredAxis(float *first, float *second, float extent, u32 anchor)
{
    switch (anchor) {
    case 0:
        *first = -extent * 0.5f;
        *second = extent * 0.5f;
        break;
    case 1:
        *first = 0.0f;
        *second = extent;
        break;
    case 2:
        *first = -extent;
        *second = 0.0f;
        break;
    }
}

D3DMatrix MakeTranslationMatrix(float x, float y, float z)
{
    D3DMatrix matrix;
    memset(&matrix, 0, sizeof(matrix));
    matrix.values[0] = 1.0f;
    matrix.values[5] = 1.0f;
    matrix.values[10] = 1.0f;
    matrix.values[15] = 1.0f;
    matrix.values[12] = x;
    matrix.values[13] = y;
    matrix.values[14] = z;
    return matrix;
}

D3DVector3 GetVmWorldPosition(const u8 *vm)
{
    D3DVector3 position = {
        ReadFloat(vm, 0x334) + ReadFloat(vm, 0x340) + ReadFloat(vm, 0x34c),
        ReadFloat(vm, 0x338) + ReadFloat(vm, 0x344) + ReadFloat(vm, 0x350),
        ReadFloat(vm, 0x33c) + ReadFloat(vm, 0x348) + ReadFloat(vm, 0x354)
    };
    return position;
}

void ProjectQuadVertex(AsciiGlyphVertex *destination, const D3DVector3 *source,
                       const D3DMatrix *world)
{
    const u8 *const view = static_cast<const u8 *>(g_MainChainActiveCameraWork);
    (void)D3dxVec3Project(reinterpret_cast<D3DVector3 *>(destination), source,
        reinterpret_cast<const D3DViewport *>(view + 0xcc),
        reinterpret_cast<const D3DMatrix *>(view + 0x8c),
        reinterpret_cast<const D3DMatrix *>(view + 0x4c), world);
}

} // namespace

// TH10 0x00443b60. The case-3 local coordinates deliberately remain
// uninitialized, matching the original stack-temporary behavior.
i32 BuildPerspectiveAsciiGlyphQuad(void *vm_memory)
{
    const u8 *const vm = static_cast<const u8 *>(vm_memory);
    const D3DVector3 world_position = GetVmWorldPosition(vm);
    const D3DMatrix world = MakeTranslationMatrix(world_position.x,
                                                    world_position.y,
                                                    world_position.z);
    const D3DVector3 origin = {0.0f, 0.0f, 0.0f};
    D3DVector3 projected_origin;
    ProjectQuadVertex(reinterpret_cast<AsciiGlyphVertex *>(&projected_origin),
                      &origin, &world);
    if (projected_origin.z < 0.0f || projected_origin.z > 1.0f)
        return -1;

    const u8 *const view = static_cast<const u8 *>(g_MainChainActiveCameraWork);
    D3DVector3 projected_reference;
    (void)D3dxVec3Project(&projected_reference,
        reinterpret_cast<const D3DVector3 *>(view + 0x30),
        reinterpret_cast<const D3DViewport *>(view + 0xcc),
        reinterpret_cast<const D3DMatrix *>(view + 0x8c),
        reinterpret_cast<const D3DMatrix *>(view + 0x4c), &world);
    const float delta_x = projected_reference.x - projected_origin.x;
    const float delta_y = projected_reference.y - projected_origin.y;
    const float delta_z = projected_reference.z - projected_origin.z;
    const float scale = 0.5f * static_cast<float>(sqrt(
        static_cast<double>(delta_x * delta_x + delta_y * delta_y +
                            delta_z * delta_z)));
    const float width = ReadFloat(vm, 0x3c) * ReadFloat(vm, 0x4c) * scale;
    const float height = ReadFloat(vm, 0x40) * ReadFloat(vm, 0x50) * scale;
    float left;
    float right;
    float top;
    float bottom;
    const u32 flags = ReadU32(vm, 0x35c);
    WriteAnchoredAxis(&left, &right, width, (flags >> 18) & 3);
    WriteAnchoredAxis(&top, &bottom, height, (flags >> 20) & 3);

    const float angle = ReadFloat(vm, 0x2c);
    const float cosine = static_cast<float>(cos(static_cast<double>(angle)));
    const float sine = static_cast<float>(sin(static_cast<double>(angle)));
    const float input_x[] = {left, right, left, right};
    const float input_y[] = {top, top, bottom, bottom};
    for (u32 index = 0; index != 4; ++index) {
        g_AsciiGlyphScratchQuad[index].x = input_x[index] * cosine -
            input_y[index] * sine + projected_origin.x;
        g_AsciiGlyphScratchQuad[index].y = input_x[index] * sine +
            input_y[index] * cosine + projected_origin.y;
        g_AsciiGlyphScratchQuad[index].z = projected_origin.z;
    }
    return 0;
}

// TH10 0x00444240 semantic body. Every D3DX result is intentionally ignored.
u32 BuildAndProjectMode7AsciiGlyphQuad(void *vm_memory, void *owner)
{
    u8 *const vm = static_cast<u8 *>(vm_memory);
    u32 flags = ReadU32(vm, 0x35c);
    if ((flags & 0x4000U) == 0 && (flags & 0x0cU) != 0) {
        flags &= ~0x08U;
        *reinterpret_cast<u32 *>(vm + 0x35c) = flags;
        D3DMatrix *const matrix = reinterpret_cast<D3DMatrix *>(vm + 0x27c);
        memcpy(matrix, vm + 0x23c, sizeof(*matrix));
        matrix->values[0] *= ReadFloat(vm, 0x3c);
        matrix->values[5] *= ReadFloat(vm, 0x40);
        D3DMatrix rotation;
        if (ReadFloat(vm, 0x24) != 0.0f) {
            D3dxMatrixRotationX(&rotation, ReadFloat(vm, 0x24));
            D3dxMatrixMultiply(matrix, matrix, &rotation);
        }
        if (ReadFloat(vm, 0x28) != 0.0f) {
            D3dxMatrixRotationY(&rotation, ReadFloat(vm, 0x28));
            D3dxMatrixMultiply(matrix, matrix, &rotation);
        }
        if (ReadFloat(vm, 0x2c) != 0.0f) {
            D3dxMatrixRotationZ(&rotation, ReadFloat(vm, 0x2c));
            D3dxMatrixMultiply(matrix, matrix, &rotation);
        }
        *reinterpret_cast<u32 *>(vm + 0x35c) &= ~0x04U;
    }

    D3DMatrix world = *reinterpret_cast<D3DMatrix *>(vm + 0x27c);
    const D3DVector3 position = GetVmWorldPosition(vm);
    world.values[12] += position.x;
    world.values[13] += position.y;
    world.values[14] = position.z;

    float left;
    float right;
    float top;
    float bottom;
    flags = ReadU32(vm, 0x35c);
    WriteAnchoredAxis(&left, &right, 256.0f, (flags >> 18) & 3);
    WriteAnchoredAxis(&top, &bottom, 256.0f, (flags >> 20) & 3);
    const D3DVector3 local[] = {
        {left, top, 0.0f}, {right, top, 0.0f},
        {left, bottom, 0.0f}, {right, bottom, 0.0f}
    };
    for (u32 index = 0; index != 4; ++index)
        ProjectQuadVertex(&g_AsciiGlyphScratchQuad[index], &local[index], &world);
    memcpy(static_cast<u8 *>(owner) + 0x3ad0f0, &world, sizeof(world));
    return 0;
}

// TH10 0x00444580 semantic body.
u32 DrawAsciiAnimationVmProjectedMode5(void *vm, void *owner)
{
    (void)BuildAndProjectMode7AsciiGlyphQuad(vm, owner);
    SubmitAsciiGlyphScratchQuad(vm, owner, 0);
    for (u32 index = 0; index != 4; ++index)
        g_AsciiGlyphScratchQuad[index].rhw = 1.0f;
    return 0;
}

// TH10 0x00443fb0 semantic body. The native preserves the evidenced absence
// of fog-range/denominator checks and does not clamp the fogged channels.
i32 DrawAsciiAnimationVmPerspectiveFadedMode6(void *vm_memory, void *owner)
{
    const u8 *const vm = static_cast<const u8 *>(vm_memory);
    const u8 *const owner_bytes = static_cast<const u8 *>(owner);
    if (BuildPerspectiveAsciiGlyphQuad(vm_memory) != 0)
        return -1;

    const D3DVector3 world = GetVmWorldPosition(vm);
    const float dx = world.x - g_AsciiFogPosition.x;
    const float dy = world.y - g_AsciiFogPosition.y;
    const float dz = world.z - g_AsciiFogPosition.z;
    const float distance = static_cast<float>(sqrt(static_cast<double>(
        dx * dx + dy * dy + dz * dz)));
    const u32 flags = ReadU32(vm, 0x35c);
    u32 color = (flags & 0x8000U) != 0 ? ReadU32(vm, 0x300) :
        ReadU32(vm, 0x2fc);
    if (*reinterpret_cast<const u32 *>(owner_bytes + 0x73245c) != 0)
        color = ModulateColor(color, owner_bytes);
    if (distance > g_AsciiFogNearDistance) {
        const float fade = (g_AsciiFogNearDistance - distance) /
            (g_AsciiFogNearDistance - g_AsciiFogCutoffDistance);
        if (fade >= 1.0f)
            return -1;
        const u32 sources[] = {g_AsciiFogBlue, g_AsciiFogGreen, g_AsciiFogRed};
        for (u32 index = 0; index != 3; ++index) {
            const u32 shift = index * 8;
            const i32 channel = static_cast<i32>((color >> shift) & 0xffU);
            const i32 adjusted = channel - static_cast<i32>(
                static_cast<float>(channel - static_cast<i32>(sources[index])) * fade);
            color = (color & ~(0xffU << shift)) |
                ((static_cast<u32>(adjusted) & 0xffU) << shift);
        }
        const i32 alpha = static_cast<i32>((1.0f - fade) *
            static_cast<float>(color >> 24));
        color = (color & 0x00ffffffU) |
            ((static_cast<u32>(alpha) & 0xffU) << 24);
    }
    for (u32 index = 0; index != 4; ++index)
        g_AsciiGlyphScratchQuad[index].color = color;
    SubmitAsciiGlyphScratchQuad(vm_memory, owner, 2);
    return 0;
}

u32 DrawAsciiAnimationVmProjectedFoggedMode7(void *owner, void *vm_memory)
{
    const u8 *const vm = static_cast<const u8 *>(vm_memory);
    const u8 *const owner_bytes = static_cast<const u8 *>(owner);
    (void)BuildAndProjectMode7AsciiGlyphQuad(vm_memory, owner);
    const u32 source_color = (ReadU32(vm, 0x35c) & 0x8000U) != 0 ?
        ReadU32(vm, 0x300) : ReadU32(vm, 0x2fc);
    for (u32 index = 0; index != 4; ++index) {
        D3DVector4 transformed;
        (void)D3dxVec3Transform(&transformed,
            reinterpret_cast<const D3DVector3 *>(owner_bytes + 0x3ada78 +
                                                 index * 0x14),
            reinterpret_cast<const D3DMatrix *>(owner_bytes + 0x3ad0f0));
        const float dx = transformed.x - g_AsciiFogPosition.x;
        const float dy = transformed.y - g_AsciiFogPosition.y;
        const float dz = transformed.z - g_AsciiFogPosition.z;
        const float distance = static_cast<float>(sqrt(static_cast<double>(
            dx * dx + dy * dy + dz * dz)));
        u32 color = source_color;
        if (distance > g_AsciiFogNearDistance) {
            const float fade = (g_AsciiFogNearDistance - distance) /
                (g_AsciiFogNearDistance - g_AsciiFogCutoffDistance);
            if (!(fade < 1.0f)) {
                color = (source_color & 0xff000000U) |
                    (g_AsciiFogFixedRgb & 0x00ffffffU);
            } else {
                const u8 targets[] = {g_AsciiFogBlue, g_AsciiFogGreen,
                                      g_AsciiFogRed};
                for (u32 channel_index = 0; channel_index != 3;
                     ++channel_index) {
                    const u32 shift = channel_index * 8;
                    const i32 channel = static_cast<i32>((source_color >> shift) & 0xffU);
                    const i32 adjusted = channel - static_cast<i32>(
                        static_cast<float>(channel - static_cast<i32>(targets[channel_index])) *
                        fade);
                    color = (color & ~(0xffU << shift)) |
                        ((static_cast<u32>(adjusted) & 0xffU) << shift);
                }
            }
        }
        g_AsciiGlyphScratchQuad[index].color = color;
    }
    SubmitAsciiGlyphScratchQuad(vm_memory, owner, 2);
    for (u32 index = 0; index != 4; ++index)
        g_AsciiGlyphScratchQuad[index].rhw = 1.0f;
    return 0;
}

} // namespace th10
