# Float constant audit (canonical th10.exe)

Date: 2026-09-28. Scope: every float/double literal in `src/` tied to a TH10
data address (`flt_470xxx`, `dbl_470xxx`, `DAT_0047xxxx`, `0x4xxxxx`
comments), plus `FloatFromBits(...)` literals and comments quoting poisoned
IDA values.

## Method

1. `objdump -d -M intel resources/th10.exe` was scanned for every x87
   instruction with a memory operand. Addresses used only by *write* forms
   (`fst`/`fstp`/`fist`/`fistp`) were excluded; addresses that are also
   written by plain `mov` are runtime variables, not constants (the
   `0x491xxx`/`0x497xxx` BSS block, e.g. `DAT_00491d7c`).
2. For each remaining address the 4 bytes were read straight from the
   canonical `resources/th10.exe` image (section map verified with
   `objdump -h`: `.text` VMA 0x401000 @ file 0x400, `.rdata` VMA 0x466000 @
   file 0x65000, `.data` VMA 0x473000 @ file 0x71c00) and decoded as
   little-endian IEEE-754 binary32. Result: 162 constants, saved to
   `/tmp/float-constants.csv` (address,value,bytes,section).
3. Every `src/*.cpp` / `src/*.hpp` was scanned for float literals near an
   address annotation, and every `FloatFromBits(...)` literal was decoded
   and compared. Each candidate mismatch was then verified against the raw
   disassembly of the native function before editing; annotations
   themselves are unreliable (they came from the poisoned IDB), so where a
   comment-to-literal mapping looked inconsistent the actual `ds:0x4xxxxx`
   operand decided.

Spot checks of the table: `ds:0x470b0c` = 0.5, `ds:0x470c7c` = 1/120
(0.0083333338), `ds:0x470b18` = 3.14159274 (pi), `ds:0x470b14` = 6.2831855
(2*pi), `ds:0x470b60` = -1.0, `ds:0x470b90` = 0.125, `ds:0x470bb8` = 65.0,
`ds:0x470ce8` = -2.984375 (= -191/64), `ds:0x470cf4` = 3.25 (exists in the
image but is referenced by no instruction — the "3.25" claims elsewhere
were misattributed to `flt_470b18`).

## Mismatches found and fixed

