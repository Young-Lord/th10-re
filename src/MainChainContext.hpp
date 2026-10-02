#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"
#include "ThreadControl.hpp"
#include "MainChainRender.hpp"

namespace th10 {

struct D3D9Device;

enum MainChainState {
    MainChainState_Init = 0,
    MainChainState_Unknown1 = 1,
    MainChainState_Unknown2 = 2,
    MainChainState_FailureExit = 3,
    MainChainState_GameManager = 4,
    MainChainState_Unknown5 = 5,
    MainChainState_Unknown6 = 6,
    MainChainState_TitleScreen = 7,
    MainChainState_Unknown8 = 8,
    MainChainState_Unknown9 = 9,
    MainChainState_TitleTransition10 = 10,
    MainChainState_TitleTransition11 = 11,
    MainChainState_TitleTransition12 = 12,
    MainChainState_TitleTransition13 = 13,
    MainChainState_Unknown14 = 14,
    MainChainState_StartGameTransition = 15,
};

enum MainChainAdvanceResult {
    MainChainAdvance_Continue = 1,
    MainChainAdvance_Failed = 4,
};

// Packed BITMAPFILEHEADER + 2 unknown bytes living at context +0x510..0x520.
// The native writes dword fields at unaligned offsets (+2/+0xa), so the
// record must keep 1-byte alignment.
#pragma pack(push, 1)
struct SnapshotBmpFileHeader {
    u16 type;          // +0x00 ('BM')
    u32 size;          // +0x02 (54, later += 0xe1000)
    u32 reserved;      // +0x06
    u32 data_offset;   // +0x0a (54; the native also zeroes a u16 at +0x0c,
                       //   an overlapping write of the offset's high half)
    u8 pad_000e[2];
};
#pragma pack(pop)

typedef char AssertSnapshotBmpFileHeaderSize[
    sizeof(SnapshotBmpFileHeader) == 0x10 ? 1 : -1];

// Partial-but-substantially-complete layout of g_MainChainContext
// (TH10 0x00491c28, 0x784 bytes; native 0x00421f00 memsets exactly 0x784).
// The named camera-work bank is indexed as slot + 0x154 + index*0x118
// (GateVmSlots.cpp / TitleScreenDrawPasses.cpp); the two-state machine
// fields, scheduler state, lock bank and snapshot scratch follow.
struct MainChainContext {
    u8 unknown_0000[8];
    D3D9Device *draw_target;
    void *input_root_000c;
    void *input_keyboard_0010;
    void *input_controller_0014;
    u8 unknown_0018[0x30];
    void *window_0048;
    u8 unknown_004c[0xa0];
    u32 render_mode_selector;      // +0x0ec (22 selects the 24bpp snapshot)
    u8 unknown_00f0[0x4d];
    u8 startup_input_diagnostic;   // +0x13d
    u8 unknown_013e[0x12];
    u32 input_setup_flags_0150;
    MainChainCameraWork camera_work_bank[2]; // +0x154, index 1 = draw work
    void *draw_work_pointer;       // +0x384 (published gate-record pointer)
    i32 draw_initialized;          // +0x388 (published gate-record index)
    MainChainState previous_state;
    MainChainState requested_state;
    MainChainState transition_source_state;
    i32 transition_flag;
    u8 unknown_039c[0x18];
    u32 field_03b4;                // cleared during config load
    u8 unknown_03b8[0x10];
    void *anm_manager_work;        // +0x3c8 (background-VM script bind)
    u8 callback_state_byte;        // +0x3cc
    u8 unknown_03cd[0x13f];
    u32 snapshot_busy;             // +0x50c (writer-thread busy word)
    SnapshotBmpFileHeader snapshot_bmp_header; // +0x510 (packed)
    void *snapshot_bmp_info;       // +0x520 (malloc'd 0x2c BITMAPINFO)
    void *snapshot_pixel_buffer;   // +0x524 (malloc'd 0xe1000 pixels)
    char snapshot_path[0x104];     // +0x528 (output path copy, NUL-ended)
    ThreadControl thread_control;  // +0x62c (update status at +0x1c = 0x648)
    Win32CriticalSection state_locks[7];  // +0x64c..0x6f4
    u8 state_update_depths[7];     // +0x6f4..0x6fb
    u32 background_vm_latch;       // +0x6fc (one-time background-VM spawn)
    u8 unknown_0700[0x50];         // +0x700..0x750
    u32 fog_enabled_cache;         // +0x750
    u32 z_write_enabled_cache;     // +0x754
    u8 unknown_0758[8];
    i32 replay_header_text_remaining; // +0x760
    const char *replay_header_text;   // +0x764
    void *field_0768;
    u8 unknown_076c[0x14];
    u32 transition_color;

    void EnterStateLock(i32 lock_index);
    void LeaveStateLock(i32 lock_index);
    MainChainAdvanceResult AdvanceState();

