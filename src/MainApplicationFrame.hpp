#pragma once

#include <stddef.h>

#include "Th10Types.hpp"

namespace th10 {

// Sparse state rooted at TH10 DAT_004924f0. Its leading word is also the
// published window handle; the frame function receives the base address.
struct MainApplicationFrameState {
    void *window_0000;
    u8 unknown_0004[0x10];
    u8 present_counter_0014;
    u8 unknown_0015[0x23];
    double frame_now_0038;
    double previous_now_0040;
    double scheduled_now_0048;
};

typedef char AssertMainApplicationFrameNowOffset[
    offsetof(MainApplicationFrameState, frame_now_0038) == 0x38 ? 1 : -1];
typedef char AssertMainApplicationFrameScheduledOffset[
    offsetof(MainApplicationFrameState, scheduled_now_0048) == 0x48 ? 1 : -1];

// TH10 0x00439390 semantic body. Native stack-argument/ret-4 ABI is separate.
i32 RunMainChainFrame(MainApplicationFrameState *state);
// TH10 0x004391f0 semantic body. Native entry has no stack arguments.
void PresentAndRecoverMainChainDevice();

} // namespace th10
