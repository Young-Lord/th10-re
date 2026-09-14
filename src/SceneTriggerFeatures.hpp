#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Flag-dispatch feature helpers of the 0x7f0-stride stage trigger object.
// All ten run with the object in a register at the native boundary (ESI for
// the 0x4074b0..0x407ef0 block, EBX for the two region-wrap twins) and read
// the record fields described in docs/evidence/scene-trigger-features.md.

// TH10 0x4074b0 (flag bit 1). Launch slowdown: while the +0x618 count is
// <= 0x10 the polar speed becomes 5.0 - acc*0.3125 + y-speed; past that the
// feature bit is toggled off (native XOR).
void UpdateSceneTriggerLaunchSlowdownEsiAbi(void *object);

// TH10 0x407560 (flag bit 0x10). Per-axis acceleration of the screen
// velocity with the y-speed rate, angle recompute above the 1e-4 threshold,
// count-limit self-disable at +0x670.
void UpdateSceneTriggerAccelerationEsiAbi(void *object);

// TH10 0x4076a0 (flag bit 0x20). Angle turn (+0x694 per frame) plus the
// +0x690 speed rate; count-limit self-disable at +0x6a4.
void UpdateSceneTriggerAngleTurnEsiAbi(void *object);

// TH10 0x407780 (flag bit 0x40) / 0x4078e0 (0x100) / 0x407a30 (0x80).
// Slow-to-stop refire family: the polar speed decays linearly to zero over
// the +0x6d8 frame budget, then the object fires (sound + angle update +
// speed reset to +0x6c4 + timer re-arm) and repeats; after +0x6dc fires the
// feature bit clears.
void UpdateSceneTriggerSlowStopRelaunchEsiAbi(void *object);
void UpdateSceneTriggerSlowStopFixedAngleEsiAbi(void *object);
void UpdateSceneTriggerSlowStopAimPlayerEsiAbi(void *object);

// TH10 0x407be0 (flag bits 0x400 | 0x800 | 0x8000000). Screen-edge bounce
// with angle mirrors, the +0x6f8 speed override and the +0x70c/+0x710
// bounce budget.
void UpdateSceneTriggerWallBounceEsiAbi(void *object);

// TH10 0x407ef0 (flag bit 0x4000000). Homing turn toward the screen target
// block position (+0x3c0/+0x3c4) with the +0x7c8 turn factor, +0x7cc angle
// bias and the +0x7dc frame budget.
void UpdateSceneTriggerHomingTurnEsiAbi(void *object);

// TH10 0x407da0 (flag bit 0x100000) / 0x407e40 (0x200000). Region-wrap
// teleports over the linked region block half extents (+0x34 x, +0x30 y)
// once the position pair leaves the play field.
void WrapSceneTriggerRegionXEbxAbi(void *object);
void WrapSceneTriggerRegionYEbxAbi(void *object);

// TH10 0x44bc10 (ret 8). Wraps a + b into [-pi, pi] with 2*pi steps and a
// 33-iteration budget per direction; NaN inputs fall through both loops.
float WrapAngleSumStackAbi(float a, float b);

// TH10 0x408660 (ret 8). Shortest signed step a - b wrapped into (-pi, pi]
// with a single 2*pi correction per direction.
float AngleDifferenceWrappedStackAbi(float a, float b);

// TH10 0x406160 / 0x4061d0 (ret 8, ECX = position pair). Return 1 when the
// half-extent box around the position lies fully outside the play field
// x=(-192, 192), y=(y_min, 448); the half extents are pre-halved by the
// native 0.5 factors. NaN coordinates continue every check and answer 0.
// The 0x406160 twin uses y_min = -64.0, the 0x4061d0 twin 0.0.
i32 CheckRegionExitedPlayfield64EcxStackAbi(const float *position,
                                            float half_x, float half_y);
i32 CheckRegionExitedPlayfield0EcxStackAbi(const float *position,
                                           float half_x, float half_y);

} // namespace th10