    static i32 TH10_FASTCALL Update(MainChainContext *context);
    static i32 TH10_FASTCALL DrawInitialize(MainChainContext *context);
    static i32 TH10_FASTCALL DrawContinue(MainChainContext *context);
    static i32 TH10_FASTCALL DrawFinalize(MainChainContext *context);
};

i32 RegisterMainChainCallbacks();

typedef char AssertMainChainSize[sizeof(MainChainContext) == 0x784 ? 1 : -1];
typedef char AssertMainChainPreviousStateOffset[
    offsetof(MainChainContext, previous_state) == 0x38c ? 1 : -1];
typedef char AssertMainChainDrawTargetOffset[
    offsetof(MainChainContext, draw_target) == 0x8 ? 1 : -1];
typedef char AssertMainChainInputRootOffset[
    offsetof(MainChainContext, input_root_000c) == 0xc ? 1 : -1];
typedef char AssertMainChainInputKeyboardOffset[
    offsetof(MainChainContext, input_keyboard_0010) == 0x10 ? 1 : -1];
typedef char AssertMainChainInputControllerOffset[
    offsetof(MainChainContext, input_controller_0014) == 0x14 ? 1 : -1];
typedef char AssertMainChainWindowOffset[
    offsetof(MainChainContext, window_0048) == 0x48 ? 1 : -1];
typedef char AssertMainChainRenderModeOffset[
    offsetof(MainChainContext, render_mode_selector) == 0xec ? 1 : -1];
typedef char AssertMainChainInputFlagsOffset[
    offsetof(MainChainContext, input_setup_flags_0150) == 0x150 ? 1 : -1];
typedef char AssertMainChainCameraWorkBankOffset[
    offsetof(MainChainContext, camera_work_bank) == 0x154 ? 1 : -1];
typedef char AssertMainChainCameraWorkBank1Offset[
    offsetof(MainChainContext, camera_work_bank) + sizeof(MainChainCameraWork)
        == 0x26c ? 1 : -1];
typedef char AssertMainChainDrawWorkPointerOffset[
    offsetof(MainChainContext, draw_work_pointer) == 0x384 ? 1 : -1];
typedef char AssertMainChainDrawInitializedOffset[
    offsetof(MainChainContext, draw_initialized) == 0x388 ? 1 : -1];
typedef char AssertMainChainRequestedStateOffset[
    offsetof(MainChainContext, requested_state) == 0x390 ? 1 : -1];
typedef char AssertMainChainAnmManagerWorkOffset[
    offsetof(MainChainContext, anm_manager_work) == 0x3c8 ? 1 : -1];
typedef char AssertMainChainCallbackStateByteOffset[
    offsetof(MainChainContext, callback_state_byte) == 0x3cc ? 1 : -1];
typedef char AssertMainChainSnapshotBusyOffset[
    offsetof(MainChainContext, snapshot_busy) == 0x50c ? 1 : -1];
typedef char AssertMainChainSnapshotPathOffset[
    offsetof(MainChainContext, snapshot_path) == 0x528 ? 1 : -1];
typedef char AssertMainChainThreadControlOffset[
    offsetof(MainChainContext, thread_control) == 0x62c ? 1 : -1];
typedef char AssertMainChainThreadStatusOffset[
    offsetof(MainChainContext, thread_control) + 0x1c == 0x648 ? 1 : -1];
typedef char AssertMainChainField648Offset[
    offsetof(MainChainContext, thread_control.update_status_001c) == 0x648
        ? 1 : -1];
typedef char AssertMainChainCriticalSectionOffset[
    offsetof(MainChainContext, state_locks) + 5 * sizeof(Win32CriticalSection) == 0x6c4 ? 1 : -1];
typedef char AssertMainChainCriticalSection7Offset[
    offsetof(MainChainContext, state_locks) + 7 * sizeof(Win32CriticalSection) == 0x6f4 ? 1 : -1];
typedef char AssertMainChainStateUpdateDepthOffset[
    offsetof(MainChainContext, state_update_depths) + 5 == 0x6f9 ? 1 : -1];
typedef char AssertMainChainStateUpdateDepth7Offset[
    offsetof(MainChainContext, state_update_depths) + 7 == 0x6fb ? 1 : -1];
typedef char AssertMainChainBackgroundVmLatchOffset[
    offsetof(MainChainContext, background_vm_latch) == 0x6fc ? 1 : -1];
typedef char AssertMainChainFogEnabledCacheOffset[
    offsetof(MainChainContext, fog_enabled_cache) == 0x750 ? 1 : -1];
typedef char AssertMainChainZWriteEnabledCacheOffset[
    offsetof(MainChainContext, z_write_enabled_cache) == 0x754 ? 1 : -1];
typedef char AssertMainChainReplayHeaderTextOffset[
    offsetof(MainChainContext, replay_header_text) == 0x764 ? 1 : -1];
typedef char AssertMainChainField768Offset[
    offsetof(MainChainContext, field_0768) == 0x768 ? 1 : -1];
typedef char AssertMainChainTransitionColorOffset[
    offsetof(MainChainContext, transition_color) == 0x780 ? 1 : -1];

} // namespace th10
