// TH10 0x437750 — generated-surface texture blitter (companion of the
// GeneratedFontTable 0x437a00 static initializer; the GDI-rendered glyph
// bitmaps are blended into locked D3D texture memory here).
//
// Native ABI: EAX = destination row, ECX = destination column, EDX = source
// GeneratedSurface, stack = {texture, columns, rows}; `ret 0xc`, boolean
// result in AL. EDI and ECX survive into the kernel bodies (destination
// pointer arithmetic), EBX carries the destination column.
//
// Flow:
//   1. surface->bitmap (+0x11c) must be set, otherwise return false.
//   2. texture vtable slot +0x30 fills a D3DSURFACE_DESC-like block whose
//      first dword (pixel format) must equal surface->pixel_format_id
//      (+0x100); a mismatch skips the blend (but still unlocks, returning
//      true).
//   3. texture vtable slot +0x34 locks the texture: {pitch, bits} come back
//      in the first two dwords of the locked record; the caller pre-zeroes
//      +0x08/+0x0c and pre-fills +0x10/+0x14 with the surface dimensions.
//      A failure returns false without unlocking.
//   4. the blend kernel is selected by the surface format id:
//        0x15 (A8R8G8B8) per-byte lerp with the source alpha byte,
//        0x19 (A1R5G5B5) copy only when the 0x8000 alpha bit is set,
//        0x1a (A4R4G4B4) 4-bit nibble blend with accumulative alpha,
//      any other id skips the blend.
//   5. texture vtable slot +0x38 unlocks; return true.
//
// Destination pixel (row, column) = pitch * row + bits + column * bpp; the
// source is surface->pixel_bits with surface->pitch_bytes. Both sides
// advance by (stride - bpp * columns) per row.
#include "GeneratedSurfaceBlit.hpp"

#include "GeneratedFontTable.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

typedef i32 (TH10_STDCALL *TextureGetDescLikeFn)(void *texture,
                                                 void *descriptor);
typedef i32 (TH10_STDCALL *TextureLockLikeFn)(void *texture,
                                              void *locked, void *rect,
                                              u32 flags);
typedef i32 (TH10_STDCALL *TextureUnlockLikeFn)(void *texture);

struct LockedTextureRegion {
    i32 pitch_bytes;  // +0x00, filled by the lock call
    u8 *bits;         // +0x04, filled by the lock call
    u32 zero_0008;    // zeroed before the call
    u32 zero_000c;    // zeroed before the call
    i32 width;        // +0x10, pre-filled with surface->width
    i32 height;       // +0x14, pre-filled with surface->height
};

inline u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

inline void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

inline u8 LoadU8At(const void *base, u32 offset)
{
    return *(static_cast<const u8 *>(base) + offset);
}

// Blend one A8R8G8B8 pixel: every stored channel is
// src + ((dst - src) * src_alpha) >> 8 with 8-bit truncation; the alpha
// byte itself is not written.
void BlendArgb8888Pixel(u8 *dst, const u8 *src)
{
    const i32 alpha = static_cast<i32>(src[3]);
    for (u32 channel = 0; channel != 3; ++channel) {
        const i32 blended =
            (static_cast<i32>(dst[channel]) - static_cast<i32>(src[channel])) *
                alpha >>
            8;
        dst[channel] = static_cast<u8>(blended + src[channel]);
    }
}

// Blend one A4R4G4B4 pixel (2 bytes, nibble channels). Native quirk: the
// high nibble of byte 0 (green) blends against the destination while the
// low nibble (red) is written as src * alpha >> 4 without the destination
// baseline; the alpha nibble of byte 1 accumulates (dst + src, capped 15).
void BlendArgb4444Pixel(u8 *dst, const u8 *src)
{
    const i32 alpha = static_cast<i32>(src[1] >> 4);

    const i32 dst_green = static_cast<i32>(dst[0]) >> 4;
    const i32 src_green = static_cast<i32>(src[0]) >> 4;
    const i32 blended_green =
        (((src_green - dst_green) * alpha) >> 4) + dst_green;
    dst[0] = static_cast<u8>((blended_green & 0xff) << 4);

    i32 red = ((static_cast<i32>(src[0] & 0xf) - 0) * alpha) >> 4;
    red = static_cast<signed char>(static_cast<u8>(red) +
                                   static_cast<u8>(dst[0]));
    dst[0] = static_cast<u8>(dst[0] | static_cast<u8>(red));

    i32 accumulated_alpha = ((static_cast<i32>(dst[1]) >> 2) & 0x3c) + alpha;
    if (accumulated_alpha >= 0x10) {
        accumulated_alpha = 0xf;
    }
    const i32 dst_blue = static_cast<i32>(dst[1]) & 0xf;
    const i32 src_blue = static_cast<i32>(src[1]) & 0xf;
    const i32 blended_blue =
        (((src_blue - dst_blue) * alpha) >> 4) + dst_blue;
    dst[1] = static_cast<u8>(((accumulated_alpha & 0xff) << 4) |
                             (blended_blue & 0xff));
}

