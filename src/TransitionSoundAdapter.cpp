#include <string.h>

#include "MainChainRuntime.hpp"
#include "TransitionSoundAdapter.hpp"

namespace th10 {

namespace {

// DirectSound calls are typed by vtable slot evidence, but are isolated so
// platform headers/COM ABI machinery do not leak into transition state logic.
extern void DirectSoundBufferSetVolume(void *buffer, i32 volume);
extern i32 DirectSoundBufferStop(void *buffer);
extern i32 ConvertFloatToI32TowardZeroX87(float value);
extern i32 g_TransitionVolumeScale; // TH10 DAT_00497854
extern void DirectSoundBufferRelease(void *buffer);
extern i32 DirectSoundBufferSetCurrentPosition(void *buffer, u32 position);
extern i32 DirectSoundBufferGetStatus(void *buffer, u32 *status);
extern i32 DirectSoundBufferRestore(void *buffer);
extern i32 DirectSoundBufferLock(void *buffer, u32 offset, u32 bytes,
                                 void **region, u32 *region_bytes);
extern void DirectSoundBufferUnlockIgnoredResult(void *buffer, void *region,
                                                 u32 region_bytes);
extern i32 DirectSoundBufferPlay(void *buffer, u32 priority, u32 flags);
extern void *AllocateTransitionSoundSlots(u32 bytes); // TH10 0x00452493
extern void FreeTransitionSoundSlots(void *pointer); // TH10 0x004524a1
extern void CloseAndFreeTransitionAuxiliary(void *auxiliary);
extern void *GetTransitionSoundAdapterBaseVtable(); // TH10 0x004705d8
extern void *GetSingleTransitionSoundAdapterVtable(); // TH10 0x004705d4
extern void SleepMilliseconds(u32 milliseconds);
extern i32 CopyTransitionStreamData(void *stream_source, void *destination,
                                    u32 capacity, u32 *copied_bytes);
extern void RewindTransitionStream(void *stream_source);
extern u8 GetTransitionSilenceByte(void *stream_source);
extern u32 GetTransitionRandomValue(); // TH10 0x0045371b

void *GetTransitionBuffer(TransitionControlPartial *control)
{
    void **slots = static_cast<void **>(control->buffer_slot);
    return slots[0];
}

void ApplyTransitionBufferVolume(void **slots, i32 input_volume)
{
    if (g_TransitionVolumeScale == 0) {
        DirectSoundBufferSetVolume(slots[0], -10000);
        return;
    }

    const i32 shifted_input = static_cast<i32>(
        static_cast<u32>(input_volume) + 5000U);
    const float scale = static_cast<float>(g_TransitionVolumeScale);
    const float curve = 1.0f - scale * 0.01f;
    const float product = static_cast<float>(shifted_input) *
        (1.0f - curve * curve);
    const i32 adjusted_volume = static_cast<i32>(
        static_cast<u32>(ConvertFloatToI32TowardZeroX87(product)) - 5000U);
    DirectSoundBufferSetVolume(slots[0], adjusted_volume);
}

const i32 kCoENotInitialized = static_cast<i32>(0x800401f0U);
const i32 kEFail = static_cast<i32>(0x80004005U);
const i32 kDsErrBufferLost = static_cast<i32>(0x88780096U);
const u32 kDsStatusBufferLost = 0x2;

i32 RestoreTransitionBufferIfLost(void *buffer, bool *was_restored)
{
    u32 status = 0;
    const i32 status_result = DirectSoundBufferGetStatus(buffer, &status);
    if (status_result < 0)
        return status_result;

    *was_restored = false;
    if ((status & kDsStatusBufferLost) == 0)
        return 0;

    do {
        const i32 restore_result = DirectSoundBufferRestore(buffer);
        if (restore_result == 0) {
            *was_restored = true;
            return 0;
        }
        if (restore_result == kDsErrBufferLost)
            SleepMilliseconds(10);
    } while (true);
}

} // namespace

// TH10 0x0044d4e0 semantic target. The native entry additionally receives the
// control in EAX and volume on stack; TransitionTick uses this normal boundary.
void SetTransitionBufferVolume(TransitionControlPartial *control, i32 volume)
{
    ApplyTransitionBufferVolume(static_cast<void **>(control->buffer_slot), volume);
}

void StopTransitionBuffer(TransitionControlPartial *control)
{
    DirectSoundBufferStop(GetTransitionBuffer(control));
}

// TH10's helper uses FISTP then corrects the result toward zero. The platform
// wrapper retains x87 control-word behavior for codegen-sensitive builds.
i32 RoundTransitionVolumeX87(float value)
{
    return ConvertFloatToI32TowardZeroX87(value);
}

void ConstructTransitionSoundAdapter(TransitionSoundAdapterLayout *control,
                                     void *const *source_buffers, u32 count,
                                     void *field_0008, void *owned_auxiliary)
{
    control->vtable = GetTransitionSoundAdapterBaseVtable();
    control->slots = AllocateTransitionSoundSlots(count * sizeof(void *));
    void **const slots = static_cast<void **>(control->slots);
    for (u32 index = 0; index != count; ++index)
        slots[index] = source_buffers[index];

    control->lock_byte_count_0008 = reinterpret_cast<u32>(field_0008);
    control->owned_auxiliary = owned_auxiliary;
    control->slot_count = count;
    (void)InitializeTransitionSoundBuffer(control, slots[0], false);
    for (u32 index = 0; index != count; ++index)
        (void)DirectSoundBufferSetCurrentPosition(slots[index], 0);
    control->playback_active = 0;
}

TransitionSoundAdapterLayout *ConstructSingleTransitionSoundAdapter(
    TransitionSoundAdapterLayout *control, void *buffer, void *field_0008,
    void *owned_auxiliary, u32 field_006c)
{
    control->vtable = GetTransitionSoundAdapterBaseVtable();
    void **const slots = static_cast<void **>(AllocateTransitionSoundSlots(
        sizeof(void *)));
    control->slots = slots;
    slots[0] = buffer;
    control->owned_auxiliary = owned_auxiliary;
    control->lock_byte_count_0008 = reinterpret_cast<u32>(field_0008);
    control->slot_count = 1;
    (void)InitializeTransitionSoundBuffer(control, slots[0], false);
    (void)DirectSoundBufferSetCurrentPosition(slots[0], 0);
    control->playback_active = 0;
    control->field_005c = 0;
    control->field_0060 = 0;
    control->field_0064 = 0;
    control->field_0068 = 0;
    control->vtable = GetSingleTransitionSoundAdapterVtable();
    control->field_006c = field_006c;
    return control;
}

void DestroyTransitionSoundAdapterInPlace(TransitionSoundAdapterLayout *control)
{
    control->vtable = GetTransitionSoundAdapterBaseVtable();
    void **const slots = static_cast<void **>(control->slots);
    for (u32 index = 0; index != control->slot_count; ++index) {
        if (slots[index] != 0) {
            DirectSoundBufferRelease(slots[index]);
            slots[index] = 0;
        }
    }

    if (slots != 0) {
        FreeTransitionSoundSlots(slots);
        control->slots = 0;
    }
    if (control->owned_auxiliary != 0) {
        CloseAndFreeTransitionAuxiliary(control->owned_auxiliary);
        control->owned_auxiliary = 0;
    }
}

i32 InitializeTransitionSoundBuffer(TransitionSoundAdapterLayout *control,
                                    void *buffer, bool refill_until_full)
{
    bool restored = false;
    const i32 restore_result = RestoreTransitionBufferIfLost(buffer, &restored);
    if (restore_result < 0)
        return restore_result;

    void *region = 0;
    u32 region_bytes = 0;
    const i32 lock_result = DirectSoundBufferLock(buffer, 0,
                                                   control->lock_byte_count_0008,
                                                   &region, &region_bytes);
    if (lock_result < 0)
        return lock_result;

    u32 copied_bytes = 0;
    i32 copy_result = CopyTransitionStreamData(control->owned_auxiliary, region,
                                                region_bytes, &copied_bytes);
    if (copy_result < 0)
        return copy_result;

    if (copied_bytes == 0) {
        memset(region, GetTransitionSilenceByte(control->owned_auxiliary),
               region_bytes);
    } else if (copied_bytes != region_bytes && refill_until_full) {
        RewindTransitionStream(control->owned_auxiliary);
        while (copied_bytes != region_bytes) {
            u32 next_bytes = 0;
            copy_result = CopyTransitionStreamData(control->owned_auxiliary,
                static_cast<u8 *>(region) + copied_bytes,
                region_bytes - copied_bytes, &next_bytes);
            if (copy_result < 0)
                return copy_result;
            copied_bytes += next_bytes;
        }
    } else if (copied_bytes != region_bytes) {
        memset(static_cast<u8 *>(region) + copied_bytes,
               GetTransitionSilenceByte(control->owned_auxiliary),
               region_bytes - copied_bytes);
    }

    DirectSoundBufferUnlockIgnoredResult(buffer, region, region_bytes);
    return 0;
}

void *SelectTransitionSoundSlot(TransitionSoundAdapterLayout *control)
{
    if (control->slots == 0)
        return 0;

    void **const slots = static_cast<void **>(control->slots);
    u32 index = 0;
    while (index < control->slot_count) {
        void *const buffer = slots[index];
        if (buffer == 0)
            break;
        u32 status = 0;
        (void)DirectSoundBufferGetStatus(buffer, &status);
        if ((status & 1) == 0)
            break;
        ++index;
    }
    if (index != control->slot_count)
        return slots[index];
    return slots[GetTransitionRandomValue() % control->slot_count];
}

i32 RewindTransitionSoundAdapter(TransitionSoundAdapterLayout *control)
{
    if (control->slots == 0)
        return kCoENotInitialized;

    i32 result = 0;
    void **const slots = static_cast<void **>(control->slots);
    for (u32 index = 0; index != control->slot_count; ++index)
        result |= DirectSoundBufferSetCurrentPosition(slots[index], 0);
    return result;
}

i32 StopAndRewindTransitionSoundAdapter(TransitionSoundAdapterLayout *control)
{
    if (control->slots == 0)
        return kCoENotInitialized;

    control->playback_active = 0;
    i32 result = 0;
    void **const slots = static_cast<void **>(control->slots);
    for (u32 index = 0; index != control->slot_count; ++index) {
        result |= DirectSoundBufferStop(slots[index]);
        result |= DirectSoundBufferSetCurrentPosition(slots[index], 0);
    }
    control->transition_phase = 0;
    return result;
}

i32 StartTransitionSoundAdapter(TransitionSoundAdapterLayout *control,
                                u32 priority, u32 flags)
{
    if (control->slots == 0)
        return kCoENotInitialized;

    void *const selected = SelectTransitionSoundSlot(control);
    if (selected == 0)
        return kEFail;

    bool restored = false;
    const i32 restore_result = RestoreTransitionBufferIfLost(selected, &restored);
    if (restore_result < 0)
        return restore_result;
    if (restored) {
        const i32 initialize_result = InitializeTransitionSoundBuffer(control,
            selected, false);
        if (initialize_result < 0)
            return initialize_result;
        (void)RewindTransitionSoundAdapter(control);
    }

    control->transition_phase = 0;
    control->remaining_ticks = 0;
    control->duration_ticks = 0;
    ApplyTransitionBufferVolume(static_cast<void **>(control->slots), 0);
    control->playback_active = 1;
    control->play_priority = priority;
    control->play_flags = flags;
    control->field_002c = 0;
    return DirectSoundBufferPlay(selected, control->play_priority,
                                 control->play_flags);
}


// TH10 0x0044d300. Native ESI = DirectSound buffer, EBX = u32 out flag
// (optional). Shared "restore if lost" gate used by
// StartTransitionSoundAdapter, RefillBgmStreamNotification and
// RewindBgmStreamAdapter. Returns CO_E_NOTINITIALIZED for a null buffer,
// the GetStatus HRESULT on failure, 1 when the buffer is not lost, and 0
// after a successful Restore (raising the out flag).
i32 EnsureTransitionBufferRestoredEsiEbxAbi(void *buffer, u32 *was_restored)
{
    if (buffer == 0)
        return kCoENotInitialized;

    if (was_restored != 0)
        *was_restored = 0;

    u32 status = 0;
    const i32 status_result = DirectSoundBufferGetStatus(buffer, &status);
    if (status_result < 0)
        return status_result;

    if ((status & kDsStatusBufferLost) == 0U)
        return 1;

    // Native retry loop: Restore, Sleep(10) only after a
    // DSERR_BUFFERLOST result, Restore again, repeat while non-zero.
    while (true) {
        i32 restore_result = DirectSoundBufferRestore(buffer);
        if (restore_result == kDsErrBufferLost)
            SleepMilliseconds(10);
        restore_result = DirectSoundBufferRestore(buffer);
        if (restore_result == 0) {
            if (was_restored != 0)
                *was_restored = 1;
            return 0;
        }
    }
}

} // namespace th10
