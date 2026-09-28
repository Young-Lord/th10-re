#pragma once

#include "Th10Types.hpp"

namespace th10 {

// ---------------------------------------------------------------------------
// ECL VM value-chunk writer and script-name table lookup.
//
// The chunk is a 0x1000-byte scratch area owned by the run context
// (ctx+8) with a cursor at +0x1000 and a saved mark at +0x1004; the
// 0x103c-byte context-list manager embeds the script reference at
// +0x102c, the current record pointer at +4 and a sub-context list head
// at +0x1034. Typed stack entries are {u8 tag, 3 pad, u32 value} pairs.
// ---------------------------------------------------------------------------

// TH10 0x00450470. Native EAX = table owner (its +8 holds the entry count,
// +0x8c the sorted {const char *name; i32 value} table), stack = the
// requested name (ret 4). Binary search over the name table; returns the
// matched entry's value + 16 (an instruction-stream pointer), or 0 when
// the name is not present.
i32 ResolveScriptTableIndexEaxAbi(i32 table_owner, const char *name);

// TH10 0x00450500. Native EDI = table owner, stack = name (ret 4), EAX =
// the new manager. Allocates the 0x103c-byte context-list manager
// (vtable off_46d0d8), zeroes +0x1010/+0x1014 and captures the owner at
// +0x102c, then initializes the record at +4 through
// ResolveScriptTableIndexEaxAbi and zeroes the record head. Native quirk:
// the record pointer at +4 is read straight out of the fresh allocation
// and the allocation-failure path dereferences the null manager anyway.
void *CreateEclContextListStackAbi(void *table_owner, const char *name);

// TH10 0x00450700. Native ESI = manager, ECX = name. Rebinds the
// manager's record through ResolveScriptTableIndexEaxAbi using the script
// reference stored at manager +0x102c; always returns 0.
i32 BindEclContextListEntryEcxEsiAbi(void *manager, const char *name);

// TH10 0x00450210. Native EAX = owner, stack = record (ret 4). Stores the
// record pointer at owner +4. No direct cross-references remain in the
// retail binary; kept as a boundary accessor.
void SetEclContextListRecordEaxStackAbi(void *owner, void *record);

// TH10 0x00450190. Native EAX = context-list manager. Walks the
// sub-context list head at manager +0x1034 and clears each node's target
// object's +4 dword. The native never clears the list head itself.
void EclVmOpcode21EaxAbi(void *manager);

// TH10 0x004501b0. Native ECX = out pair, stack = {angle, value} (ret 8).
// fsincos helper used by ECL opcode 81: out[0] = cos(angle) * value,
// out[1] = sin(angle) * value.
void EclVmOpcode81HelperEcxEfxAbi(float out_pair[2], float angle,
                                  float value);

// TH10 0x004505b0. Native EAX = chunk, DL = type tag ('i'/0x69 or
// 'f'/0x66), stack = {u32 byte_count, const void *value} (ret 8). Appends
// an optional tag byte (the cursor then advances a full dword — native
// quirk) and a byte_count-sized copy, failing with -1 when the *original*
// cursor plus byte_count would reach 0x1000 (the tag dword is not
// accounted for in the overflow check).
i32 EclVmPushTypedEaxDlStackAbi(void *chunk, u8 type_tag, u32 byte_count,
                                const void *value);

// TH10 0x00450690. Native EAX = chunk, ECX = advance. Moves the cursor
// forward by `advance` (failing with -1 when it would reach 0x1000),
// links the saved mark at the new cursor (+4 more) and re-anchors the
// mark at the previous cursor. Always returns 0.
i32 EclVmPushIntEaxEcxAbi(void *chunk, i32 advance);

// TH10 0x004506d0. Native EAX = chunk. Pops the top stack entry: with
// cursor >= 4 the mark is reloaded from the entry dword at cursor-4,
// then the cursor is replaced by the saved mark. Always returns 0.
i32 EclVmPopTopEaxAbi(void *chunk);

// TH10 0x0044df20. Native ECX = chunk, EDX = offset. Returns the absolute
// address of the marked slot plus the offset:
// chunk + *(u32 *)(chunk + 0x1004) + offset. No direct cross-references
// remain in the retail binary; kept as a boundary accessor.
void *ResolveEclChunkMarkedSlotEcxEaxAbi(void *chunk, i32 offset);

} // namespace th10
