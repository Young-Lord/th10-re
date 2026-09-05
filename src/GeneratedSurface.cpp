#include <string.h>

#include "GeneratedFontTable.hpp"

namespace th10 {

namespace {

struct PixelFormatDescriptor {
    i32 id;
    i32 bits_per_pixel;
    u32 alpha_mask;
    u32 red_mask;
    u32 green_mask;
    u32 blue_mask;
};

struct BitmapV4HeaderPartial {
    u32 size;
    i32 width;
    i32 height;
    u16 planes;
    u16 bit_count;
    u32 compression;
    u32 image_size;
    u8 unknown_0018[0x10];
    u32 red_mask;
    u32 green_mask;
    u32 blue_mask;
    u32 alpha_mask;
    u8 unknown_0038[0x34];
};

typedef char AssertBitmapV4HeaderSize[
    sizeof(BitmapV4HeaderPartial) == 0x6c ? 1 : -1];

const PixelFormatDescriptor kPixelFormats[] = {
    { 0x16, 32, 0x00000000, 0x00ff0000, 0x0000ff00, 0x000000ff },
    { 0x15, 32, 0xff000000, 0x0000ff00, 0x00ff0000, 0x000000ff },
    { 0x18, 16, 0x00000000, 0x00007c00, 0x000003e0, 0x0000001f },
    { 0x17, 16, 0x00000000, 0x0000f800, 0x000007e0, 0x0000001f },
    { 0x19, 16, 0x00800000, 0x00007c00, 0x000003e0, 0x0000001f },
    { 0x1a, 16, 0x00f00000, 0x00000f00, 0x000000f0, 0x0000000f }
};

extern GdiHandle CreateGeneratedDibSection(const BitmapV4HeaderPartial *header,
                                            void **bits);
extern GdiHandle CreateGeneratedCompatibleDc();
extern GdiHandle SelectGeneratedGdiObject(GdiHandle dc, GdiHandle object);
extern void DeleteGeneratedDc(GdiHandle dc);
extern void DeleteGeneratedGdiObject(GdiHandle object);

const PixelFormatDescriptor *FindKnownPixelFormat(i32 id)
{
    for (i32 index = 0; index < 6; ++index) {
        if (kPixelFormats[index].id == id)
            return &kPixelFormats[index];
    }
    return 0;
}

i32 PositivePitch(i32 width, i32 bits_per_pixel)
{
    const i32 bytes = (width * bits_per_pixel + 7) / 8;
    return ((bytes + 3) / 4) * 4;
}

} // namespace

// TH10 0x00436a30 semantic body. A non-null bitmap with a null DC is not
// released: that conditional ownership boundary is intentional here.
void DestroyGeneratedSurfaceEsiAbi(GeneratedSurface *surface)
{
    if (surface->memory_dc == 0)
        return;

    SelectGeneratedGdiObject(surface->memory_dc, surface->previous_selection);
    DeleteGeneratedDc(surface->memory_dc);
    DeleteGeneratedGdiObject(surface->bitmap);
    surface->width = 0;
    surface->height = 0;
    surface->memory_dc = 0;
    surface->bitmap = 0;
    surface->previous_selection = 0;
    surface->pixel_bits = 0;
    surface->pixel_format_id = -1;
}

// TH10 0x00436af0 semantic body for known descriptor IDs. The native target
// reads beyond its descriptor sentinel for unknown non--1 IDs; this C++ form
// makes that malformed-input behavior an explicit adapter boundary.
bool ConstructGeneratedSurfaceEaxAbi(GeneratedSurface *surface,
                                     i32 width, i32 height,
                                     i32 pixel_format_id)
{
    DestroyGeneratedSurfaceEsiAbi(surface);
    const PixelFormatDescriptor *format = FindKnownPixelFormat(pixel_format_id);
    if (format == 0)
        return false;

    const i32 pitch = PositivePitch(width, format->bits_per_pixel);
    const i32 image_bytes = pitch * height;
    BitmapV4HeaderPartial header;
    memset(&header, 0, sizeof(header));
    header.size = sizeof(header);
    header.width = width;
    header.height = -height;
    header.planes = 1;
    header.bit_count = static_cast<u16>(format->bits_per_pixel);
    header.compression = pixel_format_id == 0x16 || pixel_format_id == 0x18
        ? 0 : 3;
    header.image_size = image_bytes;
    header.red_mask = format->red_mask;
    header.green_mask = format->green_mask;
    header.blue_mask = format->blue_mask;
    header.alpha_mask = format->alpha_mask;

    void *bits = 0;
    GdiHandle bitmap = CreateGeneratedDibSection(&header, &bits);
    if (bitmap == 0)
        return false;

    memset(bits, 0, static_cast<u32>(image_bytes));
    GdiHandle dc = CreateGeneratedCompatibleDc();
    GdiHandle previous = SelectGeneratedGdiObject(dc, bitmap);
    surface->previous_selection = previous;
    surface->pitch_bytes = pitch;
    surface->pixel_bits = bits;
    surface->image_bytes = image_bytes;
    surface->memory_dc = dc;
    surface->bitmap = bitmap;
    surface->width = width;
    surface->height = height;
    surface->pixel_format_id = pixel_format_id;
    return true;
}

} // namespace th10
