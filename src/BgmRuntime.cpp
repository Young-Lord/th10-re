#include "BgmRuntime.hpp"

#include <string.h>

#include "TransitionSoundAdapter.hpp"

namespace th10 {

namespace {

struct BgmSoundDefinition {
    u32 descriptor_word_index;
    signed short volume;
    signed short default_frequency;
};

struct BgmStreamDescriptorPartial {
    u8 unknown_0000[0x10];
    u32 data_file_offset;
    u8 unknown_0014[4];
    u32 loop_offset;
    u32 byte_count;
    u8 wave_format[4];
    u32 page_unit_count;
    u8 unknown_0028[4];
    u16 page_unit_size;
};

struct BgmTrackRecordPartial {
    char name[0x10];
    u32 data_file_offset;
    u32 memory_image_bytes;
    u32 loop_offset;
    u32 byte_count;
    u8 wave_format[4];
    u32 page_unit_count;
    u8 unknown_0028[4];
    u16 page_unit_size;
    u8 unknown_002e[6];
};

typedef char AssertBgmSoundDefinitionSize[
    sizeof(BgmSoundDefinition) == 8 ? 1 : -1];
typedef char AssertBgmTrackRecordPartialSize[
    sizeof(BgmTrackRecordPartial) == 0x34 ? 1 : -1];

typedef i32 (TH10_STDCALL *CreateSoundBufferFn)(void *, void *, void **,
                                                  void *);
typedef i32 (TH10_STDCALL *SetBgmBufferPositionFn)(void *, u32);
typedef i32 (TH10_STDCALL *SetBgmBufferVolumeFn)(void *, i32);

extern const BgmSoundDefinition g_BgmSoundDefinitions[47]; // 0x4749c8
extern signed char g_BgmVolumeInputConfig; // 0x491d68
extern signed char g_BgmVolumeEnabledConfig; // 0x491d69
extern i32 g_BgmSoundSequenceVolumeScale; // 0x497858
extern i32 ConvertFloatToI32TowardZeroX87(float value);
extern u8 g_BgmRuntimeEnabled; // 0x491d63
extern u8 g_BgmSoundSequencesEnabled; // 0x491d64
extern u8 g_BgmUsePreloadedTrackImages; // bit 0x10 at 0x491d78
extern void *CreateAutoResetEvent(); // CreateEventA(0, 0, 0, 0)
extern void *CreateBgmWorkerThread(void *argument, u32 *thread_id);
extern void *g_BgmWorkerArgument; // 0x491c70
extern const u32 g_BgmStreamFormatWords[4]; // 0x46a67c
extern u32 g_BgmDataFileBaseOffset; // 0x4977a4
extern void *OpenBgmDataFile(const char *path, u32 desired_access,
                             u32 share_mode, u32 creation_disposition,
                             u32 flags); // CreateFileA
extern void SetBgmDataFilePositionIgnoredResult(void *handle, u32 position);
extern void ReadBgmDataFileIgnoredResult(void *handle, void *destination,
                                         u32 bytes, u32 *copied_bytes);
extern void CloseBgmDataFileIgnoredResult(void *handle);
extern void *AllocateBgmTrackImage(u32 bytes);
extern void FreeBgmTrackImage(void *pointer);
extern void *AllocateBgmRuntimeMemory(u32 bytes); // TH10 0x00452493
extern void FreeBgmRuntimeMemory(void *pointer); // TH10 0x004524a1
extern i32 QueryBgmBufferNotification(void *buffer, const void *iid,
                                      void **out_notification);
extern i32 SetBgmBufferNotificationPositions(void *notification, u32 count,
                                             const void *positions);
extern void ReleaseBgmComObject(void *object);
extern u32 WaitForBgmWorkerHandle(void *handle, u32 milliseconds);
extern void SleepMilliseconds(u32 milliseconds);
extern void CloseBgmWorkerHandle(void *handle);
extern void PostBgmWorkerQuitIgnoredResult(u32 thread_id);
extern u32 WaitForBgmStreamWorker(void *handle, u32 milliseconds);
extern void CloseBgmStreamHandleIgnoredResult(void *handle);
extern void DeleteBgmStreamAdapter(void *adapter, u32 deleting_flag);
extern void *g_BgmNotificationHandle; // 0x49779c
extern TransitionSoundAdapterLayout *g_BgmStreamAdapter; // 0x497798
extern u32 MsgWaitForBgmWorker(u32 count, const void *handles,
                               i32 wait_all, u32 milliseconds, u32 wake_mask);
extern i32 PeekBgmWorkerMessage(void *message, u32 remove_flags);
extern i32 GetBgmBufferCurrentPosition(void *buffer, u32 *play_cursor,
                                       u32 *write_cursor);
extern i32 RestoreBgmBufferIfLost(void *buffer, bool *was_restored);
extern i32 LockBgmBuffer(void *buffer, u32 offset, u32 bytes,
                         void **region1, u32 *region1_bytes,
                         void **region2, u32 *region2_bytes);
extern void UnlockBgmBufferIgnoredResult(void *buffer, void *region1,
                                         u32 region1_bytes, void *region2,
                                         u32 region2_bytes);
extern u8 GetBgmStreamSilenceByte(void *source);
extern void StopBgmBufferIgnoredResult(void *buffer);
extern i32 PlayBgmBuffer(void *buffer, u32 reserved, u32 priority, u32 flags);
extern void SetBgmBufferFrequencyIgnoredResult(void *buffer, i32 frequency);

void *GetComSlot(void *object, u32 index)
{
    return static_cast<void **>(static_cast<void **>(object)[0])[index];
}

i32 ComputeBgmVolume(i32 input)
{
    const float one_minus_scale = 1.0f - static_cast<float>(input) * 0.01f;
    const float squared = one_minus_scale * one_minus_scale;
    const float curve = 1.0f - squared * squared;
    return -5000 - ConvertFloatToI32TowardZeroX87(-5000.0f * curve);
}

i32 ComputeBgmSoundSequenceVolume(i32 source_volume)
{
    if (g_BgmSoundSequenceVolumeScale == 0)
        return -10000;
    const float scale = static_cast<float>(g_BgmSoundSequenceVolumeScale);
    const float curve = 1.0f - scale * 0.01f;
    const float product = static_cast<float>(source_volume + 5000) *
        (1.0f - curve * curve);
    return ConvertFloatToI32TowardZeroX87(product) - 5000;
}

u32 ComputeBgmPageBytes(const BgmStreamDescriptorPartial *descriptor)
{
    u32 bytes = descriptor->page_unit_count *
        static_cast<u32>(descriptor->page_unit_size);
    bytes <<= 2;
    bytes >>= 4;
    return bytes - bytes % static_cast<u32>(descriptor->page_unit_size);
}

i32 FindBgmTrackRecordIndexImpl(const TransitionRootPartial *root,
                                const char *path)
{
    const char *const slash = strrchr(path, '/');
    const char *const backslash = strrchr(path, '\\');
    const char *filename = slash;
    if (filename == 0 || (backslash != 0 && backslash > filename))
        filename = backslash;
    if (filename != 0)
        ++filename;
    else
        filename = path;

    char name[0x80];
    strcpy(name, filename);
    const BgmTrackRecordPartial *record = static_cast<const BgmTrackRecordPartial *>(
        root->bgm_stream_descriptor);
    i32 index = 0;
    while (strcmp(record[index].name, name) != 0)
        ++index;
    return index;
}

} // namespace

i32 FindBgmTrackRecordIndex(const TransitionRootPartial *root,
                            const char *path)
{
    return FindBgmTrackRecordIndexImpl(root, path);
}

namespace {

struct BgmNotificationPosition {
    u32 offset;
    void *event_handle;
};

const i32 kCoENotInitialized = static_cast<i32>(0x800401f0U);
const i32 kEInvalidArg = static_cast<i32>(0x80070057U);
const i32 kEFail = static_cast<i32>(0x80004005U);
const i32 kEOutOfMemory = static_cast<i32>(0x8007000eU);
const u32 kInvalidHandleValue = 0xffffffffU;

} // namespace

i32 InitializeBgmStreamSource(BgmStreamSourceLayout *source,
                              u32 ownership_mode, const char *path,
                              void *descriptor)
{
    source->ownership_mode = ownership_mode;
    source->memory_backed = 0;
    if (ownership_mode != 1)
        return 0;
    if (path == 0)
        return kEInvalidArg;

    source->file_handle = OpenBgmDataFile(path, 0x80000000U, 1, 3,
                                           0x8000080U);
    if (reinterpret_cast<u32>(source->file_handle) == kInvalidHandleValue)
        return kEFail;

    source->descriptor = descriptor;
    BgmStreamDescriptorPartial *const stream =
        static_cast<BgmStreamDescriptorPartial *>(descriptor);
    SetBgmDataFilePositionIgnoredResult(source->file_handle,
        stream->data_file_offset + g_BgmDataFileBaseOffset);
    source->file_bytes_remaining = stream->byte_count;
    source->initial_byte_count = source->file_bytes_remaining;
    return 0;
}

i32 RewindBgmStreamSource(BgmStreamSourceLayout *source, bool use_loop_offset)
{
    BgmStreamDescriptorPartial *const stream =
        static_cast<BgmStreamDescriptorPartial *>(source->descriptor);
    if (source->memory_backed == 0) {
        if (source->file_handle == 0)
            return kCoENotInitialized;
        if (use_loop_offset && static_cast<i32>(stream->loop_offset) > 0) {
            SetBgmDataFilePositionIgnoredResult(source->file_handle,
                stream->data_file_offset + stream->loop_offset +
                g_BgmDataFileBaseOffset);
            source->file_bytes_remaining = stream->byte_count - stream->loop_offset;
            return 0;
        }
        SetBgmDataFilePositionIgnoredResult(source->file_handle,
            stream->data_file_offset + g_BgmDataFileBaseOffset);
        source->file_bytes_remaining = stream->byte_count;
        return 0;
    }

    source->memory_cursor = source->memory_base;
    if (static_cast<i32>(stream->byte_count) > 0)
        source->memory_size = stream->byte_count;
    if (use_loop_offset && static_cast<i32>(stream->loop_offset) > 0)
        source->memory_cursor = source->memory_base + stream->loop_offset;
    return 0;
}

i32 CopyBgmStreamSourceData(BgmStreamSourceLayout *source, void *destination,
                            u32 requested_bytes, u32 *copied_bytes)
{
    if (source->memory_backed == 0) {
        if (source->file_handle == 0)
            return kCoENotInitialized;
        if (destination == 0 || copied_bytes == 0)
            return kEInvalidArg;
        const u32 available = source->file_bytes_remaining;
        if (available < requested_bytes)
            requested_bytes = available;
        source->file_bytes_remaining = available - requested_bytes;
        ReadBgmDataFileIgnoredResult(source->file_handle, destination,
                                     requested_bytes, copied_bytes);
        return 0;
    }

    if (source->memory_cursor == 0)
        return kCoENotInitialized;
    if (copied_bytes != 0)
        *copied_bytes = 0;
    const u32 available = static_cast<u32>(source->memory_base +
        source->memory_size - source->memory_cursor);
    if (available < requested_bytes)
        requested_bytes = available;
    memcpy(destination, source->memory_cursor, requested_bytes);
    source->memory_cursor += requested_bytes;
    if (copied_bytes != 0)
        *copied_bytes = requested_bytes;
    return 0;
}

i32 RebindBgmStreamSourceDescriptor(BgmStreamSourceLayout *source,
                                    void *descriptor)
{
    if (source->memory_backed != 0 ||
        reinterpret_cast<u32>(source->file_handle) == kInvalidHandleValue)
        return kEFail;

    source->descriptor = descriptor;
    if (source->file_handle != 0) {
        BgmStreamDescriptorPartial *const stream =
            static_cast<BgmStreamDescriptorPartial *>(descriptor);
        SetBgmDataFilePositionIgnoredResult(source->file_handle,
            stream->data_file_offset + g_BgmDataFileBaseOffset);
        source->file_bytes_remaining = stream->byte_count;
    }
    source->initial_byte_count = source->file_bytes_remaining;
    return 0;
}

i32 CreateBgmStreamAdapter(TransitionSoundAdapterLayout **out_adapter,
                           u32 flags, void **sound_device_holder,
                           const char *path, const u32 format_guid_words[4],
                           u32 page_count, u32 page_bytes, void *event_handle,
                           void *stream_descriptor)
{
    if (*sound_device_holder == 0)
        return kCoENotInitialized;

    BgmStreamSourceLayout *const source =
        static_cast<BgmStreamSourceLayout *>(AllocateBgmRuntimeMemory(0x94));
    if (source != 0) {
        source->descriptor = 0;
        *reinterpret_cast<u32 *>(source) = 0;
        source->initial_byte_count = 0;
        source->memory_backed = 0;
    }
    if (InitializeBgmStreamSource(source, 1, path, stream_descriptor) != 0) {
        if (source != 0) {
            if (source->ownership_mode == 1) {
                CloseBgmDataFileIgnoredResult(source->file_handle);
                source->file_handle = reinterpret_cast<void *>(kInvalidHandleValue);
            }
            FreeBgmRuntimeMemory(source);
        }
        return kEFail;
    }

    BgmStreamDescriptorPartial *const stream =
        static_cast<BgmStreamDescriptorPartial *>(stream_descriptor);
    TransitionSoundBufferDescription description;
    description.size = sizeof(description);
    description.flags = flags | 0x18188;
    description.buffer_bytes = page_count * page_bytes;
    description.reserved = 0;
    description.wave_format = stream->wave_format;
    memcpy(description.format_guid_words, format_guid_words,
           sizeof(description.format_guid_words));

    void *buffer = 0;
    i32 result = reinterpret_cast<CreateSoundBufferFn>(GetComSlot(
        *sound_device_holder, 3))(*sound_device_holder, &description, &buffer, 0);
    if (result < 0)
        return kEFail;

    void *notification = 0;
    static const u8 kDirectSoundNotifyIid[16] = {
        0x3c, 0x74, 0x46, 0x46, 0x3f, 0xd4, 0xcf, 0x11,
        0xa3, 0xde, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00
    };
    result = QueryBgmBufferNotification(buffer, kDirectSoundNotifyIid,
                                        &notification);
    if (result < 0)
        return kEFail;

    BgmNotificationPosition *const positions =
        static_cast<BgmNotificationPosition *>(AllocateBgmRuntimeMemory(
            page_count * sizeof(BgmNotificationPosition)));
    if (positions == 0)
        return kEOutOfMemory;
    for (u32 index = 0, offset = page_bytes - 1; index < page_count;
         ++index, offset += page_bytes) {
        positions[index].offset = offset;
        positions[index].event_handle = event_handle;
    }
    result = SetBgmBufferNotificationPositions(notification, page_count,
                                                positions);
    ReleaseBgmComObject(notification);
    FreeBgmRuntimeMemory(positions);
    if (result < 0)
        return kEFail;

    TransitionSoundAdapterLayout *const adapter =
        static_cast<TransitionSoundAdapterLayout *>(AllocateBgmRuntimeMemory(0x78));
    if (adapter == 0)
        *out_adapter = 0;
    ConstructSingleTransitionSoundAdapter(adapter, buffer,
                                          reinterpret_cast<void *>(
                                              page_count * page_bytes), source,
                                          page_bytes);
    *out_adapter = adapter;
    adapter->recreate_buffer_description = description;
    adapter->sound_device_holder = sound_device_holder;
    adapter->notification_event = event_handle;
    adapter->worker_refill_active = 0;
    return 0;
}

i32 CreateMemoryBgmStreamAdapter(
    TransitionSoundAdapterLayout **out_adapter, u32 flags,
    void **sound_device_holder, u8 *memory_base, u32 memory_size,
    void *stream_descriptor, const u32 format_guid_words[4], u32 page_count,
    u32 page_bytes, void *event_handle)
{
    if (*sound_device_holder == 0)
        return kCoENotInitialized;

    BgmStreamSourceLayout *const source =
        static_cast<BgmStreamSourceLayout *>(AllocateBgmRuntimeMemory(0x94));
    if (source != 0) {
        source->descriptor = 0;
        *reinterpret_cast<u32 *>(source) = 0;
        source->initial_byte_count = 0;
        source->memory_backed = 0;
    }
    // The native factory intentionally does not initialize +0x78 here.
    source->descriptor = stream_descriptor;
    source->memory_size = memory_size;
    source->memory_base = memory_base;
    source->memory_cursor = memory_base;
    source->memory_backed = 1;

    BgmStreamDescriptorPartial *const stream =
        static_cast<BgmStreamDescriptorPartial *>(stream_descriptor);
    TransitionSoundBufferDescription description;
    description.size = sizeof(description);
    description.flags = flags | 0x18188;
    description.buffer_bytes = page_count * page_bytes;
    description.reserved = 0;
    description.wave_format = stream->wave_format;
    memcpy(description.format_guid_words, format_guid_words,
           sizeof(description.format_guid_words));

    void *buffer = 0;
    i32 result = reinterpret_cast<CreateSoundBufferFn>(GetComSlot(
        *sound_device_holder, 3))(*sound_device_holder, &description, &buffer, 0);
    if (result < 0)
        return kEFail;

    void *notification = 0;
    static const u8 kDirectSoundNotifyIid[16] = {
        0x3c, 0x74, 0x46, 0x46, 0x3f, 0xd4, 0xcf, 0x11,
        0xa3, 0xde, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00
    };
    result = QueryBgmBufferNotification(buffer, kDirectSoundNotifyIid,
                                        &notification);
    if (result < 0)
        return kEFail;

    BgmNotificationPosition *const positions =
        static_cast<BgmNotificationPosition *>(AllocateBgmRuntimeMemory(
            page_count * sizeof(BgmNotificationPosition)));
    if (positions == 0)
        return kEOutOfMemory;
    for (u32 index = 0, offset = page_bytes - 1; index < page_count;
         ++index, offset += page_bytes) {
        positions[index].offset = offset;
        positions[index].event_handle = event_handle;
    }
    result = SetBgmBufferNotificationPositions(notification, page_count,
                                                positions);
    ReleaseBgmComObject(notification);
    FreeBgmRuntimeMemory(positions);
    if (result < 0)
        return kEFail;

    TransitionSoundAdapterLayout *const adapter =
        static_cast<TransitionSoundAdapterLayout *>(AllocateBgmRuntimeMemory(0x78));
    if (adapter == 0)
        *out_adapter = 0;
    ConstructSingleTransitionSoundAdapter(adapter, buffer,
                                          reinterpret_cast<void *>(
                                              page_count * page_bytes), source,
                                          page_bytes);
    *out_adapter = adapter;
    adapter->recreate_buffer_description = description;
    adapter->sound_device_holder = sound_device_holder;
    adapter->notification_event = event_handle;
    adapter->worker_refill_active = 0;
    return 0;
}

i32 RecreateBgmStreamBuffers(TransitionSoundAdapterLayout *adapter)
{
    adapter->playback_active = 0;
    void **const old_slots = static_cast<void **>(adapter->slots);
    for (u32 index = 0; index < adapter->slot_count; ++index) {
        if (old_slots[index] != 0) {
            ReleaseBgmComObject(old_slots[index]);
            old_slots[index] = 0;
        }
    }
    if (old_slots != 0) {
        FreeBgmRuntimeMemory(old_slots);
        adapter->slots = 0;
    }

    void **const slots = static_cast<void **>(AllocateBgmRuntimeMemory(
        adapter->slot_count * sizeof(void *)));
    adapter->slots = slots;
    for (u32 buffer_index = 0; buffer_index < adapter->slot_count;
         ++buffer_index) {
        i32 result = reinterpret_cast<CreateSoundBufferFn>(GetComSlot(
            *adapter->sound_device_holder, 3))(*adapter->sound_device_holder,
            &adapter->recreate_buffer_description, &slots[buffer_index], 0);
        if (result < 0)
            return kEFail;

        void *notification = 0;
        static const u8 kDirectSoundNotifyIid[16] = {
            0x3c, 0x74, 0x46, 0x46, 0x3f, 0xd4, 0xcf, 0x11,
            0xa3, 0xde, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00
        };
        result = QueryBgmBufferNotification(slots[buffer_index],
                                            kDirectSoundNotifyIid,
                                            &notification);
        if (result < 0)
            return kEFail;

        BgmNotificationPosition *const positions =
            static_cast<BgmNotificationPosition *>(AllocateBgmRuntimeMemory(0x80));
        if (positions == 0)
            return kEOutOfMemory;
        for (u32 index = 0; index < 16; ++index) {
            positions[index].offset = adapter->field_006c * (index + 1) - 1;
            positions[index].event_handle = adapter->notification_event;
        }
        result = SetBgmBufferNotificationPositions(notification, 16, positions);
        ReleaseBgmComObject(notification);
        FreeBgmRuntimeMemory(positions);
        if (result < 0)
            return kEFail;
    }
    return 0;
}

i32 RewindBgmStreamAdapter(TransitionSoundAdapterLayout *adapter)
{
    void **const slots = static_cast<void **>(adapter->slots);
    if (slots[0] == 0 || adapter->owned_auxiliary == 0)
        return kCoENotInitialized;

    adapter->field_005c = 0;
    adapter->field_0060 = 0;
    adapter->field_0064 = 0;
    adapter->field_0068 = 0;

    void *const buffer = slots[0];
    bool was_restored = false;
    i32 result = RestoreBgmBufferIfLost(buffer, &was_restored);
    if (result < 0)
        return result;
    if (was_restored) {
        result = InitializeTransitionSoundBuffer(adapter, buffer, false);
        if (result < 0)
            return result;
    }

    result = RewindBgmStreamSource(
        static_cast<BgmStreamSourceLayout *>(adapter->owned_auxiliary), false);
    if (result < 0)
        return result;
    return reinterpret_cast<SetBgmBufferPositionFn>(GetComSlot(buffer, 13))(
        buffer, 0);
}

i32 StopBgmStreamAdapter(TransitionSoundAdapterLayout *adapter)
{
    if (adapter->slots == 0)
        return kCoENotInitialized;
    adapter->playback_active = 0;
    void *const buffer = static_cast<void **>(adapter->slots)[0];
    return reinterpret_cast<i32 (TH10_STDCALL *)(void *)>(GetComSlot(
        buffer, 18))(buffer);
}

i32 StartBgmStreamAdapter(TransitionSoundAdapterLayout *adapter)
{
    if (adapter->slots == 0)
        return kCoENotInitialized;
    void *const buffer = static_cast<void **>(adapter->slots)[0];
    adapter->playback_active = 1;
    return PlayBgmBuffer(buffer, 0, adapter->play_priority,
                         adapter->play_flags);
}

i32 LoadBgmTrackImage(TransitionRootPartial *root, u32 slot,
                      const char *path)
{
    if (root->bgm_track_allocations[slot] != 0 &&
        strcmp(path, root->bgm_track_paths[slot]) == 0)
        return 0;

    strcpy(root->bgm_track_paths[slot], path);
    if ((g_BgmUsePreloadedTrackImages & 0x10) == 0 ||
        root->sound_build_gate == 0)
        return 0;

    if (root->bgm_track_allocations[slot] != 0) {
        FreeBgmTrackImage(root->bgm_track_allocations[slot]);
        root->bgm_track_allocations[slot] = 0;
    }

    void *const file = OpenBgmDataFile(root->bgm_stream_path, 0x80000000U,
                                        1, 3, 0x8000080U);
    if (reinterpret_cast<u32>(file) == kInvalidHandleValue)
        return -1;

    const u32 record_index = static_cast<u32>(FindBgmTrackRecordIndex(root, path));
    BgmTrackRecordPartial *const record =
        static_cast<BgmTrackRecordPartial *>(root->bgm_stream_descriptor) +
        record_index;
    SetBgmDataFilePositionIgnoredResult(file, record->data_file_offset);
    void *const image = AllocateBgmTrackImage(record->memory_image_bytes);
    if (image == 0) {
        CloseBgmDataFileIgnoredResult(file);
        return -1;
    }

    u32 ignored_read_bytes;
    ReadBgmDataFileIgnoredResult(file, image, record->memory_image_bytes,
                                 &ignored_read_bytes);
    CloseBgmDataFileIgnoredResult(file);
    root->bgm_track_descriptors[slot] = record;
    root->bgm_track_allocations[slot] = image;
    root->bgm_track_memory[slot] = static_cast<u8 *>(image);
    root->bgm_track_memory_bytes[slot] = record->memory_image_bytes;
    return 0;
}

i32 RebindBgmTrackFromPath(TransitionRootPartial *root, const char *path)
{
    if (root->control == 0)
        return -1;
    const u32 record_index = static_cast<u32>(FindBgmTrackRecordIndex(root, path));
    BgmTrackRecordPartial *const record =
        static_cast<BgmTrackRecordPartial *>(root->bgm_stream_descriptor) +
        record_index;
    (void)RebindBgmStreamSourceDescriptor(
        static_cast<BgmStreamSourceLayout *>(reinterpret_cast<
            TransitionSoundAdapterLayout *>(root->control)->owned_auxiliary),
        record);
    return 0;
}

i32 LoadBgmTrack(TransitionRootPartial *root, u32 slot)
{
    if (root->sound_build_gate == 0 || g_BgmRuntimeEnabled != 1 ||
        root->sound_device == 0)
        return -1;

    if ((g_BgmUsePreloadedTrackImages & 0x10) == 0) {
        return RebindBgmTrackFromPath(root, root->bgm_track_paths[slot]);
    }

    if (root->bgm_track_allocations[slot] == 0)
        return -1;
    BgmStreamDescriptorPartial *const descriptor =
        static_cast<BgmStreamDescriptorPartial *>(root->bgm_track_descriptors[slot]);
    const u32 page_bytes = ComputeBgmPageBytes(descriptor);
    root->bgm_stream_event = CreateAutoResetEvent();
    root->bgm_thread_handle = CreateBgmWorkerThread(g_BgmWorkerArgument,
                                                     &root->bgm_thread_id);
    const i32 result = CreateMemoryBgmStreamAdapter(
        reinterpret_cast<TransitionSoundAdapterLayout **>(&root->control),
        0x10100, static_cast<void **>(root->sound_build_gate),
        root->bgm_track_memory[slot], root->bgm_track_memory_bytes[slot],
        descriptor, g_BgmStreamFormatWords, 16, page_bytes,
        root->bgm_stream_event);
    if (result < 0)
        return -1;
    root->bgm_active_track = slot;
    return 0;
}

i32 AdvanceBgmCommandQueue(TransitionRootPartial *root)
{
    if (root->sound_build_gate == 0)
        return 0;

    bool immediately_advance_next = false;
    do {
        immediately_advance_next = false;
        BgmCommandPartial &command = root->bgm_commands[0];
        bool remove_command = false;
        TransitionSoundAdapterLayout *const adapter =
            reinterpret_cast<TransitionSoundAdapterLayout *>(root->control);

        switch (command.opcode) {
        case 1:
            if ((g_BgmUsePreloadedTrackImages & 0x10) != 0) {
                if (command.stage == 0) {
                    DestroyPreviousBgmStream(root);
                    (void)LoadBgmTrackImage(root,
                        static_cast<u32>(command.track_slot), command.path);
                    remove_command = true;
                    immediately_advance_next = true;
                } else {
                    ++command.stage;
                }
            } else {
                (void)LoadBgmTrackImage(root, static_cast<u32>(command.track_slot),
                                        command.path);
                remove_command = true;
                immediately_advance_next = true;
            }
            break;

        case 2:
            if ((g_BgmUsePreloadedTrackImages & 0x10) != 0 &&
                command.track_slot >= 0) {
                if (command.stage == 0) {
                    if (LoadBgmTrack(root, static_cast<u32>(command.track_slot)) != 0)
                        remove_command = true;
                    else
                        ++command.stage;
                } else if (command.stage == 2) {
                    if (adapter == 0 || RewindBgmStreamAdapter(adapter) < 0)
                        remove_command = true;
                    else
                        ++command.stage;
                } else if (command.stage == 1) {
                    if (adapter->worker_refill_active == 0) {
                        (void)RecreateBgmStreamBuffers(adapter);
                        ++command.stage;
                    }
                } else if (command.stage == 5) {
                    void *buffer = 0;
                    if (adapter->slots != 0 && adapter->slot_count != 0)
                        buffer = static_cast<void **>(adapter->slots)[0];
                    const BgmStreamSourceLayout *const source =
                        static_cast<const BgmStreamSourceLayout *>(
                            adapter->owned_auxiliary);
                    command.track_slot = source->descriptor == 0 ? 0 :
                        (static_cast<BgmStreamDescriptorPartial *>(
                            source->descriptor)->byte_count != 0);
                    if (InitializeTransitionSoundBuffer(adapter, buffer,
                        command.track_slot != 0) < 0)
                        remove_command = true;
                    else
                        ++command.stage;
                } else if (command.stage == 7) {
                    (void)StartTransitionSoundAdapter(adapter, 0, 1);
                    ++command.stage;
                } else {
                    ++command.stage;
                }
            } else if (adapter != 0) {
                if (command.stage == 0) {
                    (void)StopAndRewindTransitionSoundAdapter(adapter);
                    ++command.stage;
                } else if (command.stage == 1) {
                    if (adapter->worker_refill_active == 0) {
                        (void)RecreateBgmStreamBuffers(adapter);
                        ++command.stage;
                    }
                } else if (command.stage == 2) {
                    const char *path = command.track_slot < 0 ? command.path :
                        root->bgm_track_paths[command.track_slot];
                    (void)RebindBgmTrackFromPath(root, path);
                    ++command.stage;
                } else if (command.stage == 3) {
                    void *buffer = 0;
                    if (adapter->slots != 0 && adapter->slot_count != 0)
                        buffer = static_cast<void **>(adapter->slots)[0];
                    (void)RewindBgmStreamAdapter(adapter);
                    const BgmStreamSourceLayout *const source =
                        static_cast<const BgmStreamSourceLayout *>(
                            adapter->owned_auxiliary);
                    command.track_slot = source->descriptor == 0 ? 0 :
                        (static_cast<BgmStreamDescriptorPartial *>(
                            source->descriptor)->byte_count != 0);
                    if (InitializeTransitionSoundBuffer(adapter, buffer,
                        command.track_slot != 0) < 0)
                        remove_command = true;
                    else
                        ++command.stage;
                } else if (command.stage == 4 || command.stage == 7) {
                    (void)StartTransitionSoundAdapter(adapter, 0, 1);
                    ++command.stage;
                } else {
                    ++command.stage;
                }
            } else {
                remove_command = true;
            }
            break;

        case 3:
            if (adapter == 0) {
                remove_command = true;
            } else if (command.stage == 0) {
                (void)StopAndRewindTransitionSoundAdapter(adapter);
                ++command.stage;
            } else if (command.stage == 1) {
                if (root->bgm_thread_handle == 0)
                    remove_command = true;
                else {
                    PostBgmWorkerQuitIgnoredResult(root->bgm_thread_id);
                    ++command.stage;
                }
            } else if (command.stage == 2) {
                if (WaitForBgmStreamWorker(root->bgm_thread_handle, 0x100) == 0) {
                    root->bgm_thread_handle = 0;
                    ++command.stage;
                } else {
                    PostBgmWorkerQuitIgnoredResult(root->bgm_thread_id);
                }
            } else if (command.stage == 3) {
                CloseBgmStreamHandleIgnoredResult(root->bgm_thread_handle);
                CloseBgmStreamHandleIgnoredResult(root->bgm_stream_event);
                root->bgm_thread_handle = 0;
                DeleteBgmStreamAdapter(root->control, 1);
                root->control = 0;
                ++command.stage;
            } else if (command.stage == 10) {
                remove_command = true;
            } else {
                ++command.stage;
            }
            break;

        case 4:
            if (adapter == 0) {
                remove_command = true;
            } else if (command.stage == 0) {
                (void)StopAndRewindTransitionSoundAdapter(adapter);
                ++command.stage;
            } else if (command.stage == 1) {
                if (root->bgm_thread_handle == 0)
                    remove_command = true;
                else {
                    PostBgmWorkerQuitIgnoredResult(root->bgm_thread_id);
                    ++command.stage;
                }
            } else if (command.stage == 2) {
                if (WaitForBgmStreamWorker(root->bgm_thread_handle, 0x100) == 0) {
                    root->bgm_thread_handle = 0;
                    ++command.stage;
                } else {
                    PostBgmWorkerQuitIgnoredResult(root->bgm_thread_id);
                }
            } else if (command.stage == 3) {
                CloseBgmStreamHandleIgnoredResult(root->bgm_thread_handle);
                CloseBgmStreamHandleIgnoredResult(root->bgm_stream_event);
                root->bgm_thread_handle = 0;
                DeleteBgmStreamAdapter(root->control, 1);
                root->control = 0;
                ++command.stage;
            } else if (command.stage == 10) {
                remove_command = true;
            } else {
                ++command.stage;
            }
            break;

        case 5:
            if (g_BgmStreamAdapter != 0) {
                g_BgmStreamAdapter->transition_phase = 1;
                const i32 ticks = ConvertFloatToI32TowardZeroX87(
                    static_cast<float>(command.track_slot) * 0.01f);
                g_BgmStreamAdapter->remaining_ticks = ticks;
                g_BgmStreamAdapter->duration_ticks = ticks;
            }
            remove_command = true;
            break;

        case 6:
            if (g_BgmRuntimeEnabled == 1 && adapter != 0) {
                if (adapter->worker_refill_active == 0) {
                    (void)StopBgmStreamAdapter(adapter);
                    remove_command = true;
                }
            } else {
                remove_command = true;
            }
            break;

        case 7:
            if (g_BgmRuntimeEnabled == 1 && adapter != 0) {
                if (adapter->worker_refill_active == 0) {
                    (void)StartBgmStreamAdapter(adapter);
                    remove_command = true;
                }
            } else {
                remove_command = true;
            }
            break;

        case 8:
            if (adapter != 0)
                SetTransitionBufferVolume(
                    reinterpret_cast<TransitionControlPartial *>(adapter),
                    root->bgm_volume);
            remove_command = true;
            break;

        default:
            remove_command = true;
            break;
        }

        if (remove_command) {
            for (u32 index = 0; index != 31; ++index) {
                if (root->bgm_commands[index].opcode == 0)
                    break;
                root->bgm_commands[index] = root->bgm_commands[index + 1];
            }
        }
    } while (immediately_advance_next);

    return root->bgm_commands[0].opcode;
}

void AdvanceBgmSoundSequences(TransitionRootPartial *root)
{
    for (u32 channel = 0; channel != 12; ++channel) {
        const i32 sound_index = root->bgm_active_sound_indices[channel];
        if (sound_index < 0)
            break;
        const i32 count = root->bgm_sound_sequence_counts[channel];
        root->bgm_active_sound_indices[channel] = -1;

        if (count < 0) {
            if (root->bgm_sound_buffers[sound_index] != 0)
                StopBgmBufferIgnoredResult(root->bgm_sound_buffers[sound_index]);
            root->bgm_sound_sequence_counts[channel] = 0;
            continue;
        }

        i32 total = 0;
        for (i32 index = 0; index < count; ++index)
            total += root->bgm_sound_sequence_values[channel][index];
        root->bgm_sound_sequence_counts[channel] = 0;

        void *const buffer = root->bgm_sound_buffers[sound_index];
        if (buffer == 0)
            continue;
        StopBgmBufferIgnoredResult(buffer);
        (void)reinterpret_cast<SetBgmBufferPositionFn>(GetComSlot(buffer, 13))(
            buffer, 0);
        SetBgmBufferFrequencyIgnoredResult(buffer, total / count);
        const i32 volume = ComputeBgmSoundSequenceVolume(
            static_cast<i32>(g_BgmSoundDefinitions[sound_index].volume));
        (void)reinterpret_cast<SetBgmBufferVolumeFn>(GetComSlot(buffer, 15))(
            buffer, volume);
        (void)PlayBgmBuffer(buffer, 0, 0, 0);
    }
}

void EnqueueBgmSoundValue(TransitionRootPartial *root, u32 sound_index,
                          i32 value)
{
    for (u32 channel = 0; channel != 12; ++channel) {
        const i32 active = root->bgm_active_sound_indices[channel];
        if (active < 0) {
            root->bgm_active_sound_indices[channel] = sound_index;
            root->bgm_sound_default_frequencies[sound_index] =
                g_BgmSoundDefinitions[sound_index].default_frequency;
            root->bgm_sound_sequence_values[channel][0] = value;
            ++root->bgm_sound_sequence_counts[channel];
            return;
        }
        if (active == static_cast<i32>(sound_index)) {
            const i32 count = root->bgm_sound_sequence_counts[channel];
            if (count >= 128)
                return;
            root->bgm_sound_sequence_values[channel][count] = value;
            ++root->bgm_sound_sequence_counts[channel];
            return;
        }
    }
}

void EnqueueBgmSoundValueFromFloat(TransitionRootPartial *root,
                                   u32 sound_index, float value)
{
    EnqueueBgmSoundValue(root, sound_index,
        ConvertFloatToI32TowardZeroX87(value * 5.2083335f));
}

void RequestStopBgmSound(TransitionRootPartial *root, u32 sound_index)
{
    for (u32 channel = 0; channel != 12; ++channel) {
        const i32 active = root->bgm_active_sound_indices[channel];
        if (active < 0)
            return;
        if (active == static_cast<i32>(sound_index)) {
            root->bgm_active_sound_indices[channel] = sound_index;
            root->bgm_sound_sequence_counts[channel] = -1;
            return;
        }
    }
}

void InitializeBgmSoundQueueState(TransitionRootPartial *root)
{
    for (u32 index = 0; index != 128; ++index)
        root->bgm_sound_default_frequencies[index] = -1;
    for (u32 channel = 0; channel != 12; ++channel)
        root->bgm_active_sound_indices[channel] = -1;
}

void QueueBgmCommand(TransitionRootPartial *root, const char *path,
                     i32 opcode, i32 track_slot)
{
    for (u32 index = 0; index != 31; ++index) {
        BgmCommandPartial &command = root->bgm_commands[index];
        if (command.opcode != 0)
            continue;
        command.opcode = opcode;
        command.track_slot = track_slot;
        strcpy(command.path, path);
        command.stage = 0;
        return;
    }
}

i32 AdvanceBgmRuntime(TransitionRootPartial *root)
{
    if (root->sound_build_gate == 0)
        return 0;
    (void)AdvanceBgmCommandQueue(root);
    if (g_BgmSoundSequencesEnabled != 0)
        AdvanceBgmSoundSequences(root);
    return root->bgm_commands[0].opcode;
}

// TH10 0x0043cc40. Unlike ThreadControl, this routine owns a fixed pair of
// handles and uses the first one as its only entry guard. The state word is
// deliberately not cleared after stopping the pair.
void StopBgmWorkerControls(TransitionRootPartial *root)
{
    if (root->bgm_control_thread0 == 0)
        return;

    if (root->bgm_control_state == 0)
        root->bgm_control_state = 1;

    const u32 kWaitTimeout = 0x102;
    u32 result = WaitForBgmWorkerHandle(root->bgm_control_thread0, 100);
    while (result == kWaitTimeout) {
        SleepMilliseconds(1);
        result = WaitForBgmWorkerHandle(root->bgm_control_thread0, 100);
    }

    result = WaitForBgmWorkerHandle(root->bgm_control_thread1, 100);
    while (result == kWaitTimeout) {
        SleepMilliseconds(1);
        result = WaitForBgmWorkerHandle(root->bgm_control_thread1, 100);
    }

    CloseBgmWorkerHandle(root->bgm_control_thread0);
    CloseBgmWorkerHandle(root->bgm_control_thread1);
    root->bgm_control_thread0 = 0;
    root->bgm_control_thread1 = 0;
}

// TH10 0x0043dab0. The adapter pointer is the sole guard: event and worker
// values are intentionally untouched when no adapter is installed. A worker
// wait must return exactly zero before close/destruction can proceed.
void DestroyPreviousBgmStream(TransitionRootPartial *root)
{
    if (root->control == 0)
        return;

    (void)StopAndRewindTransitionSoundAdapter(
        reinterpret_cast<TransitionSoundAdapterLayout *>(root->control));
    if (root->bgm_thread_handle != 0) {
        PostBgmWorkerQuitIgnoredResult(root->bgm_thread_id);
        while (WaitForBgmStreamWorker(root->bgm_thread_handle, 0x100) != 0)
            PostBgmWorkerQuitIgnoredResult(root->bgm_thread_id);

        CloseBgmStreamHandleIgnoredResult(root->bgm_thread_handle);
        CloseBgmStreamHandleIgnoredResult(root->bgm_stream_event);
        root->bgm_thread_handle = 0;
    }

    DeleteBgmStreamAdapter(root->control, 1);
    root->control = 0;
}

// TH10 0x0044d850. A successful Lock is intentionally not paired with an
// automatic Unlock on copy, rewind, or second-region failures: those paths
// are externally observable native ownership states.
i32 RefillBgmStreamNotification(TransitionSoundAdapterLayout *adapter,
                                 i32 loop_mode)
{
    const i32 kCoENotInitialized = static_cast<i32>(0x800401f0U);
    const i32 kUnexpectedSecondRegion = static_cast<i32>(0x8000ffffU);
    if (adapter->slots == 0 || adapter->owned_auxiliary == 0)
        return kCoENotInitialized;

    void *const buffer = static_cast<void **>(adapter->slots)[0];
    u32 ignored_play_cursor = 0;
    u32 write_cursor = 0;
    (void)GetBgmBufferCurrentPosition(buffer, &ignored_play_cursor,
                                      &write_cursor);
    const u32 lower_limit = write_cursor - adapter->field_006c;
    if (adapter->field_0064 >= lower_limit &&
        adapter->field_0064 < write_cursor)
        return kCoENotInitialized;

    bool was_restored = false;
    i32 result = RestoreBgmBufferIfLost(buffer, &was_restored);
    if (result < 0)
        return result;
    if (was_restored) {
        result = InitializeTransitionSoundBuffer(adapter, buffer, false);
        return result < 0 ? result : 0;
    }

    void *region1 = 0;
    u32 region1_bytes = 0;
    void *region2 = 0;
    u32 region2_bytes = 0;
    result = LockBgmBuffer(buffer, adapter->field_0064, adapter->field_006c,
                           &region1, &region1_bytes, &region2,
                           &region2_bytes);
    if (result < 0)
        return result;
    if (region2 != 0)
        return kUnexpectedSecondRegion;

    if (adapter->field_0068 == 0) {
        u32 copied = 0;
        result = CopyBgmStreamSourceData(
            static_cast<BgmStreamSourceLayout *>(adapter->owned_auxiliary), region1,
                                         region1_bytes, &copied);
        if (result < 0)
            return result;

        if (copied != region1_bytes) {
            if (loop_mode == 0) {
                memset(static_cast<u8 *>(region1) + copied,
                       GetBgmStreamSilenceByte(adapter->owned_auxiliary),
                       region1_bytes - copied);
                adapter->field_0068 = 1;
            } else {
                do {
                    result = RewindBgmStreamSource(
                        static_cast<BgmStreamSourceLayout *>(adapter->owned_auxiliary),
                                                   true);
                    if (result < 0)
                        return result;
                    u32 next = 0;
                    result = CopyBgmStreamSourceData(
                        static_cast<BgmStreamSourceLayout *>(adapter->owned_auxiliary),
                        static_cast<u8 *>(region1) + copied,
                        region1_bytes - copied, &next);
                    if (result < 0)
                        return result;
                    copied += next;
                } while (copied < region1_bytes);
            }
        }
    } else {
        memset(region1, GetBgmStreamSilenceByte(adapter->owned_auxiliary),
               region1_bytes);
    }

    UnlockBgmBufferIgnoredResult(buffer, region1, region1_bytes, region2,
                                 region2_bytes);
    u32 play_cursor = 0;
    result = GetBgmBufferCurrentPosition(buffer, &play_cursor, 0);
    if (result < 0)
        return result;

    const u32 delta = play_cursor >= adapter->field_005c ?
        play_cursor - adapter->field_005c :
        adapter->lock_byte_count_0008 - adapter->field_005c + play_cursor;
    adapter->field_005c = play_cursor;
    adapter->field_0060 += delta;
    if (adapter->field_0068 != 0 &&
        adapter->field_0060 >= *reinterpret_cast<u32 *>(
            static_cast<u8 *>(adapter->owned_auxiliary) + 0x2c))
        StopBgmBufferIgnoredResult(buffer);
    adapter->field_0064 = (adapter->field_0064 + region1_bytes) %
        adapter->lock_byte_count_0008;
    return 0;
}

// TH10 0x0043e3a0. The CreateThread argument is unused. This is deliberately
// driven by factory-published globals, including the second global read when
// clearing the refill marker after an ignored notification result.
u32 TH10_STDCALL BgmWorkerThread(void *)
{
    bool stopped = false;
    while (!stopped) {
        const u32 result = MsgWaitForBgmWorker(
            1, &g_BgmNotificationHandle, 0, 0xffffffffU, 0xbf);
        TransitionSoundAdapterLayout *adapter = g_BgmStreamAdapter;
        if (adapter == 0)
            stopped = true;

        if (result == 0) {
            if (adapter != 0 && adapter->playback_active != 0) {
                adapter->worker_refill_active = 1;
                (void)RefillBgmStreamNotification(adapter, 1);
                g_BgmStreamAdapter->worker_refill_active = 0;
            }
        } else if (result == 1) {
            u8 message[0x20];
            while (PeekBgmWorkerMessage(message, 1) != 0) {
                if (*reinterpret_cast<const u32 *>(message + 4) == 0x12)
                    stopped = true;
            }
        }
    }
    return 0;
}

i32 InitializeBgmRelatedState(TransitionRootPartial *root)
{
    for (u32 index = 0; index != 12; ++index)
        root->bgm_active_sound_indices[index] = -1;
    StopBgmWorkerControls(root);

    if (root->sound_build_gate == 0)
        return -1;
    if (root->sound_device == 0)
        return 0;

    void **const descriptor_words = reinterpret_cast<void **>(root);
    for (u32 index = 0; index != 47; ++index) {
        const BgmSoundDefinition &definition = g_BgmSoundDefinitions[index];
        void *const description = descriptor_words[
            definition.descriptor_word_index + 2];
        void *const device = root->sound_device;
        (void)reinterpret_cast<CreateSoundBufferFn>(GetComSlot(device, 5))(
            device, description, &root->bgm_sound_buffers[index], 0);

        void *const buffer = root->bgm_sound_buffers[index];
        (void)reinterpret_cast<SetBgmBufferPositionFn>(GetComSlot(buffer, 13))(
            buffer, 0);
        (void)reinterpret_cast<SetBgmBufferVolumeFn>(GetComSlot(buffer, 15))(
            buffer, definition.volume);
    }

    root->bgm_volume_input = static_cast<i32>(g_BgmVolumeInputConfig);
    root->bgm_volume_enabled = static_cast<i32>(g_BgmVolumeEnabledConfig);
    root->bgm_volume = root->bgm_volume_enabled == 0 ? -10000 :
        ComputeBgmVolume(root->bgm_volume_input);
    return 0;
}

// TH10 0x0043d690. The source-level form isolates EAX/stack and factory
// register ABIs while retaining its exact resource creation order. In
// particular, a factory failure deliberately keeps the new event/thread.
i32 LoadBgmDataFile(TransitionRootPartial *root, const char *path)
{
    strcpy(root->bgm_stream_path, path);
    if (root->sound_build_gate == 0 || root->sound_device == 0)
        return -1;

    DestroyPreviousBgmStream(root);
    BgmStreamDescriptorPartial *const descriptor =
        static_cast<BgmStreamDescriptorPartial *>(root->bgm_stream_descriptor);
    const u32 page_bytes = ComputeBgmPageBytes(descriptor);
    root->bgm_stream_event = CreateAutoResetEvent();
    root->bgm_thread_handle = CreateBgmWorkerThread(g_BgmWorkerArgument,
                                                     &root->bgm_thread_id);

    const i32 result = CreateBgmStreamAdapter(
        reinterpret_cast<TransitionSoundAdapterLayout **>(&root->control),
        0x10100, static_cast<void **>(root->sound_build_gate), path,
        g_BgmStreamFormatWords, 16,
        page_bytes, root->bgm_stream_event, descriptor);
    return result < 0 ? -1 : 0;
}

} // namespace th10
