#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0040c9a0. Native EAX = position record {x, y, z} (forwarded in
// ESI), EDI = the death-particle kind/count slot; plain ret. Spawns one
// explosion particle of the pending kind (when positive) at the position,
// runs the 11-row scatter (0x40c9d0), then clears the slot.
void FlushEnemyDeathDropEaxEdiAbi(const float position[3], u32 *kind_slot);

// TH10 0x0040c9d0. Native ESI = position record {x, y, z}, stack (ret 4) =
// scatter table base. Walks 11 count dwords at table+4..table+0x2c and
// spawns explosion particles (kind = row + 1) scattered around the
// position; clears the 12 dwords at table+4..table+0x30. Returns 0.
i32 SpawnEnemyDeathScatterEsiStackAbi(const float position[3],
                                      void *scatter_table);

} // namespace th10
