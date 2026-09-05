#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Native EAX/plain-ret entry at 0x443b60.
i32 BuildPerspectiveAsciiGlyphQuad(void *vm);

// Native EAX=vm plus stack owner/ret-4 entry at 0x444240.
u32 BuildAndProjectMode7AsciiGlyphQuad(void *vm, void *owner);

// Mode 5 native input is ESI=vm, EDI=owner. Mode 6 uses EDI=vm, ESI=owner.
u32 DrawAsciiAnimationVmProjectedMode5(void *vm, void *owner);
i32 DrawAsciiAnimationVmPerspectiveFadedMode6(void *vm, void *owner);
u32 DrawAsciiAnimationVmProjectedFoggedMode7(void *owner, void *vm);

} // namespace th10
