# Title-Screen Score-Anim Triggers (TH10 0x00404530 / 0x004045b0)

Implemented as `TriggerTitleScoreAnim30EsiAbi` and
`TriggerTitleScoreAnim60EaxAbi` in `src/TitleScoreAnimTriggers.cpp/.hpp`.

## ABI

- 0x00404530: native ESI = title-screen state (the DAT_004776e8 object);
  returns the block flag in EAX.
- 0x004045b0: native EAX = title-screen state; returns the state pointer
  in EAX.

## Body

Both operate on the embedded score-anim block, a1[2694..2699]:

| dword | offset | role |
|---|---|---|
| 2694 | +0x2a18 | block flags |
| 2695 | +0x2a1c | accumulator (seeded to -999999) |
| 2696 | +0x2a20 | counter |
| 2697 | +0x2a24 | value |
| 2698 | +0x2a28 | rate pointer (&flt_476F78) |
| 2699 | +0x2a2c | initialized-flag dword |

- 0x00404530 first creates the overlay context via 0x43c8b0
  (`CreateAsciiOverlayContext(2, 30, 0, 0, 0, 0)` — the semantic body's
  sixth argument is the native EBX priority; the native call passes no
  meaningful value beyond the five observed words, modeled as 0).
- First use seeds the block (-999999 / 0 / rate / flag |= 1).
- 0x00404530 arms counter 30, value 30.0f (0x41f00000), limit 29, and
  sets block-flag bit 1; 0x004045b0 arms counter 60, value 60.0f
  (0x42700000), limit 59, and sets block-flag bit 2.

Note: the native 0x00404530 writes the block-flag bit with
`a1[2694] |= 2`, i.e. the same flag dword the armed fields live in
is not touched; the block-flags dword at +0x2a18 is separate.

## Callers

Both are called from the covered game-manager state body region
(0x00418190, sub-state sequences) with the title-screen state object.

## Status

Baselines pass; CSV rows appended; IDA names applied.
