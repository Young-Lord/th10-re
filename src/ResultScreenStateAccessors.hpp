#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00415db0. EDI = main chain context, ESI = slot. Allocates a
// 0x90 result-screen script state, initializes it against the script
// blob at [ctx+0x9ebc] + [base+8*slot+4], publishes it at ctx+0x9eb8,
// stamps the slot at state+0, advances the result-slot counter
// (DAT_00474c84 = slot+1) and clears DAT_00474c8c when the counter
// actually moved. Returns slot+1. The native stores the slot stamp
// unconditionally, so a failed allocation would null-deref (operator
// new aborts in practice).
i32 CreateResultScreenScriptSlotEdiEsiAbi(void *manager, i32 slot);

// TH10 0x004175b0 / 0x00417600 / 0x00417690. EAX = record. Flag bit
// 2 / 4 / 5 reads of +0x60.
i32 ReadEntityFlagBit2EaxAbi(const void *record);
i32 ReadEntityFlagBit4EaxAbi(const void *record);
i32 ReadEntityFlagBit5EaxAbi(const void *record);

// TH10 0x00417710. EAX = main chain context. Flag bit 3 read of
// +0x2a18.
i32 ReadManagerFlagBit3At2A18EaxAbi(const void *manager);

// TH10 0x004177c0 / 0x004177e0 (identical twins). ECX = owner. Marks
// flag bit 1 (0x2) on the two records at owner+8 / owner+0xc (no null
// checks; the records are always present for live owners).
void MarkResultPairFlagsThiscallA(void *owner);
void MarkResultPairFlagsThiscallB(void *owner);

// TH10 0x00417800. EAX = block. Re-arms the animation timer at +0x40
// (flags at +0x50, rate pointer at +0x4c), then zeroes +0x58/+0x5c and
// returns 0.
i32 RearmInterpTimerAt40EaxAbi(void *block);

} // namespace th10
