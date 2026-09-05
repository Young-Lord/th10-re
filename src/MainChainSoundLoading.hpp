#pragma once

#include "MainChainResourceThread.hpp"

namespace th10 {

// Semantic bodies for the mixed-ABI native entries at TH10 0x0043cf60 and
// 0x0043d390. Native-facing callers require separate register/stack thunks.
i32 ConsumeMainChainWaveImage(TransitionRootPartial *root, u32 index,
                              const char *resource_name);
i32 ConsumeAllMainChainWaveImages(TransitionRootPartial *root);

} // namespace th10
