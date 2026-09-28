# Stage trigger update & spawn (0x406240 / 0x4067d0)

Reconstruction in `src/SceneTriggerUpdate.cpp/.hpp`; sibling interpreter
0x406d90 lives in `src/SceneTriggerObject.cpp`. Verified against fresh IDA
decompiles (session `th10re`).

## 0x406240 UpdateSceneTriggerObjectStackAbi (stdcall, ret 0)

Per-frame update of the 0x7f0-stride stage trigger object:
- Record flag bit 3 (`+0 & 8`) or a failed tail step releases the object
  through 0x405be0 and returns -1.
- State word at +0x446: state 2 (entering) integrates the velocity at
  +0x3c0..+0x3c8 scaled by flt_476F78 and 0.5 (raw `fmul ds:0x470b0c`)
  into the position at +0x3b4..+0x3bc; when the +0x314 gate opens it
  falls into state 1. State 3 (exiting) does the same integration and
  jumps to the tail. State 1 skips straight to the body.
  (Correction 2026-09-28: decompile text served by the pre-breakdown MCP
  session displayed this as "flt_476FA8 * 192"; the canonical target's
  raw bytes — fld ds:0x476f78, fmul ds:0x470b0c = 0.5 — are authoritative.
  The same session's get_bytes returned correct bytes, so the root cause
  was MCP response corruption, not a different binary.)
- Active body: runs the instruction queue (0x406d90, ECX), then the
  feature dispatch on the +0x43c flags: bits 1/0x10/0x20/0x40/0x100/
  0x80/0x8000C00/0x4000000 call the corresponding 0x407xxx feature
  helpers (all implemented in src/SceneTriggerFeatures.cpp — note the
  0x407a30 gate is `test al,al; jns`, i.e. bit 0x80, not 0x80000000);
  bit 0x8000 shifts the +0x718 timer by -1.0 (0x44bf40) while +0x71c > 0,
  otherwise clears the bit.
- World-space drift (no 192 scale) after the queue run.
- With record bit 1 set, the timeout-region check 0x4266b0 runs against
  DAT_00477834 and the +0x3b4 position: result 1 latches state 3, sets
  +0x30c = 1 and spawns effect +0x438 (0x448db0) when non-negative;
  result 2 (with record bit 2 clear) latches bit 2, spawns effect 434 and
  enqueues sound 0x1c via 0x43dd10.
- Tail (+0x39c region link): flag bits 0x100000/0x200000 run 0x407da0/
  0x407e40 (EBX-ABI region-wrap twins, implemented in
  src/SceneTriggerFeatures.cpp); when +0x434 expires and the 0x406160
  region-exit check on the linked region's +0x30/+0x34 fires, the object
  is released.
- Counters +4 and +0x434 decrement; finally the +8 VM ticks through
  0x43ee30 and its non-zero result releases the object (return -1).

## 0x4067d0 SpawnSceneTriggerFromDescriptorEbxStackAbi (stdcall ret 0x10)

- Free-slot scan over the 0x7f0 pool from manager+0x10 (base +0x60): up
  to 2000 (0x7d0) probes in groups of 5; state-5 sentinel entries wrap to
  the pool base; exhaustion returns 1.
- Spawn y interpolates descriptor +0x18/+0x1c across the row count
  (+0x1f6).
- Spawn x selects on the movement mode at +0x1f8: 0/1 half-offset
  alternating sign multiplied by the column; 2→3 and 4→5 chain through
  the native fallthroughs (a direct 3/5 entry reads an uninitialized
  stack float — preserved as the `stale_x` slot); 6/8 randomize from
  0x44bb20; 7 randomizes y instead. Defaults only latch the state words.
  (Constant note: the cases-3/5/7 column term rides `fmul
  ds:flt_470B14` = 2π while case 5's leading term is genuine π
  (flt_470B18); jump table @0x406d58 confirms case 3 has no leading
  term. See docs/evidence/float-constant-audit.md for the full
  poisoned-IDB constant corrections.)
- Record initialization: flag |= 1, state 1; two pointer-rate timers at
  +0x3f8/+0x40c lazily initialized then force-armed to -1; y into +0x3d8;
  +0x3e4 angle from 0x44bc10; base position from descriptor +4/+8/+c
  (z then forced to 0.1); polar velocity at +0x3c0; flags +0x43c from
  descriptor +0x1fc; words +0x7ec/+0x7ea; record flags (|2, ~4); +0x454
  cleared.
- VM bind: script = DAT_00474170[kind] + descriptor word +2 through
  InitializePlayerMainVmEsiStackAbi on the +8 VM with the manager pool at
  +0x3e0b50.
- Per-kind +0x438 dword from DAT_004742C0 (0: 2*word2+0x11, 1:
  DAT_0047432C[word2], 2: -1, 3: 0x1d, 4: 0x13); +0x460 from
  DAT_00474250; +0x458 from descriptor +0x204; +0x434 = 10; +0x3f0/+0x3f4
  from DAT_004741E0[kind].
- Anchor words from descriptor flags: bit 2 → word 7, bit 4 → 8, bit 8 →
  9 (each latching state 2 and scaling the velocity by 1/4 via
  flt_470C40 before subtracting it from the position); otherwise word 1.
- The full 18-entry instruction queue (+0x464..) copies from descriptor
  +0x20, +0x440 = flags, +0x43c cleared, +0x45c from descriptor +0x208;
  the queue then runs once and the +8 VM setup finalizes.
- Manager cursor advances by 0x7f0 with the state-5 wrap.
