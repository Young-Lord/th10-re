#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x4172e0. Native EAX = result-screen mode (0..6 jump table, larger
// modes fall through to the epilogue), stack = {state owner, bonus}; ret 8.
// The state owner is the ASCII HUD record (DAT_0047770c) whose +0x9df4
// score-digit slots, +0x9e14 and +0x9e18 banner handles are rebuilt here.
void ApplyResultScreenStateEaxStackAbi(i32 mode, void *state_owner,
                                      i32 bonus);

} // namespace th10
