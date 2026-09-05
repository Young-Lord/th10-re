# TH10 0x00427e90 And Timer/Motion Helpers

Modules: `src/PlayerShotSpawner.cpp`, `src/PlayerTimerHelpers.cpp`,
`src/PlayerMotionHelpers.cpp`.

## 0x00427e90 SpawnPlayerShotStackAbi (player, descriptor, frame; ret 0xc)

Silent rejection paths: type-3 duplicate guard (`0x42f4 + slot*4` nonzero)
and a full 128-slot table (scan order 0..127, state at `+0x40 == 0` is
free). Initialization order on the claimed record:

1. `state = 1`, `descriptor = arg1`; the slot latch (flags bit 0 at
   `+0x3c`) arms only once per slot — snapshot/accumulator/rate-pointer
   seed with the `0xfff0bdc1` sentinel happens then; every spawn resets
   age/accumulator and writes snapshot `-1`. The movement-mode branch
   reads the latch **before** it is armed, so fresh slots take the polar
   path and re-fired slots keep their recorded mode.
2. Position: slot 0 copies the player position verbatim; other slots read
   the option table at `player+0x3244/0x3248 + slot*0x98` (integers ×
   0.01f), z = 0. Type 3 marks the guard slot.
3. Speed/angle from descriptor `+0x18/+0x14` (angle wrapped by
   `0x44bc70`); velocity per mode (polar rebuild with z = 0, or mode-1
   `speedAcc += accel`, `angle = wrap(speed + angle)`).
4. `pos += descriptor spawn offset (+0x4/+0x8) - vel` (one velocity-step
   behind the spawn point).
5. Entity A: pool VM (`0x449950`), `+0x20 = 0xf`, flag `0x40000000`,
   script `(i16)effect + 5` (`0x449870`), register + id (`0x4489d0`),
   validate against the manager lists (zero the slot when unresolved) and
   inject the spawn angle when the entity requests it (flag `0x08000000`
   → `+0x2c`, flag bit 2). Entity B only for type 3 (script `0x10`).
6. Descriptor update callback `+0x24`: ECX = player, EDX = record, frame
   on the stack (fastcall convention). Sound `(i16)+0x22 >= 0` queues via
   `0x43dd10` (12 channels × 128 entries, silent drops).

The caller `0x428160` performs no stack cleanup — the callee pops 12
bytes.

## 0x00405410 / 0x00404ed0 / 0x0044bf40 (PlayerTimerHelpers.cpp)

- Arm (`0x405410`): lazy init with the `-NaN` sentinel (only the flag and
  rate pointer survive), then `count = duration`, `prev = duration - 1`,
  `accumulator = (float)duration`.
- Forward tick (`0x404ed0`): `prev = count`; inside the (0.99, 1.01)
  window count += 1 / accumulator += 1.0f; outside (or NaN) accumulator
  += rate and count = round-half-away(accumulator).
- Shift (`0x44bf40`): `prev = count`; accumulator += delta (rate-scaled
  outside the window); count re-derived in both branches; the snapshot is
  deliberately left stale (the magnet's change detection relies on it).

## 0x0044bc70 / 0x0044c5d0 / 0x0044c2a0 (PlayerMotionHelpers.cpp)

- Angle wrap: subtract 2π while `value <= π`, then add 2π while
  `value < -π`, one shared budget of ~32 iterations per direction; an
  unconverged value (or NaN) returns unnormalized.
- Polar: `out = {cos(angle)·radius, sin(angle)·radius}`.
- Motion integration (block `{pos, vel, angle, radius, flags@0x28}`):
  flag bit 0 selects `pos += polar(angle, radius) + vel` (z = vel.z)
  versus componentwise `pos += vel`; x/y quantize with
  `floor(v * 100) * 0.01`, z untouched. This corrects the earlier
  1/200-rounding guess for the dispatcher's sub-effect records.
