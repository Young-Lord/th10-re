# DispatchSetupOpcode Jump-Table Re-key Verification

Module: `src/TimelineRenderObjectSetup.cpp` (`DispatchSetupOpcode`), binary
`0x0043ee30` (`FinalizeTimelineRenderObjectSetup`), jump table `0x4413a4`.

## Dispatch formula (from disassembly)

```text
0043eea4: MOV EBX,[EBP+0x390]        ; pc = current setup record
0043eeaa: MOVSX EDX,word[EBX+4]      ; limit field
0043eeae: CMP EDX,[EBP+0x60]; JG 0x440e42   ; done -> epilogue
0043eeb7: MOVSX EAX,word[EBX]        ; opcode (signed 16-bit)
0043eeba: INC EAX
0043eebb: CMP EAX,0x5d ; JA 0x43f532 ; out-of-range -> default
0043eec4: JMP [EAX*4 + 0x4413a4]     ; table[(opcode + 1)]
```

Therefore the authoritative handler of opcode `N` (-1 .. 0x5c) is the dword
at `0x4413a4 + (N+1)*4`. The table (read from `resources/th10.exe`) and the
C++ `switch` in `DispatchSetupOpcode` agree on every case checked (see
below). Opcodes `0x00` and `0x40`, plus anything above `0x5c`, share the
default handler `0x43f532`.

## Verification result: no re-key required

The ported switch already uses the binary opcode numbers. A live pass over
the decompiled switch (`0x43ee30`, exported locally) confirmed the following
case numbers match their binary handlers one-to-one (offsets and helper
calls equivalent):

- termination cluster: `0xffff` / `1` (clear flag bit 0, stop), `2` (stop)
- int/float set/add/sub/mul/div/fmod/min block: `3` .. `0x2f`
- vector/flag/anim-start block: `0x30` .. `0x51`
- flag/polyline/spawn block: `0x52` .. `0x5c`

Notable samples verified against the table bytes:

- binary `0x3b` is the vec3 animation start for block `+0x134` (guard
  `+0x178`), which is exactly C++ `case 0x3b` `StartVec3Anim(...,0x178,
  0x134,...)`.
- binary `0x33`/`0x34` are the byte writes to `+0x2ff` and
  `+0x2fe/+0x2fd/+0x2fc`; binary `0x35`/`0x36` are the rotation/scale float
  vector writes (`+0x30/0x34/0x38` and `+0x44/0x48`).
- binary `0x58`/`0x5a`/`0x5b`/`0x5c` are the four spawn variants
  (`0x448d00`, `0x448f60`, `0x448e30`, `0x449090`), matching the four C++
  spawn cases; the copy tails are identical six-dword copies (the C++
  `copy_all_three` parameter is deliberately ignored).

The earlier notes in `docs/evidence/vm-leaf-helpers.md` that "the opcode
ids diverge" were based on an incomplete table read and are now withdrawn.

## Body-level resolution (2026-09-03)

The four native setup-spawn creators were implemented and wired into the
switch: `0x448d00`/`0x448e30`/`0x448f60`/`0x449090` become
`SpawnSetupEffectVmListABack`/`ListAFront`/`ListBBack`/`ListBFront`
(`src/TimelineRenderObjects.cpp`). Each allocates a 0x3ac pool VM record
via the global render owner (`DAT_00491c10`), writes kind at +0x20,
sets `+0x35c |= 0x40000000`, binds the setup script (`0x449870`
semantics), then registers the record through the matching list helper
(list A/B, tail/front). The native first stack argument is dead (`ret
0xc` ABI; the manager is the global, not an argument). `DispatchSetupOpcode`
spawn cases now route: opcode `0x58`→A-back, `0x5a`→B-back, `0x5b`→A-front,
`0x5c`→B-front, matching the binary handlers.

Remaining attribution item: `CreateTimelineObject` (the owner-node +
manager-work clone helper still used by the record interpreter opcode 8 /
15..17) is no longer claimed to be `0x448d00`; its true native creator
address needs a separate xref audit of `TimelineRecordInterpreter.cpp`.
