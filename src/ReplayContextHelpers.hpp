#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004296f0. Stack = source (retn 4). Allocates the 0x2d4 demo
// record, installs the frame record reset (0x0042ac20) / frame
// release (0x0042ac60) pair through the 0x45252d pool iterator over
// 0x24-byte records at record+0xa0 (8 count), zeroes the whole 0x2d4
// record and sets +0x10 = 2, then parses the source through
// 0x0042a200 (record in ESI). On parse failure the record is torn
// down (0x004294a0) and freed; returns the record or 0.
void *ParseDemoRecordEsiStackAbi(u32 source);

// TH10 0x004297b0. ESI = demo record. Tears the record down through
// 0x004294a0 and frees it (skipped entirely when the record is 0).
void DestroyDemoRecordEsiAbi(void *record);

// TH10 0x00429a70. EAX = ignored, EDX = replay context (retn 4).
// While the main chain context (DAT_00477810) is live and the context
// mode at +0x10 is exactly 1, colors the difficulty label: difficulty
// at ctx+0x1c4 compared against DAT_00470c30 (red 0xff5050ff below
// the threshold) and DAT_00470c2c (light 0xffa0a0ff), white
// (0xffffffff) above both; the color lands at DAT_004776e0+0x8974
// and is reset to white after the "%3d" format text is submitted
// through the ascii manager. Returns 1.
i32 DrawDifficultyLabelEaxEdxAbi(i32 unused, void *context);

// TH10 0x0042a820. ECX = demo record. Returns the byte delta between
// the record's frame cursor at +0x6274 and the frame table at
// +0x5464 (the cursor slot minus its own base).
i32 ComputeReplayFrameOffsetDeltaEcxAbi(const void *record);

// TH10 0x0042a990. EAX = frame record. Slides the two dwords at
// +0/+8 down into +4/+0xc (snapshot -> previous) and clears the
// +0x14 counter.
void SnapshotReplayFrameFieldsEaxAbi(void *record);

// TH10 0x0042aa10. EAX = replay header. Zeroes the first 0x24 bytes,
// writes the "t10r" magic with the 5 format byte at +4 and the 0x100
// game version at +0x10.
void InitializeReplayHeaderDefaultsEaxAbi(void *header);

// TH10 0x0042ac20. ECX = frame record. Zeroes the first 0x24 bytes,
// slides the two dwords at +0/+8 into +4/+0xc, threads the record's
// self pointer at +0x18 and clears +0x1c/+0x20.
void ResetReplayFrameRecordEcxAbi(void *record);

} // namespace th10
