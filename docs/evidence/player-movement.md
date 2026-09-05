# TH10 0x004250b0 — Player Movement

Module: `src/PlayerMovement.cpp` (`UpdatePlayerMovementEdiAbi`). Native
entry receives the player in EDI (dispatcher 0x425730 does
`MOV EDI,EBP; CALL 0x4250b0`) and returns 0.

## Behavior (program order)

1. Input byte `0x474e5c`: bit2 focus, bit4 L, bit5 R, bit6 U, bit7 D.
   Diagonals require both bits and win over cardinals; states:
   UL=5, UR=7, DL=6, DR=8, R=2, L=1, U=3, D=4, idle=0 → `+0x454`.
2. Focus gate: when `0x477704` is null, `+0x60` is zero, or the second
   timer count `+0x48c < 4`, force `+0x4474 = 0` and snap `+0x4308 = 30`.
   Otherwise `+0x4474 = (input>>2)&1`, and only when
   `shot_type + character*3 == 5` does `+0x4308` refill (0 while focusing,
   +1 per frame to 30 otherwise).
3. Unfocused branch: stops and clears the tracked focus-effect entity
   `+0x329c` (via `0x4491c0`/`0x449210`), speed switch uses `+0x3d4`
   (cardinals) and `+0x3dc` (diagonals, same value both axes); direction
   changes fire animation ids through `0x43e710`
   (enter-left 1, leave-left 2, enter-right 3, leave-right 4).
4. Focused branch: lazily spawns the tracked entity (pool VM via
   `0x449950`, type 9 at `+0x20`, flag 0x40000000, script 0x160, id via
   `0x4489d0` → `+0x329c`); speed switch uses `+0x3d8`/`+0x3e0`; no
   direction animations.
5. `+0x44c/+0x450` = dx/dy; one frame-scaled fixed-point step:
   `round_half_away(dx * frameScale)` (sub-stepping lives in the caller's
   timer structure, not here); results to `+0x3f0/+0x3f4`, added to
   `+0x3cc/+0x3d0`.
6. Clamps: x ∈ [-0x47e0, 0x47e0], y ∈ [0xc80, 0xa8c0]; float sync
   `+0x3c0/0x3c4 = int * 0.01f`; tracked entity position published with
   the +224/+16 offset (via `0x4492f0`, fields set verbatim after the
   caller adds the offsets), clearing `+0x329c` when the id no longer
   resolves.
7. History ring `+0x436c` (32 dword pairs): only shifts while unfocused
   and moving; the valid window is `optionCount*8` pairs with the tail
   saturated to that entry before the shift; `hist[0] = current fixed
   position` runs every frame.
8. Option loop (all 4 records, stride 0x98; state 0 skipped): target =
   fixed position + `R+0x44/0x48` (unfocused) or `R+0x4c/0x50` (focused);
   `R+0x90` callback invoked with ECX = record when non-null; render pos
   `R+0x3c/0x40` snaps one-shot when `R+0x8c != 0`, else lerps only while
   `+0x4308 > 29` (signed division by 100, snap when both deltas are 0);
   both entity ids `R+0x68` and `R+0x6c` receive
   `{0.01*R3c, 0.01*R40, 0}` through `0x449350` (which adds the 224/16
   offset). Whether the second entity intentionally omits the +16 needs a
   verification pass; the current model uses the same offset for both.

## Callee contracts

- `0x4491c0` find by id over the two manager lists (EDX = manager).
- `0x449210` stop entity (`word +0x304 = 1`) + children via `ent+0x14`
  when `ent+0x18 == 0`.
- `0x4492f0` set `ent+0x340/344/348` verbatim from an ESI float3.
- `0x449350` set `+0x340 = p0 + 224.0f`, `+0x344 = p1 + 16.0f`,
  `+0x348 = p2` from an ESI float3.
- `0x43e710` direction animation (ECX = owner `player+0x10`, EAX = VM
  `player+0x14`, EBX = animation id).
- `0x427950` / `0x427ad0` per-record callbacks (ECX = record).
