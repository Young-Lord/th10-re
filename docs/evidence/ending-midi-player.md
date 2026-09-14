# Ending MIDI Playback Scheduler — 0x0043af30

Module: `src/EndingMidiPlayer.cpp/.hpp` (`UpdateEndingMidiPlayback`).
Native ECX = ending player context, plain `ret`; published as an update
callback in the ending-state function pointer table at 0x46f80c (the
`.rdata` entry sits behind the `"------ \r\n"` separator string). This is
the caller side of 0x43b110 (`InterpretEndingMidiEventBlockStackAbi`,
EndingMidiSequencer.cpp), matching the context layout documented in
`ending-midi-sequencer.md`.

## Per-update flow

1. **Running time (registers only).** `now = [+0x130:+0x134] +
   sext64([+0x120]) * i64([+0x128:+0x12c]) * 1000 / sext64([+0x124])` — the
   native `__allmul` (0x456640) / `__allmul` / `__alldiv` (0x456b80) chain.
   The accumulated field is NOT stored back here; the interpreter's tempo
   meta (0x51) owns it, and the running value is re-derived after every
   interpreted event.
2. **Fade.** Gate +0x2e0; while counter +0x2e8 < duration +0x2e4:
   `+0x2c8 = 1.0f - counter/duration` (flt_470afc / flt_470bf4 context),
   and when `ftol(scale * 128.0f)` (the `_ftol2` 0x463b2c, modeled as
   round-half-away) differs from the cached +0x2cc the fade-step helper
   0x43b7a0 (boundary; native EDI = context plus one stack argument, called
   with 0) runs; the counter then increments. When the counter reaches the
   duration the scale is stored as 0 directly (0x43b0fb early-out).
3. **Block walk.** For each of the +0x118 event blocks at +0x138
   (stride 0x20): a zero +0x00 active flag skips the block; otherwise the
   "any active" flag is set and the interpreter runs while the block stays
   active and its sign-extended +0x04 end time (`cdq` + unsigned
   high/low-dword compares, equivalent to an i64 compare for these small
   positive times) is at or before the re-derived running time. The block
   array base is re-read each pass, exactly like the native.
4. **Tick.** The elapsed delta +0x128:+0x12c increments by one (64-bit).
5. **Stop.** When the walk never saw an active block, 0x43ace0 (boundary;
   native EDI = context) runs.

## Preserved quirk: the stale any-active flag

The flag lives in the stack slot [esp+0x14] and is only ever *set* (to 1)
inside the walk; it is never initialized. When the walk never executes the
active-block branch the slot keeps the stale value of the `__allmul`
argument that was pushed before the first call — the low dword of
`sext64([+0x120])`, i.e. the ticks-per-quarter scalar. The final
`cmp [esp+0x14], 0` therefore also suppresses the 0x43ace0 stop whenever
the ticks-per-quarter scalar is nonzero, even if no block was active.
Reproduced literally with `i32 any_active = LoadI32At(ctx, 0x120U);`.

## Boundaries kept

0x43b7a0 (fade step), 0x43ace0 (playback finished), WINMM/CRT helpers
inside 0x43b110, `_ftol2` 0x463b2c.

## Neighbors already covered

- 0x43b110: `EndingMidiSequencer.cpp` (unchanged).
- 0x437a00 (generated table + fonts): already implemented as
  `InitializeGeneratedTableAndFonts` in `GeneratedFontTable.cpp`; no new
  reconstruction required (a CSV row was added for it).

## Status

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` passes. CSV row
appended for 0x0043af30 and 0x00437a00.
