#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004423e0. Native register ABI: EAX = render owner (the large
// DAT_00491c10 object whose render caches live at +0x3ada60..+0x3ada6e and
// whose modulation multipliers live at +0x732458..+0x73245b), EBX = the
// sprite/effect entity record (+0x35c flag dword, +0x2fc/+0x300 color
// pair). Applies the entity's blend mode, modulated texture-factor color,
// and point/linear sampler selection to the device while keeping the
// owner-side caches coherent; every cache miss flushes the pending glyph
// vertex buffer first. Ends by bumping the owner state counter at +0x54.
void ApplyEntityRenderStateEaxEbxAbi(void *owner /* EAX */,
                                     const void *entity /* EBX */);

} // namespace th10
