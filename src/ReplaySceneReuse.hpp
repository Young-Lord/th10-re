#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0042a6a0 (native usercall, EBX = the 0x2d4-byte replay game-context
// record whose pointer lives in DAT_00477838; no return value). Scene-reuse
// preparation step of the title -> game-scene setup (0x00417870, mode-flags
// bit 1 path), run before the scene script open. Branches on the context
// mode at +0x10:
//   0 (continue/fresh): allocate and zero a 0x1c4-byte stage record into the
//     per-stage pointer slot ctx+0x1c+4*stage, then seed it with the stage
//     word, the 0x4918b0 PRNG pair (counter cleared) and bit 0 of +0x1c0 set
//     to the game-active flag's low bit (DAT_00491fc4).
//   1 (replay load): refresh the per-stage cursor record at ctx+0xa0+36*stage
//     (saved/copy stores and cleared +0x14), then restore the run state from
//     the loaded stage snapshot at record+0x10 — PRNG pair, run score, the
//     0x474c48 word, the score-block maximum reset (0x00418b80 with
//     snapshot+0x14 * 10), the reduced inline of the power-timer reset
//     (0x0042a930 with snapshot+0x18), practice start index, rank, practice
//     score seed and play-count seed. The snapshot pointer is dereferenced
//     without a null check (native quirk, preserved).
//   other modes: untouched.
void PrepareReplaySceneReuse(void *context);

} // namespace th10
