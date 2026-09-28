# Ending MIDI player edge functions (0x0043a910-0x0043af00)

Module: `src/EndingMidiEdge.cpp`

Complements the registered `EndingMidiBlockLoader` / `EndingMidiPlayer` /
`EndingMidiSequencer` modules. Sequencer record: +0x10 selected block,
+0x14[32] MIDIHDR ring, +0x98+4i raw blobs, +0x114 scratch, +0x138 row
table, +0x140 timer id, +0x144 timer period, +0x13c MIDI out.

- `0x0043ae20 ResetSequencerSideStateEaxAbi`: was declared (but never
  defined) in `EndingMidiBlockLoader.cpp`; this module carries the body:
  -1 when the row table is empty, release all 32 prepared headers
  (midiOutUnprepareHeader + payload/header frees, native 0x0043aeb0 body
  duplicated here because the registered copy has internal linkage),
  `timeKillEvent`, `timeEndPeriod`, `midiOutReset`/`Close`, and the
  selected index reset to -1.
- `0x0043a910 DestroyEndingMidiPlayerEaxAbi`: vtable 46f80c -> side
  reset -> `FreeMidiBlockRowsEsiAbi` -> 32 blob slots -> midi device ->
  `timeKillEvent` -> doubled `timeEndPeriod`; vtable 46f810.
- `0x0043a9b0 LoadEndingMidiFileBlobEaxEsiStackAbi`: resets side state
  when reloading the selected block, frees and reloads the blob slot via
  `LoadMainChainFile`, prints `"error : MIDI File %s"` through the
  0x0044b810 console printer on failure.
- `0x0043ac90`: percussion loader (block 31) dropping the +0x114 scratch.
- `0x0043af00`: re-arms the +0x2c8..+0x2e8 tempo window words.
