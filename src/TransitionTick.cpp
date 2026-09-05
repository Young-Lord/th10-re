#include "MainChainRuntime.hpp"
#include "TransitionSoundAdapter.hpp"

namespace th10 {

namespace {

i32 WrapSubtractOne(i32 value)
{
    return static_cast<i32>(static_cast<u32>(value) - 1U);
}

i32 WrapMultiply(i32 left, i32 right)
{
    return static_cast<i32>(static_cast<u32>(left) * static_cast<u32>(right));
}

void ApplyTransitionVolume(TransitionControlPartial *control, i32 input)
{
    SetTransitionBufferVolume(control, input);
}

void TickPhase(TransitionControlPartial *control, TransitionPhase phase,
               i32 multiplier, i32 bias, bool stop_on_expiry)
{
    const i32 remaining = WrapSubtractOne(control->remaining_ticks);
    control->remaining_ticks = remaining;
    if (remaining <= 0) {
        control->phase = TransitionPhase_None;
        if (stop_on_expiry)
            StopTransitionBuffer(control);
        return;
    }

    // Native code uses signed IMUL/CDQ/IDIV. duration_ticks == 0 faults in
    // both the source-level division and the original executable.
    const i32 volume = WrapMultiply(remaining, multiplier) /
        control->duration_ticks + bias;
    ApplyTransitionVolume(control, volume);
}

} // namespace

// TH10 0x00421e00 semantic body. The binary entry receives root in EAX; this
// source function is deliberately named for that boundary while callers use
// the normal C++ signature during reconstruction.
void AdvanceTransitionEaxAbi(TransitionRootPartial *root)
{
    TransitionControlPartial *control = root->control;
    if (control == 0)
        return;

    switch (control->phase) {
    case TransitionPhase_StopFade:
        TickPhase(control, TransitionPhase_StopFade, 5000, -5000, true);
        break;

    case TransitionPhase_FadeIn:
        TickPhase(control, TransitionPhase_FadeIn, -5000, 0, false);
        break;

    case TransitionPhase_FadeInShort:
        TickPhase(control, TransitionPhase_FadeInShort, 1000, -1000, false);
        break;

    case TransitionPhase_FadeOutShort:
        TickPhase(control, TransitionPhase_FadeOutShort, -1000, 0, false);
        return;

    default:
        break;
    }
}

} // namespace th10
