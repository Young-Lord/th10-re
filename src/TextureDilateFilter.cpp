#include "TextureDilateFilter.hpp"

#include "Th10Platform.hpp"

namespace th10 {

namespace {

// D3DFORMAT values handled by the native switch; anything else skips the
// pass entirely.
const u32 k_format_a8r8g8b8 = 0x15U;
const u32 k_format_a1r5g5b5 = 0x19U;
const u32 k_format_a4r4g4b4 = 0x1aU;

// Observed vtable offsets on the texture holder's object and on the
// retrieved level-0 surface object.
const u32 k_texture_get_surface_level = 0x48U;
const u32 k_surface_get_desc = 0x30U;
const u32 k_surface_lock_rect = 0x34U;
const u32 k_surface_unlock_rect = 0x38U;
const u32 k_com_release = 0x08U;

// D3DSURFACE_DESC as consumed by the native: Format at +0, Width at +0x18,
// Height at +0x1c. D3DLOCKED_RECT: Pitch at +0, pBits at +4.
struct SurfaceDesc {
    u32 format;
    u32 reserved[5];
    u32 width;
    u32 height;
};

struct LockedRect {
    i32 pitch;
    void *bits;
};

typedef i32 (TH10_STDCALL *GetSurfaceLevelFn)(void *object, u32 level,
                                              void **out_surface);
typedef i32 (TH10_STDCALL *GetDescFn)(void *object, SurfaceDesc *desc);
typedef i32 (TH10_STDCALL *LockRectFn)(void *object, LockedRect *rect,
                                       const void *rect_region, u32 flags);
typedef i32 (TH10_STDCALL *SimpleFn)(void *object);

void **VtableOf(void *object)
{
    return *static_cast<void ***>(object);
}

u32 ReadU32(const void *address)
{
    const u8 *const bytes = static_cast<const u8 *>(address);
    return static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8)
         | (static_cast<u32>(bytes[2]) << 16)
         | (static_cast<u32>(bytes[3]) << 24);
}

void WriteU32(void *address, u32 value)
{
    u8 *const bytes = static_cast<u8 *>(address);
    bytes[0] = static_cast<u8>(value);
    bytes[1] = static_cast<u8>(value >> 8);
    bytes[2] = static_cast<u8>(value >> 16);
    bytes[3] = static_cast<u8>(value >> 24);
}

u16 ReadU16(const void *address)
{
    const u8 *const bytes = static_cast<const u8 *>(address);
    return static_cast<u16>(static_cast<u16>(bytes[0])
                            | (static_cast<u16>(bytes[1]) << 8));
}

void WriteU16(void *address, u16 value)
{
    u8 *const bytes = static_cast<u8 *>(address);
    bytes[0] = static_cast<u8>(value);
    bytes[1] = static_cast<u8>(value >> 8);
}

// 32-bit formats (0 / 0x15): for every pixel whose alpha byte is zero,
// average the R/G/B channels of the adjacent opaque pixels (left, right,
// above, below) and write the result into the transparent pixel. The
// division only runs when more than one neighbor contributed.
void Dilate32Bit(u8 *bits, i32 pitch, u32 width, u32 height)
{
    const u32 pixel_stride = 4U;
    const u32 row_pixels = pitch / 4;
    for (u32 y = 0U; y < height; ++y) {
        u8 *row = bits + pitch * y;
        for (u32 x = 0U; x < width; ++x) {
            u8 *const pixel = row;
            u8 *const cursor = row - 2; // cursor[k] == pixel[k - 2]
            if (cursor[5] == 0U) {      // current alpha == 0
                u32 r = 0U;
                u32 g = 0U;
                u32 b = 0U;
                u32 count = 0U;
                if (x != 0U && cursor[1] != 0U) { // left alpha
                    r = cursor[0];
                    g = cursor[-1];
                    b = cursor[-2];
                    ++count;
                }
                if (x < width - 1U && cursor[9] != 0U) { // right alpha
                    r += cursor[8];
                    g += cursor[7];
                    b += cursor[6];
                    ++count;
                }
                if (y != 0U) {
                    const u8 *const above = row - pixel_stride * row_pixels;
                    if (above[3] != 0U) {
                        r += above[2];
                        g += above[1];
                        b += above[0];
                        ++count;
                    }
                }
                if (y < height - 1U) {
                    const u8 *const below = row + pixel_stride * row_pixels;
                    if (below[3] != 0U) {
                        r += below[2];
                        g += below[1];
                        b += below[0];
                        ++count;
                    }
                }
                if (count > 1U) {
                    r /= count;
                    g /= count;
                    b /= count;
                }
                cursor[3] = static_cast<u8>(g);
                pixel[0] = static_cast<u8>(b);
                cursor[4] = static_cast<u8>(r);
            }
            row += pixel_stride;
        }
    }
}

// A1R5G5B5 (0x19): fill pixels whose alpha bit is clear with the averaged
// 5-bit channels of adjacent opaque pixels, keeping the pixel's own alpha
// bit.
void DilateA1R5G5B5(u8 *bits, i32 pitch, u32 width, u32 height)
{
    const u32 pixel_stride = 2U;
    const u32 row_pixels = pitch / 2;
    for (u32 y = 0U; y < height; ++y) {
        u8 *row = bits + pitch * y;
        for (u32 x = 0U; x < width; ++x) {
            if (static_cast<signed char>(row[1]) >= 0) { // alpha bit clear
                u32 r = 0U;
                u32 g = 0U;
                u32 b = 0U;
                u32 count = 0U;
                u16 word;
                if (x != 0U) {
                    word = ReadU16(row - 2);
                    if ((word & 0x8000U) != 0U) {
                        r = (word >> 10) & 0x1fU;
                        g = (word >> 5) & 0x1fU;
                        b = word & 0x1fU;
                        ++count;
                    }
                }
                if (x < width - 1U) {
                    word = ReadU16(row + 2);
                    if ((word & 0x8000U) != 0U) {
                        r += (word >> 10) & 0x1fU;
                        g += (word >> 5) & 0x1fU;
                        b += word & 0x1fU;
                        ++count;
                    }
                }
                if (y != 0U) {
                    word = ReadU16(row - 2 * row_pixels);
                    if ((word & 0x8000U) != 0U) {
                        r += (word >> 10) & 0x1fU;
                        g += (word >> 5) & 0x1fU;
                        b += word & 0x1fU;
                        ++count;
                    }
                }
                if (y < height - 1U) {
                    word = ReadU16(row + 2 * row_pixels);
                    if ((word & 0x8000U) != 0U) {
                        r += (word >> 10) & 0x1fU;
                        g += (word >> 5) & 0x1fU;
                        b += word & 0x1fU;
                        ++count;
                    }
                }
                if (count > 1U) {
                    r /= count;
                    g /= count;
                    b /= count;
                }
                const u16 current = ReadU16(row);
                WriteU16(row, static_cast<u16>(
                                  (b & 0x1fU)
                                  | (current & 0x8000U)
                                  | ((g & 0x1fU) << 5)
                                  | ((r & 0x1fU) << 10)));
            }
            row += pixel_stride;
        }
    }
}

// A4R4G4B4 (0x1a): same neighborhood fill over 4-bit channels, keyed on
// the alpha nibble.
void DilateA4R4G4B4(u8 *bits, i32 pitch, u32 width, u32 height)
{
    const u32 pixel_stride = 2U;
    const u32 row_pixels = pitch / 2;
    for (u32 y = 0U; y < height; ++y) {
        u8 *row = bits + pitch * y;
        for (u32 x = 0U; x < width; ++x) {
            if ((row[1] & 0xf0U) == 0U) { // current alpha nibble clear
                u32 r = 0U;
                u32 g = 0U;
                u32 b = 0U;
                u32 count = 0U;
                u16 word;
                if (x != 0U) {
                    word = ReadU16(row - 2);
                    if ((word & 0xf000U) != 0U) {
                        r = (word >> 8) & 0xfU;
                        g = (word >> 4) & 0xfU;
                        b = word & 0xfU;
                        ++count;
                    }
                }
                if (x < width - 1U) {
                    word = ReadU16(row + 2);
                    if ((word & 0xf000U) != 0U) {
                        r += (word >> 8) & 0xfU;
                        g += (word >> 4) & 0xfU;
                        b += word & 0xfU;
                        ++count;
                    }
                }
                if (y != 0U) {
                    word = ReadU16(row - 2 * row_pixels);
                    if ((word & 0xf000U) != 0U) {
                        r += (word >> 8) & 0xfU;
                        g += (word >> 4) & 0xfU;
                        b += word & 0xfU;
                        ++count;
                    }
                }
                if (y < height - 1U) {
                    word = ReadU16(row + 2 * row_pixels);
                    if ((word & 0xf000U) != 0U) {
                        r += (word >> 8) & 0xfU;
                        g += (word >> 4) & 0xfU;
                        b += word & 0xfU;
                        ++count;
                    }
                }
                if (count > 1U) {
                    r /= count;
                    g /= count;
                    b /= count;
                }
                const u16 current = ReadU16(row);
                WriteU16(row, static_cast<u16>(
                                  (b & 0xfU)
                                  | (current & 0xf000U)
                                  | ((g & 0xfU) << 4)
                                  | ((r & 0xfU) << 8)));
            }
            row += pixel_stride;
        }
    }
}

} // namespace

