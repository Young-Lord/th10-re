# Script-Name Request Popper (TH10 0x004127a0)

Reconstruction: body of `AcquireScriptNameRequestThisAbi` at the end of
`src/EclScriptLibrary.cpp` (replaces the former boundary declaration; call
sites in the same file at the boss damage/kill passes are unchanged).

## Native ABI

`__thiscall` ECX = ECL script manager. Returns the popped request id, or 0
when nothing is due.

## Slot pool

Eight request slots at manager+0x2494, stride 0x10 (initialized to
{-1, -1, 0} by the ctor, see EclEasedTransforms.cpp):

- `+0x00` scheduled battle-frame deadline (-1 = empty)
- `+0x04` secondary countdown deadline (-1 = disarmed), measured against
  the `+0x115c` animation-tail timer
- `+0x08` request id handed to the caller (the call sites pass it to
  0x450470 `ResolveScriptTableIndexEaxAbi` after 0x40c6e0/0x40c730)

Related manager state: battle timer `+0x23fc`, published deadline delta
`+0x2404`, `+0x2480` flag dword (bit 0x10000 = secondary countdown expiry),
animation tail `+0x1158..+0x1168`.

## First pass (primary deadline)

Scans slots in order for the first with `slot+0 >= 0`:

1. Always publishes `[+0x23fc] - slot+0` into `+0x2404` (the manager timer
   delta consumed elsewhere, e.g. the 300/900 gates in this file).
2. If the battle timer is already past the deadline (`>`), the slot is
   NOT consumed and control falls into the second pass.
3. Otherwise the slot is consumed: the deadline becomes the new `+0x23fc`,
   `slot+0 = -1`, the `+0x1158` animation tail is reset (one-time init
   behind flag bit 0 with the 0xFFF0BDC1 poison and the &flt_476f78 rate
   pointer, then unconditional stopped-state arm with prev = -1 — same
   shape as the tails documented in EclEasedTransforms.cpp),
   `+0x2480` bit 0x10000 is CLEARED, and `slot+8` is returned.

## Second pass (secondary countdown)

Scans slots for `slot+0 >= 0 && slot+4 > 0`:

1. HUD countdown: `(slot+4 - [+0x115c] + 59) / 60` clamped at 99 (native
   signed divide-by-60 via magic 0x88888889) stored into
   `(*(0x47770c))+0x9ec0` (g_AsciiHudOwner seconds field).
2. While `[+0x115c] < slot+4`, returns 0 without consuming (the countdown
   display was still updated).
3. At expiry, consumes the slot with the full side effects: tail reset as
   above, `+0x2480` bit 0x10000 SET, score block dword 0x474c4c decremented
   by 3000 (stored unconditionally, then clamped to 5000), and — only when
   the spell-bullet base (0x4776f4) has `+0x378c` bit 0x8 clear and
   `+0x3738 >= 60` — zeroes `+0x3790` and clears bit 1 of `+0x378c` and of
   the seven per-VM flag dwords at `+0xad4, +0xe80, +0x15d8, +0x1984,
   +0x1d30, +0x20dc, +0x2488` (0x3ac stride VM pool fields).
   Then `slot+8` is returned.

Falls through to `return 0` when neither pass finds a due slot.

## Verification notes

- Disassembly 0x4127a0-0x4129f0 (593 bytes), tail return at 0x4129eb.
- Call sites: 0x40e1c2 and 0x40e27b (the ECL script manager step), which
  call 0x40c6e0/0x40c730 around the pop and resolve the id via 0x450470
  with EAX = manager+0x102c (the persistent request object).
- The earlier boundary guess ("returns the next pending request or 0") was
  directionally right but missed the two-pass deadline logic and the
  expiry side effects documented above.
