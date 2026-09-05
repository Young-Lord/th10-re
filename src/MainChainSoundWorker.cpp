#include "MainChainSoundWorker.hpp"

#include <string.h>

#include "BgmRuntime.hpp"
#include "MainChainSoundLoading.hpp"

namespace th10 {

namespace {

typedef u32 (TH10_STDCALL *ComReleaseFn)(void *object);
typedef i32 (TH10_STDCALL *DirectSoundSetCooperativeLevelFn)(void *device,
                                                               void *window,
                                                               u32 level);
typedef i32 (TH10_STDCALL *DirectSoundCreateBufferFn)(void *device,
                                                        const void *description,
                                                        void **out_buffer,
                                                        void *unknown);
typedef i32 (TH10_STDCALL *DirectSoundBufferLockFn)(void *buffer, u32 offset,
                                                      u32 bytes, void **region1,
                                                      u32 *region1_bytes,
                                                      void **region2,
                                                      u32 *region2_bytes,
                                                      u32 flags);
typedef i32 (TH10_STDCALL *DirectSoundBufferUnlockFn)(void *buffer,
                                                        void *region1,
                                                        u32 region1_bytes,
                                                        void *region2,
                                                        u32 region2_bytes);
typedef i32 (TH10_STDCALL *DirectSoundBufferPlayFn)(void *buffer,
                                                      u32 reserved1,
                                                      u32 reserved2, u32 flags);
typedef i32 (TH10_STDCALL *DirectSoundBufferSetFormatFn)(void *buffer,
                                                           const void *format);

#pragma pack(push, 2)
struct WaveFormatExPartial {
    u16 format_tag;
    u16 channel_count;
    u32 samples_per_second;
    u32 average_bytes_per_second;
    u16 block_alignment;
    u16 bits_per_sample;
    u16 extra_size;
};

struct DirectSoundBufferDescriptionPartial {
    u32 size;
    u32 flags;
    u32 buffer_bytes;
    u32 reserved;
    WaveFormatExPartial *format;
    u8 algorithm_guid[16];
};
#pragma pack(pop)

typedef char AssertWaveFormatExPartialSize[
    sizeof(WaveFormatExPartial) == 0x12 ? 1 : -1];
typedef char AssertDirectSoundBufferDescriptionPartialSize[
    sizeof(DirectSoundBufferDescriptionPartial) == 0x24 ? 1 : -1];

extern void *AllocateMainChainSoundHolder(u32 bytes); // TH10 0x00452493
extern void FreeMainChainSoundHolder(void *holder); // TH10 0x004524a1
extern i32 TH10_STDCALL DirectSoundCreate8(const void *device_guid,
                                            void **out_device,
                                            void *unknown);
extern void AppendMainChainDirectSoundFailure(); // TH10 0x0044b810
extern u32 TH10_STDCALL SetTimer(void *window, u32 timer_id,
                                  u32 milliseconds, void *callback);

void *GetComSlot(void *object, u32 index)
{
    return static_cast<void **>(*static_cast<void **>(object))[index];
}

void ReleaseAndClearSoundHolder(void **holder)
{
    if (*holder != 0) {
        (void)reinterpret_cast<ComReleaseFn>(GetComSlot(*holder, 2))(*holder);
        *holder = 0;
    }
}

WaveFormatExPartial MakeMainChainSoundFormat(u16 bits_per_sample,
                                             u16 channel_count,
                                             u32 sample_rate)
{
    WaveFormatExPartial format;
    format.format_tag = 1;
    format.channel_count = channel_count;
    format.samples_per_second = sample_rate;
    format.block_alignment = static_cast<u16>(channel_count *
        (bits_per_sample >> 3));
    format.average_bytes_per_second = sample_rate * format.block_alignment;
    format.bits_per_sample = bits_per_sample;
    format.extra_size = 0;
    return format;
}

i32 ConfigureMainChainPrimarySoundFormat(u16 bits_per_sample,
                                         u16 channel_count, u32 sample_rate,
                                         void **sound_holder)
{
    if (*sound_holder == 0)
        return static_cast<i32>(0x800401f0U);

    DirectSoundBufferDescriptionPartial description;
    memset(&description, 0, sizeof(description));
    description.size = sizeof(description);
    description.flags = 1;

    void *temporary_buffer = 0;
    const i32 create_result = reinterpret_cast<DirectSoundCreateBufferFn>(
        GetComSlot(*sound_holder, 3))(*sound_holder, &description,
                                      &temporary_buffer, 0);
    if (create_result < 0)
        return create_result;

    const WaveFormatExPartial format = MakeMainChainSoundFormat(
        bits_per_sample, channel_count, sample_rate);
    const i32 format_result = reinterpret_cast<DirectSoundBufferSetFormatFn>(
        GetComSlot(temporary_buffer, 14))(temporary_buffer, &format);
    if (format_result < 0)
        return format_result;

    (void)reinterpret_cast<ComReleaseFn>(GetComSlot(temporary_buffer, 2))(
        temporary_buffer);
    return 0;
}

i32 CompleteMainChainSoundRootInitialization(TransitionRootPartial *root,
                                              void *window, void **holder)
{
    // TH10 0x43cdf9 ignores this helper's result.
    (void)ConfigureMainChainPrimarySoundFormat(16, 2, 44100, holder);

    root->sound_device = *holder;
    root->bgm_thread_handle = 0;

    const WaveFormatExPartial format = MakeMainChainSoundFormat(16, 2, 44100);
    DirectSoundBufferDescriptionPartial description;
    memset(&description, 0, sizeof(description));
    description.size = sizeof(description);
    description.flags = 0x8008;
    description.buffer_bytes = 0x8000;
    description.format = const_cast<WaveFormatExPartial *>(&format);

    if (reinterpret_cast<DirectSoundCreateBufferFn>(GetComSlot(
            root->sound_device, 3))(root->sound_device, &description,
                                    &root->sound_device_unknown_0608, 0) < 0) {
        return -1;
    }

    void *region1 = 0;
    void *region2 = 0;
    u32 region1_bytes = 0;
    u32 region2_bytes = 0;
    void *const primary_buffer = root->sound_device_unknown_0608;
    if (reinterpret_cast<DirectSoundBufferLockFn>(GetComSlot(primary_buffer, 11))(
            primary_buffer, 0, 0x8000, &region1, &region1_bytes, &region2,
            &region2_bytes, 0) < 0) {
        return -1;
    }

    // The target clears a fixed 0x8000 bytes from only the first returned
    // region, irrespective of the reported lock-region lengths.
    memset(region1, 0, 0x8000);
    (void)reinterpret_cast<DirectSoundBufferUnlockFn>(GetComSlot(
        primary_buffer, 19))(primary_buffer, region1, region1_bytes, region2,
                             region2_bytes);
    (void)reinterpret_cast<DirectSoundBufferPlayFn>(GetComSlot(
        primary_buffer, 12))(primary_buffer, 0, 0, 1);

    root->bgm_volume_input = 100;
    root->bgm_volume_enabled = 100;
    (void)SetTimer(window, 0, 250, 0);
    root->sound_timer_window_060c = window;
    return ConsumeAllMainChainWaveImages(root);
}

} // namespace

i32 InitializeMainChainSoundRoot(TransitionRootPartial *root, void *window)
{
    // TH10 0x43cd30 begins with these sparse sentinel stores; in particular,
    // it does not initialize the sequence count/value tables at +0x650/+0x680.
    InitializeBgmSoundQueueState(root);

    void **const holder = static_cast<void **>(AllocateMainChainSoundHolder(4));
    if (holder != 0)
        *holder = 0;
    root->sound_build_gate = holder;

    // This read precedes DirectSoundCreate8 in the native code. It makes an
    // allocation failure fault immediately, before any DirectSound call.
    if (*holder != 0)
        ReleaseAndClearSoundHolder(holder);

    if (DirectSoundCreate8(0, holder, 0) < 0 ||
        reinterpret_cast<DirectSoundSetCooperativeLevelFn>(GetComSlot(
            *holder, 6))(*holder, window, 2) < 0) {
        AppendMainChainDirectSoundFailure();
        if (holder != 0) {
            ReleaseAndClearSoundHolder(holder);
            FreeMainChainSoundHolder(holder);
        }
        root->sound_build_gate = 0;
        return -1;
    }

    return CompleteMainChainSoundRootInitialization(root, window, holder);
}

} // namespace th10
