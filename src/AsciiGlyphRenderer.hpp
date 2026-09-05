#pragma once

#include "Th10Types.hpp"

namespace th10 {

void DrawAsciiAnimationVmUnscaled(void *vm);
void DrawAsciiAnimationVmScaled(void *vm);
void DrawAsciiAnimationVmUnscaledToOwner(void *vm, void *owner);
void DrawAsciiAnimationVmScaledToOwner(void *vm, void *owner);
void DrawAsciiAnimationVmRotatedMode1(void *vm, void *owner);
void DrawAsciiAnimationVmRotatedMode3(void *vm, void *owner);
i32 DrawAsciiAnimationVmProjectedMode4(void *vm, void *owner);

} // namespace th10
