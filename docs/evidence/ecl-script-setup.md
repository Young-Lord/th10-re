# ECL Script Setup (`0x0040dc80`)

Implemented as `RunEclScriptSetupStackAbi` in `src/EclScriptLibrary.cpp`
(replacing the former boundary). Called from `CreateEclScriptObjectEaxStackAbi`
(0x40cfb0, with `record + 0x1044`), from `0x40d750`, and by a third caller at
`0x40dc76` region. Native ABI: one stack argument (`ret 4`) = the ECL script
object's `+0x1044` sub-record; **all offsets below are relative to that
sub-record** (so `+0x1444` = record `+0x2488`). Returns 0 normally, `-1` on
the two abort paths, and `1` after the enemy-death sequence.

## Flow

1. **Run gate**: flag dword at `+0x1444` bit `0x400`. Already set -> return 0;
   otherwise the bit is stored before any other work.
2. **Working block refresh**: 0x2c bytes copied from `+0x2c` (base block) to
   `+0x00`.
3. **Four vec2 animation blocks** (0x38 bytes each, armed by the dword at
   block `+0x34`), ticked through the `0x412ac0` boundary
   (`TickVec2AnimInterpolatorEdiEsiAbi`, native EDI = out pair, ESI = block):
   A0 `+0x1d4` -> angle `+0x74` wrapped through `0x44bc70`
   (`WrapAngleToPi`), slope `+0x70`; A1 `+0x210` -> angle `+0xa0` (wrapped),
   slope `+0x9c`; A2 `+0x24c` -> direct pair `+0x78`/`+0x7c`; A3 `+0x288` ->
   direct pair `+0xa4`/`+0xa8`.
4. **Motion block 1** (`pos +0x58, vel +0x64, radius +0x70, angle +0x74,
   flags +0x80`): with `+0x180` armed, `0x404610`
   (`TickVec3Interpolator`, block `+0x13c`) yields a target position and the
   velocity is `target - pos`; else with `flags.0` set, `+0x78 += +0x7c` and
   `+0x74 = wrap(+0x70 + +0x74)`; else `0x44c5d0`
   (`PolarToCartesianEdiAbi`, out = vel.xy, angle = `+0x74`, radius =
   `+0x70`) and `+0x6c = 0`.
5. **Motion block 2** (`pos +0x84 ... flags +0xac`): same three modes with
   the vec3 animation at `+0x188` (armed by `+0x1cc`).
6. **Integration**: `0x44c2a0` (`IntegrateSubEffectPositionEsiAbi`) over
   block 1; with flag `0x40000` block 2's position is shifted by the global
   offsets `flt_491e6c/70/74` first; then block 2; then the base block's
   velocity is set to `block2.pos + block1.pos - base.pos` and the base
   block is integrated too.
7. **Clamp (flag `0x200`)**: base position clamped into the rectangle
   centered `(+0x13ac, +0x13b0)` with half extents `(+0x13b4, +0x13b8) * 0.5`
   (the 0.5 is `flt_470b0c`); then block-1 position re-derived as
   `base - block2`.
8. **Playfield gate**: half extents `+0x13a4/+0x13a8 * 0.5` against the
   constants `flt_470b40` (-192), `flt_470b3c` (192), `flt_470b04` (0),
   `flt_470b38` (448). Outside the region with the `0x100` latch already set
   and without the `0x4` stay flag -> return `-1`; inside sets the latch.
9. **Bind (flag `0x100000`)**: reads `*(DAT_004776ec) + 0x28`. With no
   active stage node, flag `0x200000` selects id `+0x144c`, stores it at
   `+0xf4`, rebinds the first entity slot through `0x4496d0`
   (`RebindEntitySlotEaxStackAbi`: EAX = id-slot pointer, stack = new id),
   and clears bits `0x200000|1`. With an active node and `0x200000` clear,
   id `+0x1448` is used and bits `0x200001` are set.
10. **Entity-list scan**: `0x44fd10` (`ScanManagerEntityListEdiStackAbi`,
    EDI = script manager from `+0x14d8`, stack = `**(float**)+0x128`) —
    nonzero -> return `-1`. The `0x2000` flicker-arm bit is then cleared.
11. **Damage pass** (only when the cleared flags have no `0x11` bits):
    `0x428630` (`CollectItemCollisionsStackAbi`, native ECX = null, stack =
    {DAT_00477834, `+0x2c`, `+0xb0`}) returns the raw damage. Modes 0/2 of
    `DAT_00477834+0x458` divide by 5. Zero damage sets `DAT_00474c50 = 1`
    and jumps to the shared tail. With `DAT_004776f4+0x378c` bit 0 and flag
    `0x8000`, the damage is halved again (floored at 1). Unless flag `0x8`
    or the `+0x1420` timer count is positive, `+0x13c0` (boss HP) loses the
    damage.
    - Pending script request: `0x4127a0` (thiscall, ECX = script manager);
      nonzero runs `0x40c6e0`/`0x40c730`, resolves the name via `0x450470`
      (`ResolveScriptTableIndexEaxAbi`), stores the result at
      `*(manager+4)+4` and zero at `*(manager+4)`, then repeats the
      `0x44fd10` scan (nonzero -> `-1`).
    - Without flag `0x40`, a nonpositive HP adds `+0x13c8` to the score
      block `0x474c40` through `0x409d90` (`AddScoreBlockValueEcxStackAbi`)
      and runs the death sequence `0x40e5f0` (returning `1` from this
      function after it).
    - Flag `0x2000` is re-armed and `DAT_00474c50 = 1`.
