# Main Chain State Helpers (`0x00420b80` family)

Batch B reconstruction evidence for the game-state helper layer: BGM
queueing, replay header search, stage-load critical sections, flag reads and
misc record helpers. Bodies live in `src/MainChainStateHelpers.cpp`, except
where noted.

| Address | Name | Contract |
|---------|------|----------|
| 0x00420b80 | `QueueBgmTrackWavPathEsiStackAbi` | ESI = track id, stack = path (`retn 4`). Copies the path, rewrites the extension after the last '.' to "wav", raises the queued byte at `DAT_00477783c`[track + 0x1d892] and queues BGM command 2 (-1) on the global queue (0x00492590). Returns 0. |
| 0x00420c00 | `QueueBgmResumeModeSelect` | Queues BGM command 4 when `DAT_00491d78` bit 4 is set, else command 3 (both with argument 0). Returns 0. |
| 0x00420dd0 | `SearchReplayHeaderKeyEaxStackAbi` | EAX = replay header state, stack = {key, value_a, value_b} (`retn 0xc`). Scans the replay header text at state+0x764 (length at state+0x760) for the 5-char key: keys starting with "debug" return 0, the dead `strcmp("0100a","debug")` gate always falls through, and each matching line is parsed with `"%d %d"` and compared against the two values. Returns 0 on a match, -1 when the buffer is exhausted, 0 when the header pointer is missing. |
| 0x00421420 | `EnterAllStageLoadSectionsEaxAbi` | Enters the seven stage-load critical sections at state+0x64c (0x18 stride) via the imported thunk 0x004660b8. |
| 0x00421450 | `LeaveAllStageLoadSectionsEaxAbi` | Leaves the same seven sections via 0x004660bc. |
| 0x00421b70 | `SelectStageRecordSlotThiscall` | ECX = stage practice selector. Copies the slot at +0x40 into +0x3c and publishes the 0x30-byte record at `DAT_00474788` + 48 * slot into `DAT_004777848`. Returns the record. |
| 0x00421b90 | `CrossProductVec3EaxDxEcxAbi` | Cross product out = a x b (three floats) into EAX, EDX/ECX operands. |
| 0x00421c50 | `LeaveStageLoadSectionEdiEsiAbi` | Leaves the single stage-load critical section and decrements the lock counter byte at state+index+0x6f4. |
| 0x00421c90 | `ReadGameFlagBit6EaxAbi` | Flag bit 6 read of the state word at +0x150. |
| 0x00421ca0 | `ReadGameFlagBit5EaxAbi` | Flag bit 5 read of +0x150. |
| 0x00421cb0 | `ReadGameFlagBit3EaxAbi` | Flag bit 3 read of +0x150. |
| 0x00421cc0 | `ReadGameFlagBit1EaxAbi` | Flag bit 1 read of +0x150. |
| 0x00421cf0 | `ReadGameFlagBit2EaxAbi` | Flag bit 2 read of +0x150. |
| 0x00421d00 | `SeedScoreRecordRetryCountersEaxAbi` | Seeds the retry counters at +0x38c = -2 and +0x390 = 0; returns the record. |
| 0x00421d20 | `ReleaseScoreRecordSlotEsiAbi` | Releases the record at +0xc through its vtable slot 2 (+0x8) and clears the pointer; returns the record (0 when it was already gone). |
| 0x00421e00 | `TickBgmFadeSequencerEaxAbi` | Body in `src/TransitionTick.cpp` (see `docs/evidence/transition-tick.md`): ticks the BGM fade sequencer at state+0x5208 with the four ramp modes and stops the BGM through vtable slot 18 (+0x48) at completion. |
| 0x00421f00 | `ResetGameStateObjectEcxEsiAbi` | Initializes the sub-object at state+0x48, zeroes state+0x630..0x63c, installs the stage-entity vtable (0x004703e4) at state+0x62c, zeroes the whole 0x784 state and raises flag bits 6+8 (0x140) at +0x3cc. Returns the state. |
| 0x00421f60 | `RecordStageFrameCountPeakEaxAbi` | Body in `src/PostRunReplaySaveMenu.cpp` (see `docs/evidence/post-run-replay-save-menu.md`): copies the stage frame count `DAT_00474c44` into state+0x9e98 and raises the running peak `DAT_00474c40`. |
| 0x004349e0 | `SkipLineBreaksEaxEdxAbi` | EAX = cursor (updated in place), EDX = remaining length pointer. Skips to the next line break: unless the current char is already \\n or \\r it advances while the remaining budget lasts, then consumes the \\n / \\r run (still bounded by the budget). Returns the advanced cursor. |

## Verification

Reference disassembly: `build/reference/00420b80_*.asm` ..
`build/reference/00421f60_*.asm` and `build/reference/004349e0_*.asm`.
The 0x004349e0 skip loop was re-verified instruction by instruction against
the body in `MainChainStateHelpers.cpp` (budget check ordering: the first
character is tested before any budget decrement; the trailing newline run
re-checks the budget on every character).
