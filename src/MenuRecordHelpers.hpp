#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004220e0. EDX = 0x2c8 score record. Restores the record
// defaults (clears flag bit 0 of +0x20, zeroes +0xb0/+0x24/+0xf8/
// +0x188/+0x1d0/+0xfc, sets +0xf4/+0x1cc to 1 and +0x2c/+0x104 to
// 999), then zeroes the whole 0x2c8 record, sets flag bit 1 and
// publishes it at DAT_00477830. Returns the record.
void *ResetScoreRecordDefaultsEdxAbi(void *record);

// TH10 0x00422150. EBX = record owner. Registers the replay name
// entry calc callback 0x00422a90 and draw callback 0x00422aa0 at
// owner+8/+0xc, then re-arms the interpolation timer at owner+0x10
// (flags at +0x20, poison 0xfff0bdc1, rate pointer = the frame-time
// scale) with cur = 0, accum = 0, prev = -1. Returns 0.
i32 InstallReplayNameEntryCallbacksEbxAbi(void *owner);

// TH10 0x00422360. Allocates the 0x2c8 score record, resets its
// defaults, installs the name entry callbacks and returns the record;
// on callback failure the record is destroyed in place (0x00422220)
// and freed. Returns 0 on failure.
void *CreateScoreRecordOwner();

// TH10 0x004223f0. ESI = pause record. Sequencer tick over the pause
// mode at +4: mode 0 waits for the pause trigger (DAT_00474ca0 bit 5
// clear, DAT_00474e36 bit 3 or DAT_00491ff4 bit 4 set, main chain
// sub-object live with flag bit 1 and +0x14 frame count >= 30) and
// enters the pause setup; modes 1-5 run the pause menu; modes 6-13
// run the post-run replay save menu. Then advances the fade timer at
// +0x10..+0x20 through the rate-unity window and returns 1.
i32 TickPauseMenuSequencerEsiAbi(void *record);

// TH10 0x00423510. ESI = pause record. Selects post-run replay save
// mode 13 (+4), re-arms the interpolation timer at +0x10 (flags at
// +0x20, poison 0xfff0bdc1, rate pointer = the frame-time scale), then
// clears cur/accum with prev = -1, releases the two timeline
// continuation handles at +0x1d8/+0x1d4 and finally restores the raw
// frame-time scale dword from +0x2c0. Native quirk: the scale global
// is re-published as a raw dword copy.
void SeedPostRunReplaySaveModeEsiAbi(void *record);

} // namespace th10
