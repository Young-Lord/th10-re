#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH11 0x00426360 semantic body. Native entry receives EAX=object and
// returns one; its register ABI remains a thunk boundary.
i32 RenderAsciiSceneObjectAndBarOverlay(void *object);

} // namespace th10