12. **Shared tail**: the same pending-request handling as above; the timeout
    region check `0x4266b0` (skipped with flags `0x12` or a positive
    `+0x1434` count); the direction state machine (flag `0x1000`) mapping
    the base-block X delta through the `[-0.1, 0.1]` dead zone into
    animation-id steps off `+0xf4` (`prev -1 -> 3-(dir!=0)`, `prev 0 ->
    (dir!=-1)+1`, `prev 1 -> dir!=0 ? 1 : 4`) with the slot rebind;
    position publication into the eight entity slots `+0xc0..+0xdc`
    (`0x4492f0` verbatim with flag `0x40000`, else `0x449350` with the
    +224/+16 game-area offset); closest-enemy tracking in `DAT_00477834`
    (`+0x3504` target manager pointer, `+0x3508` latch, distances against
    the player X at `+0x3c0`, skipped with `0x11`/`0xc0000`).
13. **Flicker**: entity resolved by `0x4491c0`
    (`FindEntityEdxStackAbi`, EDX = `DAT_00491c10`, stack = slot `+0xc0`);
    a null result clears the slot but the native still dereferences the
    pointer afterwards (unchecked-pointer quirk, preserved). Counter
    `+0x1414` running: clears `0x8000` on `entity+0x35c` (and on
    `DAT_0047770c+0x9da4` with flag `0x8000`) and decrements. Otherwise with
    `0x2000` armed: sets `entity+0x35c |= 0x8000`, `entity+0x300 =
    0xff0000ff`, counter = 4, and queues a positional sound (`0x43dd10`,
    EBX = id, ESI = `0x492590`, stack = base X): id `0x23` when flag
    `0x8000` passes the gate (`DAT_004776f4+0x378c` bit 0 clear, or bit 0
    set without bit 3 and manager timer `+0x2404 < 300`, and always
    `< 900`), else id `0x13`.
14. **Timers and frame counter**: `0x44bf40` (`ShiftTimerByEsiStackAbi`,
    -1.0f) over the timer blocks at `+0x141c`/`+0x1430` while their counts
    `+0x1420`/`+0x1434` are positive. Finally the `{prev +0x11c, count
    +0x120, accumulator +0x124, rate pointer +0x128}` block advances:
    `prev = count`; outside the 0.99..1.01 rate window (`flt_470b68/b64`)
    the accumulator absorbs the rate and the count re-derives from it
    (native truncates through an unsigned 64-bit conversion); inside, both
    count and accumulator simply increment.

## Preservation Notes

- The unchecked `entity` dereference in the flicker branch (step 13) and the
  write-through-null behavior of the enclosing creator are native quirks and
  kept.
- The rate/truncation idiom in step 14 mirrors the shared anim-timer code
  (`0x412ac0` epilogue) but with a single-deref rate pointer (`+0x128` holds
  pointer-to-float), so the shared `0x405410`/`0x44bf40` semantic bodies
  (which use a pointer-to-pointer rate at `+0xc`) are not reused for it.
- `0x412ac0` is now implemented as `TickVec2AnimInterpolator` in
  `src/EclScriptLibrary.cpp` (see below); `0x4496d0`, `0x44fd10`,
  `0x428630`, `0x4127a0`, `0x40c6e0`, `0x40c730`, `0x450470`, `0x40e5f0`,
  `0x4266b0`, and `0x43dd10` remain explicit register-ABI boundaries
  declared in `src/EclScriptLibrary.cpp`.

## `0x412ac0` TickVec2AnimInterpolator (2026-09-14)

The vec2 sibling of `0x00404610`: native EDI = out pair, ESI = the
`0x3c`-byte block `{cur[2]@0x00, end[2]@0x08, handle1[2]@0x10,
velocity/handle2[2]@0x18, timer {prev@0x20, cur@0x24, accum@0x28,
rate ptr@0x2c, flags@0x30}, duration@0x34, mode@0x38}`. The timer shares
the vec3 semantics (unity rate window `0.99 < rate < 1.01`, ftol of the
accumulator otherwise, poison `0xfff0bdc1`, rate reset to `DAT_00476f78`)
with two differences: the completion zeroes the duration (so the block is
really stopped afterwards) and outputs `cur` for mode 7 versus `end` for
everything else. Duration `<= 0` skips the timer entirely and interpolates
with `t = accum / (float)duration` — a zero duration yields inf/NaN
(preserved). Modes: 7 adds the end pair into cur, 0x11 integrates
velocity (`vel += end` after `pos += vel`), 8 rides the cubic Hermite —
unlike the color track, handle 2 here gets the proper `(t-1)*t*t` basis —
and every other mode eases through the `0x0044c350` curve selector. All
four `RunEclScriptSetupStackAbi` call sites (`rec+0x1d4/0x210/0x24c/0x288`,
armed by `+0x208/+0x244/+0x280/+0x2bc`) now call the semantic body.
