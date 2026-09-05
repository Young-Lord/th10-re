# ECL Select / Spell Practice Menu (TH10 0x0040a450)

Implemented as `UpdateEclSelectMenuStackAbi` in
`src/EclSelectMenu.cpp/.hpp`.

## ABI

- Native stdcall, one stack argument (`ret 4`): the menu state object;
  returns 1 in EAX. Sub-state dword at object+0x30 drives a 0/1/2/4
  state machine (other values fall through to the shared return).

## State 0 (enumerate)

Calls the 0x409eb0 reset (native ESI = menu), counts
`../../data/*.ecl` via `FindFirstFileA`/`FindNextFileA`/`FindClose`
(platform boundary), stores the count at +0x38, and on a non-zero count
mallocs (CRT 0x452706) a pointer array at +0x34 plus one
strlen+1 string per file (second enumeration pass). Seeds:
+0x44 = 3 (a1[17]), +0x10c = 1 (a1[67]), +0x3c = SeedCursor(3, count)
(a1[15]), +0x11c = count (a1[71]), +0x1e4 = 1 (a1[121]),
+0x114 = SeedCursor(count, count) (a1[69]), +0x1f4 = 1 (a1[125]),
+0x2bc = 1 (a1[175]), +0x1ec = SeedCursor(1, count) (a1[123]),
+0x30 = 1, +0x674 = 0 (a1[413]), +0x678 = 0x42000000 / 32.0f (a1[414]),
+0x67c = 0 (a1[415]), +0x680 = 1000 (a1[416]).
SeedCursor is the native three-way compare (equal keeps the count,
greater yields zero, less yields max-1); with the constant-propagated
zero this collapses to the binary's emitted compares.

## State 1 (cursors)

First cursor record at +0x3c: value copied to +0x40, then shifts via
`ShiftManagerSelector` (0x44bea0) gated on raw byte tests of the
0x474e36 low byte / 0x474e34 byte with masks 0x20 (up) and 0x10 (down).
Dispatch on the shifted value:

- 0: second cursor record at +0x114 (+0x118 copy; byte masks 0x80/0x40).
  When `dword_474e36 & 0x1001` is set: the light teardown sweep over
  0x47770c (0x4148e0), 0x477834 (0x425090), 0x4776f0 (0x406140),
  0x4776ec (0x405730), 0x477818 (0x41afb0), 0x477840 (0x42b6d0),
  0x477704 (0x40d730) — all native ESI-argument boundaries — then clears
  bit 1 of the dword at +0x684 and starts the continuation worker
  (0x44c1c0 with native EAX = object+0x10, EDI = thread entry 0x40a340,
  stack = object; kept as `StartEclMenuContinuationWorker` because the
  shared `RegisterTimelineContinuation` hardcodes a different entry);
  sub-state = 2.
- 1: spell select cursor at +0x1ec (+0x1f0 copy; word polls
  `PollMenuInputState` 0x80/0x40 with `ShiftManagerSelector`). When
  `dword_474e36 & 0x10010000` is set: the full teardown sweep (0x40a010
  EAX, 0x424d90 ESI, 0x409e20 EAX over 0x4776f0, 0x405ed0 ESI, 0x40d510
  EAX over 0x477704, `ReleaseAsciiHudConditionalState` 0x409f90, 0x413bc0
  EDI over 0x47770c, 0x409e20 EAX over 0x477818, memset 0x21cea0 bytes at
  0x477818+0x14), then the 64-byte descriptor (zeroed twice, dword 5 =
  10000) with the script id read from
  `*(*(*(0x477704 + 0x54) + 0x8c) + cursor*8)` feeds
  `CreateEclScriptObjectEaxStackAbi` (list owner = 0x477704);
  sub-state = 4.
- 2: when `dword_474e36 & 0x1001` is set,
  `RequestGameStateTransitionEaxStackAbi(DAT_00491c28, 3)`.

## State 4 (input snapshot + disable)

Saves the input words: word[0x474e5c] = word[0x474e30],
word[0x474e5c+2] = old word[0x474e5c], then the 0x40ac20 snapshot
boundary (native ECX = bank), word[0x474e62] = word[0x474e36]. When the
low byte carries bit 3: for the managers at 0x477834 / 0x4776f0 /
0x477704 / 0x477818 the scheduler records at manager+8/+12 get their
enabled bit (word at record+4) cleared (`&= ~2`), interleaved with the
0x424d90 / 0x405ed0 / 0x409f90 boundaries; then 0x21cea0 bytes at
0x477818+0x14 are zeroed; sub-state = 1.

## Status

Baselines pass (`scripts/compile-main-chain-cpp.sh`, g++ -m32 -std=c++98
syntax check). IDA name/comment applied at 0x40a450.

## Boundaries resolved into semantic bodies

- `0x0040a010` `EnableOptionRecordsAndRebuildEaxAbi` (native EAX =
  manager): re-enables both scheduler records (+8/+0xc, word at
  record+4 |= 2) and tail-calls `RebuildPlayerOptionRecords`
  (0x00426f70); the native return value is unused by all callers.
- `0x00409e20` `EnableManagerSchedulerRecordsEaxAbi` (native EAX =
  manager): re-enables the +8/+0xc scheduler records when present and
  returns the +0xc record (or the manager when absent). Called with both
  0x4776f0 and 0x477818 from this menu and by 0x417870.

## Boundaries resolved into semantic bodies (continued)

- `0x0040ac20` `AdvanceInputBankTriggersEcxAbi` (native __thiscall ECX =
  the 0x474e30 bank): clears the +4 gate bytes, advances/resets the 16
  u16 repeat counters at +0x38 from the saved word at +0x2c (wrap flag at
  +0x30, threshold 26, subtract-8 wrap), then computes pressed
  (+0x32 = cur & (cur ^ prev)) and released (+0x34 = (cur ^ prev) &
  ~cur) from the current (+0x2c) and previous (+0x2e) saved words,
  returning the released mask.
