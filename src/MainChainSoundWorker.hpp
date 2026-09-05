#pragma once

#include "MainChainRuntime.hpp"

namespace th10 {

// Semantic body of the mixed-ABI TH10 0x43cd30 sound-root initializer.
// The native edge receives root in ECX and the window on the stack.
i32 InitializeMainChainSoundRoot(TransitionRootPartial *root, void *window);

} // namespace th10
