# TH10 0x0042e5a0 — Audio-Options Volume Presentation Refresh

Semantic body: `src/BgmUpdateLoop.cpp` (`RefreshAudioOptionPresentationEdiAbi`),
declaration in `src/BgmUpdateLoop.hpp`. Called by the state-4 audio-options
game-manager body `0x0042d920` (eight call sites, reconstructed as
`RunManagerStateBody4`); the native entry receives the game-manager state in
EDI, whose `+0x2c8` field caches the options-screen text container handle.

## Behavior

1. Publishes the BGM volume byte `DAT_00491d68` into `DAT_00497854`
   (BGM volume scale) and queues a BGM command through `0x0043e460`
   (`QueueBgmCommand`) with EAX = `DAT_00492590` (transition/sound root),
   EDX = `"SetVol"` (0x46ece8), stack `(opcode=8, track_slot=0)`.
2. Publishes the SE volume byte `DAT_00491d69` into `DAT_00497858`.
3. Recomputes the DirectSound attenuation `DAT_0049795c`:
   - SE volume zero: `-10000` (full attenuation).
   - Otherwise, with `t = 1 - volume * 0.0099999998f`:
     `-5000 - (int)((1 - t*t*t*t... ) * -5000.0)` where the native computes
     `(1 - (t*t)*(t*t))`; i.e. `-5000 - trunc((1 - t^4) * -5000)` (floats
     `flt_470afc=1.0`, `flt_470b00=0.01`, `flt_470b80=-5000.0` via `__ftol2`).
     Volume 100 yields 0 (no attenuation).
4. Re-binds twelve options-screen volume digit glyphs. For each slot it:
   - resolves the container entity from the cached `+0x2c8` handle through
     `0x0044491c0`-family lookup (`FindEntityEdxStackAbi`, manager
     `DAT_00491c10`), clearing the cache when the lookup fails;
   - walks the container's child list at `container+0x10` (nodes
     `{child_entity, next}`) for the child whose u16 id at `+0x38a` equals the
     slot id and takes the child's first dword as its handle id (0 when the
     walk exhausts);
   - resolves that handle id again in the manager list and, when non-null,
     calls `0x0043e5a0` (`InitializeAsciiAnimationVmEntry`, native
     EAX=vm, EDX=char code, ECX=vm+0x308 resource block).
   Slot ids and characters: BGM volume ids 37/38/39 get
   `volume/100 + 0x33`, `volume/10%10 + 0x33`, `volume%10 + 0x33`; ids
   41/42/43 the same digits with base `0x3d` (emphasized style); ids 45/46/47
   and 49/50/51 repeat both styles for the SE volume byte `DAT_00491d69`.
5. Toggles the disabled bit 0x200 in each digit VM's state dword at `+0x35c`
   per the leading-zero rule (both hundreds digits and the plain tens digit
   hidden below 100/10 respectively):
   - value >= 100: ids {37,38,41} / {45,46,49} set; last id (42 / 50) is
     read-modify-write and set.
   - 10..99: 37 & 41 (45 & 49) cleared, 38 (46) set, 42 (50) set.
   - < 10: all four cleared.

## Preserved native quirks

- The child-list walk is unchecked: a failed container lookup writes 0 to the
  cache, the next resolution receives handle 0 (lookup returns null) and still
  computes the list head as `null + 0x10`, then dereferences address 0x10 —
  the semantic body keeps this unchecked arithmetic rather than adding guards.
- The visibility update dereferences the resolved VM record without a null
  check (native would fault on a missing glyph VM); only the digit-char bind
  path null-checks.
- The attenuation is gated on the SE volume byte although it is computed from
  the BGM volume value.
- Digit characters are computed from signed byte loads (`movsx`), so the
  native arithmetic is signed; values are 0..100 in practice.

## Implementation mapping

- `QueueBgmCommand` — existing semantic body of `0x0043e460` (`BgmRuntime`).
- `FindEntityEdxStackAbi` — existing semantic body of `0x004491c0`
  (`EntityHelpers`); native passes the manager in EDX and the handle on the
  stack (EAX leftover ignored), matching all call sites here.
- `InitializeAsciiAnimationVmEntry` — existing semantic body of `0x0043e5a0`
  (`AsciiAnimationVm`).

CSV row: `0x0042e5a0,RefreshAudioOptionPresentationEdiAbi,BgmUpdateLoop`.
The no-argument `RefreshAudioOptionPresentation` extern previously declared in
`GameManagerStateBodies.cpp` remains as a boundary stub name; the typed body
here takes the game-manager state pointer that native EDI carries.
