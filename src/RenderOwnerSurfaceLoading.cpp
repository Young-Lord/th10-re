// TH10 0x447c80 / 0x446d70 — render-owner surface loading helpers.
//
// 0x447c80 (LoadRenderOwnerCachedSurfaceFromFile) is the file-backed cache
// loader over the render-owner surface-cache arrays:
//   +0x3ad4e0  primary surfaces[32]        +0x3ad560  shadow surfaces[32]
//   +0x3ad5e0  cached file buffers[32]     +0x3ad660  cached sizes[32]
//   +0x3ad6e0  D3DXIMAGE_INFO descriptors[32] (stride 0x1c)
// It releases the previous pair (only when a primary surface exists),
// loads the named file through the resource loader 0x44b360 or reuses the
// cached buffer, creates the fixed 640x1024 image surface, decodes the
// image into it with D3DXLoadSurfaceFromFileInMemory (the D3DXIMAGE_INFO
// lands in the slot descriptor and supplies the real dimensions), creates
// the per-slot primary (render-target first, offscreen fallback) and shadow
// surfaces, and copies the image into both with D3DXLoadSurfaceFromSurface.
// Native quirk: when the initial image-surface creation fails the routine
// returns -1 without releasing the file buffer (leak preserved).
//
// 0x446d70 (LoadGeneratedGlyphIntoSurface) uploads one generated-font glyph
// into a fresh surface. The record holder's +0x00 wrapper object creates
// the destination surface into the reused source-size stack slot (vtable
// +0x48 with (this, 0, &out)). When the embedded flag (4th stack argument)
// is set, the glyph record's embedded bitmap (record + [+0x30], selector
// u16 at +0x06, u16 width at +0x08, u16 height at +0x0a, pixels at +0x10)
// is uploaded with D3DXLoadSurfaceFromMemory using the selector tables at
// 0x46f288 (selector -> pixel format id) and 0x46f2a0 (selector -> bytes
// per pixel); otherwise the EDI pointer is treated as a file-in-memory
// image and uploaded with D3DXLoadSurfaceFromFileInMemory, with the
// destination rectangle sized from the created surface's descriptor. The
// destination rectangle is {0, top, width, height + top} in both paths
// (top = the native ECX input). Afterwards the holder receives the source
// size (+0x08) and the format byte count of the remapped selector (+0x0c),
// and the post-processing helper 0x4465b0 runs on the holder. The selector
// is remapped to 5 or 3 when the gate byte DAT_00491d78 has bit 0 set and
// the format table maps the selector to 0x15/0 or 0x14.
#include "RenderOwnerSurfaceLoading.hpp"

#include "MainChainRender.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

extern D3D9Device *g_D3D9ClearDevice;   // TH10 DAT_00491c30
extern u32 g_MainChainBackBufferFormat; // TH10 DAT_00491d14
extern u8 g_GlyphFormatGateByte;        // TH10 DAT_00491d78

// TH10 0x46f288 / 0x46f2a0 (.rdata tables: selector -> format id / bytes
// per pixel). The glyph data selector indexes both.
const u32 kGlyphSelectorFormats[6] = {
    0x00U, 0x15U, 0x19U, 0x17U, 0x14U, 0x1aU
};
const u32 kGlyphSelectorBytesPerPixel[6] = { 4U, 4U, 2U, 2U, 3U, 2U };

typedef i32 (TH10_STDCALL *D3DCreateRenderTargetFn)(D3D9Device *, u32, u32,
                                                    u32, u32, u32, i32,
                                                    void **, void *);
typedef i32 (TH10_STDCALL *D3DCreateOffscreenPlainSurfaceFn)(D3D9Device *,
                                                             u32, u32, u32,
                                                             u32, void **,
                                                             void *);
typedef u32 (TH10_STDCALL *D3DReleaseFn)(void *);

inline void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

inline u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

inline void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

// D3DX imports (IAT thunks 0x4521d0 / 0x4521d6).
extern "C" {

i32 TH10_STDCALL D3DXLoadSurfaceFromFileInMemory(
    void *destination, const void *destination_palette,
    const void *destination_rect, const void *data, u32 data_size,
    const void *source_rect, u32 filter, u32 color_key, void *image_info);

i32 TH10_STDCALL D3DXLoadSurfaceFromSurface(
    void *destination, const void *destination_palette,
    const void *destination_rect, const void *source,
    const void *source_palette, const void *source_rect, u32 filter,
    u32 color_key);

i32 TH10_STDCALL D3DXLoadSurfaceFromMemory(
    void *destination, const void *destination_palette,
    const void *destination_rect, const void *source_memory, u32 source_format,
    u32 source_pitch, const void *source_palette, const void *source_rect,
    u32 filter, u32 color_key);

}

