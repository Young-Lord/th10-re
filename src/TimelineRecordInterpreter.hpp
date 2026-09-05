#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0040bd80. Native ABI is one stack state pointer and ret 4.
i32 ExecuteTimelineRecords(void *state);
// TH10 0x0040bd20. Native receives the state in ESI; the source signature
// models its semantic input while keeping that register ABI at the boundary.
i32 AdvanceTimelineRecordController(void *state);

} // namespace th10