| File | Address | Old value | Canonical value | Fixed | Native evidence |
|---|---|---|---|---|---|
| src/AsciiHudGameplayUpdate.cpp | 0x470bdc | kEnterY = 432.0f | 416.0f | yes | `fcomp ds:0x470bdc` @ 0x4149bc |
| src/AsciiHudGameplayUpdate.cpp | 0x470ce8 | kBossAlphaScale = 0.75f | -2.984375f | yes | `fmul ds:0x470ce8` @ 0x415722; alpha byte = `0x40 - (i32)(|dx| * -2.984375)`, ramp 64 -> 254 over |dx| < 64 |
| src/EffectPoolEntityUpdate.cpp | 0x470bcc | k_bottom_fade_span = 3.0f | 32.0f | yes | `fcom ds:0x470bcc` @ 0x41b948 |
| src/EffectPoolEntityUpdate.cpp | 0x470bf8 | k_fade_scale_b = 128.0f | 255.0f | yes | `fmul ds:0x470bf8` @ 0x41b962 (alpha = round((y-8)/32*255)) |
| src/PlayerShotHoming.cpp | 0x470c3c | k_straight_angle = 0.2653f | 0.2617994f (pi/12) | yes | `fcomp ds:0x470c3c` @ 0x428bc3 |
| src/PlayerShotHoming.cpp | 0x470c48 | k_quarter_turn = 1.5707964f | 0.7853982f (pi/4) | yes | `fcom ds:0x470c48` @ 0x428b97 |
| src/RegistrationDrawOwner.cpp | 0x470bb8 | k_zero_fps = 0.0f | 65.0f (renamed k_fps_gate) | yes | `fcomp ds:0x470bb8` @ 0x4134fd; fps > 65 keeps the phase counter, else resets |
| src/SceneTriggerObject.cpp | 0x470b50 / 0x470ccc | kClampLow/-High = -999/999 | -990.0f / 990.0f | yes | `fcomp ds:0x470b50` @ 0x406e6e/0x406f74; `fcomp ds:0x470ccc` @ 0x406e86/0x406f8b (angle resolve, opcodes 16/64/128) |
| src/SceneTriggerObject.cpp | 0x470cc8 | y-fallback gate used kClampLow (-999) | -999.0f is its own constant | yes (new kFallbackYLow) | `fcomp ds:0x470cc8` @ 0x406fb6 (opcode 64/128 y gate) |
| src/SceneTriggerObject.cpp | 0x470b04 / 0x470b94 | degenerate atan2 gate `dy==30 && dx==30 -> 1.75f` | `dy==0 && dx==0 -> 1.5707964f (pi/2)` | yes | `fld ds:0x470b04`/`fucompp` @ 0x426671-0x426691, `fld ds:0x470b94` @ 0x426697 (function 0x426660) |
| src/SceneTriggerUpdate.cpp | 0x470b14 vs 0x470b18 | movement cases 3/5/7 used kPi for the column term | column term is 2*pi (kTwoPi); case 5 leading term stays pi | yes | jump table @ 0x406d58: case 3 body 0x406974 `fmul ds:0x470b14` @ 0x4069cd... ; case 5 body 0x4069ae `fld ds:0x470b18` @ 0x4069bd + `fmul ds:0x470b14` @ 0x4069cd; case 7 @ 0x406a31 |
| src/SpellBulletVtable.cpp | 0x470c80 | kPhaseALine bits 1119629312 (94.09375) | 1119879168 (96.0) | yes | `fcomp ds:0x470c80` @ 0x408e81 |
| src/SpellBulletVtable.cpp | 0x470c84 | kFollowScale bits 1025447567 (0.0388399) | 1028443341 (0.05) | yes | `fmul ds:0x470c84` @ 0x409139/0x409143/0x40914d |
| src/EnemyDeathEffects.cpp | 0x470b18 | initial angle `* 3.25f` | `* 3.14159274f` (pi) | yes | `call 0x44bb90; fmul ds:0x470b18` @ 0x40c9df-0x40c9e4 |
| src/TimelineRenderObjectSetup.cpp | 0x470b18 | ECL leaf 10010 `* 3.25f` | `* 3.14159274f` (pi) | yes | leaf @ 0x4123a6: `call 0x44bb90; fmul ds:0x470b18` |
| src/VmLeafHelpers.cpp | 0x470b18 | ribbon angle wrap `if (angle >= 3.25f)` | `if (angle >= 3.1415927f)` | yes | `fcomp ds:0x470b18` @ 0x4453f4 (function 0x4452f0) |

### Comment-only fixes (literal already correct, comment poisoned)

| File | Address | Comment said | Canonical | Native evidence |
|---|---|---|---|---|
| src/TitleBulletUpdate.cpp | 0x470bf4 | "= 134.0f" | 128.0f | `fcomp ds:0x470bf4` @ 0x41b078 |
| src/TitleBulletUpdate.cpp | 0x470be0 | "= 500.0f" | 472.0f | `fmul`/`fsub` users @ 0x43bd5c region; image bytes 0000ec43 |
| src/TitleBulletUpdate.cpp | 0x470cd4 | "= 150.0f" | 144.0f | `fcomp ds:0x470cd4` @ 0x41b4b0, `fsub` @ 0x41b4db |
| src/TitleBulletUpdate.cpp | 0x470ce0 | "= 5120.0f" | 5000.0f | `fsub ds:0x470ce0` @ 0x41b4f3 |

## Verified correct (no change)

Full-row verification against the table, all matching:

- src/AsciiHudGameplayUpdate.cpp: 0.99/1.01/1.0 (0x470b68/b64/afc), dbl
  -128.0 (0x470d10), 400.0 (0x470d08), dbl -112.0 (0x470d00), 80.0
  (0x470c28), 64.0 (0x470bc8), 0.0 (0x470b04), -64.0 (0x470b5c), 0.025
  (0x470cf8), 224.0 (0x470b4c), dbl 64.0 (0x470cf0), ±192.0 (0x470b40/b3c).
- src/AsciiProjectedQuadSubmit.cpp: 0.5 @ 0x4700e8.
- src/ConditionalStateSubrecords.cpp, src/EclScriptLibrary.cpp,
  src/PauseEnterSetup.cpp, src/PlayerObjectLifecycle.cpp,
  src/PlayerShotSpawner.cpp, src/ResultScreenScript.cpp,
  src/StageTextEffects.cpp/.hpp, src/TitleBackgroundScript.cpp,
  src/TitleCalcCluster.cpp, src/TitleGameManagerLifecycle.cpp: flt_476f78
  default 1.0f (image bytes 0000803f at 0x476f78).
