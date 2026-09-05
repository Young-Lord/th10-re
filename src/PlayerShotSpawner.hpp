#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00427e90. Native entry takes player, descriptor, and frame as
// three stack arguments and pops them (ret 0xc); it always returns zero.
// Its stack ABI remains a thunk boundary.
i32 SpawnPlayerShotStackAbi(void *player, const void *descriptor,
                            i32 frame);

// TH10 0x00427b50. Native EDX = player, ECX = ignored position, five stack
// args (pos[3], velX, velY, count, limit); ret 0x14. Returns the record
// pointer, or one-past-end when all 32 slots are busy.
void *SpawnPlayerSubEffectEcxDxStackAbi(void *position, void *player,
                                        float vel_x, float vel_y, i32 count,
                                        i32 limit);

} // namespace th10
