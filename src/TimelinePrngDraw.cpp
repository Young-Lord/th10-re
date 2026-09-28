// Timeline PRNG draws (TH10 0x0044b9b0 / 0x0044b9e0 / 0x0044ba80 /
// 0x0044bb20 / 0x0044bb90). The standalone native accessors over the
// DAT_004918b0 LCG-B state; the ECL VM (0x44e1a0) and the render-object
// setup paths inline the same recurrence.
#include "TimelinePrngDraw.hpp"

namespace th10 {

u32 TimelinePrngHalfStep(u32 state)
{
    const u32 x = (state ^ 0x9630U) - 0x6553U;
    // The shift term only ever reads the low 16 bits (native `mov dx, ax`
    // / `shr dx, 14`), giving a 2-bit correction to x * 4.
    return (x << 2) + ((x & 0xffffU) >> 14);
}

u32 TimelinePrngAdvanceSingleEcxEaxAbi(TimelinePrngState *state)
{
    // Native ECX = state.
    u32 value = state->seed; // zero-extended seed feeds the first step
    value = TimelinePrngHalfStep(value);
    state->seed = static_cast<u16>(value);
    ++state->draw_counter;
    return value;
}

u32 TimelinePrngDrawPairEcxEaxAbi(TimelinePrngState *state)
{
    // Native ECX = state. Note the native stores h1 into the seed slot
    // mid-way and then overwrites it with h2; only the final seed and the
    // packed return are observable.
    u32 value = state->seed;
    value = TimelinePrngHalfStep(value);
    const u32 first = value & 0xffffU;
    value = TimelinePrngHalfStep(value);
    state->seed = static_cast<u16>(value);
    state->draw_counter += 2U;
    return (first << 16) | (value & 0xffffU);
}

u32 TimelinePrngDrawQuadPackedEcxEaxAbi(TimelinePrngState *state)
{
    // Native ECX = state. Four half-steps; the packed return carries the
    // first two draws while the seed lands on the fourth.
    u32 value = state->seed;
    value = TimelinePrngHalfStep(value);
    const u32 first = value & 0xffffU;
    value = TimelinePrngHalfStep(value);
    const u32 second = value & 0xffffU;
    value = TimelinePrngHalfStep(value);
    value = TimelinePrngHalfStep(value);
    state->seed = static_cast<u16>(value);
    state->draw_counter += 4U;
    return (first << 16) | second;
}

namespace {

// The native converts the packed u32 through a signed fild and, when
// negative, adds the float 2^32 before the final multiply — i.e. the
// unsigned interpretation of the packed dword, scaled in double precision
// (float 2^32 and the 2^-n scale constants are exact powers of two).
double PackedToUnitDouble(u32 packed)
{
    double value = static_cast<double>(static_cast<i32>(packed));
    if (value < 0.0)
        value += 4294967296.0; // DAT_00470b98 (2^32)
    return value;
}

} // namespace

double TimelinePrngDrawUnitDoubleEcxEfiAbi(TimelinePrngState *state)
{
    // Native ECX = state; scale DAT_00470bf0 = 2^-32.
    const u32 packed = TimelinePrngDrawPairEcxEaxAbi(state);
    return PackedToUnitDouble(packed) / 4294967296.0;
}

double TimelinePrngDrawSignedDoubleEcxEfiAbi(TimelinePrngState *state)
{
    // Native ECX = state; scale DAT_00470bec = 2^-31, minus
    // DAT_00470afc = 1.0.
    const u32 packed = TimelinePrngDrawPairEcxEaxAbi(state);
    return PackedToUnitDouble(packed) / 2147483648.0 - 1.0;
}

} // namespace th10
