#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"

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

// Partial layout for g_MainChainContext (TH10 0x00491c28).
// Every named field below is read, written, or passed by address in 0x0041ff80
// or 0x004218d0. The object size and all omitted fields remain unknown.
struct MainChainContext {
    u8 unknown_0000[8];
    D3D9Device *draw_target;
    void *input_root_000c;
    void *input_keyboard_0010;
    void *input_controller_0014;
    u8 unknown_0018[0x30];
    void *window_0048;
    u8 unknown_004c[0x104];
    u32 input_setup_flags_0150;
    u8 unknown_0154[0x118];
    u8 draw_work_026c[0x118];
    void *draw_work_pointer;
    i32 draw_initialized;
    MainChainState previous_state;
    MainChainState requested_state;
    MainChainState transition_source_state;
    i32 transition_flag;
    u8 unknown_03a0[0x30];
    u8 callback_state_byte;
    u8 unknown_03cd[0x26f];
    void *field_063c;
    u8 unknown_0640[8];
    i32 field_0648;
    Win32CriticalSection state_locks[6];
    u8 unknown_06dc[0x18];
    u8 state_update_depths[6];
    u8 unknown_06fa[0x56];
    u32 fog_enabled_cache;
    u32 z_write_enabled_cache;
    u8 unknown_0758[0x10];
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
typedef char AssertMainChainInputFlagsOffset[
    offsetof(MainChainContext, input_setup_flags_0150) == 0x150 ? 1 : -1];
typedef char AssertMainChainDrawWorkOffset[
    offsetof(MainChainContext, draw_work_026c) == 0x26c ? 1 : -1];
typedef char AssertMainChainDrawWorkPointerOffset[
    offsetof(MainChainContext, draw_work_pointer) == 0x384 ? 1 : -1];
typedef char AssertMainChainDrawInitializedOffset[
    offsetof(MainChainContext, draw_initialized) == 0x388 ? 1 : -1];
typedef char AssertMainChainRequestedStateOffset[
    offsetof(MainChainContext, requested_state) == 0x390 ? 1 : -1];
typedef char AssertMainChainCallbackStateByteOffset[
    offsetof(MainChainContext, callback_state_byte) == 0x3cc ? 1 : -1];
typedef char AssertMainChainField63cOffset[
    offsetof(MainChainContext, field_063c) == 0x63c ? 1 : -1];
typedef char AssertMainChainField648Offset[
    offsetof(MainChainContext, field_0648) == 0x648 ? 1 : -1];
typedef char AssertMainChainCriticalSectionOffset[
    offsetof(MainChainContext, state_locks) + 5 * sizeof(Win32CriticalSection) == 0x6c4 ? 1 : -1];
typedef char AssertMainChainStateUpdateDepthOffset[
    offsetof(MainChainContext, state_update_depths) + 5 == 0x6f9 ? 1 : -1];
typedef char AssertMainChainFogEnabledCacheOffset[
    offsetof(MainChainContext, fog_enabled_cache) == 0x750 ? 1 : -1];
typedef char AssertMainChainZWriteEnabledCacheOffset[
    offsetof(MainChainContext, z_write_enabled_cache) == 0x754 ? 1 : -1];
typedef char AssertMainChainField768Offset[
    offsetof(MainChainContext, field_0768) == 0x768 ? 1 : -1];
typedef char AssertMainChainTransitionColorOffset[
    offsetof(MainChainContext, transition_color) == 0x780 ? 1 : -1];

} // namespace th10
