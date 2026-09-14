# Replay/Continue Scene Reuse Preparation — 0x0042a6a0

Implemented as `PrepareReplaySceneReuse` in
`src/ReplaySceneReuse.cpp/.hpp`. Native usercall with the record in EBX
(`ret`, no return value). The single caller is the title → game-scene setup
0x00417870 (mode-flags bit 1 path, `mov ebx, DAT_00477838; call 0x42a6a0`),
which runs it right before opening the scene script (0x00413a20) and
re-creating the title state (0x402640).

`EBX` is the 0x2d4-byte replay game-context record whose pointer is published
at DAT_00477838 (built by the 0x429610/0x428f60 factory pair, see
`docs/evidence/stage-practice-replay-context.md` semantics in
`src/StagePracticeReplayContext.cpp`). The function branches on the context
mode at +0x10 — the same field 0x428f60 writes — and is the reduced sibling
of that function's mode-0/mode-1 bodies: the same stage-record seeding and
the same first four snapshot restores, but no secondary header, no
difficulty stores and no callback registration, plus a different tail set of
restored globals.

## Mode 0 (fresh stage / continue)

1. Allocate 0x1c4 bytes (operator new 0x452493), zero all 0x71 dwords
   (`rep stosd`, guarded by a null check on the allocation result only).
2. Store the pointer into the per-stage slot `ctx + 0x1c + 4 * stage`
   (stage = DAT_00474c7c).
3. Reload the slot and dereference it **without a null check** (native quirk
   — a failed allocation would crash here; preserved):
   - `u16[+0x02] = word at 0x4918b0` (the shared 16-bit PRNG state);
   - `0x4918b4 = 0` (PRNG counter latch);
   - `u16[+0x00] = stage`;
   - `+0x1c0 ^= (DAT_00491fc4 ^ +0x1c0) & 1` — the flag's bit 0 becomes the
     game-active flag's low bit.

## Mode 1 (replay load restore)

1. Per-stage cursor record refresh at `entry = ctx + 0xa0 + 36 * stage`:
   `entry[+0x04] = entry[+0x00]` (saved = value), `entry[+0x0c] =
   entry[+0x08]`, `entry[+0x14] = 0`.
2. Snapshot restore from `snap = *(entry + 0x10)` — the 0x1c4-byte stage
   record loaded with the replay; the pointer is dereferenced unchecked:
   - `0x4918b0 = u16[snap+0x02]`, `0x4918b4 = 0`;
   - `DAT_00474c44` (run score) = `[snap+0x0c]`;
   - `u16` at `DAT_00474c48` = `u16[snap+0x10]`;
   - `0x00418b80` thiscall (semantic
     `SnapshotScoreBlockMaxScoreThisAbi`) with `this = DAT_00474c40` and
     value `[snap+0x14] * 10`;
   - reduced inline of the 0x0042a930 power-timer reset with value
     `[snap+0x18]`: the flag/rate guard (`DAT_00474c64 |= 1`,
     `DAT_00474c60 = &DAT_00476f78` when bit 0 was clear — dead in practice
     because 0x00418b80 always latches the flag bit), then the unconditional
     `DAT_00474c58 = v`, `DAT_00474c54 = v - 1` (native `dec eax`),
     `DAT_00474c5c = (float)v` (native `fild`/`fstp`); the full 0x0042a930
     NaN/zero sub-stores are omitted in this inline (they would be
     overwritten anyway);
   - `DAT_00474c70` (practice start index) = `[snap+0x1c]`;
   - `DAT_00474c98` = `[snap+0x20]`;
   - `DAT_00474c90` (practice score seed) = `[snap+0x1b4]`;
   - `DAT_00474c9c` (play-count seed) = `[snap+0x1b8]` — the native reads
     this through a second slot load (`ctx + 0xb0 + 36*stage`, i.e. the
     record's +0x10 pointer) before dereferencing +0x1b8.
3. Any other mode value leaves the context untouched.

## Global-name note

`DAT_00474c98` carries both the `g_ResultScore` reading (replay header
restore in `StagePracticeReplayContext.cpp`) and the 0xFFFFFE00 "rank"
sentinel write in `TitleSceneSetup.cpp`; this module uses the neutral
`g_RunRankValue` extern for the same address.

## Verification

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` — 0 errors.
