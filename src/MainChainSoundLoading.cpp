#include <string.h>

#include "MainChainSoundLoading.hpp"

namespace th10 {

namespace {

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

struct WaveChunkPayload {
    const u8 *format;
    const u8 *data;
};

extern volatile u32 g_MainChainResourceGate; // TH10 DAT_004977b4
extern const char *const g_MainChainWaveNames[kMainChainWaveCount]; // 0x474b40

// These are normal C++ adapter boundaries around known DirectSound vtable
// operations; they are not declarations of the original mixed-ABI entries.
extern void ReleaseDirectSoundBuffer(void *buffer); // IUnknown::Release +0x08
extern i32 CreateDirectSoundBuffer(void *device,
                                   const DirectSoundBufferDescriptionPartial *description,
                                   void **out_buffer); // IDirectSound +0x0c
extern i32 LockDirectSoundBuffer(void *buffer, u32 offset, u32 bytes,
                                 void **region1, u32 *region1_bytes,
                                 void **region2, u32 *region2_bytes,
                                 u32 flags); // IDirectSoundBuffer::Lock
extern void UnlockDirectSoundBuffer(void *buffer, void *region1,
                                    u32 region1_bytes, void *region2,
                                    u32 region2_bytes); // HRESULT ignored
extern void ReleaseGameHeapImage(void *image); // TH10 0x00452422
extern void SleepMilliseconds(u32 milliseconds); // KERNEL32!Sleep
extern void AppendSoundFileLoadError(const char *name); // TH10 0x0044b810
extern void AppendDirectSoundReadyMessage(); // TH10 0x0044b810

u32 ReadU32(const u8 *bytes)
{
    return static_cast<u32>(bytes[0]) |
        (static_cast<u32>(bytes[1]) << 8) |
        (static_cast<u32>(bytes[2]) << 16) |
        (static_cast<u32>(bytes[3]) << 24);
}

// 0x0043d250 only guards the initial scan and then advances by 8 + chunk
// size without reducing a remaining bound. This helper deliberately models
// that normal-input traversal; it must not be used for hostile file data.
const u8 *FindWaveChunkNormalInput(u32 initial_scan, const u8 *chunk,
                                   u32 expected_tag)
{
    if (initial_scan == 0)
        return 0;

    for (;;) {
        if (ReadU32(chunk) == expected_tag)
            return chunk + 8;

        const u32 advance = 8 + ReadU32(chunk + 4);
        if (advance == 0)
            return 0;
        chunk += advance;
    }
}

bool ParseWaveNormalInput(const u8 *image, WaveChunkPayload *wave)
{
    const u32 kRiff = 0x46464952U; // "RIFF" in little-endian memory.
    const u32 kWave = 0x45564157U; // "WAVE" in little-endian memory.
    const u32 kFmt = 0x20746d66U; // "fmt " in little-endian memory.
    const u32 kData = 0x61746164U; // "data" in little-endian memory.

    if (ReadU32(image) != kRiff || ReadU32(image + 8) != kWave)
        return false;

    const u32 initial_scan = ReadU32(image + 4) - 0xc;
    wave->format = FindWaveChunkNormalInput(initial_scan, image + 0xc, kFmt);
    wave->data = FindWaveChunkNormalInput(initial_scan, image + 0xc, kData);
    return wave->format != 0 && wave->data != 0;
}

i32 BuildSoundBufferFromTransferredWaveImage(TransitionRootPartial *root,
                                              u32 index, u8 *image)
{
    WaveChunkPayload wave;
    if (!ParseWaveNormalInput(image, &wave)) {
        ReleaseGameHeapImage(image);
        return -1;
    }

    WaveFormatExPartial format;
    memcpy(&format, wave.format, sizeof(format));

    DirectSoundBufferDescriptionPartial description;
    memset(&description, 0, sizeof(description));
    description.size = sizeof(description);
    description.flags = 0x80c8;
    description.buffer_bytes = ReadU32(wave.data - 4);
    description.format = &format;

    if (CreateDirectSoundBuffer(root->sound_device, &description,
                                &root->sound_buffers[index]) < 0) {
        ReleaseGameHeapImage(image);
        return -1;
    }

    void *region1 = 0;
    void *region2 = 0;
    u32 region1_bytes = 0;
    u32 region2_bytes = 0;
    if (LockDirectSoundBuffer(root->sound_buffers[index], 0,
                              description.buffer_bytes, &region1,
                              &region1_bytes, &region2, &region2_bytes, 0) < 0) {
        ReleaseGameHeapImage(image);
        return -1;
    }

    memcpy(region1, wave.data, region1_bytes);
    if (region2_bytes != 0)
        memcpy(region2, wave.data + region1_bytes, region2_bytes);
    UnlockDirectSoundBuffer(root->sound_buffers[index], region1, region1_bytes,
                            region2, region2_bytes);
    ReleaseGameHeapImage(image);
    return 0;
}

} // namespace

// Semantic form of TH10 0x0043d390. Its native ECX/EDX/stack ABI requires a
// separate thunk; resource_name is retained here for that explicit boundary.
i32 ConsumeMainChainWaveImage(TransitionRootPartial *root, u32 index,
                              const char *)
{
    if (root->sound_build_gate == 0)
        return 0;

    if (root->sound_buffers[index] != 0) {
        ReleaseDirectSoundBuffer(root->sound_buffers[index]);
        root->sound_buffers[index] = 0;
    }

    while (root->raw_wave_images[index] == 0) {
        SleepMilliseconds(10);
        if (g_MainChainResourceGate == 2)
            return 0;
    }

    u8 *const image = root->raw_wave_images[index];
    root->raw_wave_images[index] = 0;
    return BuildSoundBufferFromTransferredWaveImage(root, index, image);
}

// Semantic form of the caller loop at TH10 0x0043cf60.
i32 ConsumeAllMainChainWaveImages(TransitionRootPartial *root)
{
    for (u32 index = 0; index != kMainChainWaveCount; ++index) {
        if (g_MainChainResourceGate == 2)
            return -1;
        if (ConsumeMainChainWaveImage(root, index,
                                      g_MainChainWaveNames[index]) != 0) {
            AppendSoundFileLoadError(g_MainChainWaveNames[index]);
            return -1;
        }
    }

    AppendDirectSoundReadyMessage();
    return 0;
}

} // namespace th10
