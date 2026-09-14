# Bullet Manager Per-Frame Update Cluster (TH10 0x0041afd0 / 0x0041ba00 / 0x0041ba50)

Implemented in `src/TitleBulletUpdate.cpp/.hpp` (semantic bodies plus the
small effect/score helpers 0x0041beb0, 0x00426660, 0x00405b60, 0x0041be80,
0x00418930, 0x0043dd10, 0x00412ff0 and the boundaries 0x004054b0 /
0x0042b9c0). All offsets/constants below were read from the idalib
disassembly of th10.exe; the Hex-Rays decompiler fails on 0x0041afd0, so
that body was reconstructed directly from the listing.

## Object layout

- Bullet manager argument (DAT_00477818): 0x896 bullets of 0x3f0 bytes
  starting at +0x14 (the exact region `memset(+0x14, 0, 0x21cea0)` of the
  title calc body game-start reset), with two counter dwords at
  +0x21ceb4 / +0x21cebc.
- Per-bullet fields (record base = slot + 0x3ac; the native addresses them
  through `ebp = slot + 0x3b0`): position x/y/z at +0x3ac/+0x3b0/+0x3b4,
  velocity x/y/z at +0x3b8/+0x3bc/+0x3c0, counter dword at +0x3c8,
  integer distance at +0x3cc, float distance at +0x3d0, pointer to the
  speed limiter float at +0x3d4, movement state at +0x3dc, kind at
  +0x3e0, speed at +0x3e8, spawn delay at +0x3ec.

## 0x0041ba00 BulletCalcRecordCallbackEcxStackAbi

Native ECX = the manager (also forwarded as the one stack argument).
Gate on DAT_00477810: while it exists and `+0x58` has bit 0/0x100
(`((flags | flags>>2) & 1) != 0`) or bit 0x400 set, return 1 without
updating; otherwise run 0x0041afd0 on the same manager.

## 0x0041afd0 UpdateBulletManagerStackAbi

Clears +0x21ceb4/+0x21cebc, then walks all 0x896 bullets:

- State 0: skip (native jumps straight to the loop advance).
- State 5 (spawn pending): `--delay` (+0x3ec); while >= 0 skip; otherwise
  state = 2 and 0x00404f30 initializes the player script VM
  (EAX = kind + 0x176, ESI = slot, stack = [*DAT_004776f0] + 0x3e0b50);
  no VM tick this frame.
- State 1: if the option-position manager's +0x458 is 2/4 or the player
  y (flt at mgr+0x3c4) is below 134.0f, the bullet moves with gravity
  (velocity scaled by flt_476f78; `vy += 0.03f*scale`; `vx = 0` when
  `vy >= 0`; vy clamps to 2.0f when above; state = 0 when `vx > 500`).
  Otherwise it homes: speed = [*mgr+0x45c]+8, state = 3.
- State 2: same gravity movement; `vy >= 0` transitions to state 3;
  `vy < 0` and y > 500 deactivates with AddPowerValue(0x474c40, -4)
  (0x00405b60); otherwise falls through to the kind gate.
- State 3: angle = atan2(player - bullet) with the exact (0,0) case
  returning pi (0x3fc90fdb); 0x0041beb0 converts (angle, speed) into the
  velocity; movement; speed += 0.2f once speed >= 12.0f; option state 4
  switches to state 1 and zeroes vx/vy.
- State 4: angle via 0x00426660 (special case dx == dy == 30.0f ->
  1.75f), same movement/accel; option state 4 exit as above.
- Kind gate: option state 2 short-circuits to the VM tick. Otherwise the
  bullet must be inside the play-field rectangle [mgr+0x4330, mgr+0x4324]
  x [mgr+0x4334, mgr+0x4328] to run the 11-case switch on +0x3e0
  (kind-1); off-screen bullets take the retargeting path below.

Bonus switch (all exits skip the VM tick; cases falling through share the
death effect with kind 0x14 and state = 0):

- Cases 1/10: AddLifeFragment(frame, 1) (0x00418930); digit anim
  0x004054b0 with (word_474c48/20, rem*5) on the HUD; on ladder crossing
  RebuildPlayerOptionRecords, death effect kind 0x1d, PIV digits
  (0x0042b9c0) with value -480/color 0xffffff40, AddPowerValue(+0xc) and
  (word >= 100) the deferred cleanup flag; otherwise PIV digits
  (|word|/2, 0xffff4040). Popup timer tick 0x00412ff0 with 0x3c.
- Case 2: player y >= 150 -> value = piv*10 rounded down to a multiple of
  10, color 0xffffff00, power +8; else the formula
  `(piv*10 rounded)/2 - (py-150)*kBonusScale*(piv*0.5 - 5120)` (floored to
  a multiple of 10), color -1, power +1; then AddScoreBlockValue and a
  0x64 popup tick.
- Case 5: as case 2's fast branch (power +8).
- Case 3: difficulty table (5000/5000/8000/10000/10000), color
  0xff00ff00, AddPivValue (0x0041be80), 0x78 popup tick.
