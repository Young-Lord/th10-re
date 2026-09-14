# Player shot homing callbacks (0x00428ad0 / 0x00428b10) and angle delta (0x00428ce0)

Reconstruction in `src/PlayerShotHoming.cpp/.hpp`. Analysis was done
offline with objdump; every address below was read from the raw listing
of `resources/th10.exe`.

## Binding context

The 0x34-byte shot descriptors are bound at load time by the inline
chain binder at 0x004265b0 (inside the 0x426578 binding pass): each
sub-record's +0x24 index resolves through the dword table at 0x47476c
(entry 1 = 0x00428ad0) and +0x28 through the table at 0x474778 (entry 1
= 0x00428b10, entry 2 = 0x00428c20). 0x00428280
(`UpdatePlayerProjectilesStackAbi`) calls the +0x28 callback with
ECX = player and EDX = record; the spawner 0x00427e90 calls the +0x24
callback with the frame number on the stack. The existing
`PlayerProjectileManager.cpp` dispatch models only the ECX half of that
ABI and keeps working unchanged since 0x00428b10 never reads the
player.

## 0x00428ad0 AcquireHomingTargetEcxEdxStackAbi (ret 4, stack arg unread)

Clears record+0x4c, copies the homing target from player+0x3504, then
drops it again when the target's +0x1068 x coordinate is beyond 224.0f
(0x470b4c) from the playfield centre. The comparison is `fabs; fcomp;
test ah,0x41 / jne`: an unordered (NaN) x keeps the target.

## 0x00428b10 TickHomingShotMovementEdxAbi (ECX ignored, EAX = 0)

Record fields: +0x4 age, +0x14/+0x18 position, +0x2c speed, +0x30
angle, +0x40 state, +0x4c target.

1. State 2 returns immediately.
2. A target whose +0x2480 flag dword has bits 0x1/0x10 or 0xc0000 set is
   cleared from +0x4c.
3. Without a target: speed = min(speed + 0.1f (0x470c18), 16.0f
   (0x470b48)) and return — the comparison routes less/equal/unordered
   to the store and only an ordered "greater" clamps.
4. With a target: delta = 0x00428ce0(atan2(target_y +0x106c - y,
   target_x +0x1068 - x), angle) — the fpatan computes atan2(dy, dx).
5. Age >= 120 (0x78): speed += 0.2f (0x470c38), uncapped, no steering,
   return.
6. |delta| < pi/2 (1.5707964f @0x470c48; a NaN delta takes this branch
   via the unordered C0 flag): the inner 0.2653f (0x470c3c) comparison
   cannot change the outcome — both ordered sides run the identical
   min(speed + 0.1f, 16.0f) ramp, only the unordered case skips it.
   The dead-looking constant is preserved as a documented comparison.
7. |delta| >= pi/2 (ordered): speed - 0.3f (0x470c44) is compared
   against 4.0f (0x470c40) with `test ah,0x5 / jp`, which routes *every
   ordered* result to the 4.0f constant; only a NaN speed survives as
   speed - 0.3f.
8. Both paths end with angle = WrapAngleToPi(angle + 0.2f * delta)
   (0x44bc70) and the speed store; the caller then re-integrates the
   position from the polar pair.

## 0x00428ce0 WrapAngleDeltaStackAbi (stack a, b, ret 8, ST0 result)

Wraps a-b into [-pi, pi]: `diff > pi` (ordered) subtracts 2pi
(0x470b14); the second stage's `fcomp pi` returns the delta unchanged
for `(b - a) <= pi or unordered` and adds 2pi only when `diff < -pi`
ordered; NaN propagates.

## Sibling still at the boundary

0x00428c20 (table 0x474778 entry 2) is the option-locked spawn/position
variant: it seeds the record position from the option grid
(player+0x3244 + slot*0x98, scaled by 0.01f) and applies the descriptor
scroll offsets; it remains unreconstructed.
