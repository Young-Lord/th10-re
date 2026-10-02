#include "TitleScoreAnimTriggers.hpp"

#include "AsciiOverlayFactory.hpp"
#include "TitleScreenState.hpp"

namespace th10 {

namespace {

const u32 k_rate_pointer = 0x476f78U; // &flt_476F78

// Seeds the score-anim block on first use (initialized-flag bit 0 of
// score_anim_timer.flags at +0x2a2c) and arms it with the given counter,
// float value and limit (the +0x2a1c limit lands in TimerNode.prev).
u32 SeedAndArm(TitleScreenState &state, u32 counter, u32 value_bits,
               u32 limit)
{
    u32 flag = state.score_anim_timer.flags;
    if ((flag & 1U) == 0U) {
        flag |= 1U;
        state.score_anim_timer.count = 0;
        *reinterpret_cast<u32 *>(&state.score_anim_timer.prev) =
            static_cast<u32>(-999999); // 0xfff0bdc1 poison
        state.score_anim_timer.accum = 0;
        state.score_anim_timer.rate =
            reinterpret_cast<const float *>(k_rate_pointer);
        state.score_anim_timer.flags = flag;
    }
    state.score_anim_timer.count = static_cast<i32>(counter);
    *reinterpret_cast<u32 *>(&state.score_anim_timer.accum) = value_bits;
    state.score_anim_timer.prev = static_cast<i32>(limit);
    return flag;
}

} // namespace

u32 TriggerTitleScoreAnim30EsiAbi(void *state_ptr)
{
    TitleScreenState &state = *reinterpret_cast<TitleScreenState *>(state_ptr);
    (void)CreateAsciiOverlayContext(2U, 30U, 0U, 0U, 0U, 0U);
    const u32 flag = SeedAndArm(state, 30U, 0x41f00000U /* 30.0f */, 29U);
    state.master_flags |= 2U;
    return flag;
}

void *TriggerTitleScoreAnim60EaxAbi(void *state_ptr)
{
    TitleScreenState &state = *reinterpret_cast<TitleScreenState *>(state_ptr);
    SeedAndArm(state, 60U, 0x42700000U /* 60.0f */, 59U);
    state.master_flags |= 4U;
    return state_ptr;
}

} // namespace th10
