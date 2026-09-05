#pragma once

#include <stddef.h>

#include "Th10Types.hpp"

namespace th10 {

struct ChainElem;

struct TimelineGateStatePartial {
    u32 flags;
    u32 unknown_0004;
    ChainElem *calculation_callback;
    ChainElem *draw_callback;
    u32 unknown_0010;
    u8 *stream_buffer;
    u8 *inner_timeline_state;
    i32 message_index;
    u32 gate_word_0020;
    i32 frame_counter;
};

typedef char AssertTimelineGateSize[sizeof(TimelineGateStatePartial) == 0x28 ? 1 : -1];
typedef char AssertTimelineGateStreamBufferOffset[
    offsetof(TimelineGateStatePartial, stream_buffer) == 0x14 ? 1 : -1];
typedef char AssertTimelineGateInnerStateOffset[
    offsetof(TimelineGateStatePartial, inner_timeline_state) == 0x18 ? 1 : -1];
typedef char AssertTimelineGateMessageIndexOffset[
    offsetof(TimelineGateStatePartial, message_index) == 0x1c ? 1 : -1];
typedef char AssertTimelineGateWordOffset[
    offsetof(TimelineGateStatePartial, gate_word_0020) == 0x20 ? 1 : -1];
typedef char AssertTimelineGateFrameCounterOffset[
    offsetof(TimelineGateStatePartial, frame_counter) == 0x24 ? 1 : -1];

extern TimelineGateStatePartial *g_TimelineGateState; // TH10 DAT_00477700

TimelineGateStatePartial *CreateTimelineGateState();
i32 InitializeTimelineGateState(TimelineGateStatePartial *gate_state);
void DestroyTimelineGateState(TimelineGateStatePartial *gate_state);

// TH10 0x0040b9d0. Native ESI = gate object: in-place destroy then free.
void DestroyTimelineGateStateObject(TimelineGateStatePartial *gate_state);

} // namespace th10
