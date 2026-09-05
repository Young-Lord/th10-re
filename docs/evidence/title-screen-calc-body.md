# Title Screen Calc Body (0x00418190)

Implemented as `RunTitleScreenCalcBodyStackAbi` in
`src/TitleScreenCalcBody.cpp/.hpp`. Native ABI: one stack argument (the
0x60-byte title screen published by `CreateTitleScreen` / forwarded through
the one-instruction ECX callback `0x004187c0`), `ret 4`, result in EAX
(1, or 3 from the end-of-frame path). This is the target of the
`TitleScreenCalcCallback` forwarder in `src/TitleGameManagerLifecycle.cpp`,
which now calls the semantic body.

## Dispatch

The dword at `state+0x14` selects the body:

- `0` — title idle frame.
- `30` — restart frame (e.g. returning to the game after a result screen).
- anything else — only the common epilogue.

## State 0

- If `state+0x58` bit 3 (8) is set: `StopThreadControl` (0x44c150, native
  ESI = DAT_00492254) and
  `DAT_00491fb8 = ((~(DAT_00491ff4 >> 12)) & 1) | 2`; return 1.
- `0x00404450(DAT_004776e8)` (stack-arg boundary).
- If `DAT_004776e4` (primary title state) is non-null:
  - `0x00404530` (ESI = DAT_004776e4) and `0x004045b0` (EAX = DAT_004776e8)
    — the semantic `TriggerTitleScoreAnim30/60Esi/EaxAbi`.
  - `state+0x58 |= 0x800`.
  - `0x00409e50` (`ExpireEntityHandleEaxAbi`) on `DAT_0047770c + 0x9e14`.
  - jump to epilogue.
- Otherwise (first frame without a title state — the game-start reset):
  - `state+0x58 &= ~0x800`.
  - `0x00405ed0` (ESI = DAT_004776f0) and `0x00424d90` (ESI = DAT_00477834)
    — the `CleanupEffectManagerEsiBoundary` /
    `CleanupOptionPositionEsiBoundary` boundaries shared with
    `EclSelectMenu.cpp`.
  - `memset(DAT_00477818 + 0x14, 0, 0x21cea0)` (rep stosd of 0x873a8 dwords).
  - `0x00409f90(DAT_00477704)` (stack arg; boundary
    `ReleaseAsciiHudConditionalState`), `0x00417770` (EAX = DAT_00477781c,
    boundary), `DAT_00474c88 = DAT_00474c8c = 0`, `0x0042a450` (no inputs,
    boundary).
  - 64-byte stack descriptor zeroed, then `0x0040cfb0`
    (`CreateEclScriptObjectEaxStackAbi`) with EAX = descriptor and stack
    `(DAT_00477704, "main")` — the "main" ECL script object for the run.
  - `0x00413bc0` (`ResetAsciiHudOverlayEdiAbi`, EDI = DAT_0047770c).
  - Scheduler-record enable-bit sweep (word at record+4 `|= 2`), in native
    order: DAT_00477830 (both slots checked), DAT_00477834 (both slots
    UNCHECKED — native dereferences without the null test) followed by
    `0x00426f70` (`RebuildPlayerOptionRecords`, stack arg DAT_00477834),
    DAT_004776f0 (checked), DAT_00477704 (checked), DAT_00477818 (checked),
    DAT_00477781c (checked), DAT_004776fc (unchecked), DAT_004776ec
    (checked), DAT_00477840 (unchecked), DAT_004776f4 (checked at +8 and
    +0xc, plus an unchecked third slot loaded from `+0x37ac`), DAT_00477814
    (checked).
  - When `DAT_00474ca0 & 0x20 == 0`:
    `0x00420b10(0, [DAT_00477848 + 0x24])` — semantic
    `SelectTimelineAudioMode`.
  - `0x00409e50` on `DAT_004776e0 + 0x89a0` and `+0x89a4`, then
    `*(u32*)(DAT_004776e0 + 0x89a4) = 0`.

## State 30

Gated on `state+0x58 & 0x800` (otherwise straight to the epilogue). Clears
`0x800`, repeats the entire game-start reset above, then:

- `0x0043e460` (`QueueBgmCommand(&g_TransitionRoot, "dummy", 4 or 3, 0)`):
  opcode 4 when `DAT_00491d78 & 0x10`, else 3 (state 0 has no such call).
- `0x00420b10(0, [DAT_00477848 + 0x24])` unconditionally.
- `0x00409e50` on `DAT_004776e0 + 0x89a4` only, then zeroing of that slot.
- `0x00405410` (`TickPlayerTimerEaxStackAbi(state + 0x10, 0)`).

## Common epilogue

- If `DAT_004776e4 != 0` and `*(u8*)(DAT_004776e4 + 0x2a18) & 8` (the native
  repeats the null test): `0x00402440`
  (`DestroyTitleScreenStateBufferInPlace`) then `j__free` (0x4524a1 =
  `FreeMainChainObject`).
- If `state+0x58 & 4`: `state+0x58 |= 0x80`; return 1.
- `StopThreadControl` (0x44c150).
- If `DAT_00474ca0 & 0x20`:
  - when `DAT_00474e30 & 0x160b` or `state+0x58 & 0x70`:
    `DAT_00491fb8 = (DAT_00491ff4 & 0x1000) ? 2 : 4` (neg/sbb idiom);
  - `state+0x14 == 2940 (0xb7c)`: `0x0043c8b0` with stack
    `(5, 0x3c, 0, 0, 0)` and EBX = 0x2b — semantic
    `CreateAsciiOverlayContext(5, 60, 0, 0, 0, 0x2b)`;
  - `state+0x14 == 3000 (0xbb8)`: `0x0040ac90` with EAX = 0x491c28 and stack
    state 4 — semantic `RequestGameStateTransitionEaxStackAbi`.
- `0x00417040` (boundary, native ESI = DAT_0047770c).
- If `state+0x58 & (0x10|0x20|0x40)`: return 3.
- Play-time accumulator: skipped while `*(u32*)(DAT_00477838 + 0x10) == 1`
  (DAT_00477838 dereferenced without a null check); otherwise at
  `DAT_00477783c + (3 * DAT_00474c68 + DAT_00474c6c) * 0x437c + 0x4cc` the
  dword is incremented while below 215999999 (signed `jge` guard).
- When `*(u32*)(DAT_00477704 + 0x10) == 0` and
  `*(u32*)(DAT_0047770c + 0x9eb8) == 0` and `state+0x14 >= 0x5a` (signed):
  `0x00418a00` (boundary, native EAX = 0x474c40).
- `++DAT_00474c88; ++DAT_00474c8c;`
- `0x00404ed0` (`TickTimerForwardEsiAbi`, ESI = `state + 0x10`); return 1.

## Boundary set kept extern (undefined, link-time, as in EclSelectMenu.cpp)

`0x00404450` (stack arg), `0x00405ed0` (ESI), `0x00424d90` (ESI),
`0x00409f90` (stack arg), `0x00417770` (EAX), `0x0042a450` (no inputs),
`0x00417040` (ESI), `0x00418a00` (EAX).

## Verification

`scripts/compile-main-chain-cpp.sh` (with the new
`TitleScreenCalcBody.cpp` line), `g++ -m32 -std=c++98 -fsyntax-only -I src
src/*.cpp`, and `git diff --check` all pass.
