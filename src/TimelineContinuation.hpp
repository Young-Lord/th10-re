#pragma once

#include "Th10Platform.hpp"
#include "Th10Types.hpp"

namespace th10 {

struct ThreadControl;

// TH10 0x0040c540. Native receives AsciiManager in ESI; the floats are the
// continuation object parameters observed from opcode 7.
void StartTimelineContinuation(float width, float height);

// TH10 0x0044c1c0. Stops any prior thread, then starts the continuation
// worker with the supplied entry and argument.
void RegisterTimelineContinuation(ThreadControl *control, void *argument);

// TH10 0x0040c3a0. beginthreadex entry that runs the continuation body against
// the timeline state referenced from the gate object.
u32 TH10_STDCALL TimelineContinuationThreadEntry(void *argument);

} // namespace th10
