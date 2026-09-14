# Scene-trigger feature helpers (0x4074b0..0x407ef0 family)

Reconstruction in `src/SceneTriggerFeatures.cpp/.hpp`. These are the bodies
dispatched by `UpdateSceneTriggerObjectStackAbi` (0x406240) on the trigger
object's accumulated `+0x43c` flag word, plus the two EBX-ABI region-wrap
twins (0x407da0/0x407e40) called from the same tail, and the small shared
leaves 0x44bc10 / 0x408660 / 0x406160 / 0x4061d0. All bodies were taken from
fresh objdump disassembly (the record fields were cross-checked against the
queue-opcode handlers in `src/SceneTriggerObject.cpp`).

Field map used throughout (0x7f0-stride trigger object):

| offset | meaning |
| --- | --- |
| +0x0 / +0x4 | record flag dword / down-counter |
| +0x3b4..+0x3bc | screen anchor xyz |
| +0x3c0..+0x3c8 | polar velocity xyz (0x408750 writes xy from angle/speed) |
| +0x3d8 | polar speed (the `b` argument of 0x408750) |
| +0x3e4 | polar angle (the `a` argument of 0x408750) |
| +0x39c | linked region block (half extents at +0x34 x / +0x30 y) |
| +0x43c | accumulated feature flag dword |
| +0x458 | state word; >= 0 gates the sound reserve |
| +0x614/0x618/0x61c/0x620 | timer block for flag 1 (prev/count/acc/rate-ptr) |
| +0x648..+0x654 | timer block for flag 0x10; +0x65c speed rate, +0x664..+0x66c per-axis accel, +0x670 count limit |
| +0x67c..+0x688 | timer block for flag 0x20; +0x690 speed rate, +0x694 turn rate, +0x6a4 count limit |
| +0x6b0..+0x6bc | timer block shared by flags 0x40/0x80/0x100; +0x6c4 refire speed, +0x6c8 angle bias, +0x6d8 frame budget, +0x6dc fire budget, +0x6e0 fire counter |
| +0x6f8 | speed override dword (raw opcode bits 0x400/0x800/0x8000000) |
| +0x70c / +0x710 | bounce counter / bounce budget |
| +0x718..+0x724 | timer block for flag 0x8000 (handled in 0x406240) |
| +0x74c / +0x750 | timer block for flag 0x100000 (count at +0x750) |
| +0x780 / +0x784 | timer block for flag 0x200000 (count at +0x784) |
| +0x7b4..+0x7c0 | timer block for flag 0x4000000; +0x7c8 turn factor, +0x7cc angle bias, +0x7dc frame budget |

The timer blocks are the shared five-field `{prev i32, count i32, acc f32,
rate ptr, flags}` records; every "tick" below is exactly the 0x404ed0
forward tick (`TickTimerForwardEsiAbi`, 0.99..1.01 rate window, 0x463b2c
round-half-away re-derivation outside it), and the re-arms are exactly
`TickPlayerTimerEaxStackAbi(timer, 0)` (lazy init + count 0 / prev -1 /
acc 0).

## Shared leaves

### 0x44bc10 `WrapAngleSumStackAbi(a, b)` (ret 8)

`v = a + b`; while `v > pi` (flt_470b18) subtract 2*pi (flt_470b14);
while `v < -pi` (flt_470b10) add 2*pi. Each loop has the native counter
budget (checked *after* the subtraction, exits when the previous count
exceeds 0x20, i.e. at most 33 corrections). NaN exits both loops via the
fcom parity chain and returns NaN unchanged.

### 0x408660 `AngleDifferenceWrappedStackAbi(a, b)` (ret 8)

`d = a - b`; if `d > pi` return `a - (b + 2pi)`; else if `b - a > pi`
return `a - (b - 2pi)`; else return the raw `d` (the second x87 compare
pops only the duplicate, leaving `a - b` in st0). NaN returns NaN.

### 0x406160 / 0x4061d0 `CheckRegionExitedPlayfield{64,0}EcxStackAbi`
(ECX = position pair, stack = {half_x = region+0x34, half_y = region+0x30})

Answers 1 (fully exited) when any of
`half_x*0.5 + x <= -192`, `x - half_x*0.5 >= 192`,
`half_y*0.5 + y <= y_min`, `y - half_y*0.5 >= 448` holds, else 0
(still overlapping). Every comparison *continues* on NaN (the C3==C0 /
C0-only parity chains), so a NaN coordinate answers 0. The two entries
differ only in the top bound `y_min`: 0x406160 uses -64.0 (flt_470b5c),
0x4061d0 uses 0.0 (flt_470b04). 0x406240's tail exit check calls the
-64.0 twin; the 0x407da0/0x407e40 pair calls the 0.0 twin.

## Flag 1 — 0x4074b0 `UpdateSceneTriggerLaunchSlowdownEsiAbi` (ESI = object)

