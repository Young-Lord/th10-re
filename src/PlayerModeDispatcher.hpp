#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00425730 semantic body. Native entry receives the player as one
// stack argument (thunk 0x426500 converts ECX); its stack ABI remains a
// thunk boundary.
i32 UpdatePlayerModeDispatcher(void *player);

} // namespace th10
