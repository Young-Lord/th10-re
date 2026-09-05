# TH10 0x00425730 / 0x00426f70 — Player Mode Dispatcher And Option Rebuild

Modules: `src/PlayerModeDispatcher.cpp` (`UpdatePlayerModeDispatcher`,
stack ABI via thunk `0x426500`) and `src/PlayerOptionRecords.cpp`
(`RebuildPlayerOptionRecords`, stack `ret 4`). Both were reconstructed from
subagent behavioral specs cross-checked against Ghidra decompilation,
disassembly, and raw binary constants.

## Mode dispatcher (0x00425730)

Dispatch on `player+0x458`; `> 4` skips the switch. Returns 1 always. Fall
throughs happen inside one call: mode 0 at timer >= 60 sets mode 1 and runs
the mode-1 body; mode 4 at timer > 8 runs the death processor and then the
mode-2 body.

| mode | meaning |
|---|---|
| 0 | respawn intro: `+0x3d0 = 48000 - t*8000/60`, `+0x3c4 = that*0.01`, latch four tier flags, shift the 32-entry position ring `+0x436c`, intro tweens (`0x408100` x2, `0x41c800`) until t=30 then stage activation; at t>=60 hand off to mode 1 |
| 1 | normal play: deathbomb-bar timer tick (0x10e on `+0x430c`), context tick, lose a life (`0x474c48 -= 20`), option rebuild, HUD lives refresh, 3000-frame sequence on `0x474c40`; enemy activation while t<30; movement `0x4250b0` |
| 2 | death explosion: t==3 penalty `-0x40` (clamped), burst of 7 particles (`0x41bb00`, angles from `0x426610`), option rebuild; t>30 respawn teleport (`+0x3c4 = 480.0`, `+0x3d0 = 48000`) or game over when `0x474c70 < 0` (state 4 via `0x40ac90`, else `0x4231d0`) |
| 3 | bomb freeze: at t==15 activate enemies (no `+4==0` guard) and clear bullets `0x41c850` |
| 4 | hit: t<=8 deathbomb window (bomb key costs a life, jumps straight to mode 1); t>8 real death `0x4269d0` then mode-2 body |

Common epilogue (all modes): 32 sub-effect records at `+0x350c` (stride
0x6c; active bit `rec+0x68&1`; angular flag `rec+0x40&1`; polar conversion
`0x44c5d0`, angle wrap `0x44bc70` (gives up after 32 iterations),
integration `0x44c2a0` with 1/200-grid quantization; timer block
count `rec+0x44`/prev `+0x48`/acc `+0x4c`/src `+0x50`, deactivate when
count < 1); invincibility flash timer `+0x430c` block with the
`tick==prev || tick%3` red-flash logic (`+0x370` bit 0x8000, color
`+0x314`); main VM update `0x43ee30`; four box recomputations
(`+0x404`, `+0x4324` with 0.5-scaled ext2, `+0x433c`, `+0x4354` unscaled
ext2); mode timer tick (`+0x474` block and the flag-less `+0x488` block);
hourly score counter `0x474c98` (clamped ±0x400) with item magnet
`0x4281d0` skipping the entity-record reinit; projectile manager
`0x00428280` last.

Timer semantics (all blocks): when the rate at `*src` lies in
(0.99, 1.01) (constants `0x470b68`/`0x470b64`) the count is derived from the
float accumulator (round half away from zero via `0x463b2c`); otherwise the
count steps by ±1 and the accumulator by ±1.0f. NaN takes the fixed-step
branch. The main mode timer freezes only through the accumulator path.

## Option rebuild (0x00426f70)

Phase 1 over `R+0x6c` ids: when the low word of `0x474c48` is >= 100,
soft-release (0x35c flag 0x4000000, child propagation via `ent+0x14` when
`ent+0x18==0`), zero, and respawn per `shot + character*3` script
0x14/0x15/0x16 through `0x448e30`; below 100 hard-release (`+0x304 = 1`).

Phase 2: `count = min(word/20, 4)`; early out when `player+0x3500 == count`
(early-out guards only the sprite rebuild). Per record: positions from
player `+0x3cc/+0x3d0`, soft-release + zero of `R+0x68`, index write
`R+0x88`, then character branches:

