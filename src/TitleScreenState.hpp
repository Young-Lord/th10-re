#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "VmRecord.hpp"
#include "GameContext.hpp"
#include "MainChainRender.hpp"

namespace th10 {

struct ChainElem;

// ---------------------------------------------------------------------------
// The title-screen state object (0x2b64 bytes — the recurring "0x2a78" in
// older comments is stale; the real size is pinned by ::operator new(0x2b64)
// in CreateTitleScreenVmHostStackAbi 0x00426240, the 0xad9-dword wipe in
// InitTitleScreenVmHostStackAbi 0x00421660, and the camera snapshot ending
// exactly at +0x2b64). Two instances exist, published to TH10 DAT_004776e4
// (primary) and DAT_004776e8 (secondary) by the ctor 0x00426230.
//
// +0x180 holds 8 background VM records and +0x1f08 three aux VM records
// (eh-vector lifecycle). The 0x118 camera-work snapshot at +0x2a4c doubles
// as live camera state while the background script writes its vec3s into
// the eye/unknown_001c/target areas before the epilogue copies it back to
// g_AsciiCameraWork (DAT_00491d7c).
// ---------------------------------------------------------------------------

struct TitleScreenState {
    u8 unknown_0000[8];
    ChainElem *calc_element;           // +0x008 (0x403050 -> 0x402720)
    ChainElem *draw_element;           // +0x00c (0x403060 -> 0x402850)
    u8 *stage_script_buffer;           // +0x010 (s16 [buf] = stage count)
    u8 *script_pointer_table;          // +0x014 (= buffer + 0x90)
    u32 script_base;                   // +0x018 (buffer + table dword + 4)
    u32 script_base_2;                 // +0x01c
    u32 wave_mode;                     // +0x020 (script opcode 12)
    u8 unknown_0024[0x14];
    TimerNode wait_timer;              // +0x038 (prev NaN sentinel)
    u8 *script_cursor;                 // +0x04c (advances by i16 record strides)
    float interp_a_start[3];           // +0x050
    float interp_a_end[3];             // +0x05c
    u8 unknown_0068[0xc];
    float interp_a_extra[3];           // +0x074
    TimerNode interp_a_timer;          // +0x080
    u32 interp_a_gate;                 // +0x094 (nonzero = run)
    u32 interp_a_mode;                 // +0x098
    float interp_b_start[3];           // +0x09c (snapshot of live +0x2a4c)
    float interp_b_end[3];             // +0x0a8
    float interp_b_gate[3];            // +0x0b4
    float interp_b_third[3];           // +0x0c0
    TimerNode interp_b_timer;          // +0x0cc
    u32 interp_b_gate_flag;            // +0x0e0
    u32 interp_b_record_hi;            // +0x0e4
    u8 color_track_state[0x1c];        // +0x0e8 (7-dword TickColorTrack input)
    u8 color_track_scratch[0x1c];      // +0x104 (0x405040 output copy)
    u8 unknown_0120[0x38];
    TimerNode color_track_timer;       // +0x158
    u32 color_track_gate;              // +0x16c
    u32 color_track_record;            // +0x170
    u8 unknown_0174[4];
    void *anm_manager_work;            // +0x178 ("title.anm" manager work)
    u8 *vm_heap_array;                 // +0x17c (i16 base[2] x 0x3ac)
    VmRecord background_vms[8];        // +0x180..0x1ee0
    float background_fade;             // +0x1ee0 (9610000.0f scalar)
    u32 idle_frame_latch;              // +0x1ee4
    u32 modulation_color;              // +0x1ee8 (0x808080 / 0xffffff; the
                                       //   +0x1eeb enable byte is the high
                                       //   byte of this dword)
    u32 aux_vm_arm_latch;              // +0x1eec (forces 1.0 rate)
    u8 unknown_1ef0[0x18];
    VmRecord aux_vms[3];               // +0x1f08..0x2a0c
    u32 scene_counter;                 // +0x2a0c (cleared by draw pass 0)
    u32 scene_counter_clear;           // +0x2a10 (write-only clear)
    u32 op_counter;                    // +0x2a14
    u32 master_flags;                  // +0x2a18 (bit0 fade-in/bg gate, bit1
                                       //   score-anim-30, bit2 fade active,
                                       //   bit3 shutdown/draw-off)
    TimerNode score_anim_timer;        // +0x2a1c (accum seeds 30.0f/60.0f)
    u32 scene_mode_copy;               // +0x2a30 (copy of DAT_00474c7c)
    u32 intro_counter;                 // +0x2a34
    u8 unknown_2a38[8];
    ChainElem *draw_pass1_element;     // +0x2a40 (0x403070 -> 0x402ca0)
    u8 *file_buffer;                   // +0x2a44 (whole-file script load)
    u32 file_size;                     // +0x2a48
    MainChainCameraWork camera_snapshot; // +0x2a4c..0x2b64 (mirrors
                                       //   g_AsciiCameraWork; the background
                                       //   script's live vec3 writes overlap
                                       //   the eye/unknown_001c/target areas)
};

typedef char AssertTitleScreenStateSize[
    sizeof(TitleScreenState) == 0x2b64 ? 1 : -1];
typedef char AssertTitleScreenStateBackgroundVmsOffset[
    offsetof(TitleScreenState, background_vms) == 0x180 ? 1 : -1];
typedef char AssertTitleScreenStateBackgroundFadeOffset[
    offsetof(TitleScreenState, background_fade) == 0x1ee0 ? 1 : -1];
typedef char AssertTitleScreenStateAuxVmsOffset[
    offsetof(TitleScreenState, aux_vms) == 0x1f08 ? 1 : -1];
typedef char AssertTitleScreenStateAuxVmsEndOffset[
    offsetof(TitleScreenState, scene_counter) == 0x2a0c ? 1 : -1];
typedef char AssertTitleScreenStateMasterFlagsOffset[
    offsetof(TitleScreenState, master_flags) == 0x2a18 ? 1 : -1];
typedef char AssertTitleScreenStateScoreAnimTimerOffset[
    offsetof(TitleScreenState, score_anim_timer) == 0x2a1c ? 1 : -1];
typedef char AssertTitleScreenStateSceneModeCopyOffset[
    offsetof(TitleScreenState, scene_mode_copy) == 0x2a30 ? 1 : -1];
typedef char AssertTitleScreenStateDrawPass1Offset[
    offsetof(TitleScreenState, draw_pass1_element) == 0x2a40 ? 1 : -1];
typedef char AssertTitleScreenStateFileBufferOffset[
    offsetof(TitleScreenState, file_buffer) == 0x2a44 ? 1 : -1];
typedef char AssertTitleScreenStateCameraSnapshotOffset[
    offsetof(TitleScreenState, camera_snapshot) == 0x2a4c ? 1 : -1];
typedef char AssertTitleScreenStateWaitTimerOffset[
    offsetof(TitleScreenState, wait_timer) == 0x38 ? 1 : -1];
typedef char AssertTitleScreenStateScriptCursorOffset[
    offsetof(TitleScreenState, script_cursor) == 0x4c ? 1 : -1];
typedef char AssertTitleScreenStateAnmWorkOffset[
    offsetof(TitleScreenState, anm_manager_work) == 0x178 ? 1 : -1];
typedef char AssertTitleScreenStateVmHeapArrayOffset[
    offsetof(TitleScreenState, vm_heap_array) == 0x17c ? 1 : -1];

} // namespace th10
