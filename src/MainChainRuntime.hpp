#pragma once

#include <stddef.h>

#include "Th10Platform.hpp"
#include "ManagerWork.hpp"

namespace th10 {

// Partial objects consumed by MainChainUpdate. They are intentionally not
// complete engine types: only the offsets directly established by the binary
// are represented here.
struct TransitionControlPartial;

struct BgmCommandPartial {
    i32 opcode;
    i32 track_slot;
    i32 stage;
    char path[0x100];
};

typedef char AssertBgmCommandPartialSize[
    sizeof(BgmCommandPartial) == 0x10c ? 1 : -1];

struct TransitionRootPartial {
    void *sound_device;
    u8 unknown_0004[4];
    void *sound_buffers[128];
    void *bgm_sound_buffers[128];
    i32 bgm_sound_default_frequencies[128];
    void *sound_device_unknown_0608;
    void *sound_timer_window_060c;
    void *sound_build_gate;
    u32 bgm_thread_id;
    void *bgm_thread_handle;
    u8 unknown_061c[4];
    i32 bgm_active_sound_indices[12];
    i32 bgm_sound_sequence_counts[12];
    i32 bgm_sound_sequence_values[12][128];
    void *bgm_track_descriptors[16];
    void *bgm_track_allocations[16];
    u8 *bgm_track_memory[16];
    u32 bgm_track_memory_bytes[16];
    u32 bgm_active_track;
    void *bgm_stream_descriptor;
    BgmCommandPartial bgm_commands[32];
    char bgm_track_paths[16][0x100];
    char bgm_stream_path[0x100];
    TransitionControlPartial *control;
    void *bgm_stream_event;
    u8 unknown_5210[8];
    void *bgm_control_thread0;
    void *bgm_control_thread1;
    u32 bgm_control_thread_id;
    u32 bgm_control_state;
    u32 bgm_control_argument;
    u8 unknown_522c[4];
    u8 *raw_wave_images[37];
    i32 bgm_volume_input;
    i32 bgm_volume_enabled;
    i32 bgm_volume;
};

enum TransitionPhase {
    TransitionPhase_None = 0,
    TransitionPhase_StopFade = 1,
    TransitionPhase_FadeIn = 2,
    TransitionPhase_FadeOutShort = 3,
    TransitionPhase_FadeInShort = 4,
};

struct TransitionControlPartial {
    u8 unknown_0000[4];
    void *buffer_slot;
    u8 unknown_0008[0xc];
    i32 remaining_ticks;
    i32 duration_ticks;
    TransitionPhase phase;
};

struct ManagerWorkOwnerPartial {
    u8 unknown_0000[0x3ad06c];
    void *work_slots[0x21];
};

struct RenderOwnerPartial;

typedef char AssertTransitionRootControlOffset[
    offsetof(TransitionRootPartial, control) == 0x5208 ? 1 : -1];
typedef char AssertTransitionRootSoundBuildGateOffset[
    offsetof(TransitionRootPartial, sound_build_gate) == 0x610 ? 1 : -1];
typedef char AssertTransitionRootRawWaveImagesOffset[
    offsetof(TransitionRootPartial, raw_wave_images) == 0x5230 ? 1 : -1];
typedef char AssertTransitionRootBgmBuffersOffset[
    offsetof(TransitionRootPartial, bgm_sound_buffers) == 0x208 ? 1 : -1];
typedef char AssertTransitionRootBgmThreadIdOffset[
    offsetof(TransitionRootPartial, bgm_thread_id) == 0x614 ? 1 : -1];
typedef char AssertTransitionRootBgmVolumeInputOffset[
    offsetof(TransitionRootPartial, bgm_volume_input) == 0x52c4 ? 1 : -1];
typedef char AssertTransitionRootBgmDescriptorOffset[
    offsetof(TransitionRootPartial, bgm_stream_descriptor) == 0x1f84 ? 1 : -1];
typedef char AssertTransitionRootBgmTrackDescriptorsOffset[
    offsetof(TransitionRootPartial, bgm_track_descriptors) == 0x1e80 ? 1 : -1];
typedef char AssertTransitionRootBgmSoundSequenceCountsOffset[
    offsetof(TransitionRootPartial, bgm_sound_sequence_counts) == 0x650 ? 1 : -1];
typedef char AssertTransitionRootBgmSoundSequenceValuesOffset[
    offsetof(TransitionRootPartial, bgm_sound_sequence_values) == 0x680 ? 1 : -1];
typedef char AssertTransitionRootBgmSoundDefaultFrequenciesOffset[
    offsetof(TransitionRootPartial, bgm_sound_default_frequencies) == 0x408 ? 1 : -1];
typedef char AssertTransitionRootBgmTrackAllocationsOffset[
    offsetof(TransitionRootPartial, bgm_track_allocations) == 0x1ec0 ? 1 : -1];
typedef char AssertTransitionRootBgmTrackMemoryOffset[
    offsetof(TransitionRootPartial, bgm_track_memory) == 0x1f00 ? 1 : -1];
typedef char AssertTransitionRootBgmTrackMemoryBytesOffset[
    offsetof(TransitionRootPartial, bgm_track_memory_bytes) == 0x1f40 ? 1 : -1];
typedef char AssertTransitionRootBgmActiveTrackOffset[
    offsetof(TransitionRootPartial, bgm_active_track) == 0x1f80 ? 1 : -1];
typedef char AssertTransitionRootBgmTrackPathsOffset[
    offsetof(TransitionRootPartial, bgm_track_paths) == 0x4108 ? 1 : -1];
typedef char AssertTransitionRootBgmCommandsOffset[
    offsetof(TransitionRootPartial, bgm_commands) == 0x1f88 ? 1 : -1];
typedef char AssertTransitionRootBgmPathOffset[
    offsetof(TransitionRootPartial, bgm_stream_path) == 0x5108 ? 1 : -1];
typedef char AssertTransitionRootBgmEventOffset[
    offsetof(TransitionRootPartial, bgm_stream_event) == 0x520c ? 1 : -1];
typedef char AssertTransitionRootBgmControlThread0Offset[
    offsetof(TransitionRootPartial, bgm_control_thread0) == 0x5218 ? 1 : -1];
typedef char AssertTransitionRootBgmControlThread1Offset[
    offsetof(TransitionRootPartial, bgm_control_thread1) == 0x521c ? 1 : -1];
typedef char AssertTransitionControlRemainingOffset[
    offsetof(TransitionControlPartial, remaining_ticks) == 0x14 ? 1 : -1];
typedef char AssertTransitionControlDurationOffset[
    offsetof(TransitionControlPartial, duration_ticks) == 0x18 ? 1 : -1];
typedef char AssertTransitionControlPhaseOffset[
    offsetof(TransitionControlPartial, phase) == 0x1c ? 1 : -1];
typedef char AssertManagerWorkSlotsOffset[
    offsetof(ManagerWorkOwnerPartial, work_slots) == 0x3ad06c ? 1 : -1];

// These wrappers preserve the native call boundaries without claiming that
// their targets use normal C++ member ABIs.
void AdvanceTransitionEaxAbi(TransitionRootPartial *root); // TH10 0x00421e00
void TH10_FASTCALL UpdateInputSlotEcxAbi(i32 slot); // TH10 0x0044a5f0
void FlushRenderOwnerPendingVerticesEsiAbi(RenderOwnerPartial *owner); // 0x442f50

} // namespace th10
