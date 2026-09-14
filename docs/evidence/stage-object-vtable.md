# Stage object callback-table family (0x41c030..0x41f670)

Reconstruction in `src/StageObjectVtable.cpp/.hpp`. Analysis was done
offline with `objdump` (the IDA MCP was unavailable this session); every
address below was read from the raw listing of `resources/th10.exe`.

## Identification

None of the twelve target bodies is referenced by an absolute data word, so
the "vtable" was located by raw little-endian search over the whole image:
the family is referenced from three null-terminated 19-entry callback tables
in `.rdata`, and the tables themselves are referenced from code:

| table | address  | installed by   | flavour |
|-------|----------|----------------|---------|
| T0    | 0x46dab0 | 0x41c030       | all-stub default (0x41bef0..0x41c020) |
| T1    | 0x46da60 | 0x41c5b0       | kind A objects (0xd58 bytes) |
| T2    | 0x46da10 | 0x41c680       | kind B objects (0xd74 bytes) |

Table references: `mov [edx],0x46dab0` at 0x41c030,
`mov [ebx],0x46da60` at 0x41c5b9, `mov [ebx],0x46da10` at 0x41c689.
The manager is the object published at DAT_0047781c (the existing modules
call it `g_BulletListRootSlot`); its slot-5/slot-7 broadcasts are the already
reconstructed `BroadcastBulletClearEaxAbi` (0x41c850) and
`BroadcastEntranceTweenEaxEbxStackAbi` (0x41c800), which confirms the slot
roles: slot 5 = bullet-clear spread, slot 7 = radial entrance sweep.

### Slot map

| slot | T1 (kind A) | T2 (kind B) | role |
|------|-------------|-------------|------|
| 0    | 0x41ca80    | 0x41bef0 (ret) | per-frame script interpreter (A only; 18 records of 0x18 bytes at obj+0x460, 64-opcode jump table 0x41cf78/0x41cf8c) |
| 1    | 0x41c8c0    | 0x41e5c0    | descriptor spawn (this, descriptor; ret 4) |
| 2    | 0x41d3d0    | 0x41e700    | per-frame update (this; ret) |
| 3/4  | 0x41d7c0 / 0x41d870 | 0x41ea40 / 0x41eaf0 | not reconstructed (boundaries) |
| 5    | 0x41e260    | 0x41f400    | bullet-clear spread (this, flag; ret 4) |
| 6    | 0x41d880    | 0x41eb00    | box sweep (this, pos, extent, flag; ret 0xc) |
| 7    | 0x41dd80    | 0x41efa0    | radial sweep (this, target, radius, flag; ret 0xc) |
| 8    | 0x41e4d0    | 0x41f670    | not reconstructed (boundary) |
| 9..18| stubs / 0x41d2c0 | stubs  | feature-flag dispatch targets (no stack args) |

## Object layout (header shared by both kinds)

- +0x00 table pointer; +0x04/+0x08 manager list node (head manager+0x434,
  count +0x438, cursor +0x43c, cap 0x100)
- +0x0c state dword (A spawns set 2, B spawns set 3, spreads latch 1)
- +0x10 timer record (0x405410 tick target); +0x14 tick value; +0x18 rate
- +0x24/+0x28/+0x2c position; +0x30/+0x34/+0x38 velocity
- +0x3c angle; +0x40 depth; +0x44 alpha; +0x48 depth rate; +0x4c depth
  velocity; +0x50 byte sweep latch; +0x404 feature flags; +0x40c cutoff kind
- +0x424 descriptor copy (A 0x1dc bytes, B 0x1f8 bytes)
- kind A descriptor words: +0x430 angle, +0x438 depth, +0x440 alpha,
  +0x444 z rate, +0x448/+0x44a script/kind words, +0x44c flags byte
- kind B descriptor words: +0x43c angle, +0x444 z limit, +0x448 depth,
  +0x44c alpha scale, +0x450 z rate, +0x454/+0x458/+0x45c/+0x460 entrance
  timers, +0x464/+0x466 script/kind words, +0x468 flags byte
- animation VM records: A +0x600/+0x9ac, B +0x61c/+0x9c8 (0x3ac each);
  rec+0x35c flags, rec+0x304 mode word, rec+0x394 pointer to the scale
  record (+0x30/+0x34 divisors)

