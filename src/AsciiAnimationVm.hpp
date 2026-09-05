#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00401de0 semantic body. The native entry receives this pointer in
// EDX; callers use this typed interface and leave that ABI detail to a thunk.
void ResetAsciiAnimationVmRecord(void *record);

// TH10 0x0043e5a0 semantic body. The native entry receives EAX=vm,
// EDX=entry_index, and ECX=resource; its register ABI belongs in a thunk.
i32 InitializeAsciiAnimationVmEntry(void *vm, u32 entry_index,
                                    void *resource);

} // namespace th10
