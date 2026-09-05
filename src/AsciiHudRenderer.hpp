#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0043bda0 semantic body. Native entry receives EBX=color and
// EDI={left, top, right, bottom}; its register ABI remains a thunk concern.
void DrawImmediateAsciiColoredRectangle(const float rectangle[4], u32 color);

// TH10 0x00415800 semantic body. Native entry receives one stack owner and
// returns one after ret 4.
i32 RenderAsciiHudBatch(void *owner);

} // namespace th10
