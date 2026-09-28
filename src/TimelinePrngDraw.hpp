#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Native PRNG state object at DAT_004918b0 (reconstructed as the u16[4]
// global g_TimelinePrngStateB in TimelineRenderObjectSetup.cpp):
//   +0x00 u16 seed;         // low half of the LCG-B state
//   +0x04 u32 draw_counter; // incremented by 1/2/4 per draw call
struct TimelinePrngState {
    u16 seed;
    u32 draw_counter;
};

typedef char AssertTimelinePrngStateSize[
    sizeof(TimelinePrngState) == 8 ? 1 : -1];

// One LCG-B half-step over the 32-bit state chain:
//   x = (state ^ 0x9630) - 0x6553;  next = x * 4 + ((u16)x >> 14)
// Only the low 16 bits of each step feed the seed sequence, so the high
// bits carried between native steps never change the observable results.
u32 TimelinePrngHalfStep(u32 state);

// Native ECX = state. TH10 0x0044b9b0. Advances the seed one half-step,
// bumps the counter by 1 and returns the full 32-bit step value.
u32 TimelinePrngAdvanceSingleEcxEaxAbi(TimelinePrngState *state);

// Native ECX = state. TH10 0x0044b9e0. Advances two half-steps, bumps the
// counter by 2 and returns (h1 << 16) | h2.
u32 TimelinePrngDrawPairEcxEaxAbi(TimelinePrngState *state);

// Native ECX = state. TH10 0x0044ba80. Advances four half-steps, bumps the
// counter by 4, leaves the seed at h4 and returns (h1 << 16) | h2 (the
// first two draws packed).
u32 TimelinePrngDrawQuadPackedEcxEaxAbi(TimelinePrngState *state);

// Native ECX = state, result in ST0. TH10 0x0044bb20. Two half-steps, then
// (double)(u32)((h1 << 16) | h2) / 2^32 — a [0, 1) float draw.
double TimelinePrngDrawUnitDoubleEcxEfiAbi(TimelinePrngState *state);

// Native ECX = state, result in ST0. TH10 0x0044bb90. Two half-steps, then
// (double)(u32)((h1 << 16) | h2) / 2^31 - 1 — a [-1, 1) float draw.
double TimelinePrngDrawSignedDoubleEcxEfiAbi(TimelinePrngState *state);

} // namespace th10
