# TH10 0x004281d0 / 0x00428160 / 0x00428280 / 0x00427950 / 0x00427ad0

Modules: `src/PlayerItemMagnet.cpp`, `src/PlayerProjectileManager.cpp`,
`src/PlayerOptionCallbacks.cpp`.

## 0x004281d0 TickItemMagnetEaxAbi (EAX = player, returns 0)

Gate: `+0x458 != 1` clears `+0x3504/0x3508` and returns. The autocollect
timer block sits at `+0x460` with its current value mirrored at `+0x464`:
- timer < 0 (inactive): return unless input bit0 (focus) is held, then
  arm the timer at 0 (`0x405410`).
- timer changed since the snapshot (`+0x464 != +0x460`): fire the
  schedule (`0x428160`, EDI = player, EBX = frame).
- timer > 14: focus held rewinds the accumulator 15 frames (`0x44bf40`)
  keeping the sawtooth; otherwise `0x405410(timer, -1)` cancels.
- otherwise: advance one frame (`0x404ed0`).

## 0x00428160 FireScheduledShotsEdiBbxAbi

Row = `min(0x474c48/20, 4)`, +5 when focused; the descriptor list pointer
lives at `shotBuffer+0x110 + row*8` (the same buffer loaded by
`0x426520`). Each 0x34-byte descriptor carries an i8 period at +0 (list
ends at a negative byte) and i8 phase at +1; when `frame % period ==
phase` the shot spawner `0x427e90` runs (still an extern boundary: it
allocates a record at `player+0x49c`, positions from the player or option
table, allocates the entities, and plays the optional sound).

## 0x00428280 UpdatePlayerProjectilesStackAbi (stack ret 4, returns 0)

Per active record (state at +0x40; 0 = free): type-3 despawn checks
(`+0x464 < 0` or `slot-1 >= +0x3500`, or the global abort:
`[0x47770c+0x9eb8] != 0` or `0x477704 == 0`) expire both entities via
`0x409e50` (word `+0x304 = 1`), set state 2, and clear the slot flag
`player+0x42f4 + slot*4`; the one-shot fire consumes `+0x54` via
`0x40c4d0` (word `+0x304 = 3`); the descriptor's `+0x28` update callback
runs with ECX = player; motion is either polar-rebuilt (`0x44c5d0`, z
forced 0) or self-integrated (speed += accel, angle wrapped via
`0x44bc70`); position integration/quantization via `0x44c2a0`; entity A
resolution failure deactivates the record (soft-releasing entity B);
off-field cull (skipped for type-3 and the first 10 frames) uses
`0x428d70` with half extents from the entity size descriptor — a pure
playfield-extents cull, no enemy collision or damage in this pass;
positions publish as `+224/+16` to both entities; the angle publishes
when the entity requests it (flag 0x08000000 → `+0x2c`, flag bit 2);
finally the age counter advances with the shared scaled-timer semantics.

## 0x00427950 UpdateHomingOptionRecord (ECX = record, returns 0)

Position-mode update only (no shots). Unfocused: freeze the spread offset
`R+0x44/0x48 = segment head - player fixed pos`. Focused: pin the
segment head to player + frozen offset, then fill the eight trail slots
with `head + (tail - head) * k / 8` (x87 0.125 at `0x470b90`, conversion
`0x463b2c`), where the tail is the entry one segment ahead
(`history + (idx+1)*0x40`). Tail: `R+0x34/0x38 = player + offset`,
`R+0x84 = mode`.

## 0x00427ad0 UpdateAngularOptionRecord (ECX = record, returns 0)

Focus/unfocus transition only. Unfocused: on a focused→unfocused edge set
the option sprite state word to 6 via `0x449470` and latch the anchor
`R+0x4c/0x50 = render pos`. Focused: on the reverse edge set the state
word to 3 and restore the anchor as the target. `R+0x84` caches the mode.
`0x449470` (EAX = &id, ESI = value) silently no-ops on stale ids.

## Notes

- The player pointer inside the callbacks comes from the global
  `0x477834` (`g_OptionPositionBase`), not from the record.
- The projectile pass publishes but never awards: damage/score are
  computed by consumers of the published entity positions.
- The `rec+0x48` double clear in the native deactivate path is kept as a
  single write; the duplicate is a compiler-visible dead store.
- Timers share the verified window semantics (fixed step inside
  0.99..1.01, rate-scaled outside/NaN).
