# Title background script interpreter (0x403c80)

Reconstruction in `src/TitleBackgroundScript.cpp/.hpp`. Verified against a
fresh IDA decompile (session `th10re`); 0x403c80 is the scripted
title-screen background driver whose stream the state machine walks.

## State block

The 0x2c00-ish state carries: the frame-count timer triple (+0x38 prev /
+0x3c count / +0x40 accumulator, pointer-rate layout with flag bit 0),
the +0x4c stream cursor, the +0xcc/+0x80 interpolator timers, target
vectors at +0x50/+0x5c/+0x9c/+0xa8/+0xe0, live positions at +0x2a4c/
+0x2a58/+0x2a64, scalars +0x2a94, color floats +0x2b50..+0x2b5c, the
color dword +0x2b60, the delta vector +0x2b3c, and the pause-snapshot
window at +0x2a4c..(+0x46 dwords).

## Interpreter

Loops while the cursor opcode stream (word at cursor+4) is within the
+0x3c frame budget; opcodes:

- 1: wait-N — arms the +0x38 timer with the payload frame count, advances
  the cursor by the payload size (+8) instead of the default +6.
- 2: position A — the live +0x2a4c vector moves to +0x2b3c (as the delta:
  new − old is stored over the old slot), the record vec3 becomes the new
  +0x2a4c.
- 3: position B — record vec2 → +0xe0, vec3 → +0xa8, live +0x2a4c →
  +0x9c; the +0xcc timer is lazily initialized and force-armed to -1.
- 4 / 6: record vec3 → +0x2a58 / +0x2a64.
- 5: interpolator A — flags +0x94/+0x98 from the record; +0x50 ← live
  +0x2a58; +0x5c ← record vec3; +0x80 timer armed.
- 7: dword → +0x2a94.
- 8: color — dword payload → +0x2b60 and its four bytes as floats into
  +0x2b50..+0x2b5c.
- (remaining opcodes follow the same timer/interpolator/palette pattern;
  see the source for the full table, including the 0x405040 ramp helper
  filling the 7-dword scratch copied to +0x104).

After the loop: the pause-state snapshot copies 0x46 dwords from +0x2a4c
into the global snapshot block (DAT_00491d7c), clears +0x1eec, advances
the +0x2a34 frame counter and returns 1.

## Declared boundaries

0x403990 (background pre-update), 0x405040 (ramp/palette fill), 0x404f30
variant (BindTitleScriptVmEsiStackAbi), and the D3DXVec3Normalize import.
`0x4049a0` (color-track tick, ECX block + 7-dword scratch out) is now the
semantic `TickColorTrack` in `src/VmLeafHelpers.cpp/.hpp`; the call site
copies the seven result dwords to `st+0x2b48` (see
`docs/evidence/vm-leaf-helpers.md`).

## 2026-09-14 additions: 0x403850 load step and 0x402720 verification

`0x403850` is now implemented as `LoadTitleBackgroundScriptEbxStackAbi`
(native EBX = state, stack = filename; the filename travels in ECX from the
0x402640 factory through 0x402230, and in EAX through the 0x403080
scheduler thunk). Body: first-entry path append into the global 0x497c38
buffer, 0x44b360 load into +0x2a44/+0x2a48, byte copy into a CRT buffer at
+0x10, `RequestManagerWork(DAT_00491c10, (state+0x2a30 & 1)+4, buffer+0x10)`
into +0x178 (failure appends the 0x46cbc0 "stage data not found" line via
0x44b810 and returns -1), pointer-table rebase (+0x14 = base+0x90 with
(i16)base[0] base-relative dwords; +0x18/+0x1c from base+4/base+8), and the
(i16)base[2] * 0x3ac VM array malloc into +0x17c.

`0x402720` was re-verified against raw disassembly: the calc body clears
two additional dwords at +0x2b34/+0x2b38 alongside the +0x2b3c vec3, and
the normalize helper 0x45217c is confirmed as the D3DXVec3Normalize import
thunk (IAT 0x4662bc, d3dx9_31.dll). The C++ body gained the missing zeros.
See also `docs/evidence/scene-trigger-features.md` for the shared-leaf
detail.
