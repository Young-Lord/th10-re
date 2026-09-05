#include "TitleScoreAnimTriggers.hpp"

#include "AsciiOverlayFactory.hpp"

namespace th10 {

namespace {

const u32 k_rate_pointer = 0x476f78U; // &flt_476F78

u32 LoadU32From(const void *address)
{
    const u8 *const bytes = static_cast<const u8 *>(address);
    return static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8)
         | (static_cast<u32>(bytes[2]) << 16)
         | (static_cast<u32>(bytes[3]) << 24);
}

void StoreU32To(void *address, u32 value)
{
    u8 *const bytes = static_cast<u8 *>(address);
    bytes[0] = static_cast<u8>(value);
    bytes[1] = static_cast<u8>(value >> 8);
    bytes[2] = static_cast<u8>(value >> 16);
    bytes[3] = static_cast<u8>(value >> 24);
}

// Seeds the score-anim block on first use (initialized-flag bit 0 at
// +0x2a3c) and arms it with the given counter, float value and limit.
u32 SeedAndArm(u8 *state, u32 counter, u32 value_bits, u32 limit)
{
    u32 flag = LoadU32From(state + 0x2a2cU);
    if ((flag & 1U) == 0U) {
        flag |= 1U;
        StoreU32To(state + 0x2a20U, 0U);
        StoreU32To(state + 0x2a1cU, static_cast<u32>(-999999));
        StoreU32To(state + 0x2a24U, 0U);
        StoreU32To(state + 0x2a28U, k_rate_pointer);
        StoreU32To(state + 0x2a2cU, flag);
    }
    StoreU32To(state + 0x2a20U, counter);
    StoreU32To(state + 0x2a24U, value_bits);
    StoreU32To(state + 0x2a1cU, limit);
    return flag;
}

} // namespace

u32 TriggerTitleScoreAnim30EsiAbi(void *state)
{
    u8 *const base = static_cast<u8 *>(state);
    (void)CreateAsciiOverlayContext(2U, 30U, 0U, 0U, 0U, 0U);
    const u32 flag = SeedAndArm(base, 30U, 0x41f00000U /* 30.0f */, 29U);
    StoreU32To(base + 0x2a18U, LoadU32From(base + 0x2a18U) | 2U);
    return flag;
}

void *TriggerTitleScoreAnim60EaxAbi(void *state)
{
    u8 *const base = static_cast<u8 *>(state);
    SeedAndArm(base, 60U, 0x42700000U /* 60.0f */, 59U);
    StoreU32To(base + 0x2a18U, LoadU32From(base + 0x2a18U) | 4U);
    return state;
}

} // namespace th10
