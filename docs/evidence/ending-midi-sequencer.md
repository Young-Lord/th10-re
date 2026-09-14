# Ending MIDI Sequencer Event Interpreter — 0x0043b110

Module: `src/EndingMidiSequencer.cpp/.hpp`
(`InterpretEndingMidiEventBlockStackAbi`). Native `retn 8` stdcall body:
(context, event block).

## Subsystem

The ending music player (ending.cpp cluster — the assertion strings
`"\src\game\ending.cpp:131 EndingInfo..."` sit at 0x46cfd8 next to the
blank-row format this interpreter area uses) schedules 0x20-byte event
blocks: the caller `0x0043af30` (not a reconstruction target here)
advances the 64-bit playback time, drives the fade (`+0x2c8`, step
helper `0x43b7a0`) and runs every block whose `[block+0x00,
block+0x04]` time window covers the current time. `0x0043b110` decodes
one event of the block's stream — the stream format is a standard MIDI
file event stream:

| Block offset | Meaning |
| ------------ | ------- |
| `+0x00` | active flag (cleared by the 0xff 0x2f end-of-track meta) |
| `+0x04` | next-event time (each event's delta-time varint is added) |
| `+0x0c` | running status byte |
| `+0x14` | pointer to the next stream byte |
| `+0x18` / `+0x1c` | saved (cursor, time) marks |

Player context (constructor `0x0043a750`): `+0x14[32]` ring of 0x40-byte
MIDIHDR slots for SysEx messages with the round-robin index at `+0x94`;
`+0x118`/`+0x138` block count/array; `+0x120` ticks-per-quarter scalar,
`+0x124` tempo accumulator, `+0x128:+0x12c` elapsed delta, `+0x130:
+0x134` accumulated playback time; `+0x13c` MIDI out device handle;
`+0x154` sixteen 0x17-byte per-channel records (key bitmap at +0x00,
then the program number and controller bytes 0x00/0x0a/0x57/0x59/0x07
raw and faded at +0x10..+0x16); `+0x2c4` note offset byte; `+0x2c8`
fade scale.

## Event decoding

The first stream byte selects between explicit status (>= 0x80, cursor
advances) and running status (byte < 0x80: the block's `+0x0c` status is
reused and the byte is the first data byte). `bl = status & 0xf` is the
channel, and `(status & 0xf0) - 0x80` indexes the dispatch tables:

- First dispatch (`0x43b64c` / jump table `0x43b63c`): statuses
  0x80/0x90/0xa0/0xb0/0xe0 read two data bytes, 0xc0/0xd0 read one,
  0xf0..0xff take the meta/SysEx path, everything else falls through
  (status < 0x80 always falls through with data1 = data2 = 0).
- Meta/SysEx: `0xf0` reads a varint length, takes the next MIDIHDR slot
  of the 32-slot ring (releasing the previous one through `0x43aeb0`),
  allocates `length+1` bytes, stores `0xf0` + the stream bytes, sets
  `dwBufferLength`/`dwFlags` and submits via `midiOutPrepareHeader`
  (IAT 0x4662a0) then `midiOutLongMsg` (0x46629c); a failed submission
  frees the payload and header and clears the slot. The slot index
  advances with the signed `%(+1) 32` idiom. `0xff <type> <len>`:
  type 0x2f ends the track (`*block = 0`, immediate return, no delta
  added); type 0x51 runs the tempo math below and re-seeds the `+0x124`
  accumulator from `len` stream bytes with multiplier **0x101 instead
  of 0x100** (native quirk kept); any other type skips `len` bytes.
  Statuses 0xf1..0xfe read no operands.
- Second dispatch (`0x43b6d4` / jump table `0x43b6c0`, entry = the same
  `(status & 0xf0) - 0x80`, entries above 0x40 skipped): 0x80/0xe0 clear
  the channel's key bit; 0x90 sets it (velocity 0 = note-off); 0xb0
  sub-dispatches on the controller number (`0x43b738` byte table):
  ctrl 0 → `row[0x11]`, ctrl 2 → save mark, ctrl 4 → restore mark,
  ctrl 7 → `row[0x15]` plus the faded alpha `ftol(value * [+0x2c8])`
  clamped to 0..0x7f stored in `row[0x16]` (and replacing data2 for the
  epilogue), ctrl 0x0a → `row[0x12]`, ctrl 0x57 → `row[0x13]`, ctrl
  0x59 → `row[0x14]`; 0xc0 stores the program in `row[0x10]`; 0xa0 has
  no per-channel update.

Key-bit index: `(u8)(note + ctx[0x2c4])`, byte `(index >> 3)`, bit
`(index & 7)` — unchecked, so a large note/offset can run past the
0x10-byte bitmap into the following record fields (native quirk
preserved).

Mark/restore (ctrl 2 / ctrl 4, `0x43b502` / `0x43b56e`): shift every
event block's cursor (+0x14) and time (+0x04) into (+0x18/+0x1c) or back
out, and snapshot/restore the five dwords `+0x124..+0x134`.

Tempo math (meta 0x51): the accumulated playback time
`+0x130:+0x134` gains `sext64([+0x120]) * i64[+0x128:+0x12c] * 1000 /
sext32([+0x124])` (native `__allmul`/`__allmul`/`__alldiv` chain,
0x456640/0x456b80); then `+0x128`, `+0x12c` and `+0x124` are zeroed and
`+0x124` is rebuilt from the meta bytes.

## Common epilogue

Stores the (running) status into `block+0x0c`, forwards every voice
message (`status < 0xf0`) through `midiOutShortMsg` (IAT 0x466298) as
`status | data1<<8 | data2<<16` when the device at `+0x13c` is open —
including running-status replays and the status-0 case (native quirk) —
and finally reads the delta-time varint (7-bit big-endian, 0x80
continuation; the inlined copy of `0x0043a730`) and adds it to
`block+0x04`.

## Boundaries kept

WINMM imports (`midiOutPrepareHeader`/`midiOutLongMsg`/
`midiOutShortMsg`/`midiOutUnprepareHeader`), CRT `malloc`/`free`
(0x452706/0x452422), `_ftol2` (0x463b2c, modeled as the round-half-away
`FloatToI32`), and the caller-side helpers `0x43af30` (scheduler),
`0x43ace0`, `0x43b7a0`. Small unreconstructed helpers were implemented
inline and documented: `0x0043a730` (varint reader) and `0x0043aeb0`
(header release: find slot, unprepare, free payload + header).

## Verification

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` — 0 errors.
