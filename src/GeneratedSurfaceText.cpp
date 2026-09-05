#include "GeneratedSurfaceText.hpp"

#include <string.h>

#include "Th10Platform.hpp"

namespace th10 {

namespace {

extern GeneratedSurface g_GeneratedSurface; // TH10 DAT_00474cb0
extern GdiHandle g_GeneratedFontSlots[14]; // TH10 DAT_004918a0..68

extern GdiHandle SelectGeneratedGdiObject(GdiHandle dc, GdiHandle object);
extern i32 SetGeneratedBkMode(GdiHandle dc, i32 mode);
extern i32 SetGeneratedTextColor(GdiHandle dc, u32 color);
extern i32 TextOutGeneratedA(GdiHandle dc, i32 x, i32 y, const char *text,
                             i32 length);
extern void LoadSurfaceFromMemoryIgnoredResult(void *surface, const void *data,
                                               i32 source_pitch, u32 format,
                                               i32 width, i32 height);

struct TimelineSurfaceVtable {
    void *unknown[2];
    void (TH10_STDCALL *release)(void *object);
    u8 unknown_0c[0x3c];
    void *(TH10_STDCALL *prepare)(void *object, u32 flags);
};

i32 LengthZ(const char *text)
{
    const char *cursor = text;
    while (*cursor != '\0')
        ++cursor;
    return static_cast<i32>(cursor - text);
}

void UploadGeneratedSurfaceToTimelineTarget(TimelineD3DSurface *surface,
                                            i32 font_size, i32 column,
                                            const i32 src_rect[4])
{
    const i32 surface_width = surface->right - surface->left;
    i32 upload_width = surface_width + surface_width + 0x16;
    if (upload_width > 0x400)
        upload_width = 0x400;
    const i32 upload_height = column + column + 2;

    TimelineSurfaceVtable *const vtable =
        *reinterpret_cast<TimelineSurfaceVtable **>(surface);
    void *const hook = vtable->prepare(surface, 0);
    LoadSurfaceFromMemoryIgnoredResult(
        surface, g_GeneratedSurface.pixel_bits,
        g_GeneratedSurface.pitch_bytes,
        static_cast<u32>(g_GeneratedSurface.pixel_format_id),
        upload_width, upload_height);
    if (hook != 0)
        vtable->release(hook);
    (void)font_size;
    (void)src_rect;
}

} // namespace

void TintGeneratedSurfacePixels(GeneratedSurface *surface, u32 color)
{
    u16 *pixels = static_cast<u16 *>(surface->pixel_bits);
    i32 index = 0;
    while (index < surface->image_bytes) {
        *pixels = static_cast<u16>(
            (((color >> 0x14) & 0xf) << 4 | (color >> 0xc) & 0xf) << 4 |
            (color >> 4) & 0xf);
        index += 2;
        ++pixels;
    }
}

void PostProcessGeneratedSurfaceRows(GeneratedSurface *surface, i32 row_height)
{
    u8 *const pixels = static_cast<u8 *>(surface->pixel_bits);
    const i32 byte_count = surface->pitch_bytes * row_height;
    const i32 format = surface->pixel_format_id;

    if (format == 0x15) {
        i32 index = 3;
        if (3 < byte_count) {
            do {
                pixels[index] = static_cast<u8>(~pixels[index]);
                index += 4;
            } while (index < byte_count);
        }
        return;
    }

    if (format == 0x19) {
        if (byte_count > 0) {
            u16 *words = reinterpret_cast<u16 *>(pixels);
            i32 count = ((byte_count - 1) >> 1) + 1;
            do {
                u32 value = ~static_cast<u32>(*words) ^ 0x7fffU;
                *words = static_cast<u16>(value);
                if (static_cast<i32>(value >> 8) >= 0)
                    *words = static_cast<u16>(value & 0x8000U);
                ++words;
                --count;
            } while (count != 0);
        }
        return;
    }

    if (format != 0x1a)
        return;

    i32 index = 1;
    if (1 < byte_count) {
        do {
            pixels[index] = static_cast<u8>(pixels[index] ^ 0xf0);
            index += 2;
        } while (index < byte_count);
    }
}

void SmoothGeneratedSurfacePixels(GeneratedSurface *surface, u32 row_count)
{
    const u32 format = static_cast<u32>(surface->pixel_format_id);
    if (format == 0x15) {
        u8 *puVar8 = static_cast<u8 *>(surface->pixel_bits);
        for (u32 row = 0; row < row_count; ++row) {
            const u32 width = static_cast<u32>(surface->width);
            for (u32 column = 0; column < width; ++column) {
                u8 *const pbVar6 = puVar8 - 2;
                if (pbVar6[5] == 0) {
                    u32 neighbor_count = 0;
                    u32 red = 0;
                    u32 green = 0;
                    u32 blue = 0;
                    if (column != 0 && pbVar6[1] != 0) {
                        blue = pbVar6[-2];
                        green = pbVar6[-1];
                        red = pbVar6[0];
                        neighbor_count = 1;
                    }
                    if (column < width - 1 && pbVar6[9] != 0) {
                        red += pbVar6[8];
                        green += pbVar6[7];
                        blue += pbVar6[6];
                        ++neighbor_count;
                    }
                    if (row != 0) {
                        u8 *const above = puVar8 - surface->pitch_bytes;
                        if (above[3] != 0) {
                            red += above[2];
                            green += above[1];
                            blue += above[0];
                            ++neighbor_count;
                        }
                    }
                    if (row < static_cast<u32>(surface->height) - 1U) {
                        u8 *const below = puVar8 + surface->pitch_bytes;
                        if (below[3] != 0) {
                            red += below[2];
                            green += below[1];
                            blue += below[0];
                            ++neighbor_count;
                        }
                    }
                    if (neighbor_count > 1) {
                        red /= neighbor_count;
                        green /= neighbor_count;
                        blue /= neighbor_count;
                    }
                    pbVar6[4] = static_cast<u8>(red);
                    pbVar6[3] = static_cast<u8>(green);
                    puVar8[0] = static_cast<u8>(blue);
                }
                puVar8 += 4;
            }
        }
        return;
    }

    if (format != 0x1a)
        return;

    u16 *puVar10 = static_cast<u16 *>(surface->pixel_bits);
    const i32 row_stride = surface->pitch_bytes / 2;
    for (u32 row = 0; row < row_count; ++row) {
        const u32 width = static_cast<u32>(surface->width);
        for (u32 column = 0; column < width; ++column) {
            if ((reinterpret_cast<u8 *>(puVar10)[1] & 0xf0) == 0) {
                u32 neighbor_count = 0;
                u32 red = 0;
                u32 green = 0;
                u32 blue = 0;
                if (column != 0) {
                    const u16 previous = puVar10[-1];
                    if ((previous & 0xf000) != 0) {
                        red = (previous >> 8) & 0xf;
                        green = (previous >> 4) & 0xf;
                        blue = previous & 0xf;
                        neighbor_count = 1;
                    }
                }
                if (column < width - 1) {
                    const u16 next = puVar10[1];
                    if ((next & 0xf000) != 0) {
                        red += (next >> 8) & 0xf;
                        green += (next >> 4) & 0xf;
                        blue += (next & 0xf);
                        ++neighbor_count;
                    }
                }
                if (row != 0) {
                    const u16 above = puVar10[-row_stride];
                    if ((above & 0xf000) != 0) {
                        red += (above >> 8) & 0xf;
                        green += (above >> 4) & 0xf;
                        blue += (above & 0xf);
                        ++neighbor_count;
                    }
                }
                if (row < static_cast<u32>(surface->height) - 1U) {
                    const u16 below = puVar10[row_stride];
                    if ((below & 0xf000) != 0) {
                        red += (below >> 8) & 0xf;
                        green += (below >> 4) & 0xf;
                        blue += (below & 0xf);
                        ++neighbor_count;
                    }
                }
                if (neighbor_count > 1) {
                    red /= neighbor_count;
                    green /= neighbor_count;
                    blue /= neighbor_count;
                }
                *puVar10 = static_cast<u16>(
                    ((((red >> 1) & 0xf) << 4 | (green >> 1) & 0xf) << 4 |
                     (blue >> 1) & 0xf) |
                    (*puVar10 & 0xf000));
            }
            ++puVar10;
        }
    }
}

GdiHandle SelectGeneratedFontForSize(i32 font_size)
{
    i32 index = 0;
    if (font_size > 0x11) {
        index = font_size - 0x11;
        if (index > 13)
            index = 13;
    }
    return g_GeneratedFontSlots[index];
}

void RenderTimelineTextNormal(const i32 src_rect[4], i32 column, i32 font_size,
                              u32 color, const char *text,
                              TimelineD3DSurface *surface)
{
    GeneratedSurface *const gs = &g_GeneratedSurface;
    if (font_size < 0x11)
        font_size = 0x11;

    GdiHandle font = SelectGeneratedFontForSize(font_size);
    memset(gs->pixel_bits, 0, static_cast<u32>(gs->image_bytes));

    GdiHandle previous = SelectGeneratedGdiObject(gs->memory_dc, font);
    const i32 row_height = font_size * 2 + 6;
    PostProcessGeneratedSurfaceRows(gs, row_height);

    SetGeneratedBkMode(gs->memory_dc, 1);
    const i32 length = LengthZ(text);

    SetGeneratedTextColor(gs->memory_dc, 0);
    TextOutGeneratedA(gs->memory_dc, column * 2 + 2, 2, text, length);
    SetGeneratedTextColor(gs->memory_dc, color);
    TextOutGeneratedA(gs->memory_dc, column * 2, 0, text, length);

    SelectGeneratedGdiObject(gs->memory_dc, previous);
    PostProcessGeneratedSurfaceRows(gs, row_height);
    SmoothGeneratedSurfacePixels(gs, static_cast<u32>(row_height));

    UploadGeneratedSurfaceToTimelineTarget(surface, font_size, column, src_rect);
}

void RenderTimelineTextSelected(const i32 src_rect[4], i32 column, i32 font_size,
                                u32 color, const char *text,
                                TimelineD3DSurface *surface)
{
    GeneratedSurface *const gs = &g_GeneratedSurface;
    if (font_size < 0x11)
        font_size = 0x11;

    TintGeneratedSurfacePixels(gs, color);

    GdiHandle font = SelectGeneratedFontForSize(font_size);
    GdiHandle previous = SelectGeneratedGdiObject(gs->memory_dc, font);
    const i32 row_height = font_size * 2 + 6;
    PostProcessGeneratedSurfaceRows(gs, row_height);

    SetGeneratedBkMode(gs->memory_dc, 1);
    const i32 length = LengthZ(text);
    SetGeneratedTextColor(gs->memory_dc, color);
    TextOutGeneratedA(gs->memory_dc, column * 2, 0, text, length);

    SelectGeneratedGdiObject(gs->memory_dc, previous);
    PostProcessGeneratedSurfaceRows(gs, row_height);
    SmoothGeneratedSurfacePixels(gs, static_cast<u32>(row_height));

    UploadGeneratedSurfaceToTimelineTarget(surface, font_size, column, src_rect);
}

void SubmitTimelineTextDispatch(i32 font_size, TimelineTextWrapper *wrapper,
                                const char *text, TimelineD3DSurface *surface,
                                i32 column, u32 color, i32 selected)
{
    if (font_size < 1)
        font_size = 0x11;
    else if (font_size <= 8)
        return;

    extern i32 ConvertFloatToI32TowardZeroX87(float value);
    i32 rect[4];
    rect[0] = ConvertFloatToI32TowardZeroX87(wrapper->rect[0]);
    rect[1] = ConvertFloatToI32TowardZeroX87(wrapper->rect[1]);
    rect[2] = ConvertFloatToI32TowardZeroX87(wrapper->rect[2]);
    rect[3] = ConvertFloatToI32TowardZeroX87(wrapper->rect[3]);

    if (selected)
        RenderTimelineTextSelected(rect, column, font_size, color, text, surface);
    else
        RenderTimelineTextNormal(rect, column, font_size, color, text, surface);
}

} // namespace th10