// The shared row walk: dst starts at pitch*row + bits + column*bpp, src at
// the surface bitmap; both advance by (stride - bpp*columns) per row. The
// native re-derives the strides from the reused argument slots; the locals
// below carry the same values.
void RunBlendKernel(u8 *destination, u8 *source, i32 destination_stride,
                    i32 source_stride, i32 columns, i32 rows, u32 bpp,
                    u32 format_id)
{
    for (i32 row = 0; row < rows; ++row) {
        u8 *dst = destination;
        u8 *src = source;
        for (i32 column = 0; column < columns; ++column) {
            if (format_id == 0x15U) {
                BlendArgb8888Pixel(dst, src);
            } else if (format_id == 0x1aU) {
                BlendArgb4444Pixel(dst, src);
            } else if (format_id == 0x19U) {
                const u16 pixel =
                    *reinterpret_cast<const u16 *>(src);
                if (static_cast<i16>(pixel) < 0) {
                    *reinterpret_cast<u16 *>(dst) = pixel;
                }
            }
            dst += bpp;
            src += bpp;
        }
        destination += destination_stride - bpp * columns;
        source += source_stride - bpp * columns;
    }
}

} // namespace

bool BlendGeneratedSurfaceIntoTexture(void *texture,
                                      const GeneratedSurface *surface,
                                      u32 destination_row,
                                      u32 destination_column, i32 columns,
                                      i32 rows)
{
    if (LoadU32At(surface, 0x11cU) == 0) { // surface->bitmap
        return false;
    }

    void **const texture_vtable =
        *reinterpret_cast<void ***>(texture);

    u32 texture_format = 0;
    {
        u32 descriptor[8]; // D3DSURFACE_DESC-like; only the format is read
        (void)reinterpret_cast<TextureGetDescLikeFn>(texture_vtable[12])(
            texture, descriptor);
        texture_format = descriptor[0];
    }

    LockedTextureRegion locked;
    locked.zero_0008 = 0;
    locked.zero_000c = 0;
    locked.width = static_cast<i32>(LoadU32At(surface, 0x104U));
    locked.height = static_cast<i32>(LoadU32At(surface, 0x108U));
    {
        const u32 source_rect[2] = { 0U, 0U };
        if (reinterpret_cast<TextureLockLikeFn>(texture_vtable[13])(
                texture, &locked, const_cast<u32 *>(source_rect), 0U) != 0) {
            return false;
        }
    }

    const u32 surface_format = LoadU32At(surface, 0x100U);
    if (texture_format != surface_format) {
        (void)reinterpret_cast<TextureUnlockLikeFn>(texture_vtable[14])(
            texture);
        return true;
    }

    const i32 pitch = locked.pitch_bytes;
    u8 *destination =
        locked.bits + pitch * static_cast<i32>(destination_row) +
        static_cast<i32>(destination_column) *
            (surface_format == 0x15U
                 ? 4U
                 : (surface_format == 0x1aU || surface_format == 0x19U
                        ? 2U
                        : 0U));
    u8 *source = reinterpret_cast<u8 *>(LoadU32At(surface, 0x120U));
    const i32 source_pitch = static_cast<i32>(LoadU32At(surface, 0x110U));

    switch (surface_format) {
    case 0x15U:
        RunBlendKernel(destination, source, pitch, source_pitch, columns,
                       rows, 4U, surface_format);
        break;
    case 0x1aU:
        RunBlendKernel(destination, source, pitch, source_pitch, columns,
                       rows, 2U, surface_format);
        break;
    case 0x19U:
        RunBlendKernel(destination, source, pitch, source_pitch, columns,
                       rows, 2U, surface_format);
        break;
    default:
        break;
    }

    (void)reinterpret_cast<TextureUnlockLikeFn>(texture_vtable[14])(texture);
    return true;
}

} // namespace th10