- character 0: offsets from `shot[(0x476f7c[count] + i)*12 + 0x20/0x24]` and
  `+0x98/0x9c` (each `*100` then `0x463b2c`), position sum chosen by
  `player+0x4474`, spawn script 0x11/0x12/0x13 (inline
  `0x449950`/`0x449870`/`0x4489d0` sequence), no callback stored.
- character 1, sub 0 (homing): static positions from `0x477834 + i*0x40 +
  0x43ac/0x43b0`, spawn-then-callback `R+0x90 = 0x427950`, spawn script
  0x11, then overwrite `R+0x3c/0x40` from the in-player history table
  `+0x436c + i*0x40`.
- character 1, sub 1: offset sums like character 0 without table, spawn
  0x12, no callback.
- character 1, sub 2: offsets, spawn 0x13, deliberate spawn-then-kill via
  `0x449470(&R+0x68, ESI=3)` when focused, callback `R+0x90 = 0x427ad0`.

Tail: remaining records get state 0 and hard release of `R+0x68`. Final
writes: `+0x3500 = count` and the four tier latches `+0x332c/+0x33c4/
+0x345c/+0x34f4 = 1`.

Documented asymmetries preserved: rebuild uses soft release, tail and
<100 paths hard release; early-out does not guard the R+0x6c effect
respawn; `count` (not `i`) selects the `0x476f7c` table entry.

## Timer semantics (verified against disassembly 2026-09-03)

All scaled timer blocks step by a fixed ±1 (accumulator ±1.0f) while the
rate at `*src` lies inside (0.99, 1.01] — outside that window, or for NaN,
the accumulator changes by the rate itself and the count is re-derived with
`0x463b2c` (round half away from zero), so a ~zero rate freezes the count.
The earlier subagent description had the branch direction inverted; the
disassembly (`TEST AH,0x41` / `TEST AH,0x5; JP` idiom) settles it.

Sub-effect records (EDI = rec+0x24, stride 0x6c): `rec+0x44 = rec+0x48`
(count = prev), accumulator `rec+0x4c` adjusts (−1.0f in-window, −rate
otherwise), `rec+0x48 = F2I(acc)`, and the active bit `rec+0x68` clears
when the F2I result is <= 0. The pre-adjust copy also covers
`rec+0x0 += rec+0x4` and `rec+0x8 += rec+0xc`, and the polar path zeroes
`rec+0x28` (z) after `0x44c5d0`.

The invincibility block `+0x430c` runs only when count > 0, sets
`+0x430c = count`, adjusts the accumulator (−1.0f in-window, −rate
otherwise), and re-derives `+0x4310 = F2I(acc)` in both branches.

## Known approximations

- The second option entity position update in movement (id `R+0x6c`)
  shares the float3 with the first; whether the native second call
  intentionally omits the +16 y offset needs one more verification pass.
- The dispatcher mode-0/1 stage activation loop is modeled as one pass; the
  native uses two compiler-split copy loops for the history shift whose net
  effect (tail saturation to `hist[optionCount*8]`, then shift) is what the
  movement module now implements.
- `0x404f30` / `0x426520` remain extern boundaries in
  `PlayerObjectLifecycle.cpp` (specs on file: 0x404f30 writes
  `vm+0x334..0x358 = 0`, `+0x3a0/0x3a1 = 0x10`, `+0x38a = script index`,
  then `0x43e710`; 0x426520 allocates via `0x44b360`, scales
  `buf+0x10/0x14` by `sin(π/4)` from the double at `0x470c50`, fixes up
  relative pointers at `buf+0x110 + i*8`, and resolves index tables
  `0x47476c/0x474778/0x491bf4/0x491bf8` over 0x34-stride records).
- Callees reconstructed elsewhere or still external keep their native ABIs
  behind `...Abi`-named externs.

## Ghidra names

- `0x425730` `UpdatePlayerModeDispatcherStackAbi`
- `0x426f70` `RebuildPlayerOptionRecordsStackAbi`
- `0x4269d0` `ProcessPlayerDeath` (external, unreconstructed)
- `0x4250b0` `UpdatePlayerMovement` (external, unreconstructed)
- `0x428280` `UpdatePlayerProjectiles` (external, unreconstructed)