## Bodies

- **0x41c030 `InitStageObjectHeaderDefaultsEdxAbi`** - stores T0 into
  [edx], clears bit 0 of +0x20, of eighteen +0x68+0x34*i dwords and of
  +0x420, then `memset(edx, 0, 0x424)` erases all of it; the surviving
  behaviour is the tail: lazy +0x10 timer seed (sentinel 0xfff0bdc1, rate
  pointer &flt_476f78, bit 0 of +0x20) overwritten by +0x10 = -1,
  +0x14/+0x18 = 0. The dead stores are kept in the native order.
- **0x41c100 `CreateStageObjectManagerInPlaceEsiAbi`** - calls 0x41c030 on
  esi+0x10 (dead), zeroes 0x45c bytes, publishes DAT_0047781c = esi.
- **0x41c5b0 / 0x41c680 `InitStageObjectKindA/B_EbxAbi`** - header defaults,
  table pointer, descriptor-region wipe (+0x1dc/+0x1f8 bytes; B additionally
  stores 8.0f at +0x450), then per VM record: nine dead flag-bit clears,
  0x3ac-byte wipe, word +0x384 = 0xffff. The native reads rec2+0x6c before
  the second wipe (obj+0xa18 / obj+0x688 / obj+0xa34) - dead, preserved as
  commented reads.
