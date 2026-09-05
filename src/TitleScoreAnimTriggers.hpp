#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Both triggers operate on the title-screen state object's embedded
// score-anim block (a1[2694..2699] = +0x2a18 block flags, +0x2a1c
// accumulator, +0x2a20 counter, +0x2a24 value, +0x2a28 rate pointer,
// +0x2a2c initialized-flag dword).

// TH10 0x00404f30?? no — 0x00404530. Native ESI = title-screen state:
// creates the kind-2 / value-30 overlay context (0x43c8b0 semantic),
// seeds the score-anim block on first use, then arms it with counter 30,
// value 30.0f, limit 29, and sets block-flag bit 1 (+0x2a18). Returns
// the block flag in EAX.
u32 TriggerTitleScoreAnim30EsiAbi(void *state);

// TH10 0x004045b0. Native EAX = title-screen state: same seeding, then
// arms with counter 60, value 60.0f, limit 59, and sets block-flag bit 2.
// Returns the state pointer in EAX.
void *TriggerTitleScoreAnim60EaxAbi(void *state);

} // namespace th10
