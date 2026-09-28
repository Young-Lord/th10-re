#pragma once

#include "MainChainRuntime.hpp"

namespace th10 {

struct TransitionSoundAdapterLayout;

// The stream source is an independently allocated 0x94-byte object owned by
// TransitionSoundAdapterLayout::owned_auxiliary.
struct BgmStreamSourceLayout {
    u8 unknown_0000[8];
    u32 file_bytes_remaining;
    u8 unknown_000c[0x20];
    u32 initial_byte_count;
    u8 unknown_0030[0x48];
    u32 ownership_mode;
    u32 memory_backed;
    u8 *memory_base;
    u8 *memory_cursor;
    u32 memory_size;
    void *file_handle;
    void *descriptor;
};

typedef char AssertBgmStreamSourceSize[
    sizeof(BgmStreamSourceLayout) == 0x94 ? 1 : -1];

// TH10 0x0043db60 semantic body. The native EBX/plain-ret entry remains a
// separate adapter concern.
i32 InitializeBgmRelatedState(TransitionRootPartial *root);
i32 LoadBgmDataFile(TransitionRootPartial *root, const char *path);
void StopBgmWorkerControls(TransitionRootPartial *root); // TH10 0x0043cc40
void DestroyPreviousBgmStream(TransitionRootPartial *root); // TH10 0x0043dab0
u32 TH10_STDCALL BgmWorkerThread(void *unused); // TH10 0x0043e3a0
i32 RefillBgmStreamNotification(TransitionSoundAdapterLayout *adapter,
                                 i32 loop_mode); // TH10 0x0044d850
i32 InitializeBgmStreamSource(BgmStreamSourceLayout *source,
                              u32 ownership_mode, const char *path,
                              void *descriptor); // TH10 0x0044dbf0
i32 RewindBgmStreamSource(BgmStreamSourceLayout *source,
                          bool use_loop_offset); // TH10 0x0044dd40
i32 CopyBgmStreamSourceData(BgmStreamSourceLayout *source, void *destination,
                            u32 requested_bytes, u32 *copied_bytes); // 0x44de10
i32 CreateBgmStreamAdapter(TransitionSoundAdapterLayout **out_adapter,
                           u32 flags, void **sound_device_holder,
                           const char *path, const u32 format_guid_words[4],
                           u32 page_count, u32 page_bytes, void *event_handle,
                           void *stream_descriptor); // TH10 0x0044c8f0
i32 CreateMemoryBgmStreamAdapter(TransitionSoundAdapterLayout **out_adapter,
                                 u32 flags, void **sound_device_holder,
                                 u8 *memory_base, u32 memory_size,
                                 void *stream_descriptor,
                                 const u32 format_guid_words[4],
                                 u32 page_count, u32 page_bytes,
                                 void *event_handle); // TH10 0x0044cbf0
i32 RebindBgmStreamSourceDescriptor(BgmStreamSourceLayout *source,
                                    void *descriptor); // TH10 0x0044dca0
void ClearBgmStreamSourceStateEaxAbi(
    BgmStreamSourceLayout *source); // TH10 0x0044dbb0
i32 RecreateBgmStreamBuffers(TransitionSoundAdapterLayout *adapter); // 0x44cf20
i32 RewindBgmStreamAdapter(TransitionSoundAdapterLayout *adapter); // 0x44dad0
i32 StopBgmStreamAdapter(TransitionSoundAdapterLayout *adapter); // 0x44d5b0
i32 StartBgmStreamAdapter(TransitionSoundAdapterLayout *adapter); // 0x44d5d0
i32 LoadBgmTrackImage(TransitionRootPartial *root, u32 slot,
                      const char *path); // TH10 0x0043d7d0
i32 RebindBgmTrackFromPath(TransitionRootPartial *root,
                           const char *path); // TH10 0x0043d790
i32 FindBgmTrackRecordIndex(const TransitionRootPartial *root,
                            const char *path); // TH10 0x0043d2a0
i32 LoadBgmTrack(TransitionRootPartial *root, u32 slot); // TH10 0x0043d950
i32 AdvanceBgmCommandQueue(TransitionRootPartial *root);
void AdvanceBgmSoundSequences(TransitionRootPartial *root);
void EnqueueBgmSoundValue(TransitionRootPartial *root, u32 sound_index,
                          i32 value); // TH10 0x0043dc90
void EnqueueBgmSoundValueFromFloat(TransitionRootPartial *root,
                                   u32 sound_index, float value); // 0x43dd10
void RequestStopBgmSound(TransitionRootPartial *root,
                         u32 sound_index); // 0x43ddb0
void QueueBgmCommand(TransitionRootPartial *root, const char *path,
                     i32 opcode, i32 track_slot); // TH10 0x0043e460
i32 AdvanceBgmRuntime(TransitionRootPartial *root); // TH10 0x0043ddf0
void InitializeBgmSoundQueueState(TransitionRootPartial *root); // 0x43cd30 subset

} // namespace th10
