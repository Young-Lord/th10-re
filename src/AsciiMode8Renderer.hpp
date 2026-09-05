#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00444760 semantic body. The native entry is EAX=vm plus stack owner.
i32 DrawAsciiAnimationVmMode8(void *vm, void *owner);

} // namespace th10