- **0x41c510 `SpawnStageObjectEsiEdiStackAbi`** (ret 4) - ESI = manager,
  stack = kind, EDI forwarded verbatim to the slot-1 call (native leaks the
  caller's EDI; both call sites pass a descriptor pointer). Cap
  [+0x438] >= 0x100 returns null; cursor +0x43c wraps past zero to 1; kind 0
  allocates 0xd58 bytes and calls 0x41c5b0, kind 1 allocates 0xd74 bytes and
  calls 0x41c680; other kinds reach the int3 padding in the binary (modeled
  as null). The +0x54 store happens before any null check on the allocation
  (native would fault into the list link); the object is then linked at the
  head of the manager+0x434 list and slot 1 is dispatched through the
  installed table.
- **0x41c8c0 `StageObjectSpawnDescriptorA`** (slot 1) - copies 0x1dc bytes,
  state = 2, binds VM1 (script = DAT_00474170[(i16)+0x448] + (i16)+0x44a
  through 0x404f30 + 0x43ee30), rec1+0x304 word = 2, descriptor flag bit 0
  lifts rec1+0x35c bit 0x10, rec1+0x35c masked 0xfc63ffff | 0x600000; VM2
  (script = kind + 0x103), rec2+0x304 = 2, rec2+0x35c bit 0x10 (always),
  rec2+0x35c masked 0xfc7fffff | 0x400000. Lazy arm of the +0x410 timer then
  +0x414 = 0x1e, +0x418 = 30.0f, +0x410 = 0x1d. Motion seed: depth =
  [+0x438], position = descriptor +0/+4/+8, z rate = [+0x444], angle =
  [+0x430], alpha = [+0x440], kind = 0x18, depth velocity = 0.01f when the
  depth is positive else 0, velocity = SetVectorFromAngle(0x41f800) with
  (angle, length) = (+0x430, +0x444).
- **0x41e5c0 `StageObjectSpawnDescriptorB`** (slot 1) - kind B twin with the
  0x1f8-byte copy, state = 3, scripts from +0x464/+0x466, rec flag bits from
  descriptor +0x468 bit 1, position from descriptor +0/+4/+8, angle +0x43c,
  depth +0x448, alpha = 2.0f, z rate +0x450.
- **0x41d3d0 `StageObjectUpdateA`** (slot 2) - slot-0 dispatch through the
  table; ten-way feature dispatch on +0x404 (bits 1/0x10/0x20/0x40/0x100/
  0x80/0x8000c00/0x100000/0x200000/0x4000000 to slots 9..18; bit 0x80 is
  tested with the `test al,al; jns` idiom); bit 0x8000 shifts the +0x15c
  timer by -1 while +0x160 > 0 else clears the bit. Depth handling: while
  the depth is below +0x434 it advances by rate*(+0x48) and clamps to the
  limit; once above, the depth velocity accumulates, the velocity drifts
  the position, and a reflection at the +0x43c ceiling sets depth = zlim =
  ceiling - depth velocity (a result <= 0 releases the object, return 1).
  When the +0x414 timer has expired, tip 1 is emitted from
  (angle, depth) and released when both 0x41f7a0 checks (extent = alpha)
  report out-of-field. Tip 2 (depth > 16, alpha > 3) runs the 0x4267f0
  player-region classifier with (angle, half_y, depth*0.8): region 1 calls
  table slot 6 with (player pos, tip2, 0); region 2 every fifth tick spawns
  effect 0x1b2 (0x448db0) and queues sound 0x1c (0x43dd10). Finally
  rec1+0x35c |= 8, rec1+0x3c = alpha / scale->+0x34, rec1+0x40 = depth /
  scale->+0x30 (scale = *(rec1+0x394)), 0x43ee30(rec1) and, when the depth
  velocity is exactly zero, 0x43ee30(rec2).
- **0x41e700 `StageObjectUpdateB`** (slot 2) - kind B update: depth advance
  against +0x444 with clamp, angle wrap via 0x44bc10
  (`WrapAngleSumStackAbi`), optional position follow from
  [DAT_00477704+0x10]+0x1068 when descriptor +0x468 bit 0 is set, entrance
  drift from +0x430/+0x434/+0x438, the four-state entrance machine
  (states 2..5 via jump table 0x41ea30; tick +0x14, timers +0x454/+0x458/
  +0x45c/+0x460, alpha scale +0x44c with the rate at +0x18; reaching the
  +0x460 timer returns 1), the same tip-2 emission (with the 1/3 divisor
  0x470cc4 instead of 0.5) and the VM publication with the B record bases.
- **0x41cfd0 `StageObjectCutoffRespawnA`** (slot 15, flags 0x8000c00) -
  leaves early while the position is inside x=[-192,192) y=[0,448); on exit
  queues the cutoff ring entry (0x43dc90 with value 0), snapshots
  (position + velocity) and the reversed angle into the descriptor header,
  copies +0x13c to +0x444 and spawns a fresh kind A object through 0x41c510
  with the descriptor pointer; the flag-0x8000000-gated second snapshot
  repeats the spawn; the tail always clears 0x8000000|0xc00 from +0x404.
- **0x41d2c0 `StageObjectDriftSlotA`** (slot 10, flag 0x10) - frame-time
  drift: below the +0xb4 tick limit the depth rate (+0x48) and velocity
  triple (+0x30/+0x34/+0x38) are scaled by `frame_time_scale *` the
  +0xa0/+0xa8/+0xac/+0xb0 multipliers, the +0x3c angle is rebuilt from
  atan2(vel.y, vel.x) once either magnitude passes 7.5f (0x470c58; NaN
  skips), the old tick publishes to +0x8c, and the {+0x90 count,
  +0x94 accumulator, +0x98 rate pointer} record advances with the shared
  0.99/1.01 window; reaching the limit clears flag 0x10 of +0x404 and
  stops the drift. (Reconstructed 2026-09-14; see the Bodies list in
  src/StageObjectVtable.cpp.)
- **0x41d170 `StageObjectDistanceShrinkA`** (slot 12, flag 0x40) - distance
  shrink: below the +0x11c distance the vector length is
  base*(1 - rate/+0x11c); on reach it queues the cutoff, advances the +0x124
  counter (>= +0x120 clears flag 0x40), drifts the position by +0x10c,
  reloads the base from +0x108, re-arms the +0xf4 timer record and rebuilds
  the vector with the full +0x11c length. Both paths publish +0xf4 = +0xf8
  and advance the rate record: in the (0.99, 1.01) window (0x470b68/0x470b64)
  the integer timer and the accumulator step by 1; otherwise the accumulator
  += *rate pointer and the timer is rounded half-away-from-zero through
  0x463b2c.
- **0x41e260 / 0x41f400 `StageObjectSpreadA/B`** (slot 5) - bullet-clear
  spread: from the object position, every 12 degrees while the step start
  plus 6 degrees stays below +0x40, spawn the ring VM (script kind*2+0x11
  through the 0x448db0 composition: pool alloc DAT_00491c10, +0x35c |=
  0x40000000, position +224/+16, ANM bind from [DAT_00477818+0x458], list-A
  registration). Flag argument gates the explosion particle 0x41bb00
  (kind 8, colour -1, angle -pi, speed 0.6, gated by 0x41f7a0 with a 32.0f
  box). Kind B additionally gates each spawn with the 16-degree playfield
  box and latches state = 1 at the end.
- **0x41d880 / 0x41eb00 `StageObjectSweepBoxA/B`** (slot 6) - box sweep.
  Half-extents = argument extent * 0.5 around the argument position; the
  point starts at the object position and advances by
  SetVectorFromAngle(angle +0x3c, 6.0f) per 12-degree step while the step
  start stays below +0x40. Hits mark the 0x40-byte bitmap, count, spawn the
  ring VM at the initial position and (flag permitting, inside the 32-degree
  playfield box) the explosion particle. With no hits the sweep ends; when
  every step hit, +0x50 latches. Otherwise the argument position is advanced
  across the leading hit run, the depth steps back by run*12 degrees
  (published to +0x434/+0x4c while above 18 degrees, else the +0x50 latch),
  and each following miss run wider than 18 degrees re-spawns a whole stage
  object from the descriptor copy through 0x41c510.
- **0x41dd80 / 0x41efa0 `StageObjectSweepRadialA/B`** (slot 7) - radial
  twin: hit test dx^2+dy^2 against the squared radius argument, ring VMs
  spawn at each swept point, and gap handling spawns ring VMs at
  target + dir*run_start instead of stage objects.
- **0x41f7a0 `IsBoxOutsidePlayfieldEcxStackAbi`** (ret 8) - returns 1 when
  the box {center +/- d} leaves the playfield x=[-192,192), y=[0,448).
- **0x43dc90 `QueueEffectRingValueEcxStackAbi`** (ret 4) - the effect-ring
  push used by the cutoff paths: 12-slot kind table at manager+0x620, counts
  at +0x650, 128-entry rings at +0x680, kind lookup word at 0x4749ce+8*kind,
  counter stored to manager+0x408+4*kind. (0x43dd10 is the -990-encoded
  sibling already reconstructed in `TitleBulletUpdate.cpp`.)

## Preserved quirks

- 0x41c030/0x41c100/0x41c5b0/0x41c680: stores erased by the following
  memset are kept in the native order (dead-store quirk).
- 0x41c510: the caller's EDI is forwarded to the slot-1 call; the +0x54
  store runs before the allocation result is checked; kinds other than 0/1
  reach int3 padding.
- The per-record flag clears before each 0x3ac wipe are dead.
- Tip z components are seeded from stale stack slots in the native tip
  blocks; the reconstruction tracks the carry explicitly and comments it.
- 0x41d880 stores 32.0f/32.0f/0.0f into caller stack slots that no callee
  reads (dead stores; not modeled).
- /GS cookie setup and 0x458ea5 `__security_check_cookie` calls are compiler
  artifacts and are not modeled.

## Boundaries and fidelity notes

- 0x41ca80 (T1 slot 0, the 1.4 KB instruction-queue interpreter), slots
  0x41d7c0/0x41d870/0x41e4d0 (T1) and 0x41ea40/0x41eaf0/0x41f670
  (T2) remain boundaries (T1 slot 10, 0x41d2c0, is reconstructed above); the dispatch sites call through the table so they
  can be dropped in later.
- 0x4267f0 (rotated player-region classifier, results 0/1/2) is kept as an
  explicit boundary stub with its ABI documented; it is the sibling of the
  reconstructed 0x4266b0.
- The kind B sweeps are structurally identical to their kind A twins apart
  from the kind word and descriptor size; they share `SweepCommon`.
- The sweep depth bookkeeping in `SweepCommon` is a semantic
  re-expression of dense x87 sequences (fsubr/fcom chains at
  0x41dbb1..0x41dd50); the loop, bitmap, latch, spawn gating
  (gap*12 > 18) and spawn shapes are verified against the listing, but the
  exact subtract order inside the two depth-update branches deserves one
  more raw-bytes pass.
