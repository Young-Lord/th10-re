#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Shared scaled-timer utilities over the five-field block
// {prev i32, count i32, accumulator f32, rate ptr, flags}.

// TH10 0x00405410. Native inputs EAX = timer, stack = duration; ret 4.
void TickPlayerTimerEaxStackAbi(void *timer, i32 duration);

// TH10 0x00404ed0. Native input ESI = timer; advances one frame.
void TickTimerForwardEsiAbi(void *timer);

// TH10 0x0044bf40. Native inputs ESI = timer, stack = float delta; ret 4.
void ShiftTimerByEsiStackAbi(void *timer, float delta);

} // namespace th10
