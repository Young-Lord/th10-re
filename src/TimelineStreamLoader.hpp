#pragma once

#include "Th10Types.hpp"

namespace th10 {

struct TimelineGateStatePartial;

// TH10 0x0040b480. Copies the requested path into the shared scratch buffer,
// loads archive data through the packed-resource loader, and stores the result
// at gate_state+0x14 after freeing any prior buffer.
u8 *LoadTimelineStream(TimelineGateStatePartial *gate_state,
                       const char *path);

} // namespace th10
