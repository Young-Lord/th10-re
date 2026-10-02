#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "GameManagerObject.hpp"
#include "ThreadControl.hpp"
#include "VmRecord.hpp"

namespace th10 {

struct ChainElem;

// ---------------------------------------------------------------------------
// The stage host object (TH10 DAT_004776f8, source alias g_StageHostObject),
// 0x688 bytes. Allocated by ::operator new(0x688) in CreateStageHostObject
// (TH10 0x0040a3c0), field-seeded + full-wiped by InitStageHostObjectEdxAbi
// (0x0040a040, which also publishes the global), destroyed by the composite
// destructor DestroyStageHostObjectStackAbi (0x0040a1a0, stack arg, ret 4).
//
// It hosts the ECL-select / stage-select front-end: three ManagerCursorRecords
// (mode select / stage select / ECL entry select), a 0x3ac embedded animation
// record drawn by DrawAsciiAnimationVmUnscaled (0x0040abb7), and an embedded
// ThreadControl whose timeline continuation (0x0040a340) re-enters
// EnterGameModeSetupEsiAbi when the game mode boots.
//
// Scheduler: calc node (fn 0x0040abe0, priority 5) at +0x8, draw node
// (fn 0x0040abf0, priority 0x27) at +0xc; both freed via 0x00449f60.
// ---------------------------------------------------------------------------

struct StageHostObject {
    u32 flags_0000;                 // +0x000 (bit 1 set at publish, 0x40a118)
    u32 field_0004;
    ChainElem *calc_element;        // +0x008 (calc chain, priority 5)
    ChainElem *draw_element;        // +0x00c (draw chain, priority 0x27)
    ThreadControl thread_control;   // +0x010 (vtable off_4703e4; continuation
                                    //   sub_40A340 registered at 0x40a79d)
    u32 state_machine;              // +0x030 (0 init scan / 1 select / 2
                                    //   entering game / 3 exit cleanup / 4 in
                                    //   game; switches 0x40a46d + 0x40a981)
    char **stage_table;             // +0x034 malloc'd array of ECL filename
                                    //   pointers (malloc(4*count) 0x40a801;
                                    //   read as stage_table[index] 0x40a377)
    u32 stage_table_count;          // +0x038 ("File not found." gate 0x40aa0f)
    ManagerCursorRecord cursor_a;   // +0x03c..0x114 mode/config select cursor
                                    //   (wrap modulus seeded 999, forced to 3
                                    //   in state 0 at 0x40a870)
    ManagerCursorRecord cursor_b;   // +0x114..0x1ec stage/ECL-script select
                                    //   cursor (wrap modulus = file count,
                                    //   0x40a89a)
    ManagerCursorRecord cursor_c;   // +0x1ec..0x2c4 ECL-entry select cursor
                                    //   (wrap modulus seeded from the
                                    //   conditional-state name-registry count
                                    //   at 0x40a396)
    u32 field_02c4;                 // +0x2c4 (4-byte gap before the VM record)
    VmRecord animation_vm;          // +0x2c8..0x674 (0x3ac record; 320.0f at
                                    //   +0x5fc / 240.0f at +0x600 / dead u16
                                    //   0xFFFF at +0x64c; 9 busy flags bit 0)
    float position_display_x;       // +0x674 (=0 in state 0 at 0x40a8f3)
    float unknown_0678;             // +0x678 (float 32.0f seed, 0x40a8f9)
    u32 field_067c;                 // +0x67c (=0)
    u32 field_0680;                 // +0x680 (=1000)
    u8 flags_0684;                  // +0x684 bit 1 tested in draw (0x40ab77),
                                    //   cleared in update (0x40a797)
    u8 unknown_0685[3];
};

// ALIASING (deliberate raw): the destructor frees the malloc'd buffer pointer
// stored at host+0x620, which lies INSIDE animation_vm (VmRecord+0x358). The
// dtor (0x0040a2ee/0x40a303) therefore must keep raw access there; do not
// "fix" it into the VmRecord view.

typedef char AssertStageHostObjectSize[
    sizeof(StageHostObject) == 0x688 ? 1 : -1];
typedef char AssertStageHostObjectCalcOffset[
    offsetof(StageHostObject, calc_element) == 0x8 ? 1 : -1];
typedef char AssertStageHostObjectThreadControlOffset[
    offsetof(StageHostObject, thread_control) == 0x10 ? 1 : -1];
typedef char AssertStageHostObjectStateMachineOffset[
    offsetof(StageHostObject, state_machine) == 0x30 ? 1 : -1];
typedef char AssertStageHostObjectStageTableOffset[
    offsetof(StageHostObject, stage_table) == 0x34 ? 1 : -1];
typedef char AssertStageHostObjectCursorAOffset[
    offsetof(StageHostObject, cursor_a) == 0x3c ? 1 : -1];
typedef char AssertStageHostObjectCursorBOffset[
    offsetof(StageHostObject, cursor_b) == 0x114 ? 1 : -1];
typedef char AssertStageHostObjectCursorCOffset[
    offsetof(StageHostObject, cursor_c) == 0x1ec ? 1 : -1];
typedef char AssertStageHostObjectAnimationVmOffset[
    offsetof(StageHostObject, animation_vm) == 0x2c8 ? 1 : -1];
typedef char AssertStageHostObjectPositionDisplayOffset[
    offsetof(StageHostObject, position_display_x) == 0x674 ? 1 : -1];
typedef char AssertStageHostObjectFlags684Offset[
    offsetof(StageHostObject, flags_0684) == 0x684 ? 1 : -1];

} // namespace th10
