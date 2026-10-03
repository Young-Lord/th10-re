#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0040cfb0. Native register ABI: EAX = source descriptor (three
// leading dwords copied to record+0x1094 (= work.anchor1_pos_0058[0..2]),
// dwords 3/4/5 to record+0x23f8 (= work.death_score_13bc) / +0x2408
// (= work.kind_13cc) / +0x23fc (= work.hp_13c0), bit 0 of dword 6 into
// +0x2480 (= work.flags_1444) bit 0x800, bit 0 of dword 7 into +0x2480
// bit 0x40000, and 0x20 bytes at descriptor+0x20 copied to record+0x1138
// (= work.descriptor_vars_00fc[8])), two stack arguments (ret 8): the list
// owner receiving the record node at +0x58/+0x5c/+0x60/+0x64 and the ctor
// argument forwarded to the 0x40d830 constructor. Allocates the
// 0x2518-byte ECL script object (vtable 0x46d0c0 installed by the
// constructor; the working sub-record lives at +0x103c) and returns it in
// EAX; a failed allocation keeps writing through the null pointer (native
// quirk, preserved).
void *CreateEclScriptObjectEaxStackAbi(const u32 *descriptor,
                                       void *list_owner, i32 ctor_arg);

// TH10 0x0040d830 (boundary). Native userpurge: ESI = record, stack arg =
// ctor argument; installs the 0x46d0c0 vtable and initializes the record
// (including the 0x14dc-byte memset of the working sub-record at +0x103c).
void *ConstructEclScriptObjectEsiStackAbi(void *record, i32 ctor_arg);

// TH10 0x0040dc80 (implemented in this module). Native ABI: one stack
// argument (ret 4) = the +0x103c working sub-record of an ECL script object
// (th10::EclScriptWork; both native callers push record+0x103c:
// `lea ecx,[ebp+103Ch]` at 0x40d0b3 and `add eax,103Ch` at 0x40d771); the
// typed reconstruction works on an EclScriptWork view so every field access
// below is sub-record-relative. Runs the per-frame ECL
// enemy update: re-arms the run gate (flag bit 0x400 at flags_1444),
// refreshes the working block from the base_pos_002c base block, ticks the
// four vec2 angle/
// radius animation blocks, integrates the three motion blocks, clamps the
// base position, runs the item-collision damage pass, the script-bind path,
// the direction state machine, entity position publication, the flicker
// state, and the frame counter advance. Returns 0 normally, -1 on the abort
// paths (off-screen without the 0x4 stay flag, failed entity-list scan) and
// 1 after the enemy-death sequence.
i32 RunEclScriptSetupStackAbi(void *sub_record);

} // namespace th10
