#pragma once

#include <stddef.h>

#include "Th10Types.hpp"

namespace th10 {

struct TransitionControlPartial;

// DSBUFFERDESC-sized data retained by the single-buffer BGM adapter so a
// lost DirectSound buffer can be recreated with the original parameters.
struct TransitionSoundBufferDescription {
    u32 size;
    u32 flags;
    u32 buffer_bytes;
    u32 reserved;
    void *wave_format;
    u32 format_guid_words[4];
};

typedef char AssertTransitionSoundBufferDescriptionSize[
    sizeof(TransitionSoundBufferDescription) == 0x24 ? 1 : -1];

// Full owning adapter layout is intentionally distinct from the smaller
// root-transition view used by TransitionTick.
struct TransitionSoundAdapterLayout {
    void *vtable;
    void *slots;
    u32 lock_byte_count_0008;
    void *owned_auxiliary;
    u32 slot_count;
    u32 remaining_ticks;
    u32 duration_ticks;
    u32 transition_phase;
    u32 play_priority;
    u32 play_flags;
    u8 unknown_0028[4];
    u32 field_002c;
    u32 playback_active;
    TransitionSoundBufferDescription recreate_buffer_description;
    void **sound_device_holder;
    u32 field_005c;
    u32 field_0060;
    u32 field_0064;
    u32 field_0068;
    u32 field_006c;
    void *notification_event;
    u32 worker_refill_active;
};

typedef char AssertTransitionSoundAdapterLayoutSize[
    sizeof(TransitionSoundAdapterLayout) == 0x78 ? 1 : -1];
typedef char AssertTransitionSoundAdapterSlotsOffset[
    offsetof(TransitionSoundAdapterLayout, slots) == 0x4 ? 1 : -1];
typedef char AssertTransitionSoundAdapterAuxiliaryOffset[
    offsetof(TransitionSoundAdapterLayout, owned_auxiliary) == 0xc ? 1 : -1];
typedef char AssertTransitionSoundAdapterSlotCountOffset[
    offsetof(TransitionSoundAdapterLayout, slot_count) == 0x10 ? 1 : -1];
typedef char AssertTransitionSoundAdapterField006cOffset[
    offsetof(TransitionSoundAdapterLayout, field_006c) == 0x6c ? 1 : -1];
typedef char AssertTransitionSoundAdapterWorkerRefillOffset[
    offsetof(TransitionSoundAdapterLayout, worker_refill_active) == 0x74 ? 1 : -1];
typedef char AssertTransitionSoundAdapterRecreateDescriptionOffset[
    offsetof(TransitionSoundAdapterLayout, recreate_buffer_description) == 0x34 ? 1 : -1];
typedef char AssertTransitionSoundAdapterDeviceHolderOffset[
    offsetof(TransitionSoundAdapterLayout, sound_device_holder) == 0x58 ? 1 : -1];
typedef char AssertTransitionSoundAdapterNotificationEventOffset[
    offsetof(TransitionSoundAdapterLayout, notification_event) == 0x70 ? 1 : -1];

// These calls preserve the concrete DirectSound interface boundary while the
// original mixed EAX/stack ABI remains separate from semantic C++ callers.
void SetTransitionBufferVolume(TransitionControlPartial *control, i32 volume);
void StopTransitionBuffer(TransitionControlPartial *control);
i32 RoundTransitionVolumeX87(float value);

void ConstructTransitionSoundAdapter(TransitionSoundAdapterLayout *adapter,
                                     void *const *source_buffers, u32 count,
                                     void *field_0008, void *owned_auxiliary);
TransitionSoundAdapterLayout *ConstructSingleTransitionSoundAdapter(
    TransitionSoundAdapterLayout *adapter, void *buffer, void *field_0008,
    void *owned_auxiliary, u32 field_006c);
void DestroyTransitionSoundAdapterInPlace(TransitionSoundAdapterLayout *adapter);

i32 InitializeTransitionSoundBuffer(TransitionSoundAdapterLayout *adapter,
                                    void *buffer, bool refill_until_full);
i32 StartTransitionSoundAdapter(TransitionSoundAdapterLayout *adapter,
                                u32 priority, u32 flags);
void *SelectTransitionSoundSlot(TransitionSoundAdapterLayout *adapter); // 0x44d370
i32 StopAndRewindTransitionSoundAdapter(TransitionSoundAdapterLayout *adapter);
i32 RewindTransitionSoundAdapter(TransitionSoundAdapterLayout *adapter);

} // namespace th10