// TH10 0x44b8e0 (boundary; the native passes the DAT_00474f70 text context
// in EDI) and 0x44b360 / 0x452422 (semantic bodies elsewhere).
void ReportResourceLoadFailure(const char *format, const char *name);
void *LoadMainChainFile(const char *path, u32 *file_size, i32 filesystem_mode);
void ReleaseResourceBuffer(void *pointer);

// TH10 0x452ae8: sprintf-style "%s" filename copy into a stack buffer.
void FormatFileNameCopy(char *destination, const char *format,
                        const char *name);

// TH10 0x4465b0 (boundary): post-processes the record holder after a glyph
// upload (native EAX = holder).
void PostProcessGlyphRecordHolder(void *record_holder);

// D3DSURFACE_DESC layout (only Width/Height are consumed).
struct SurfaceDescriptor {
    u32 format;
    u32 type;
    u32 usage;
    u32 pool;
    u32 multi_sample_type;
    u32 multi_sample_quality;
    u32 width;
    u32 height;
};

void ReleaseComObject(void *object)
{
    (void)reinterpret_cast<D3DReleaseFn>(
        (*static_cast<void ***>(object))[2])(object);
}

} // namespace

i32 LoadRenderOwnerCachedSurfaceFromFile(void *owner, const char *file_name,
                                         u32 cache_slot)
{
    u8 *const owner_bytes = static_cast<u8 *>(owner);
    void **const primary_slots =
        reinterpret_cast<void **>(owner_bytes + 0x3ad4e0U);
    void **const shadow_slots =
        reinterpret_cast<void **>(owner_bytes + 0x3ad560U);
    void **const buffers = reinterpret_cast<void **>(owner_bytes + 0x3ad5e0U);
    u32 *const sizes = reinterpret_cast<u32 *>(owner_bytes + 0x3ad660U);
    void *const descriptor = owner_bytes + 0x3ad6e0U + 0x1cU * cache_slot;

    if (primary_slots[cache_slot] != 0) {
        ReleaseLargeRenderOwnerCachedSurfacePair(owner, cache_slot);
    }

    void *data = 0;
    u32 data_size = 0;

    void *const cached = buffers[cache_slot];
    if (cached != 0) {
        data = cached;
        data_size = sizes[cache_slot];
        buffers[cache_slot] = 0; // ownership moves into this call
    } else {
        char path[0x100];
        FormatFileNameCopy(path, "%s", file_name);
        u32 file_size = 0;
        data = LoadMainChainFile(path, &file_size, 0);
        if (data == 0) {
            ReportResourceLoadFailure(
                "%s\x82\xaa\x93\xc7\x82\xdd\x8d\x9e\x82\xdf\x82\xc8\x82\xa2"
                "\x82\xc5\x82\xb7\x81\x42\r\n",
                file_name);
            return -1;
        }
        data_size = file_size;
    }

    // Fixed 640x1024 decode surface. Native quirk: when this creation fails
    // the routine returns -1 without releasing the loaded buffer.
    void *image_surface = 0;
    D3D9Device *const device = g_D3D9ClearDevice;
    if (reinterpret_cast<D3DCreateOffscreenPlainSurfaceFn>(GetD3DSlot(
            device, 36))(device, 0x280U, 0x400U, g_MainChainBackBufferFormat,
                         3U, &image_surface, 0) != 0) {
        return -1;
    }

    if (D3DXLoadSurfaceFromFileInMemory(image_surface, 0, 0, data, data_size,
                                        0, 1, 0, descriptor) != 0) {
        ReleaseComObject(image_surface);
        ReleaseResourceBuffer(data);
        return -1;
    }

    // Primary surface: render-target first, offscreen fallback. The real
    // dimensions come from the decoded D3DXIMAGE_INFO descriptor.
    const u32 width = LoadU32At(descriptor, 0U);
    const u32 height = LoadU32At(descriptor, 4U);
    if (reinterpret_cast<D3DCreateRenderTargetFn>(GetD3DSlot(device, 28))(
            device, width, height, g_MainChainBackBufferFormat, 0U, 0U, 1U,
            &primary_slots[cache_slot], 0) != 0 &&
        reinterpret_cast<D3DCreateOffscreenPlainSurfaceFn>(GetD3DSlot(
            device, 36))(device, width, height, g_MainChainBackBufferFormat,
                         3U, &primary_slots[cache_slot], 0) != 0) {
        ReleaseComObject(image_surface);
        ReleaseResourceBuffer(data);
        return -1;
    }

    if (reinterpret_cast<D3DCreateOffscreenPlainSurfaceFn>(GetD3DSlot(
            device, 36))(device, width, height, g_MainChainBackBufferFormat,
                         3U, &shadow_slots[cache_slot], 0) != 0) {
        ReleaseComObject(image_surface);
        ReleaseResourceBuffer(data);
        return -1;
    }

    if (D3DXLoadSurfaceFromSurface(primary_slots[cache_slot], 0, 0,
                                   image_surface, 0, 0, 1, 0) != 0 ||
        D3DXLoadSurfaceFromSurface(shadow_slots[cache_slot], 0, 0,
                                   image_surface, 0, 0, 1, 0) != 0) {
        ReleaseComObject(image_surface);
        ReleaseResourceBuffer(data);
        return -1;
    }

    ReleaseComObject(image_surface);
    ReleaseResourceBuffer(data);
    return 0;
}

