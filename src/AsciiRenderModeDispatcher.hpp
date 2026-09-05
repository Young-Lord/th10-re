#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004451c0 semantic body. The native entry receives EAX=vm, ECX=owner.
i32 DispatchAsciiAnimationVmRenderMode(void *vm, void *owner);

} // namespace th10
