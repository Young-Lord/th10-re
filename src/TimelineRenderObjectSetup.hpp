#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0043ee30. Runs the render-object setup bytecode stream at node+0x390
// until the embedded timer at node+0x60 reaches the current instruction tick.
// Returns 0 on success and 1 on script failure (native low dword).
i32 FinalizeTimelineRenderObjectSetup(void *node);

} // namespace th10
