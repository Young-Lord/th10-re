#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Scene trigger objects (the records driven by 0x406240 / 0x4067D0): every
// object carries an 18-entry instruction queue (stride 0x18, opcode at
// +0x10, "has-args" dword at +0x14) that is consumed at most 18 opcodes per
// call, plus a screen-space anchor (+0x3c0), extents (+0x41c/+0x420) and a
// state word (+0x458).

// TH10 0x00426660. Native EAX = position pair, ECX = target block whose
// +0x3c0/+0x3c4 floats are the target screen position; returns the angle
// from the position to the target. When both deltas are exactly 30.0 the
// native answers 1.75 radians instead of atan2.
float AngleToScreenTargetEaxEcxAbi(const float position[2], void *target);

// TH10 0x00408750. Native ECX (this) = float2 output, stack (angle, speed);
// writes {cos(angle) * speed, sin(angle) * speed}.
void SetPolarVelocityThisAbi(float out_xy[2], float angle, float speed);

// TH10 0x00406d90. Native ECX = trigger object; interprets up to the 18
// queued instructions (see evidence doc for the opcode table). Returns with
// the object's instruction index advanced past the consumed opcodes.
void RunSceneTriggerInstructionQueueEcxAbi(void *object);

// TH10 0x00426cf0. Native stdcall, one stack argument (ret 4): fires the
// object's expire effect — pool VM with script 0x162 plus 32 debris VMs
// with script 0x163 at the object's screen anchor, arms the expire timers,
// reserves sound channel 4, detaches the object's ANM VM, and clears the
// stage life/anchor flags once the stage timer passed 60.
void FireSceneTriggerExpireEffect(void *object);

// TH10 0x004267f0. Native EAX = position pair, ECX = trigger object, stack
// (rotation, scale, limit): rotated-region check against the object's
// screen anchor and extents. Returns 0 = no interaction, 1 = expire effect
// fired, 2 = deep-region overlap.
i32 CheckSceneTriggerRotatedRegionEaxEcxStackAbi(const float *position,
                                                 void *object, float rotation,
                                                 float scale, float limit);

} // namespace th10
