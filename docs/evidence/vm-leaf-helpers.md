# TH10 VM Leaf Helpers And Interpreter Port Corrections

Module: `src/VmLeafHelpers.cpp/.hpp`, plus corrections applied to
`src/TimelineRenderObjectSetup.cpp`.

## New semantic bodies

- `0x00404610 TickVec3Interpolator`: block `{cur@0, end@0xc, handle1@0x18,
  handle2/vel@0x24, timer@0x30, duration@0x44, mode@0x48}`. The timer
  advances by the rate — except when the rate sits at/above 1.01, where
  the count takes the integer +1 path (fast-forward capped at real-time;
  this is the inverse window of the delta-shift helper). Completion
  outputs the endpoint (mode 7 outputs the mutated start). Mode 7 adds a
  delta, 0x11 integrates velocity (`vel += end` after `pos += vel`), 8
  rides the cubic Hermite through both handles, and everything else eases
  with the mode value as the curve selector `0x0044c350` (see below).
- `0x0044c350 EasingCurveSelectorEaxStackAbi`: the shared easing-curve
  selector. Native ABI: EAX = curve mode (raw 1..16), two float stack
  arguments `{value, denominator}` (`retn 8`), result returned as a
  double in ST0. Computes `t = value / denominator` once (caller passes
  the block accumulator float and the duration converted by `FILD`), then
  dispatches through a 16-entry jump table at `0x44c550` guarded by
  `dec eax; cmp eax, 0xf; ja default`:
  - 1..3 ease-in `t^2..t^4`; 4..6 ease-out `1-(1-t)^2..^4`.
  - 9..11 ease-in-out: `u = t+t`; `!(u < 1.0)` branch `(2-(2-u)^n)/2`,
    else `u^n/2`, with n = 2/3/4.
  - 12..14 the mirrored in-out formulations: `!(u < 1.0)` branch
    `(u-1)^n/2 + 0.5`, else `0.5 - (1-u)^n/2` (same shapes, different
    x87 operation order — kept as written).
  - 15 returns 0.0 (`flt_470b04`), 16 returns 1.0 (`flt_470afc`).
  - Default (modes 0, 7, 8 and anything above 16) returns the raw ratio.
  Quirks preserved: the FCOM/FNSTSW `test ah, 5 / jp` comparison sends
  NaN down the "out" branch of each pair (unordered sets the parity flag),
  modeled by negating the `<` test; all math runs in x87 extended
  precision, modeled as doubles. Not self-recursive (the switch cases
  share merge blocks; no call to itself exists). The earlier note
  "0xf = 1.0, 0x10 = 0.5" in this file was a misread of the constant
  block and has been corrected — the two constants are 0.0 and 1.0, and
  the old inline switch omitted the 9..14 S-curves entirely.
- `0x004050d0 ResetVec3InterpolatorTimer`: lazy sentinel init then the
  unconditional stopped-timer reset; the `0xfff0bdc1` poison in prev is
  never read as a float (overwritten by `prev = cur` on the first tick).
- `0x00413200 LinkChildListNode`: push-front with next at +4 / prev at +8;
  an empty list leaves the new node's next field stale (preserved).
- `0x0041ab70 SetupScaleInterpolation`: duration/mode to `+0x1b4/+0x1b8`,
  current scale snapshot from `+0x3c/+0x40` to `+0x180`, target to
  `+0x188`, timer reset at `+0x1a0`.
- `0x00442220/0x00442050/0x00442300/0x00441f50`: the four color/alpha
  interpolation setups, unified as `SetupRgbInterpolation` (bases
  `vm+0xbc`/`vm+0x1bc` feeding outputs `0x2fc..0x2fe`/`0x300..0x302`) and
  `SetupAlphaInterpolation` (bases `vm+0x108`/`vm+0x208` feeding
  `0x2ff`/`0x303`).
- `0x00428dd0 ShiftVmTimerBack`: negates the operand and shifts the vm
  script timer (+0x5c) through the semantic `0x44bf40` body — the opcode
  0x4b time-travel mechanic (slow-mo scales the rewind too).
- `0x00463bba`/`0x00463bf0` are the CRT `_CIfmod`/`_CIacos` intrinsics —
  modeled by `std::fmod`/`std::acos` at the interpreter call sites
  (`_CIfmod`: sign-of-x remainder; `_CIacos`: NaN for |x|>1 through the
  CRT error path).
- `0x004452f0` (the 32-segment radial ribbon rebuild with its RNG walk)
  is specced in full here but intentionally left as a boundary this round:
  it depends on the 16-bit LCG pair and the two render callbacks
  (`0x445620`/`0x445880`), which deserve their own pass.

## Port corrections applied to TimelineRenderObjectSetup

