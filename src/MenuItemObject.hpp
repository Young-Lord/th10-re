#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004386B0. MSVC std::string reallocation helper used by the menu
// item records: grows/assigns the buffer (union at +4: SSO bytes or heap
// pointer; size at +0x14; capacity at +0x18) to the requested capacity,
// copying exactly `copy_size` bytes from the previous buffer (unchecked
// overread when copy_size exceeds the old length - native quirk).
// Returns the new data pointer. Native thiscall ABI remains a thunk
// boundary.
void *MenuItemStringAssign(void *string_object, u32 new_capacity,
                           u32 copy_size);

} // namespace th10