i32 DilateTextureTransparentPixelsEaxAbi(void *texture_holder)
{
    void *const texture =
        reinterpret_cast<void *>(ReadU32(texture_holder));
    void **const texture_vtable = VtableOf(texture);

    void *surface = 0;
    reinterpret_cast<GetSurfaceLevelFn>(texture_vtable[k_texture_get_surface_level / 4])(
        texture, 0U, &surface);

    void **const surface_vtable = VtableOf(surface);
    SurfaceDesc desc;
    for (u32 i = 0; i < sizeof(desc); ++i) {
        reinterpret_cast<u8 *>(&desc)[i] = 0U;
    }
    reinterpret_cast<GetDescFn>(surface_vtable[k_surface_get_desc / 4])(
        surface, &desc);

    LockedRect locked;
    reinterpret_cast<LockRectFn>(surface_vtable[k_surface_lock_rect / 4])(
        surface, &locked, 0, 0U);

    u8 *const bits = static_cast<u8 *>(locked.bits);
    switch (desc.format) {
    case 0U:
    case k_format_a8r8g8b8:
        Dilate32Bit(bits, locked.pitch, desc.width, desc.height);
        break;
    case k_format_a1r5g5b5:
        DilateA1R5G5B5(bits, locked.pitch, desc.width, desc.height);
        break;
    case k_format_a4r4g4b4:
        DilateA4R4G4B4(bits, locked.pitch, desc.width, desc.height);
        break;
    default:
        break;
    }

    reinterpret_cast<SimpleFn>(surface_vtable[k_surface_unlock_rect / 4])(
        surface);
    return reinterpret_cast<SimpleFn>(surface_vtable[k_com_release / 4])(
        surface);
}

} // namespace th10
