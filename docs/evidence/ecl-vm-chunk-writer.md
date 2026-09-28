# ECL VM chunk writer and context-list manager

Covers TH10 0x0044df20, 0x00450190, 0x004501b0, 0x00450210, 0x00450470,
0x00450500, 0x004505b0, 0x00450690, 0x004506d0, 0x00450700 — module
`src/EclVmChunkWriter.{hpp,cpp}`. These are the producer-side helpers
behind `ExecuteEclInstruction` (0x44e1a0), `BeginEclSubFrame` (0x44df70),
`SpawnEclSubContext` (0x4500d0), `RunEclScriptSetupStackAbi` (0x40dc80),
`ConstructEclScriptObjectEsiStackAbi` (0x40d830) and
`UpdateScriptTestMenuEcxAbi` (0x42bfc0).

Object shapes (all raw-dword based, as in the binary):

* value chunk (run context +8): 0x1000-byte scratch, cursor at +0x1000,
  saved mark at +0x1004; typed stack entries are `{u8 tag, 3 pad, u32
  value}` pairs with the cursor addressing the value dword of the top
  entry.
* context-list manager (0x103c bytes, vtable off_46d0d8): record pointer
  at +4 (head dword at record+0, resolved index at record+4), the table
  owner / script reference at +0x102c, free-running fields at
  +0x1010/+0x1014, sub-context list head at +0x1034.

## 0x00450470 ResolveScriptTableIndexEaxAbi

EAX = table owner (+8 = entry count, +0x8c = sorted
`{const char *name; i32 value}` table), stack = name (ret 4). Binary
search with the native's `count - 1` initial bound (negative counts bail
out) and `(low + high) / 2` midpoints; the byte comparison is the native
two-chars-per-iteration unsigned strcmp sign. On a match the entry's
value + 0x10 is returned (a pointer into the target record past its
header), otherwise 0. Covered callers (EclScriptVm.cpp,
EclEasedTransforms.cpp, EclScriptLibrary.cpp) keep a bundled one-argument
boundary declaration; this module defines the native two-argument shape
used by the entries below.

## 0x00450500 CreateEclContextListStackAbi

EDI = table owner, stack = name, EAX = manager. operator new(0x103c),
vtable off_46d0d8, zeroes +0x1010/+0x1014, stores the owner at +0x102c,
then `record[1] = Resolve(owner, name)` and `record[0] = 0` through the
record pointer read from manager+4. Native quirks preserved: the record
pointer is read straight out of the fresh (uninitialized) allocation,
and the allocation-failure path is not guarded (the native would
dereference the null manager at +0x102c).

## 0x00450700 BindEclContextListEntryEcxEsiAbi

ESI = manager, ECX = name. Re-resolves through the script reference
stored at manager+0x102c and rebinding the same record pair; returns 0.
Called from RunEclScriptSetupStackAbi (0x40e1eb).

## 0x00450210 SetEclContextListRecordEaxStackAbi

EAX = owner, stack = record: `*(owner + 4) = record`. No direct
cross-references remain; registered as a boundary accessor.

## 0x00450190 EclVmOpcode21EaxAbi

EAX = manager. Walks the list head at +0x1034 whose nodes are
`{u32 target; u32 next}` pairs and clears each target's +4 dword. The
native (and this reconstruction) leaves the list head and the nodes
themselves untouched.

## 0x004501b0 EclVmOpcode81HelperEcxEfxAbi

ECX = out pair, stack = {angle, value} (ret 8). fsincos helper used by
opcode 81: `out[0] = cos(angle) * value`, `out[1] = sin(angle) * value`
(st0 = cos is multiplied first and stored to out[0]).

## 0x004505b0 EclVmPushTypedEaxDlStackAbi

EAX = chunk, DL = tag ('i' = 0x69 / 'f' = 0x66), stack = {byte_count,
value} (ret 8). Fails with -1 when the *original* cursor plus
byte_count reaches 0x1000 (signed compare). A non-zero tag writes one
byte at the cursor and then advances the cursor by a full dword — the
overflow check never accounts for those four bytes (native quirk,
preserved). The payload is then memcpy'd at the updated cursor and the
cursor advances by byte_count.

## 0x00450690 EclVmPushIntEaxEcxAbi

EAX = chunk, ECX = advance. Cursor += advance fails with -1 on reaching
0x1000 (signed); otherwise the saved mark is linked at the new cursor
(+4 more cursor movement, guarded by `cursor + 4 < 0x1000`) and the mark
is re-anchored at the previous cursor. Always returns 0. This is the
branch/link primitive behind the 42 branch-emitting sites in
ExecuteEclInstruction.

## 0x004506d0 EclVmPopTopEaxAbi

EAX = chunk. With cursor >= 4 the mark is reloaded from the popped
entry's dword at cursor-4; the cursor is then unconditionally replaced
by the previously saved mark. Always returns 0.

## 0x0044df20 ResolveEclChunkMarkedSlotEcxEaxAbi

ECX = chunk, EDX = offset; returns
`chunk + *(u32 *)(chunk + 0x1004) + offset` — the absolute address of
the marked slot plus the offset. No direct cross-references remain;
registered as a boundary accessor.

## Verification

Reference disassembly `build/reference/0045*.asm` and
`0044df20_sub_44DF20.asm`; call-site context read from
`0044df70_BeginEclSubFrame.asm` (0x44e14c), `0040dc80_*.asm` (0x40e2a9,
0x40e1eb), `0040d830_*.asm` (0x40da17) and
`0042bfc0_UpdateScriptTestMenuEcxAbi.asm` (0x42c13d).
