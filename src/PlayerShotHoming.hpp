#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00428ad0. Native ECX = player, EDX = shot record, stack = frame
// (read by nobody, `ret 4`). Copies the homing target from player+0x3504
// into record+0x4c and drops it again when the target's x at +0x1068 has
// moved past 224.0 from the playfield centre.
void AcquireHomingTargetEcxEdxStackAbi(void *player, void *shot_record);

// TH10 0x00428b10. Native ECX = player (read by nobody), EDX = shot
// record, plain ret, EAX = 0 always. Per-frame homing movement bound
// through the shot descriptor's +0x28 callback (table 0x474778 entry 1):
// drops dead/invalid targets, steers the record's +0x30 angle 20% of the
// wrapped direction delta toward the target, and modulates the +0x2c
// speed (ramp +0.1 capped 16.0, hard 4.0 slow-down when the target is
// behind, +0.2 per frame once the +0x4 age passes 120).
i32 TickHomingShotMovementEdxAbi(void *shot_record);

// TH10 0x00428ce0. Native stack (a, b), `ret 8`, result in ST0. Wraps the
// a-b delta into [-pi, pi]; a NaN input propagates.
float WrapAngleDeltaStackAbi(float a, float b);

} // namespace th10
