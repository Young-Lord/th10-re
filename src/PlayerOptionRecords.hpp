#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00426f70 semantic body. Native entry receives the player as one
// stack argument (ret 4); its stack ABI remains a thunk boundary.
void RebuildPlayerOptionRecords(void *player);

} // namespace th10
