#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00428280. Native entry receives the player as one stack argument
// (ret 4) and returns zero; its stack ABI remains a thunk boundary.
i32 UpdatePlayerProjectilesStackAbi(void *player);

} // namespace th10
