#pragma once

#include "Th10Types.hpp"

#if defined(_MSC_VER)
#define TH10_STDCALL __stdcall
#define TH10_FASTCALL __fastcall
#define TH10_CDECL __cdecl
#else
#define TH10_STDCALL __attribute__((stdcall))
#define TH10_FASTCALL __attribute__((fastcall))
#define TH10_CDECL __attribute__((cdecl))
#endif

namespace th10 {

// Win32 CRITICAL_SECTION is 0x18 bytes in the target's x86 ABI.
// Its internals are intentionally opaque to the reconstruction.
struct Win32CriticalSection {
    u8 storage[0x18];
};

typedef char AssertWin32CriticalSectionSize[
    sizeof(Win32CriticalSection) == 0x18 ? 1 : -1];

} // namespace th10
