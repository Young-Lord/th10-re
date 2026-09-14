#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x43bfa0. Native EDI = four-float record, stack = four scalars
// {arg1..arg4}; ret 0x10. Flushes the render-owner vertex buffer, assembles
// a four-vertex XYZRHW|DIFFUSE triangle strip from the record and the stack
// scalars, runs the ending's texture-stage/render-state block, draws it,
// and resets the render-owner draw caches.
void DrawEndingOverlayQuadStackAbi(const void *record, u32 arg1, u32 arg2,
                                   u32 arg3, u32 arg4);

} // namespace th10
