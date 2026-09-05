#pragma once

#include "Th10Platform.hpp"

namespace th10 {

u32 TH10_CDECL MainChainBackgroundThread(void *unused); // TH10 0x0043ba90

// _beginthreadex accepts a stdcall public callback even though TH10's native
// entry above is a cdecl/plain-ret function.
u32 TH10_STDCALL MainChainBackgroundThreadAdapter(void *unused);

} // namespace th10
