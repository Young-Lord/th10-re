#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00427960. Native ESI = option record: refreshes the option's
// homing-slot position pair (+0x34/+0x38) from the player root's slot
// table, and either writes the current delta into the slot (+0x44/+0x48)
// and zero-fills the seven following trail slots, or (trailing mode)
// recomputes the delta from the table position. The slot's live flag at
// root+0x4474 gates the write-back. Native ESI ABI remains a thunk
// boundary.
void UpdateOptionTrailHistoryEsiAbi(void *option_record);

} // namespace th10
