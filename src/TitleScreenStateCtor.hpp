#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00402230. Title-screen state constructor (native usercall: EAX = the
// 0x2a78-byte state object, ECX = stage-data file name, stack = priority base
// dword; `ret 4`). The base value doubles as the publication selector: zero
// publishes the state into the secondary slot (DAT_004776e8), any nonzero
// value into the primary slot (DAT_004776e4), and the three scheduler-record
// priorities are base+12 (calculation), base+7 (draw pass 0) and base+10
// (draw pass 1). Loads the stage background script through
// 0x00403850 (native EBX = state register ABI); any load failure appends the
// Shift-JIS "stage data is corrupt" diagnostic (0x0044b8e0, EDI = the
// 0x474f70 text context) and returns -1. Returns 0 on success.
i32 CreateTitleScreenStateEaxEcxStackAbi(void *state,
                                         const char *stage_data_name,
                                         u32 priority_base);

} // namespace th10
