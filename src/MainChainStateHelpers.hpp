#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00420b80. ESI = track id, stack = path (retn 4). Copies the
// path, rewrites the extension after the last '.' to "wav", raises the
// queued byte at DAT_00477783c[track + 0x1d892] and queues BGM command
// 2 (-1) on the global queue (0x00492590). Returns 0.
i32 QueueBgmTrackWavPathEsiStackAbi(i32 track, const char *path);

// TH10 0x00420c00. Queues BGM command 4 when DAT_00491d78 bit 4 is
// set, else command 3 (both with argument 0). Returns 0.
i32 QueueBgmResumeModeSelect(void);

// TH10 0x00420dd0. EAX = replay header state, stack = {key, value_a,
// value_b} (retn 0xc). Scans the replay header text at state+0x764
// (length at state+0x760) for the 5-char key: keys starting with
// "debug" return 0, the dead strcmp("0100a","debug") gate always
// falls through, and each matching line is parsed with "%d %d" and
// compared against the two values. Returns 0 on a match, -1 when the
// buffer is exhausted, 0 when the header pointer is missing.
i32 SearchReplayHeaderKeyEaxStackAbi(const void *state, const char *key,
                                     i32 value_a, i32 value_b);

// TH10 0x00421420 / 0x00421450. EAX = game state. Enters / leaves the
// seven stage-load critical sections at state+0x64c (0x18 stride) via
// the imported thunk (0x004660b8 / 0x004660bc).
void EnterAllStageLoadSectionsEaxAbi(void *state);
void LeaveAllStageLoadSectionsEaxAbi(void *state);

// TH10 0x00421c50. EDI = game state, ESI = section index. Leaves the
// single stage-load critical section and decrements the lock counter
// byte at state+index+0x6f4.
void LeaveStageLoadSectionEdiEsiAbi(void *state, i32 index);

// TH10 0x00421b70. ECX = stage practice selector. Copies the slot at
// +0x40 into +0x3c and publishes the 0x30-byte record at
// DAT_00474788 + 48 * slot into DAT_004777848. Returns the record.
void *SelectStageRecordSlotThiscall(void *selector);

// TH10 0x00421b90. EAX = output, EDX/ECX = operands. Cross product
// out = a x b (three floats).
void CrossProductVec3EaxDxEcxAbi(float out[3], const float a[3],
                                 const float b[3]);

// TH10 0x00421c90 / 0x00421ca0 / 0x00421cb0 / 0x00421cc0 /
// 0x00421cf0. EAX = game state. Flag bit 6 / 5 / 3 / 1 / 2 reads of
// the state word at +0x150.
i32 ReadGameFlagBit6EaxAbi(const void *state);
i32 ReadGameFlagBit5EaxAbi(const void *state);
i32 ReadGameFlagBit3EaxAbi(const void *state);
i32 ReadGameFlagBit1EaxAbi(const void *state);
i32 ReadGameFlagBit2EaxAbi(const void *state);

// TH10 0x00421d00. EAX = score record. Seeds the retry counters at
// +0x38c = -2 and +0x390 = 0; returns the record.
void *SeedScoreRecordRetryCountersEaxAbi(void *record);

// TH10 0x00421d20. ESI = record. Releases the record at +0xc through
// its vtable slot 2 (+0x8) and clears the pointer; returns the record
// (0 when it was already gone).
void *ReleaseScoreRecordSlotEsiAbi(void *record);

// TH10 0x00421e00. EAX = game state. Ticks the BGM fade sequencer at
// state+0x5208: mode 1 ramps in (5000*rem/total-5000) and stops the
// BGM through vtable slot 18 (+0x48) at completion, mode 2 ramps out
// (-5000*rem/total), mode 4 ramps in small steps (1000*rem/total-
// 1000) and mode 3 ramps out small steps (-1000*rem/total). Returns
// the last volume write.
i32 TickBgmFadeSequencerEaxAbi(void *state);

// TH10 0x00421f00. ECX = title sub-object, ESI = 0x784 game state.
// Initializes the sub-object at state+0x48, zeroes state+0x630..0x63c,
// installs the stage-entity vtable (0x004703e4) at state+0x62c, zeroes
// the whole 0x784 state and raises flag bits 6+8 (0x140) at +0x3cc.
// Returns the state.
void *ResetGameStateObjectEcxEsiAbi(void *sub_object, void *state);

// TH10 0x00421f60. EAX = game state. Copies the stage frame count
// DAT_00474c44 into state+0x9e98 and raises the running peak
// DAT_00474c40 when it exceeds it. Returns the copied count.
i32 RecordStageFrameCountPeakEaxAbi(void *state);

// TH10 0x0042c8c0. EAX = stage table. Seeds the flag dword row at
// +0x1d882..+0x1d891 with 0x01010101 (four dword stores) and six flag
// groups at +0x4e9 + 0x437c*g (four rows of six bytes, 8-byte stride).
void InitializeStageFlagByteTableEaxAbi(void *table);

// TH10 0x004349e0. EAX = cursor (updated in place), EDX = remaining
// length pointer. Skips to the next line break: unless the current
// char is already \n or \r it advances while the remaining budget
// lasts, then consumes the \n / \r run (still bounded by the budget).
const char *SkipLineBreaksEaxEdxAbi(const char *cursor, i32 *remaining);

} // namespace th10
