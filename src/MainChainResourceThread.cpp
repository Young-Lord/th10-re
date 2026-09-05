#include "MainChainResourceThread.hpp"
#include "BgmRuntime.hpp"
#include "PackedArchive.hpp"

namespace th10 {

namespace {

extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590
extern volatile u32 g_MainChainResourceGate; // TH10 DAT_004977b4
extern const char *const g_MainChainWaveNames[kMainChainWaveCount]; // 0x00474b40

extern void AppendSoundFileLoadError(const char *name); // TH10 0x0044b810
extern void SleepMilliseconds(u32 milliseconds); // KERNEL32!Sleep
extern void ReleaseGameHeapImage(void *image); // TH10 0x00452422
extern void ReleaseDirectSoundBuffer(void *buffer); // IUnknown::Release
extern void KillMainChainWindowTimer(void *window, u32 timer_id);
extern void ShutdownMainChainSoundDevice(void *device); // vtable +0x48
extern void ReleaseMainChainSoundDevice(void *device); // IUnknown::Release
extern void DeleteTransitionRootControl(void *control, u32 deleting_flag);
extern void FreeMainChainSoundHolder(void *holder); // TH10 0x004524a1

} // namespace

// TH10 0x0043d080. The native entry ignores the CreateThread argument and
// publishes complete, caller-owned archive images without freeing earlier
// slots on a later load failure.
void PreloadMainChainWaveResources(TransitionRootPartial *root)
{
    for (u32 index = 0;
         index != kMainChainWaveCount && g_MainChainResourceGate != 2;
         ++index) {
        u8 *image = LoadPackedResource(g_MainChainWaveNames[index], 0, 0);
        root->raw_wave_images[index] = image;
        if (image == 0) {
            AppendSoundFileLoadError(g_MainChainWaveNames[index]);
            return;
        }
    }

    while (g_MainChainResourceGate == 0)
        SleepMilliseconds(1);
}

// A conventional thread callback cannot expose the native entry's plain-ret
// ABI, so this wrapper owns the defined Win32 return value.
u32 TH10_STDCALL MainChainResourceThreadAdapter(void *)
{
    PreloadMainChainWaveResources(&g_TransitionRoot);
    return 0;
}

void DestroyUnconsumedMainChainWaveImages(TransitionRootPartial *root)
{
    for (u32 index = 0; index != kMainChainWaveCount; ++index) {
        if (root->raw_wave_images[index] != 0) {
            ReleaseGameHeapImage(root->raw_wave_images[index]);
            root->raw_wave_images[index] = 0;
        }
    }
}

void DestroyTransitionRootSoundResources(TransitionRootPartial *root)
{
    if (root->bgm_stream_descriptor != 0) {
        ReleaseGameHeapImage(root->bgm_stream_descriptor);
        root->bgm_stream_descriptor = 0;
    }

    for (u32 index = 0; index != 128; ++index) {
        if (root->bgm_sound_buffers[index] != 0) {
            ReleaseDirectSoundBuffer(root->bgm_sound_buffers[index]);
            root->bgm_sound_buffers[index] = 0;
        }
        if (root->sound_buffers[index] != 0) {
            ReleaseDirectSoundBuffer(root->sound_buffers[index]);
            root->sound_buffers[index] = 0;
        }
    }

    DestroyUnconsumedMainChainWaveImages(root);

    if (root->sound_build_gate == 0)
        return;

    KillMainChainWindowTimer(root->sound_timer_window_060c, 1);
    DestroyPreviousBgmStream(root);
    root->sound_device = 0;
    ShutdownMainChainSoundDevice(root->sound_device_unknown_0608);
    if (root->sound_device_unknown_0608 != 0) {
        ReleaseMainChainSoundDevice(root->sound_device_unknown_0608);
        root->sound_device_unknown_0608 = 0;
    }

    if (root->control != 0) {
        DeleteTransitionRootControl(root->control, 1);
        root->control = 0;
    }

    void **const sound_holder = static_cast<void **>(root->sound_build_gate);
    if (sound_holder != 0) {
        if (*sound_holder != 0) {
            ReleaseMainChainSoundDevice(*sound_holder);
            *sound_holder = 0;
        }
        FreeMainChainSoundHolder(sound_holder);
        root->sound_build_gate = 0;
    }

    for (u32 index = 0; index != 16; ++index) {
        if (root->bgm_track_allocations[index] != 0) {
            ReleaseGameHeapImage(root->bgm_track_allocations[index]);
            root->bgm_track_allocations[index] = 0;
        }
    }
}

} // namespace th10
