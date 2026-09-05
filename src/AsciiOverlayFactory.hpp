#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH11 0x0043c8b0 semantic body. Native entry additionally receives scheduler
// priority in EBX and five stack words, returns with ret 20. Its fourth stack
// word is intentionally ignored; the fifth is copied to two context fields.
void *CreateAsciiOverlayContext(u32 kind, u32 value_1, u32 value_2,
                                u32 ignored_value, u32 value_3,
                                u32 draw_priority);

} // namespace th10
