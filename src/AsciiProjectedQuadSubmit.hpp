#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x442ad0. Native EAX = VM record, ECX = render owner, stack = one
// flags byte (bit 0: pixel-snap the quad, bit 1: skip the color update);
// ret 4, always returns 0. Shifts the glyph scratch quad by the owner's
// camera values, optionally snaps it to the pixel grid, re-derives the UVs
// from the VM's texture-source record, culls the quad against the active
// viewport, binds the texture, updates the blend/sampler state, modulates
// the color and appends the quad as two triangles to the owner's vertex
// buffer. Called from the projected-glyph wrapper at 0x443670.
i32 SubmitAsciiProjectedQuadEaxEcxStackAbi(void *vm, void *owner, u32 flags);

} // namespace th10
