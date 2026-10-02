#pragma once

#include <stddef.h>

#include "Th10Types.hpp"
#include "GameContext.hpp"

namespace th10 {

struct ChainElem;

// ---------------------------------------------------------------------------
// The ASCII HUD conditional state object (TH10 DAT_00477704; source alias
// g_AsciiHudConditionalState), 0x68 bytes. Created by the creator 0x0040d6b0
// (memset 0x68 then |= 2), sub-record initializer 0x0040d280, destructor
// 0x0040d530 (EAX), release wrapper 0x0040d730 (ESI), sub-block release /
// list delete 0x00409f90.
//
// It carries two parallel per-ECL-record index spaces keyed by fields of the
// ECL script record:
//   - published_ids (+0x10), indexed by record+0x248c — slot 0 doubles as the
//     primary stage/battle ECL script record pointer (nonzero = active);
//   - resource_table (+0x30), indexed by record+0x244c — entry 0 is the
//     effect-manager pool word *(DAT_004776f0 + 0x3e0b50), later entries are
//     the per-ANIM-chunk ManagerWork pointers written unboundedly by the
//     parser (0x0040d447, count comes from the file — native quirk).
// Both tables are indexed unboundedly natively, so accesses at index >= the
// modeled array bounds stay RAW in the reconstruction.
// ---------------------------------------------------------------------------

struct ConditionalState {
    u32 flags_0000;                // +0x000 (bit 1 set by the creator)
    u32 field_0004;                // +0x004 (never referenced; 0 from wipe)
    ChainElem *calc_element;       // +0x008 (frame ticker 0x0040d810, calc
                                   //   chain priority 0x12; rearm flag bit 1)
    ChainElem *draw_element;       // +0x00c (no-op 0x0040d820, priority 0x14)
    u32 published_ids[8];          // +0x010..0x2f indexed by record+0x248c;
                                   //   slot 0 = primary stage/battle ECL
                                   //   record pointer (0x405a53, 0x40910b...)
    void *resource_table[4];       // +0x030..0x3f indexed by record+0x244c;
                                   //   entry 0 = effect pool word, entry 2
                                   //   (+0x38) = battle/base resource used by
                                   //   spell-practice VM init (0x409964,
                                   //   0x416148, 0x416b91)
    TimerNode frame_timer;         // +0x040..0x53 (latch/count/accum/rate ->
                                   //   g_FrameTimeScale 0x476f78 / flags bit
                                   //   0 gates first-time seed; tick 0x40d750)
    void *name_registry_0054;      // 0x1098-byte stage-script name registry /
                                   //   viewer (vtable 0x46d0b4; destructing
                                   //   vtable 0x46d0f0 planted at teardown)
    void *script_list_head_0058;   // nodes embedded in ECL records at
                                   //   record+0x116c (next +0x1170, prev
                                   //   +0x1174)
    void *script_list_tail_005c;
    u32 script_count_0060;         // ++ on create, -- on teardown; public
                                   //   "stage script running" gate (0x425172,
                                   //   0x42626e)
    u32 aux_count_0064;            // incremented with +0x60, never
                                   //   decremented (native quirk); cleared by
                                   //   0x00409f90
};

// The +0x54 sub-object (stage-script name registry, 0x1098 bytes): three
// virtuals at vtable 0x46d0b4 recurse over each stage script file —
// 0x00450220 RegisterScriptFileNames (SNPT chunk) → 0x0040d400
// ParseAnimEcliSections ('ANIM'/'ECLI') → 0x0040cd20 LoadStageScriptFile.

struct ConditionalNameRegistry {
    void *vtable_0000;
    u32 loaded_script_count_0004;  // next file-slot index
    u32 name_entry_count_0008;     // total name-table entries (seeds the
                                   //   ECL-select cursor modulus, 0x40a386)
    void *file_data_000c[32];      // +0x0c..0x88 loaded file data pointers
                                   //   (freed by CRT free in 0x0040d530)
    void *name_table_008c;         // +0x8c malloc(8*count) table of 8-byte
                                   //   entries {const char *name; void
                                   //   *file_data}, kept sorted by strcmp;
                                   //   consumers read entry+0 ("Ecl %s" debug
                                   //   text 0x40a940) and pass the NAME
                                   //   POINTER (not an id) to
                                   //   CreateEclScriptObject
    u8 unknown_0090[0x1008];
};

typedef char AssertConditionalStateSize[
    sizeof(ConditionalState) == 0x68 ? 1 : -1];
typedef char AssertConditionalStateCalcOffset[
    offsetof(ConditionalState, calc_element) == 0x8 ? 1 : -1];
typedef char AssertConditionalStatePublishedIdsOffset[
    offsetof(ConditionalState, published_ids) == 0x10 ? 1 : -1];
typedef char AssertConditionalStateResourceTableOffset[
    offsetof(ConditionalState, resource_table) == 0x30 ? 1 : -1];
typedef char AssertConditionalStateTimerOffset[
    offsetof(ConditionalState, frame_timer) == 0x40 ? 1 : -1];
typedef char AssertConditionalStateRegistryOffset[
    offsetof(ConditionalState, name_registry_0054) == 0x54 ? 1 : -1];
typedef char AssertConditionalStateListHeadOffset[
    offsetof(ConditionalState, script_list_head_0058) == 0x58 ? 1 : -1];
typedef char AssertConditionalStateScriptCountOffset[
    offsetof(ConditionalState, script_count_0060) == 0x60 ? 1 : -1];
typedef char AssertConditionalNameRegistrySize[
    sizeof(ConditionalNameRegistry) == 0x1098 ? 1 : -1];
typedef char AssertConditionalNameRegistryTableOffset[
    offsetof(ConditionalNameRegistry, name_table_008c) == 0x8c ? 1 : -1];

} // namespace th10
