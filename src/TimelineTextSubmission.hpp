#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00447a50. Native keeps the target object in ESI; this semantic entry
// takes the object explicitly together with owner, color, and format text.
void SubmitTimelineText(void *object, void *owner, u32 color,
                        const char *text);

} // namespace th10