- src/EclEasedTransforms.cpp: 0.99/1.01/0.2026834 (2*pi/31,
  0x470b6c)/3.0 (0x470bd4).
- src/EclScriptVm.cpp: 0.0 @ 0x470b04.
- src/EndingMidiPlayer.cpp: 1.0 @ 0x470afc, 128.0 @ 0x470bf4.
- src/PlayerDamageOutput.cpp: 0.125 @ 0x470b90.
- src/PlayerModeDispatcher.cpp, src/PlayerMotionHelpers.cpp,
  src/PlayerProjectileManager.cpp, src/PlayerTimerHelpers.cpp,
  src/VmLeafHelpers.cpp (rate pairs): 0.99/1.01/0.01/±pi/2pi/100/1.5707964
  (0x470b94) all match.
- src/PlayerOptionCallbacks.cpp, src/PlayerOptionRecords.cpp: 0.125
  (0x470b90), 100.0 (0x470b44).
- src/PlayerProximityFade.cpp: 224/16/32/0.03125/0.5 all match.
- src/PlayerShotHoming.cpp remaining: 0.1 (0x470c18), 0.2 (0x470c38), 16.0
  (0x470b48), 4.0 (0x470c40), 0.3 (0x470c44).
- src/ReplayContextTimerTick.cpp: 0.5 (0x470b0c), 256.0 (0x470c34).
- src/ReplayRecordScreens.cpp, src/ScoreFileFormats.cpp,
  src/ScoreScreenRenderers.cpp, src/ScriptTestMenu.cpp: 9/15/18/16/240/10/
  0.1/80/224/64 all match their addresses.
- src/SceneTriggerFeatures.cpp: all 11 constants match (incl. -990 sentinel
  0x470b50, 9.9999997e-05 0x470c58, 0.3125 0x470c6c, 5.0 0x470c68).
- src/SpellBulletVtable.cpp remaining: 0.5/0.99/1.01/1.0/128.0 all match.
- src/StageObjectVtable.cpp: all 20 FloatFromBits values match their
  addresses (incl. 7.5 @ 0x470c58, 12.0 @ 0x470cd8, 6.0 @ 0x470d1c).
- src/StageTextEffects.cpp: 8.0 @ 0x470bd0 etc.
- src/TimelineRenderObjectSetup.cpp: DAT_00491xxx globals are BSS runtime
  variables (no file image) — not constants, left alone.
- src/RegistrationDrawOwner.cpp doubles: 0.5 @ 0x470bc0, 2^32 @ 0x470b30,
  60.0 @ 0x470bb0, 57.0 @ 0x470ba8 (8-byte reads).
- src/SceneTriggerObject.cpp: 224/16 (0x470b4c/b48) and the rotated-region
  literals 192/57/12/144/30 all exist in the canonical pool (0x470b3c,
  0x470ba8, 0x470cd8, 0x470cd4, 0x470c30). Note: the rotated-region body is
  annotated 0x4267f0 but that address's FP code uses 0.5/0.0/-0.5/1.5/-1.5
  (0x470b0c/b04/b78/ca8/ca4); the 192/57/12/144 cluster lives in the
  0x41bxxx/0x4061xx family — the body-to-address mapping for that one
  function looks shifted, but no literal disagrees with any canonical pool
  value, so nothing was edited.

## Notes

- `flt_470b18` holds pi (3.14159274f, bytes `db 0f 49 40`) in the canonical
  binary. Earlier claims that it holds 3.25 (and the two `* 3.25f` code
  sites derived from that) were wrong; 3.25 does exist in the image but at
  0x470cf4, which no instruction references.
- 0x463b2c is the MSVC round-half-away-from-zero conversion helper; call
  sites replicating it should not use a plain truncating cast. The boss
  alpha site (0x415728) is annotated accordingly in
  AsciiHudGameplayUpdate.cpp; the difference only matters on exact
  half-integer products.
- Already-fixed prior to this audit: src/SceneTriggerUpdate.cpp
  g_FrameTimeScale + 0.5f drift (untouched here).
- config/function-status.csv and scripts/ were not modified.

## Verification

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` — clean (exit 0).
