# Replay Context Helpers (`0x004296f0` family)

Batch B reconstruction evidence for the demo/replay record lifecycle.
Bodies live in `src/ReplayContextHelpers.cpp`.

| Address | Name | Contract |
|---------|------|----------|
| 0x004296f0 | `ParseDemoRecordEsiStackAbi` | Stack = source (`retn 4`). Allocates the 0x2d4 demo record, installs the frame record reset (0x0042ac20) / frame release (0x0042ac60) pair through the 0x0045252d pool iterator over 0x24-byte records at record+0xa0 (8 count), zeroes the whole 0x2d4 record and sets +0x10 = 2, then parses the source through 0x0042a200 (record in ESI). On parse failure the record is torn down (0x004294a0) and freed; returns the record or 0. |
| 0x004297b0 | `DestroyDemoRecordEsiAbi` | Tears the record down through 0x004294a0 and frees it (skipped entirely when the record is 0). |
| 0x00429a70 | `DrawDifficultyLabelEaxEdxAbi` | EAX = ignored, EDX = replay context (`retn 4`). While the main chain context (`DAT_00477810`) is live and the context mode at +0x10 is exactly 1, colors the difficulty label: difficulty at ctx+0x1c4 compared against `DAT_00470c30` (red 0xff5050ff below the threshold) and `DAT_00470c2c` (light 0xffa0a0ff), white (0xffffffff) above both; the color lands at `DAT_004776e0`+0x8974 and is reset to white after the `"%3d"` format text is submitted through the ascii manager. Returns 1. |
| 0x0042a820 | `ComputeReplayFrameOffsetDeltaEcxAbi` | Returns the byte delta between the record's frame cursor at +0x6274 and the frame table at +0x5464 (the cursor slot minus its own base). |
| 0x0042a990 | `SnapshotReplayFrameFieldsEaxAbi` | Slides the two dwords at +0/+8 down into +4/+0xc (snapshot -> previous) and clears the +0x14 counter. |
| 0x0042aa10 | `InitializeReplayHeaderDefaultsEaxAbi` | Zeroes the first 0x24 bytes, writes the "t10r" magic with the 5 format byte at +4 and the 0x100 game version at +0x10. |
| 0x0042ac20 | `ResetReplayFrameRecordEcxAbi` | Zeroes the first 0x24 bytes, slides the two dwords at +0/+8 into +4/+0xc, threads the record's self pointer at +0x18 and clears +0x1c/+0x20. |

## Build note

The 0x004296f0 body binds the 0x0042ac60 scalar destructor through the
exported `UnlinkGameModeChainRecordInPlace` from `GameModeTeardown.cpp`
(previously file-local; moved out of its anonymous namespace so the replay
frame pool can share the same destructor).

## Verification

Reference disassembly: `build/reference/004296f0_*.asm` ..
`build/reference/0042ac20_*.asm`. The difficulty color thresholds in
0x00429a70 are the two native float compares against `DAT_00470c30` /
`DAT_00470c2c` with the unordered-takes-white branch.