- Case 8: AddPivValue(10), popup 3, AddScoreBlockValue(10).
- Case 9: PIV digits 100 (0xff00ff00), AddPivValue(100), popup 0x3c.
- Cases 4/11: as case 1 but fragment increment 0x14 and AddPowerValue
  +0x18 / popup 0x14.
- Case 7: 0x004188a0 AwardExtendedLifeEaxEcxAbi(frame, 1) (semantic body
  file-local to TitleCalcCluster.cpp; declared as the boundary
  `AwardExtendLifeEaxEcxAbi`), AddPowerValue(+0x100).
- Default/case 6: death effect kind 0x14, state = 0.

Off-screen path: states 3/4 tick without retargeting; otherwise the
DAT_00474e5c bit-2 gate selects the rectangle [mgr+0x4348..0x434c]
(inside -> state = 4, speed = [*mgr+0x45c]+8 * 0.35f) or the
[mgr+0x4360..0x4364] rectangle; when the gate bit is set and the first
rectangle misses, the bullet only ticks.

Tick path: 0x0043ee30 (VM pair, one stack arg = slot), then the distance
bookkeeping — counter copy (+0x3c8 = +0x3cc bits), and either the integer
advance (`+0x3cc += 1`, `+0x3d0 += 1.0f`) when the speed limiter float is
in the (flt_470b68, >= flt_470b64) window, or `+0x3d0 += speed` with
`+0x3cc = (int)+0x3d0`. Every iteration increments manager+0x21ceb4.
After the loop a set deferred flag runs 0x0041ba50; returns 1.

## 0x0041ba50 KillPendingBulletsEsiAbi

Walks 150 position slots (manager+0x3c0, stride 0x3f0; slot+0x30/+0x34
are the state/kind dwords): an active slot with kind 1 or 4 is cleared,
the explosion particle 0x0041bb00 is queued (kind 9, color -1, angle
-pi/2, speed 2.2 — the existing `SpawnExplosionParticleEaxEcxEfxAbi`)
and a death entity spawns through the 0x00448db0 boundary (kind 393,
script list `[*(&byte_477710+16)] + 4066128`).

## Reconstructed helpers

- 0x0041beb0 SetPolarVectorThiscall: v0 = cos*r, v1 = sin*r.
- 0x00426660 AngleToPlayerPositionEaxEcxAbi: atan2(dy, dx) vs the player
  at mgr+0x3c0/0x3c4, with the (30, 30) special case returning 1.75f.
- 0x00405b60 AddPowerValueEaxEcxAbi: `+0x58 += delta` clamped to
  [-1024, 1024].
- 0x0041be80 AddPivValueEcxStackAbi: `+0x0c += value/10` capped at 99999.
- 0x00418930 AddLifeFragmentEaxStackAbi: word +8 += inc (no-op from 100);
  crossing above 100 clamps to 100, releases the entity at
  `[*(u32*)DAT_0047773c + 40472]` through the shared
  `ReleaseEntityById` (manager DAT_00491c40) and respawns it (kind 73,
  script `[slot + 40648]`; the native third stack argument 15 stored to
  entity+0x20 stays an unmodeled omission as in TitleCalcCluster.cpp);
  returns `((after - inc) / 20 != after / 20)`.
- 0x0043dd10 QueueBulletDeathEffectEbxEsiStackAbi: encodes the offset as
  `(int)(offset * -990.0)`, dedupes the kind in the 12-slot table at
  mgr+1568 (entries mgr+1664, counts mgr+1616, kind words mgr+1032,
  source word_4749fe[4*kind]) with a 128-entry ring per slot.
- 0x00412ff0 AdvanceScorePopupTimerEdiStackAbi: while +0x18 < 130 shifts
  the +0x14 timer (0x0044bf40 boundary, shared ShiftTimerByEsiStackAbi
  body); on crossing 130 re-seeds the record (first-use pattern with
  &flt_476f78) to 130 / 150.0f / -999999 / 129.

## Boundaries

0x00404f30 (EAX/ESI/stack), 0x004054b0 (EDX/ESI/stack — three
0x0043e5a0 calls on hud+0x6a8c with resource [hud+0x9ec8] and glyph
indices 8 / arg/10+8 / arg%10+8), 0x0042b9c0 (EAX/EDI/ESI/stack — the
720x64-byte digit ring at DAT_00477840+0x14; the native EDI argument is
a stale register at most call sites, modeled as the bullet velocity
pointer), 0x0043ee30 (stack), 0x0041bb00 (reused
SpawnExplosionParticleEaxEcxEfxAbi), 0x00448db0, 0x0044bf40.

## Notes

- The decompiler-visible callee lists of 0x0041afd0 are shifted by -0x10
  relative to the true call targets; the disassembly targets were used.
- The native ECX of the 0x00418930 call in cases 1/10 is an unmodeled
  register argument (the body's release uses its own manager global).
- The g_SceneGateFlags rectangles mirror the native FPU flag tests
  (C0/C2/C3 mapping) branch for branch.