If the `+0x618` count is (signed) `> 0x10`, the native XOR-clears flag
bit 1 (self-disable) and skips the motion. Otherwise the polar speed is
`5.0 (flt_470c68) - acc(+0x61c) * 0.3125 (flt_470c6c) + y-speed(+0x3d8)`
and 0x408750 republishes the velocity from the unchanged `+0x3e4` angle.
Then the `+0x614` timer block ticks. The decay reaches zero after the 16
frames of the opcode-1 armed budget.

## Flag 0x10 — 0x407560 `UpdateSceneTriggerAccelerationEsiAbi`

If the `+0x64c` count reached the `+0x670` limit, clear flag bit 0x10 and
skip. Otherwise: `+0x3d8 += rate * +0x65c`, then each velocity axis gains
`rate *` its `+0x664/+0x668/+0x66c` rate (rate = flt_476f78 read directly,
not through the timer's pointer). The `+0x3e4` angle is recomputed as
`atan2(vy, vx)` (FPATAN) only when `|vx| > 1e-4 (flt_470c58)` or, when vx
is small, `|vy| > 1e-4`; a NaN vx reaches the atan2 while a NaN vy (with
small vx) skips it — both fcomp parity directions preserved. Then the
`+0x648` timer ticks.

## Flag 0x20 — 0x4076a0 `UpdateSceneTriggerAngleTurnEsiAbi`

If the `+0x680` count reached the `+0x6a4` limit, clear bit 0x20 and skip.
Otherwise: `angle(+0x3e4) = 0x44bc10(angle, rate * +0x694)` (turn), then
`+0x3d8 += rate * +0x690` and 0x408750 republishes the velocity with the
new angle. Then the `+0x67c` timer ticks.

## Flags 0x40 / 0x80 / 0x100 — the slow-to-stop refire family
(0x407780 `UpdateSceneTriggerSlowStopRelaunchEsiAbi`,
0x407a30 `UpdateSceneTriggerSlowStopAimPlayerEsiAbi`,
0x4078e0 `UpdateSceneTriggerSlowStopFixedAngleEsiAbi`)

All three share one skeleton over the `+0x6b0` timer and the `+0x6d8`
frame budget:

- While the `+0x6b4` count is below `+0x6d8` (signed), the polar speed
  decays linearly: `z = y(+0x3d8) - y * acc(+0x6b8) / (i32)+0x6d8` — the
  divisor is the native `FIDIV`, i.e. the *integer* budget. The angle is
  untouched. 0x408750 publishes `{cos(angle)*z, sin(angle)*z}`.
- When the count reaches the budget, the fire step runs even if the flag
  bit is cleared in the same step:
  1. sound reserve 0x43dc90 (ECX = 0x492590, EDI = `+0x458`, stack = 0)
     whenever `+0x458 >= 0`;
  2. `+0x6e0` fire counter increments; when it reaches the `+0x6dc`
     budget the dispatch bit clears (`0x40` / `0x80` / `0x100` and-mask);
  3. angle update — 0x407780: `angle += +0x6c8`; 0x4078e0: `angle =
     +0x6c8`; 0x407a30: `angle = 0x44bc10(aim, +0x6c8)` where `aim` is
     the aimed angle below;
  4. speed reset: `+0x3d8 = +0x6c4`;
  5. re-arm `+0x6b0` (lazy init + count 0 / prev -1 / acc 0);
  6. 0x408750 with the updated angle and the new speed, then the `+0x6b0`
     forward tick (so the next budget counts from 1).

The aimed angle (native inline at 0x407a9f, reused by 0x407ef0) is
`atan2(py - y, px - x)` against the DAT_00477834 screen target block
(`+0x3c0`/`+0x3c4` anchor); when both deltas are *exactly* zero the
native pushes the 0x3fc90fdb (pi/2) immediate instead of calling FPATAN.
NaN deltas fail both exact-zero tests and reach the atan2.

## Flags 0x400 | 0x800 | 0x8000000 — 0x407be0 `UpdateSceneTriggerWallBounceEsiAbi`

Entry gate: bounce only when `x <= -192 (flt_470b40)` or `x >= 192
(flt_470b3c)` or `y <= 0` or `y >= 448 (flt_470b38)`; the parity chains
make NaN x fall through to the y checks (which then answer no-bounce) and
NaN y skip the bounce entirely, which the plain comparisons reproduce.

Body, in native order:

1. sound reserve (as above) whenever `+0x458 >= 0`;
2. x wall (`x < -192` or `x >= 192`): angle becomes
   `0x44bc10(-angle - pi, 0)` and the position mirrors —
   `x = -384 - x` below the left wall (flt_470b58), `x = 384 - x` past
   the right one (flt_470b54); the boundary/NaN cases take the 384
   branch. The bounce latch (`+0x70c` increment source) is set;
3. y wall: skipped wholesale when flag bit 0x8000000 is set. The ceiling
   (`y < 0`, strictly) reflects unconditionally, while the floor
   (`y >= 448`) reflection additionally requires flag bit 0x400 (the
   `test ch,0x4`). Reflection negates the angle and mirrors `y = -y`
   (ceiling) or `y = 896 - y` (floor, the exact-zero/NaN re-check falls
   to the 896 branch). Sets the bounce latch;
4. speed override: `+0x3d8 = +0x6f8` whenever `+0x6f8 > -990.0
   (flt_470b50)` or NaN. The queue handler stores the *raw opcode bits*
   (0x400/0x800/0x8000000) in `+0x6f8`, which as float bits are positive
   denormals — the net effect is a near-zero speed after the bounce;
5. 0x408750 from the updated angle and `+0x3d8`; when the bounce latch
   was set, `+0x70c` increments, and once it reaches `+0x710` the three
   dispatch bits clear together (`&= 0xf7fff3ff`).

No timer tick in this helper.

## Flag 0x4000000 — 0x407ef0 `UpdateSceneTriggerHomingTurnEsiAbi`

If the `+0x7b8` count reached the `+0x7dc` budget, clear bit 0x4000000
and skip the turn. Otherwise:

1. `target` = aimed angle at the screen target block (shared helper, pi/2
   quirk included);
2. `wrapped = 0x44bc10(+0x7cc bias, target)`;
3. `delta = 0x408660(wrapped, angle)` — shortest signed step;
4. `delta *= +0x7c8` (turn factor) `* flt_476f78` (rate);
5. `angle(+0x3e4) = 0x44bc10(angle, delta)` and 0x408750 republishes the
   velocity with the unchanged `+0x3d8` speed.

Then the `+0x7b4` timer ticks.

## Flag 0x100000 / 0x200000 — 0x407da0 / 0x407e40 `WrapSceneTriggerRegion{X,Y}EbxAbi` (EBX = object)

Both run the 0x4061d0 exit test against the linked region block at
`+0x39c` (half x = region+0x34, half y = region+0x30, y_min = 0) and
return immediately while the box still overlaps the play field. Once
fully outside:

- X twin: `x < -192` shifts `x += half_x + 384 (flt_470b54)` (wrap to the
  right edge); `x > 192` or NaN shifts `x -= half_x + 384`; the closed
  `[-192, 192]` band moves nothing. After a move: shift the `+0x74c`
  timer by -1.0 (0x44bf40), reserve the sound (EDI = `+0x458`, stack 0).
- Y twin: `y < 0` shifts `y += half_y + 448`; `y > 448` or NaN shifts
  `y -= half_y + 448`; after a move shift the `+0x780` timer by -1.0 and
  reserve the sound.
- Common tail (also reached without a move): when the paired counter
  (`+0x750` / `+0x784`) is `<= 0` the dispatch bit toggles off (native
  XOR with 0x100000 / 0x200000).

## Caller-side corrections in SceneTriggerUpdate.cpp

- The dispatcher gate for 0x407a30 is `mov al,[ebp+0x43c]; test al,al;
  jns` — flag bit **0x80**, not 0x80000000 as previously modeled; the
  dispatch order is 1, 0x10, 0x20, 0x40, 0x100, 0x80, 0x8000C00,
  0x4000000, 0x8000.
- The spawn-path 0x44bc10 call (descriptor x angle) is now the semantic
  `WrapAngleSumStackAbi(pos_x, 0.0f)`; the previous
  `ComputeSpawnAngleStackAbi` extern is gone.

## 0x403850 `LoadTitleBackgroundScriptEbxStackAbi` (title-state neighbor,
src/TitleBackgroundScript.cpp)

