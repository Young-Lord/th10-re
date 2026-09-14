# Hint-Text State Release (TH10 0x00418ee0)

Implemented as `ReleaseHintTextState` in `src/HintTextLoader.cpp/.hpp`.
Native stdcall with one stack argument (the hint-text state object); the
return value is the result of the reload worker 0x00419040. This is the
routine referencing the `"hint/hint_auto.txt"` string at 0x46d6d8, reached
when the hint-text manager state published through DAT_00477814 is torn
down.

## Flow

1. The tracked buffers at state+8 and state+12 are freed through the
   shared scheduler-removal boundary 0x449f60, bracketed by the scheduler
   critical section (DAT_00492274) and the activity-depth byte
   (DAT_0049231c) — the standard destructor idiom (a null buffer skips the
   lock entirely).
2. The ten entity-handle slots at state+0xdc are expired: each non-zero
   handle is soft-released through 0x4492a0 (entity +0x35c |= 0x4000000,
   propagated over the +0x14 child chain while +0x18 == 0 — the
   `ReleaseEntityById` semantic body in `EntityHelpers.cpp`, manager
   0x491c10), and every slot is then zeroed.
3. When the Extra-save selector byte DAT_00491d6a equals 2, the automatic
   hint loader 0x41a200 runs with `"hint/hint_auto.txt"`.
4. 0x419040 runs and its result is returned; finally DAT_00477814 is
   cleared.

## Boundaries

- 0x00449f60 (scheduler-record removal + free): modeled as the
  declaration-level boundary `FreeSchedulerRecord` (the semantic body
  lives in `src/CallbackScheduler.cpp`).
- 0x004492a0: reused `th10::ReleaseEntityById`.
- 0x0041a200 / 0x00419040: declaration-level boundaries
  (`LoadHintTextFile`, `RunHintTextReload`).