The binary's epilogue (0x441012–0x44115d) shows the sampling bases are
lower than ported (the guards are the duration slots and stay put):

| anim | old base | actual base |
|---|---|---|
| position vec3 | +0x88 | **+0x70** |
| RGB #1 | +0x104 | **+0xbc** |
| alpha #1 | +0x12c | **+0x108** |
| scale float2 | +0x1b8 | **+0x180** |
| rotation vec3 | +0x14c | **+0x134** |
| RGB #2 | +0x204 | **+0x1bc** |
| alpha #2 | +0x230 | **+0x208** |

Also: `StartVec3Anim`'s trigger block is the interpolator base itself
(`node + copy_offset`, not `node + block_offset - 0x2c`), and
`CopyCreatedObjectVectors` copies all three dwords of both vectors
unconditionally (the `copy_all_three = false` variants do not exist in the
binary).

## Jump-table validation (2026-09-03)

Resolved: the earlier "re-key" concern is withdrawn. The live Ghidra
database names the data object at `0x4413a4`
`TimelineSetupOpcodeJumpTable` (4-byte code pointers), the dispatcher
references it from `0x43eec4`, and the dispatch is
`JMP [EAX*4 + 0x4413a4]` where `EAX = (signed)opcode + 1` (bounds 0x5d).
Handler `N` therefore sits at `0x4413a4 + (N+1)*4`. A full opcode-by-opcode
comparison showed the C++ `switch` numbering in `DispatchSetupOpcode`
already matches the table; opcodes `0x00`/`0x40` and any value above `0x5c`
share the default handler `0x43f532`. Earlier claims that "binary 0x3b is
RGB-anim #1" and "0x35/0x36 are the byte sets" came from an incomplete
table read and are incorrect: binary 0x3b is the vec3 animation at
block `+0x134` and binary 0x33/0x34 are the byte writes, exactly as the
port models them.

See `docs/evidence/dispatch-setup-opcode-mapping.md` for the dispatch
formula, the confirmed entry list, and the remaining body-level caveat
(four distinct spawn creators `0x448d00`/`0x448f60`/`0x448e30`/`0x449090`
are currently routed through one `CreateTimelineObject` model).

## Color track (2026-09-14)

- `0x004049a0 TickColorTrack` (in `src/VmLeafHelpers.cpp/.hpp`): the
  four-record color-track sibling of `TickVec3Interpolator`. Native EBX =
  block, stack = 7-dword scratch out (`ret 4`, scratch pointer returned).
  Block: four `0x1c`-byte color records at `+0x00/+0x1c/+0x38/+0x54`, then
  the timer `{prev@0x70, cur@0x74, accum@0x78, rate ptr@0x7c, flags@0x80}`,
  duration `+0x84`, mode `+0x88`.
  - Rate window and poison match the vec3 interpolator (`0.99 < rate <
    1.01` steps the integer counter, otherwise `accum += rate; cur =
    ftol(accum)`); the completion latches flag bit 0 and resets the rate
    pointer to `DAT_00476f78`.
  - Completion differs from the vec3 timer: the duration is NOT stopped,
    and every later tick re-snaps `cur = duration`, `prev = duration - 1`,
    `accum = (float)duration` before outputting record 0 (mode 7) or
    record 1 (everything else).
  - Duration `<= 0` skips the timer entirely; mode 8 computes
    `t = accum / (float)duration`, so a zero duration produces inf/NaN
    weights (preserved).
  - Modes: 7 accumulates record 1 into record 0; 0x11 integrates record 3
    as velocity (`rec0 += vel; vel = rec1 + vel`, both against the old
    velocity); 8 rides a cubic Hermite — and the native reuses the
    handle-1 weight `(1-t)^2*t` for record 3 too (computed as
    `(t-1)^2*t`, the same value); every other mode eases record 0 toward
    record 1 through the `0x0044c350` curve selector.
- `0x00404d40 / 0x00404da0 / 0x00404e10`: the 28-byte record helpers
  (`to - from`, `factor * record`, `a + b`), each re-deriving the four
  bytes at `+0x18..+0x1b` from floats 2..5 through `0x463b2c` (round half
  away from zero, one x87 pop per call). Quirk preserved: `0x404d40`'s
  single stack argument (the eased factor pushed by `0x4049a0`) is never
  read — it stays on the stack and chains into `0x404da0`'s factor
  parameter (`ret` vs `ret 4`).
- Caller wiring: `RunTitleBackgroundScriptStackAbi`
  (`src/TitleBackgroundScript.cpp`) now calls `TickColorTrack` for the
  block at `st+0xe8` (gate = duration at `st+0x16c`) and copies the seven
  result dwords to `st+0x2b48`; the `TickColorTrackAbi` extern is gone.
