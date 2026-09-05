# TH10 Framework Helpers (Player Subsystem)

Module: `src/PlayerFrameworkHelpers.cpp/.hpp`, plus the sub-effect spawner
in `src/PlayerShotSpawner.cpp`.

## 0x0040ac90 RequestGameStateTransitionEaxStackAbi

Controller `0x491c28`; stores the pending state at `+0x390`. When bit 12
of `+0x3cc` is set the request is overridden with state 2 (replay/system
modes forbid transitions). The flag is not consumed; the poller of
`+0x390` performs the transition.

## 0x00412e70 AddMaximumScorePenalty

Thecaller's `3000` is `amount / 10` — it drains 300 from the maximum-score
dword `0x474c4c` with a 5000 floor. This corrects the earlier "3000-frame
sequence" reading; the deathbomb/continue cost flows through the same
dword the death processor penalizes.

## 0x00413790 RefreshLifeIconsEaxStackAbi

Nine 0x3ac-stride slots at `hud+0x4cdc`; bit 1 of each slot's first dword
is the life-icon visibility. `count > 8` sets slots 0..count-1 and skips
the hide loop entirely; `count <= 0` only hides.

## 0x00424650 ShowCautionText

ESI = `[0x477814]`, stack = text ptr + ignored position ptr (ret 8).
Decrements the lifetime (`+0x70`, guarded > 0) of every text on the active
layer (`0x474c7c * 0xc` selector), spawns the text object via `0x41a120`,
then `+0x60 = max(old-0x3c, 1)`, `+0x6c = 90`, `+0x84 = 0xffff8080`
(light blue), `+0x70 = 30`.

## 0x00426610 ComputeDeathBurstAngleEcxEaxAbi

`atan2(target.y - player.y, target.x - player.x)` with the π/2 fallback
(`0x470b94` = 1.5707964) applying only to the exact (0, 0) case; NaN
inputs propagate to NaN.

## 0x0041bb00 SpawnExplosionParticleEaxEcxEfxAbi

EAX = manager `[0x477818]`, ECX = source {x,y,z}, stack (kind, color,
angle, speed), ret 0x10, always returns 0. Path A (kind != 8): scan 150
records of 0x3f0 bytes at `manager+0x14` (free: `+0x3dc == 0`, silent
drop when full); copy position with x clamped to [-192, 192]; init the
movement block (`0x41beb0`) and the `+0x3c8` timer (first-init sentinel
then stopped-timer reset, `+0x3c4 = 0`); remap kind when the power gauge
dword exceeds 99 (1,4→9; 10,11→5); fire VM op 0x189 with the angle when
the current anim is 3; select the script (`0x176+kind`, kind 10→`0x177`
with `+0x3e4 = 1`, kind 11→`0x17a` with `+0x3e4 = 4`); store the color at
`+0x2fc`. Path B (kind == 8): 2048-entry ring at `manager+0x24eb4`, no
clamp, spread field `+0x3ec` from the count cascade (mod 32/64/128/256
plus 16/8/4/0), state `+0x3dc = 5`.

## 0x004231d0 RunGameOverPathBStackAbi

EDI = game state manager, stack = param. Early exit when the title screen
is in sub-state 1 (`[0x477810]->+0x5c == 1`): writes `0x491fb8 = 4` (2
when the replay flag `0x491ff4` bit 12 is set) and returns. Otherwise:
state 6; timer reset; `[title+0x58] |= 0x10`; two pool VMs (scripts 0 and
0x80) attached to the manager (`0x448ac0` boundary) with the param stored
at `+0x1d8/+0x1d4`; overlay box via `0x424480` (boundary); player pointer
stash at `+0x2c4`; game-over BGM `bgm/th10_17.wav` (`0x420a90` boundary);
named-sound halts (`0x43e460` boundary, mode 4 gated on `0x491d78` bit 4,
mode 2 always); replay bookkeeping byte at `0x47783c+0x1d8a3 = 1`; and
the global timescale saved to `+0x2c0` then reset to 1.0.

## 0x0043e7e0 BindEffectScriptContextEaxEcxDxAbi

EAX = effect run context, ECX = script index, EDX = vm record. Scripts at
`ctx+0x11c`, stop flag `ctx+0x124` (correcting the earlier +0x238
reading). Success: `+0x38a = idx`, `+0x386 = *(u16*)ctx`, `+0x308 = ctx`,
flags `&= ~0x600`, both instruction pointers to the script, the
sentinel-then-0xFFFFFFFF timer idiom, flags `&= ~1`, one interpreter step
(`0x43ee30` boundary), and the live-script counter bump. Failure (missing
script or stop flag): the whole 0x3ac record is zeroed — the only signal.
`AssignPoolVmScriptEcxEaxAbi` now calls this semantic body with the
runtime-filled `g_EffectScriptContext` global.

## 0x00427b50 SpawnPlayerSubEffectEcxDxStackAbi

EDX = player, ECX = ignored, five stack args (pos[3], velX, velY, count,
limit), ret 0x14. Returns the first free record of the 32 at
`player+0x350c` (stride 0x6c), or one-past-end silently. The whole
0x6c-byte record is zeroed every spawn (no carry-over; the earlier
"preserve flags" reading was wrong), then flags `|= 3`, position from the
stack arg (not ECX), velocity args, the timer block (`+0x48 = count`,
`+0x44 = count-1`, `+0x4c = (float)count`, `+0x50 = &0x476f78` which the
caller pre-sets to 1.0f, latch `+0x54`), and tail fields `+0x58 = limit`,
`+0x5c = 0`, `+0x60 = 999999`, `+0x64 = 4`. Purely player-local — no
entity allocation.
