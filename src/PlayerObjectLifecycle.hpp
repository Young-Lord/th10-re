#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004247f0 semantic body. Native entry receives EBX=player; its
// register ABI remains a thunk boundary.
i32 InitializePlayerObject(void *player);

} // namespace th10
