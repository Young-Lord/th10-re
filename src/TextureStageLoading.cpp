// TH10 stage texture creation and pixel-hit helpers.
//
//   0x00436d30 / 0x00446500: identical pixel-hit accumulators used by the
//     generated-surface and stage-texture passes: adds a 4-byte pixel
//     triple into a 3-dword accumulator and bumps a hit counter.
//   0x00446b70 / 0x00446c70: stage texture creation from a file leaf or
//     from an in-memory image, including the low-fidelity texture-format
//     downgrade (gate flag 0x1 of DAT_00491d78: formats 21/0 -> 5, 20 -> 3)
//     and the transparent-pixel dilation pass.
//   0x00449700: fills a texture surface with zeroes through its level-0
//     surface lock/unlock vtable.
#include <stdlib.h>
#include <string.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"
#include "MainChainRender.hpp"
#include "TextureDilateFilter.hpp"

namespace th10 {

i32 DilateTextureTransparentPixelsEaxAbi(void *texture_holder);

namespace {

extern D3D9Device *g_MainChainD3DDevice; // TH10 DAT_00491c30

void *GetD3DSlot(void *device, u32 index)
{
    return *static_cast<void ***>(device)[index];
}

// TH10 0x0044b360 (registered).
extern void *LoadMainChainFile(const char *path, u32 *file_size,
                               i32 filesystem_mode);
// TH10 0x00452422.
extern void FreeHeap(void *pointer);

// Per-format D3DFORMAT / bytes-per-pixel tables.
extern u32 g_TextureFormatTable[24]; // TH10 DAT_0046f288
extern u32 g_TextureBppTable[24];    // TH10 DAT_0046f2a0

extern "C" i32 TH10_STDCALL sprintf(char *buffer, const char *format, ...);
extern "C" i32 TH10_STDCALL D3DXCreateTextureFromFileInMemoryEx(
    void *device, const void *data, u32 data_size, i32 width, i32 height,
    u32 mip_levels, u32 usage, u32 format, u32 pool, u32 filter,
    u32 mip_filter, u32 color_key, void *src_info, void *palette,
    void *texture_out);
extern "C" i32 TH10_STDCALL D3DXLoadSurfaceFromMemory(void *device,
    void *dest_surface, const void *dest_rect, const void *src_data,
    u32 src_format, u32 src_pitch, const void *src_palette,
    const void *src_rect, u32 filter, u32 color_key);
extern "C" i32 TH10_STDCALL D3DXLoadSurfaceFromFileInMemory(void *device,
    void *dest_surface, const void *dest_rect, const void *src_data,
    u32 src_size, const void *src_rect, u32 filter, u32 color_key,
    void *src_info);

const u32 k_dilate_gate_flag = 0x1U;   // DAT_00491d78 bit selecting low-fi
extern u32 g_TextureQualityGate;       // TH10 DAT_00491d78

} // namespace

// TH10 0x00436d30. Native EAX = accumulator {sum, count, hit}, EDX =
// counter, ECX = 4-byte pixel {a, b, c, weight}.
void AccumulateSurfacePixelHitEaxEdxCcxAbi(u32 *accumulator, u32 *counter,
                                           const u8 *pixel)
{
    if (pixel[3] == 0U)
        return;
    accumulator[1] += pixel[2];
    accumulator[2] += pixel[1];
    accumulator[0] += pixel[0];
    *counter += 1U;
}

// TH10 0x00446500. Identical body registered under the texture-pass name.
void AccumulateTexturePixelHitEaxEdxCcxAbi(u32 *accumulator, u32 *counter,
                                           const u8 *pixel)
{
    if (pixel[3] == 0U)
        return;
    accumulator[1] += pixel[2];
    accumulator[2] += pixel[1];
    accumulator[0] += pixel[0];
    *counter += 1U;
}

// Shared format downgrade for the low-fidelity gate.
static u32 ResolveTextureFormat(u32 format_index)
{
    u32 selected = format_index;
    if ((g_TextureQualityGate & k_dilate_gate_flag) != 0U) {
        const u32 native = g_TextureFormatTable[format_index];
        if (native == 21U || native == 0U)
            selected = 5U;
        else if (native == 20U)
            selected = 3U;
    }
    return selected;
}

// TH10 0x00446b70. Native ECX = format index, ESI = texture record,
// stack = size word, path, filter argument (ret 0x10). Returns 0 on
// success, -1 when the leaf could not be loaded or D3DX refused the data.
i32 CreateStageTextureFromFileEcxAbi(u32 format_index, u32 *texture_record,
                                     u32 size_word, const char *path,
                                     u32 color_key)
{
    char resolved[260];
    (void)sprintf(resolved, "%s", path);
    const u32 selected = ResolveTextureFormat(format_index);

    u32 file_size = 0;
    void *data = LoadMainChainFile(resolved, &file_size, 1);
    if (data == 0)
        return -1;
    texture_record[2] = size_word;
    if (D3DXCreateTextureFromFileInMemoryEx(
            g_MainChainD3DDevice, data, file_size, 0, 0, 0, 0,
            g_TextureFormatTable[selected], 1U, 3U, 0xffffffffU,
            color_key, 0, 0, texture_record) != 0) {
        FreeHeap(data);
        return -1;
    }
    (void)DilateTextureTransparentPixelsEaxAbi(texture_record);
    texture_record[3] = g_TextureBppTable[selected];
    texture_record[1] = reinterpret_cast<u32>(data);
    return 0;
}

// TH10 0x00446c70. Native EAX = format index, EBX = image size,
// EDI = texture record, ESI = image data, stack = (unused, unused,
// use-memory-header flag). Returns 0.
i32 CreateStageTextureFromImageEaxAbi(u32 format_index, u32 image_size,
                                      u32 *texture_record,
                                      const void *image_data,
                                      u32 unused_a, u32 unused_b,
                                      u32 use_memory_header)
{
    const u32 selected = ResolveTextureFormat(format_index);
    void *const texture = reinterpret_cast<void *>(texture_record[0]);
    texture_record[2] = image_size;

    typedef i32 (TH10_STDCALL *GetSurfaceLevelFn)(void *, u32, void **);
    void *surface = 0;
    (void)reinterpret_cast<GetSurfaceLevelFn>(
        GetD3DSlot(texture, 0x48 / 4))(
        reinterpret_cast<void *>(texture), 0U, &surface);

    if (use_memory_header != 0U) {
        // In-memory image: a u32 at +0x30 holds the offset of the embedded
        // header {u16 x3, u16 format index, u16 width, u16 height, pixels}.
        const u8 *const base = static_cast<const u8 *>(image_data);
        const u16 *const header = reinterpret_cast<const u16 *>(
            base + *reinterpret_cast<const u32 *>(base + 0x30U));
        const u32 src_rect[4] = {0U, 0U,
            static_cast<u32>(header[4]), static_cast<u32>(header[5])};
        (void)D3DXLoadSurfaceFromMemory(
            g_MainChainD3DDevice, surface, 0, header + 8,
            g_TextureFormatTable[header[3]],
            g_TextureBppTable[header[3]] * header[4], 0, src_rect, 1U, 0);
    } else {
        (void)D3DXLoadSurfaceFromFileInMemory(
            g_MainChainD3DDevice, surface, 0, image_data, image_size, 0,
            1U, 0, 0);
    }

    typedef i32 (TH10_STDCALL *SimpleFn)(void *);
    (void)reinterpret_cast<SimpleFn>(GetD3DSlot(surface, 0x08 / 4))(surface);
    (void)DilateTextureTransparentPixelsEaxAbi(texture_record);
    texture_record[3] = g_TextureBppTable[selected];
    return 0;
}

// TH10 0x00449700. Native EAX = texture record {IDirect3DTexture9*}.
// Locks the level-0 surface, zeroes it and releases everything.
void ClearStageTextureSurfaceEaxAbi(u32 *texture_record)
{
    typedef i32 (TH10_STDCALL *GetSurfaceLevelFn)(void *, u32, void **);
    typedef void (TH10_STDCALL *GetDescFn)(void *, void *);
    typedef void (TH10_STDCALL *LockBoxFn)(void *, void *, void **, u32 *,
                                           u32, u32);
    typedef i32 (TH10_STDCALL *SimpleFn)(void *);

    void *const texture = reinterpret_cast<void *>(texture_record[0]);
    void *surface = 0;
    (void)reinterpret_cast<GetSurfaceLevelFn>(GetD3DSlot(texture, 0x48 / 4))(
        texture, 0U, &surface);

    u8 desc[0x20];
    (void)reinterpret_cast<GetDescFn>(GetD3DSlot(surface, 0x30 / 4))(
        surface, desc);
    const u32 width = *reinterpret_cast<const u32 *>(desc + 0x18U);
    const u32 height = *reinterpret_cast<const u32 *>(desc + 0x1cU);

    void *bits = 0;
    u32 row_pitch = 0;
    (void)reinterpret_cast<LockBoxFn>(GetD3DSlot(surface, 0x34 / 4))(
        surface, 0U, &bits, &row_pitch, 0U, 0U);
    if (bits != 0)
        memset(bits, 0, width * height);
    (void)reinterpret_cast<SimpleFn>(GetD3DSlot(surface, 0x38 / 4))(surface);

    (void)reinterpret_cast<SimpleFn>(GetD3DSlot(surface, 0x08 / 4))(surface);
    (void)reinterpret_cast<SimpleFn>(GetD3DSlot(texture, 0x08 / 4))(texture);
}

} // namespace th10
