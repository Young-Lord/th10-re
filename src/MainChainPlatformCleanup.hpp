#pragma once

#include "MainChainContext.hpp"

namespace th10 {

// Semantic subset of TH10 0x00438ad0 cleanup beginning at 0x00438dd0.
// The enclosing application loop, its local exit code, and later log/config
// branches remain separate reconstruction work.
void ShutdownMainChainPlatformResources(MainChainContext *context);

} // namespace th10