Native EBX = the title-screen state block, stack = filename (the caller
0x402230 receives it in ECX from the 0x402640 factory and re-pushes it;
the 0x403080 scheduler thunk passes it in EAX):

1. While `+0x2a44` (loaded image) is null: clear the first byte of the
   global path buffer 0x497c38, locate its terminator and append the
   caller's filename (with terminator), then
   `LoadMainChainFile(0x497c38, &state+0x2a48, 0)` (0x44b360). A null
   result stores 0 and returns -1.
2. The image is copied byte-exact into a fresh CRT malloc (0x452706)
   sized `+0x2a48`, whose pointer lands in `+0x10`.
3. `RequestManagerWork(DAT_00491c10, slot = (state+0x2a30 & 1) + 4,
   buffer+0x10)` (0x447280, native EDX = owner / ECX = slot / stack =
   name); the result lands in `+0x178`. Failure appends the 0x46cbc0
   "stage data not found" error line (0x44b810) and returns -1.
4. Header rebase: `+0x14 = base+0x90`, `+0x18 = base + [base+4]`,
   `+0x1c = base + [base+8]`, then `(i16)base[0]` dword entries at
   `+0x14` are rebased relative to the buffer base.
5. `+0x17c` receives a second malloc of `(i16)base[2] * 0x3ac` bytes (the
   VM-record array consumed by the calc body). Return 0.

The script stream interpreter (0x403c80) and the calc body (0x402720)
already lived in the same module; 0x402720 was verified against raw
disassembly this pass and gained its missing native quirk: the two zero
dwords at `+0x2b34`/`+0x2b38` cleared alongside the `+0x2b3c` vec3 (and
0x45217c confirmed as the D3DXVec3Normalize import thunk).

## Status

All fifteen new functions are recorded in `config/function-status.csv`.
Baselines: `g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` clean,
`scripts/compile-main-chain-cpp.sh` exit 0, `git diff --check` clean.
