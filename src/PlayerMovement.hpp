#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004250b0 semantic body. Native entry receives the player in EDI
// and returns zero; its register ABI remains a thunk boundary.
i32 UpdatePlayerMovementEdiAbi(void *player);

} // namespace th10
