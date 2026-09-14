# Player Proximity Entity Fade (TH10 0x419650 / 0x4198a0)

Reconstruction: `src/PlayerProximityFade.cpp`
(`UpdateTrackedEntityProximityFadeEdiAbi` body and
`TimelineVmProximityFadeThunkEcxAbi` thunk). The 0x419120 helper
(`UpdatePlayerAttachedObjectEaxStackAbi` boundary here) remains
unreconstructed; it is a large player-attached object updater.

## Registration and ABI

- The manager factory at 0x418e30 registers 0x4198a0 with the scheduler
  (`push 0x4198a0; call 0x449ed0`), so the callback runs on the calculation
  chain each frame with the record in ECX.
- 0x4198a0 thunk: `push ebx; mov ebx,ecx; call 0x419650; pop ebx; ret`.
  It does not set eax, so the native thunk leaks the scheduler's prior eax;
  the reconstruction returns the body's 1.
- Body: usercall over EBX = the manager record; always returns 1.

## Record fields

- `+0x0d8` stage gate dword; a mismatch with dword_00474c84 resets `+0x14`.
- `+0x0dc + i*4` tracked entity ids (10 slots; reset to 0 when lost).
- `+0x104 + i*0xc` tracked position pairs (float x, float y, 4 pad).
- `+0x17c + i*4` tracked half widths (floats, compared halved).
- `+0x10`, `+0x14` frame counters, both incremented on success.
- `+0x1c + shotmode*0xc` player-attached object forwarded to 0x419120 with
  stack arguments (record, 1) and EAX = the object.

Globals: dword_00474c84 (stage gate), dword_00474c7c (shot mode),
DAT_00477834 player block (`+0x3c0/+0x3c4` field-space player position),
DAT_00491c10 render owner (entity lists at `+0x72dad4`/`+0x72dadc`, each
node `{entity, next}` matched on the entity's first dword = id).

## Per-slot fade

Entity gates: skipped entirely when the id is 0/unresolvable (the id slot
is then zeroed) or when the entity's `+0x12c` flag is non-zero.

With d1 = |player_x - pair.x|, d2 = |player_y - pair.y|,
h = `record+0x17c[i] * 0.5`, e = `entity->f40 * entity->f50 * 0.5`,
s = signed dword `entity+0x310`, step = `s*3/4` (native signed idiom):

- `d1 <  h` (any d2)            -> alpha = low byte of s (identity)
- `d1 >= h, d2 <  e`            -> alpha = (low byte of s) >> 2
- `d1 >= h, d2 <  e+32`         -> alpha = low byte of s
- `d1 >= h, d2 >= e+32`         -> alpha = trunc(s - (e+32-d2)*step/32)

The final store writes the byte to `entity+0x2ff` (the render alpha byte
consumed by the render modes; see AsciiRenderModeDispatcher.cpp /
TimelineRenderObjectSetup.cpp).

Native decision-tree shape (fcom/fnstsw chains, jp on C0|C2 and jne on
C0|C3, so unordered follows the branch target): the x-halo fade
`trunc(s - (h+32-d1)*step/32)` for `d1 >= h+32 && d2 < e` is structurally
present but unreachable behind the earlier `d1 >= h && d2 < e` exit; it is
preserved in the reconstruction for fidelity. Constants: 0x470b4c = 224.0
and 0x470b48 = 16.0 field-centre offsets added to the player position,
0x470bcc = 32.0 halo, 0x470d20 = 1/32 step scale, 0x470b0c = 0.5.

## Verification notes

- Disassembly 0x419650-0x41988a (571 bytes), thunk 0x4198a0-0x4198a9.
- Only callers of the body go through the thunk (single call site at
  0x4198a3); the factory registration is the only reference to 0x4198a0.
- The gameplay role of the ten tracked slots is not pinned down further
  (the record is created by the 0x418xxx factory family); the field-level
  behavior above is fully derived from the disassembly.
