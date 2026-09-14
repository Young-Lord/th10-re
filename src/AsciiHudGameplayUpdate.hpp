#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x414900. Native stdcall ret 4 (the 0x9ed0-byte ASCII HUD owner,
// DAT_0047770c) — the gameplay-side HUD update. Runs the +0x4980/+0x6a8c/
// +0x793c VM pools, the 7-record region state words at +0x8398, the score
// digit VMs, the boss HP/timer/spell-state blocks (+0x9e24..+0x9ecc), the
// +0x9eb8 result-screen script teardown, and the frame timer at +0x9e64.
// Always returns 1.
i32 UpdateAsciiHudGameplayStackAbi(void *owner);

} // namespace th10
