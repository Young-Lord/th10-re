# Replay game context & player damage output

Reconstructions in `src/StagePracticeReplayContext.cpp/.hpp` and
`src/PlayerDamageOutput.cpp/.hpp`, recovered from an interrupted cluster run
and finished by hand (includes fixed, boundaries resolved). The IDA session
was unavailable for a final pass over these two modules; addresses and
offsets come from the analyzing agent's disassembly notes.

## src/StagePracticeReplayContext.cpp

- **0x0042a3e0** — replay-context chain callback: the 0x24-byte chain
  element's +0x10 record drives the state; tail-calls the shared context
  timer tick (0x0042a3d0 boundary).
- **0x0042a430** — keep-alive gate callback: idles while the keep-alive
  flag runs, otherwise tears the context down.
- **0x00418b80** — frame-state seed (50000) used on the game-start path.
- **0x0042a930** — replay game-context restore: per-mode state word
  publication, stage word latch into the record, flag-bit fixups.
- **0x00418c40** — practice-mode score-block snapshot/latch pair.
- **0x00428f60** — replay/practice game-context setup: builds the context
  record from the mode/file name, seeds the score block, latches the
  stage word (u16 at TH10 0x4918b0 — the same word the LCG state pair
  uses; flagged for a dedicated verification pass) and the replay-flag
  pair at 0x491fc4, and registers the chain callbacks.
- **0x00429610** — context record teardown: releases the record through
  DestroyUnknownMainChainObjectInPlace (0x004294a0).

Declared boundaries: 0x004297d0 (periodic timer tick), 0x00429a70 (retry
handler).

## src/PlayerDamageOutput.cpp

- **0x00449450 / 0x00449630 / 0x00448d00** — item-pickup text-slot
  helpers (boundary declarations with native ABIs).
- **0x00427c70** — enemy damage aggregation from player shots: pass 1
  walks the 128 player-shot records at player+0x4a0 (0x5c stride; box
  overlap against the enemy box, type-3 autocollect markers gated on
  y >= 0), consulting the fastcall hit callback at descriptor+48
  (nonzero suppresses collection), then pass 2 accumulates bomb damage
  from 0x004059f0 constants (near 3/5/38, direct 26/28, splash 16/20
  selected by the bomb kind) and publishes the total.
- **0x0040c480** — bomb-area damage entry (this = game context).
- **0x004243f0** — stop-word setter: resolves the entity from the handle
  slot against the 0x491C40 manager, writes u16 6 at +0x304 and
  propagates to the +0x14 child chain while +0x18 is zero. NOTE:
  `SetReplayWatchStopWord` in src/PostRunReplaySaveMenu.cpp is a second
  reconstruction of the same native function with a different child-walk
  formulation (node-indirected); reconcile during the exact-match pass.
- **0x004059f0** — bomb damage constant selector (this = player).

## Globals

- `g_ReplayContextStageWord` (TH10 0x4918b0, u16) and
  `g_ReplayContextStageLatch` (0x4918b4) — declared extern here;
  TimelineRenderObjectSetup.cpp defines the same addresses as the LCG
  state pair `g_TimelinePrngStateB[4]`. Reconcile the views before
  linking.
- `g_ReplayFlagLatch` (0x491fc4), `g_ReplayRetryEdge` (0x474e61),
  `g_ScoreBlock` (0x474c40), `g_ManagerObject810` (0x477810).
