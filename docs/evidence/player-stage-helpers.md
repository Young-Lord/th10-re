# TH10 Stage Helpers (Respawn Sweep, Lives Digits, Broadcasts)

Module: `src/PlayerStageHelpers.cpp/.hpp`. Corrects the earlier reading of
`0x00405860` (it is the respawn-effect tick, not a replay/context tick)
and of `0x00412e70` (score drain, documented with the framework helpers).

## 0x00405860 TickRespawnDeathEffectStackAbi (node, ret 4)

One-shot latch at node+0x28 (returns -1 when already set). Timer-node
reset (lazy sentinel init at +0x14..0x24 then unconditional stop), floats
+0x3c = 32.0f / +0x40 = 4.0f, position copied from the player at
`[0x477834]+0x3c0..0x3c8` into node+0x30..0x38. Effect script 0x190
(+7 for the second character) becomes 0x1b9 during the boss spell window
(stage `+0x378c` bit 0 with `+0x3788` in [0x5d, 0x60] or == 0x6d). The
effect VM spawn (`0x448db0` boundary) id lands on node+0x2c; the sound
request 0x26 goes through the existing `EnqueueBgmSoundValueFromFloat`
semantic body; the lives digits refresh through the semantic
`0x004054b0`; stage life flags clear via the `0x405500` boundary;
`0x474c98 -= 0x80` with the ±0x400 clamp; output flag node+0x44.

## 0x004054b0 RefreshHudLivesDisplayEdxStackAbi (EDX = lives, stack = percent)

Re-binds three numeric digits through the existing semantic body of
`0x0043e5a0` (`InitializeAsciiAnimationVmEntry`): integer digits at
hud+0x6a8c (idx = lives+8), tens at +0x71e4, ones at +0x7590, with the VM
from `hud+0x9ec8`. The lives stock is stored ×20 with 5% granularity
(`(v%20)*100/20`). This function never touches the icon slots at +0x4cdc
(those belong to `0x413790`).

## 0x004086b0 CheckStageEffectPositionInFieldEcxEcxStackAbi

Native ECX = the float2 position (x at +0, y at +4), stack = (margin_x,
margin_y; ret 8). Returns 1 when the position with its margins falls
outside the fixed playfield rect x = (-192, 192), y = (0, 448) — the four
constants 0x470b40 (-192.0), 0x470b3c (192.0), 0x470b04 (0.0) and
0x470b38 (448.0) — and 0 when inside. Each axis test is an independent
early return in the native order (fcomp + `test ah,0x41` / `test ah,0x1`
sign/zero decode). The previously inlined copies of this test in
`ActivateStageEnemyEsiAbi` (8px margins) and `ScanIntroActivations`
(2px margins) now call the shared semantic body.

## 0x00408030 ActivateStageEnemyEsiAbi

State machine only: requires word +0x446 ∈ {1, 2}; sets word +0xc3 = 1
and state 3. The 8px-margined on-screen test (bounds ±192 / 0..448)
selects the full path (effect spawn via the `0x448db0` boundary when the
existing handle +0x438 >= 0, timer-node reset at +0x3f8) versus the
deferred path (dword flag bit 3 at enemy+0). No ECL execution, no
position writes.

## 0x00408100 ScanIntroActivations

2000-slot sweep (base manager+0x60, stride 0x7f0). Gate: word +0x446 not
in {0, 3}, and (when require_unused) dword +4 == 0. Fires when
`(reach = half_extent(+0x3f0)*0.5 + radius)² > dist²` (full 3D, NaN
fires), then activates the slot and — when on-screen (2px margins) and
spawn_fx — spawns the downward burst via the semantic `0x41bb00` body
(kind 8, color 0xffffffff, angle -π/2, speed 0.6).

## 0x0041c800 BroadcastEntranceTweenEaxEbxStackAbi

Caches the target position into manager+0x440..0x448, then walks the
singly linked list at manager+0x18 (next at node+8), skipping nodes with
dword +0xc == 1, and calls each node's virtual slot 7 (0x1c/4) with
(node, target vec3 pointer, radius, flag); returns the accumulated sum.

## 0x0041c850 BroadcastBulletClearEaxAbi

Same walk skipping +0xc == 1, calling virtual slot 5 (0x14/4) with no
args; returns 1. The conversion/cancel work lives inside the virtuals.
The dispatcher's mode-3 call passes the manager from `0x47781c`; the
inline copies in the mode-0/1 paths use the same loop shape.