i32 LoadGeneratedGlyphIntoSurface(u32 selector, void *record_holder,
                                  void *glyph_record, u32 source_size,
                                  u32 embedded_flag, i32 destination_top)
{
    // Selector remap (native 0x446d8d..0x446db3): gated by bit 0 of the
    // DAT_00491d78 gate byte, driven by the format-table entry.
    u32 local_selector = selector;
    if ((g_GlyphFormatGateByte & 1U) != 0U) {
        const u32 mapped = kGlyphSelectorFormats[selector];
        if (mapped == 0x15U || mapped == 0U) {
            local_selector = 5U;
        } else if (mapped == 0x14U) {
            local_selector = 3U;
        }
    }

    u8 *const holder = static_cast<u8 *>(record_holder);
    StoreU32At(holder, 8U, source_size);

    // Destination surface from the holder's +0x00 wrapper (this, 0, &out);
    // the native reuses the source-size stack slot as the output cell.
    void *destination = 0;
    {
        void *const creator =
            *reinterpret_cast<void *const *>(holder);
        void **const creator_vtable = *reinterpret_cast<void ***>(creator);
        typedef i32 (TH10_STDCALL *CreateSurfaceFn)(void *, u32, void **);
        (void)reinterpret_cast<CreateSurfaceFn>(creator_vtable[0x48U / 4U])(
            creator, 0U, &destination);
    }

    if (embedded_flag != 0) {
        // Embedded-bitmap path: D3DXLoadSurfaceFromMemory.
        const u8 *const data =
            static_cast<const u8 *>(glyph_record) +
            LoadU32At(glyph_record, 0x30U);
        const u32 embedded_selector =
            static_cast<u32>(data[6] | (data[7] << 8));
        const i32 width =
            static_cast<i32>(static_cast<i16>(data[8] | (data[9] << 8)));
        const i32 height =
            static_cast<i32>(static_cast<i16>(data[10] | (data[11] << 8)));
        const u32 bytes_per_pixel =
            kGlyphSelectorBytesPerPixel[embedded_selector];
        const u32 format_id = kGlyphSelectorFormats[embedded_selector];

        const i32 destination_rect[4] = {
            0, destination_top, width, height + destination_top
        };
        const i32 source_rect[4] = { 0, 0, width, height };
        (void)D3DXLoadSurfaceFromMemory(
            destination, 0, destination_rect, data + 0x10, format_id,
            static_cast<u32>(width) * bytes_per_pixel, 0, source_rect, 1, 0);
    } else {
        // File-in-memory path: destination rectangle sized from the created
        // surface's descriptor (native GetDesc at vtable +0x30).
        SurfaceDescriptor descriptor;
        {
            void **const surface_vtable =
                *reinterpret_cast<void ***>(destination);
            typedef i32 (TH10_STDCALL *GetDescFn)(void *, void *);
            (void)reinterpret_cast<GetDescFn>(surface_vtable[0x30U / 4U])(
                destination, &descriptor);
        }
        const i32 destination_rect[4] = {
            0, destination_top, static_cast<i32>(descriptor.width),
            static_cast<i32>(descriptor.height)
        };
        (void)D3DXLoadSurfaceFromFileInMemory(
            destination, 0, destination_rect, glyph_record, source_size, 0,
            1, 0, 0);
    }

    ReleaseComObject(destination);

    StoreU32At(holder, 0xcU, kGlyphSelectorBytesPerPixel[local_selector]);
    PostProcessGlyphRecordHolder(holder);
    return 0;
}

} // namespace th10
