#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0040a450. Native stdcall, one stack argument (ret 4): the ECL
// select / spell-practice menu state object; returns 1 in EAX. Sub-state
// at object+0x30: 0 = enumerate ../../data/*.ecl and seed cursors,
// 1 = menu cursor handling (first cursor exit -> sub-state 2, spell
// select -> sub-state 4), 2 = pending transition 3 request, 4 = input
// snapshot + manager disable -> sub-state 1.
i32 UpdateEclSelectMenuStackAbi(void *menu);

} // namespace th10
