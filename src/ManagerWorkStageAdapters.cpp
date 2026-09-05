#include "ManagerWorkStageAdapters.hpp"

namespace th10 {

namespace {

typedef void (TH10_STDCALL *VirtualSlot1cFn)(void *object, i32 argument);
typedef void (TH10_STDCALL *VirtualSlot24Fn)(void *object);
typedef void (TH10_STDCALL *VirtualSlot44Fn)(void *object, i32 zero,
                                              ManagerWorkStageOutputRecord *output);

extern void *g_ManagerWorkD3DDevice; // TH10 DAT_00491c30
extern const u32 g_ManagerWorkTextureFormats[]; // TH10 0x0046f288
extern const i32 g_ManagerWorkPitchFactors[]; // TH10 0x0046f2a0
extern u8 g_ManagerWorkFormatRemapFlag; // TH10 DAT_00491d78

extern i32 D3dxCreateTextureFromMemory(void *device, const void *data,
    u32 data_bytes, i32 width, i32 height, u32 mip_levels, u32 usage,
    u32 format, u32 pool, u32 filter, i32 mip_filter, u32 color_key,
    void *source_info, void *palette, void **out_texture);
extern i32 D3dxCreateTexture(void *device, i32 width, i32 height,
                              u32 mip_levels, u32 usage, u32 format,
                              u32 pool, void **out_texture);
extern i32 GetTextureLevelZero(void *texture, void **out_surface);
extern void LoadSurfaceFromMemoryIgnoredResult(void *surface, const void *data,
                                               i32 source_pitch, u32 format,
                                               i32 width, i32 height);
extern void ReleaseTextureSurface(void *surface);
extern void PostCreateManagerWorkTexture(WorkRecordPartial *record);

u16 ReadU16(const void *address)
{
    const u8 *const bytes = static_cast<const u8 *>(address);
    return static_cast<u16>(bytes[0] | (static_cast<u16>(bytes[1]) << 8));
}

u32 SelectedFormatIndex(u32 source_index)
{
    if ((g_ManagerWorkFormatRemapFlag & 1) == 0)
        return source_index;

    const u32 format = g_ManagerWorkTextureFormats[source_index];
    if (format == 0 || format == 0x15)
        return 5;
    if (format == 0x14)
        return 3;
    return source_index;
}

void *GetVtableSlot(void *object, u32 byte_offset)
{
    return static_cast<void **>(static_cast<void **>(object)[0])[byte_offset / 4];
}

} // namespace

i32 SetupEncodedManagerWorkStage(WorkRecordPartial *record,
                                 ManagerWorkChainNodePartial *node)
{
    const u32 index = SelectedFormatIndex(static_cast<u32>(node->format_index_or_relative));
    const i32 result = D3dxCreateTextureFromMemory(
        g_ManagerWorkD3DDevice, record->owned_allocation,
        static_cast<u32>(record->encoded_size_or_state), node->horizontal_normalizer,
        node->vertical_normalizer, 0, 0, g_ManagerWorkTextureFormats[index],
        1, 1, -1, static_cast<u32>(node->adapter_argument), 0, 0,
        &record->virtual_object);
    if (result != 0)
        return -1;

    PostCreateManagerWorkTexture(record);
    record->format_pitch_factor = g_ManagerWorkPitchFactors[index];
    return 0;
}

i32 SetupRawManagerWorkStage(WorkRecordPartial *record,
                             ManagerWorkChainNodePartial *node)
{
    const u16 source_index = ReadU16(reinterpret_cast<u8 *>(node) + 0x16);
    const u32 index = SelectedFormatIndex(source_index);
    if (D3dxCreateTexture(g_ManagerWorkD3DDevice, node->horizontal_normalizer,
                          node->alternate_source_relative, 1, 0,
                          g_ManagerWorkTextureFormats[index], 1,
                          &record->virtual_object) != 0)
        return -1;

    void *surface = 0;
    (void)GetTextureLevelZero(record->virtual_object, &surface);
    const i32 source_pitch = static_cast<i32>(static_cast<short>(
        ReadU16(reinterpret_cast<u8 *>(node) + 0x18))) *
        g_ManagerWorkPitchFactors[index];
    LoadSurfaceFromMemoryIgnoredResult(surface, node->raw_source_data,
                                       source_pitch, g_ManagerWorkTextureFormats[index],
                                       node->horizontal_normalizer,
                                       node->alternate_source_relative);
    record->format_pitch_factor = g_ManagerWorkPitchFactors[index];
    ReleaseTextureSurface(surface);
    return 0;
}

void SetupEmptyManagerWorkStage(WorkRecordPartial *record,
                                ManagerWorkChainNodePartial *node)
{
    const u32 index = SelectedFormatIndex(static_cast<u32>(node->format_index_or_relative));
    (void)D3dxCreateTexture(g_ManagerWorkD3DDevice, node->horizontal_normalizer,
                            node->vertical_normalizer, 1, 0,
                            g_ManagerWorkTextureFormats[index], 1,
                            &record->virtual_object);
    record->format_pitch_factor = g_ManagerWorkPitchFactors[index];
}

void CallManagerWorkVirtualSlot1c(void *object, i32 argument)
{
    reinterpret_cast<VirtualSlot1cFn>(GetVtableSlot(object, 0x1c))(object, argument);
}

void CallManagerWorkVirtualSlot24(void *object)
{
    reinterpret_cast<VirtualSlot24Fn>(GetVtableSlot(object, 0x24))(object);
}

void FillManagerWorkVirtualOutput(void *object,
                                  ManagerWorkStageOutputRecord *output)
{
    reinterpret_cast<VirtualSlot44Fn>(GetVtableSlot(object, 0x44))(object, 0, output);
}

} // namespace th10
