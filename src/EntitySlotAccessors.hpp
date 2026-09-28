#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00412a10. Native EAX = scheduler/VM context, ECX = slot index,
// result in EAX. Resolves the entity pointer registered in ECL slot
// `slot`: [eax+0x14d8] -> work record, [+4] -> slot table. Bit `slot` of
// the table's presence word ([+8]) must be set. Entries < 0 route through
// the fallback resolver object at table+0x1014 (thiscall vtable slot 2);
// otherwise the pointer is table+8 + entry + [table+0x100c].
void *ResolveEntitySlotPointerEaxEcxAbi(const void *context, i32 slot);

// TH10 0x00412d60 / 0x00412d70 / 0x00412d80 / 0x00412d90. EAX = source
// pair, ECX = destination record; copies the 2-dword pair to the slot at
// the given byte offset (0 / 8 / 0x10 / 0x18).
void CopyVec2PairToSlotBaseEaxEcxAbi(const void *src, void *dst);
void CopyVec2PairToSlot8EaxEcxAbi(const void *src, void *dst);
void CopyVec2PairToSlot10EaxEcxAbi(const void *src, void *dst);
void CopyVec2PairToSlot18EaxEcxAbi(const void *src, void *dst);

// TH10 0x00412db0. EAX = block. Re-arms the animation timer at +0x20
// (prev/cur/accum/rate/flags at +0x20..+0x30): lazily seeds the rate
// pointer to the frame-time scale global and re-runs the stopped reset,
// then stores cur = 0, prev = -1.
void RearmAnimTimerAt20EaxAbi(void *block);

// TH10 0x00412ed0. EAX = block, ECX = duration. Stores the duration at
// +0x44; when it changed, the +0x4c cache is invalidated to zero.
void SetAnimDurationInvalidateCacheEaxEcxAbi(void *block, i32 duration);

// TH10 0x00412f70. EAX = destination, two stack dwords (retn 8): plain
// 2-dword store.
void SetSlotPairFromStackEaxStackAbi(void *dst, u32 value_a, u32 value_b);

// TH10 0x00413120. EAX = source vec3, ECX = destination record; stores
// the 3 dwords at destination +0xc.
void CopyVec3ToSlotCEaxEcxAbi(const void *src, void *dst);

// TH10 0x00413170. ECX = destination, stack = angle (retn 8). Stores
// WrapAngleToPi(angle) into destination +0x1c.
void WrapAngleIntoSlot1CEcxStackAbi(void *dst, float angle);

// TH10 0x004131a0. EAX = record. Reads bit 3 of the flag dword at +0x60.
i32 ReadEntityFlagBit3EaxAbi(const void *record);

// TH10 0x00413220. Plain 3-dword copy from EAX to ECX.
void CopyVec3EaxEcxAbi(const void *src, void *dst);

// TH10 0x00413270. EAX = output pair, stack = {angle, sin_scale,
// cos_scale} (retn 0xc). fsincos based polar vector: out[0] =
// sin(angle) * sin_scale, out[1] = cos(angle) * cos_scale (note the
// swapped sin/cos roles relative to a unit polar vector).
void SetPolarVec2SinCosScaledEaxStackAbi(float *out, float angle,
                                         float sin_scale, float cos_scale);

} // namespace th10
