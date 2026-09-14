# Replay Context Timer Tick (TH10 0x004297d0)

Reconstruction: `src/ReplayContextTimerTick.cpp` (`TickReplayContextTimers4297d0`,
plus the frame writer helper 0x0042a8a0 as
`WriteReplayFrameWordTripleEaxEdxStackAbi`). The scheduler wrapper
0x0042a3d0 (`ReplayContextTimerCallback` in `StagePracticeReplayContext.cpp`)
moves the scheduler's ECX record argument into EDI and tail-calls the tick;
its eax (always 1) is the wrapper's return value, which the wrapper now
propagates.

## Native ABI

- 0x4297d0: usercall, EDI = the 0x2d4-byte replay context record. Returns 1
  on every path.
- 0x42a8a0: usercall, EAX = 0x6284 writer segment node, EDX = first word,
  stack = remaining two words (ret 8). Returns 1 when the node's word
  stream is full.

## Record layout

- `+0x00` pointer to the current 0x6284 writer segment; `+0x9c` is a
  self-pointer back to the record (published by 0x428f60/0x42aa50 and
  re-published by the tick after each segment roll), so the tick reaches
  the segment through `*(*(record+0x9c))`.
- `+0x10` keep-alive flag: 0 = record mode, non-zero = playback mode.
- `+0xa4 + stage*0x24` per-stage playback slots (stride 0x24, 8 stages):
  `+0x00` word-triple stream cursor (6 bytes per entry), `+0x08`
  byte-stream cursor (1 byte per second), `+0x0c` pointer to the stage
  limit block (`+4` = signed frame limit), `+0x10` elapsed frame counter.
- `+0x1c4` current per-second stream byte, `+0x1c8` shared frame counter,
  `+0x1d0` current stage index (negative = nothing to play back).

## Writer segment node (0x6284 bytes, allocated by 0x42aa50)

- `+0x0000` word cursor (starts at the node base; 6 bytes per entry,
  3600 = 0xe10 entries = one minute).
- `+0x5464..+0x54DC` byte-stream area (cursor at `+0x6274`, capacity 0x78).
- `+0x6278/+0x627c/+0x6280` linked-list links into the owner's per-stage
  bucket at `record + (stage*3+0xf)*4 + 4`.

## Record-mode body (flag clear)

1. `word 0x474e5e = word 0x474e5c` (previous-state save).
2. `state = dword 0x474e30 & 0x1f7`, always published to `word 0x474e5c`;
   when `word 0x491d78 & 0x200`, the hold counter `word 0x474e5a`
   increments while state bit 0 is set (saturating at 8 and raising state
   bit 0x4), otherwise it resets to 0.
3. thiscall `0x40ac20` (`AdvanceInputBankTriggersEcxAbi`, implemented in
   EclSelectMenu.cpp) over the 0x474e30 bank.
4. Every 30 frames (`counter % 30 == 0`, signed idiv): snapshot byte =
   `(*(0x477708))->f34 + 0.5`; below 256.0 via the 0x463b2c `_ftol2`
   truncating conversion, otherwise 255 (the fcom branches NaN into the
   conversion path, i.e. `_ftol2(NaN)` -> 0, mirrored explicitly). The
   byte is written through the writer's `+0x6274` cursor, which then
   advances by 1.
5. `0x42a8a0` appends the word triple `(0x474e5c, 0x474e62, 0x474e64)` and
   reports fullness; on full, 0x42aa50 allocates the next segment for the
   stage at `[record+0x1d0]` and re-publishes the record into `+0x9c`.
6. The frame counter at `+0x1c8` increments on every path; return 1.

## Playback-mode body (flag set)

1. Stage from `+0x1d0`; when negative, zero the three HUD words and return
   after the counter increment.
2. `word 0x474e5e = word 0x474e5c`.
3. Slot = `record + 0xa4 + stage*0x24`. While the signed elapsed counter at
   `slot+0x10` is below `[slot+0x0c]->+4`: copy the current word-triple
   entry to `0x474e5c/0x474e62/0x474e64`, read the byte at `slot+8`
   cursor into `record+0x1c4`, advance the word cursor by 6, and advance
   the byte cursor by 1 only on the 30-frame cadence. When exhausted, the
   three HUD words are zeroed instead; the elapsed counter still advances
   in both cases.
4. Counter increment and return 1.

## Verification notes

- Disassembly at 0x4297d0-0x429a24 (596 bytes) and 0x42a8a0-0x42a8f2.
- Float constants decoded from the image: 0x470b0c = 0.5, 0x470c34 = 256.0.
- 0x42aa50 is already reconstructed in `TitleCalcCluster.cpp`
  (`AllocateGameModeChainEntryEsiStackAbi`); its return value is the owner
  record, matching the native store into `+0x9c`.
- The 0x42a3d0 wrapper previously returned 0; the native propagates the
  tick's eax (1), fixed in this change.
