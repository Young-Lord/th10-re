#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0040d280. Native register ABI: EBX = the 0x68-byte conditional
// state object published at DAT_00477704 (created by 0x0040d6b0), one stack
// argument (ret 4) = the stage enemy script path forwarded by the creator.
// Fills the state's sub-records: the effect-manager pool word at +0x30, the
// 0x1098-byte script viewer at +0x54 (vtable 0x46d0b4, virtually initialized
// with the path), and the two 0x24-byte scheduler records at +0x8 (frame
// ticker, calculation chain, priority 0x12) and +0xc (no-op, draw chain,
// priority 0x14). Initializes the score/countdown block at +0x40..+0x50 and
// always returns 0 (the caller treats nonzero as "destroy the state").
i32 InitializeConditionalStateSubrecordsEbxStackAbi(void *state /* EBX */,
                                                    const char *script_path);

// TH10 0x0040d750 (via 0x0040d810). Native ECX = the conditional state
// object (the scheduler record argument). Walks the state's ECL script
// object list at +0x58, forces one update per record, then advances the
// +0x44/+0x48 frame/score-anim accumulator against the +0x4c rate pointer.
// Returns 1.
i32 TickConditionalStateRecordsFastAbi(void *state);

} // namespace th10
