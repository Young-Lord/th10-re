#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00428f60. Native usercall: EAX = mode, ECX = replay file name,
// EBX = the 0x2d4-byte context record. Mode 0 builds a fresh snapshot of the
// running game into the record; mode 1 loads the replay file into the record
// and then restores it into the live globals; mode 2 only loads the file.
// Returns 0 normally, -1 when the file load fails.
i32 SetupReplayGameContextEaxEcxAbi(i32 mode, const char *file_name,
                                    void *context);

// TH10 0x00429610. Factory: allocates the 0x2d4-byte record (eight 0x24-byte
// stage slots at +0xa0), runs the setup body, and frees the record on
// failure. Returns null on failure.
void *CreateReplayGameContextStdcallAbi(i32 mode, const char *file_name);

// TH10 0x00418b80. Native thiscall ECX = score block (DAT_00474c40), stack =
// value; sets +0x0c = value/10 and resets the +0x14 timer family.
void SnapshotScoreBlockMaxScoreThisAbi(void *score_block, i32 value);

// TH10 0x0042a930. Native EAX = score block (DAT_00474c40), stack = value;
// resets the +0x14 power timer family to the value.
void ResetScoreBlockPowerTimerEaxStackAbi(void *score_block, i32 value);

// TH10 0x00418c40. Native __fastcall ECX ignored, EDX = 0x34-byte block;
// writes the default replay-header stats block, then the caller zeroes the
// whole 100-byte header over it (native order preserved).
void InitReplayHeaderStatsBlockEdxAbi(u8 *block);

} // namespace th10
