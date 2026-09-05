#pragma once

#include "MainChainContext.hpp"

namespace th10 {

// TH10 0x00420270 semantic body. The native entry receives the context in
// EAX and returns with plain ret; that register ABI remains a thunk boundary.
i32 ShutdownMainChainRuntime(MainChainContext *context);

} // namespace th10
