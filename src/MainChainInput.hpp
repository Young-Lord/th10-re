#pragma once

#include "MainChainContext.hpp"

namespace th10 {

i32 InitializeMainChainInputSemantic(MainChainContext *context);
i32 TH10_FASTCALL InitializeMainChainInputFromEax(MainChainContext *context);
// TH10 0x0043b8d0 requires an EAX-register/plain-ret thunk for native entry
// replacement; the fastcall declaration is the reconstruction-side bridge.

} // namespace th10
