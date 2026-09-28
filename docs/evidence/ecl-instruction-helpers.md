# ECL Instruction Helpers (0x403990 / 0x40e6a0)

Module: `src/EclInstructionHelpers.cpp`.

- `0x00403990` `CheckTitleScriptGroupLivenessEbxAbi` — native EBX = the
  title background script context (`ret` with EAX = 0). For each of the
  context's tracked script records ([ebx+0x10] count over the [ebx+0x14]
  pointer array) carrying flag bit 0 and a non-negative sub-entry count at
  +0x1c: walk the record's sub-entry chain (stride word at +0x2, entry slot
  at +0x6 indexes the 0x3ac-stride record bank at [ebx+0x17c]) and count
  live records (0x43ee30 liveness probe; [rec+0x390] nonzero). When a whole
  record's chain has no live entry, clear its flag bit 0. Returns 0.

- `0x0040e6a0` `TickEffectSpawnWaitListEbpStackAbi` — native EBP = the
  wait-list owner record, one stack argument (ret 4, unused by the body).
  Walks the record's +0x58 linked list (next at node+0x4): when a node's
  +0x2480 flags intersect the expire masks (bits 0x10|0x40 in the low byte
  or 0x4000|0x8000 in the high byte) and the low byte is negative-signed,
  spawns the node's effect — when the +0x2448 script index is non-negative,
  launches it through the stage host (0x448db0 raw spawn, EDX = host) with
  the +0x244c slot's effect id (the native reuses the argument slot as the
  id scratch), then sets flag bit 0x20000. Afterwards the record's +0x40
  mirror and frame accumulator advance with the (0.98, 1.01) window rule
  (constants 0x470b64/0x470b68): inside the window the counter increments
  and the rate adds 1.0; outside, the rate adds the reference and the
  counter is the __ftol2 truncation (0x463b2c boundary) of the rate.

The spawn-path constants 0x470b4c (224.0) / 0x470b48 (16.0) belong to the
caller-side position publish and are carried as dead references in the
semantic body.
