#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004281d0. Native entry receives the player in EAX and returns
// zero; its register ABI remains a thunk boundary.
i32 TickItemMagnetEaxAbi(void *player);

// TH10 0x00428160. Native inputs EDI = player, EBX = autocollect frame.
void FireScheduledShotsEdiBbxAbi(void *player, i32 frame);

} // namespace th10
