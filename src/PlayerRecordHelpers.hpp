#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00424620. EAX = value, ECX = player. Writes the value into
// the four item-value slots at player+0x334c / +0x33d4 / +0x345c /
// +0x34e4 (0x84 stride) and returns the value.
i32 WritePlayerQuadSlotValueEaxEcxAbi(i32 value, void *player);

// TH10 0x004246c0. ESI = 0x4478 player record. Clears flag bit 0 at
// the nine offsets +0x80/+0xc4/+0x110/+0x13c/+0x188/+0x1c4/+0x210/
// +0x23c/+0x38c, zeroes the 0x3ac script region at +0x14, writes
// 0xffff into the +0x398 word, clears bit 0 at +0x470/+0x484/+0x498,
// walks the 128 entry slots at +0x4ac (0x5c stride) clearing bit 0,
// clears bit 0 at +0x3320/+0x33b8/+0x3450/+0x34e8, walks the 33 shot
// slots at +0x3560 (0x6c stride), clears bit 0 at +0x431c, then
// zeroes the whole 0x4478 record and publishes it at DAT_00477834.
// Returns the record.
void *ResetPlayerRecordEsiAbi(void *player);

// TH10 0x00427ae0. EDI = player entity. Focus (slow) motion update:
// reads the focus flag dword at DAT_00477834+0x4474. Engaged: on a
// rising edge (previous focus at entity+0x84 clear) the state word
// setter 0x00449470 is called with mode 6 on the entity+0x68 slot and
// the unfocused position at +0x3c/+0x40 is parked at +0x4c/+0x50.
// Released: on a falling edge the setter runs with mode 3 and the
// parked position is restored into +0x34/+0x38. The +0x84 latch is
// then written with the current focus flag in both branches.
void UpdatePlayerFocusMotionEdiAbi(void *entity);

// TH10 0x00427c50. Stack = float (retn 4). floor of the float
// argument widened to double (native fild/fstp qword round trip into
// the CRT floor).
double FloorfToDoubleStackAbi(float value);

// TH10 0x00427d50. EAX = output, ECX/EDX = operands. out = a - b.
void SubtractVec2EaxEcxDxAbi(float out[2], const float a[2],
                             const float b[2]);

// TH10 0x00427dc0. EAX = output, EDX/ESI = operands. out = a + b.
void AddVec2EaxDxEsiAbi(float out[2], const float a[2],
                        const float b[2]);

// TH10 0x00427e20. EAX/ECX = outputs, EDX = source. Both outputs
// receive the source pair (out1 +4 is copied from out2 +4 after the
// out2 store, same value).
void CopyVec2ToDualOutputsEaxEcxDxAbi(float out1[2], float out2[2],
                                      const float src[2]);

// TH10 0x00428c20. EAX = ignored, ECX = homing table, EDX = homing
// context, stack argument dead (the native never reads it; retn 4).
// ESI = the homing target entity at context+0x58; slot =
// (signed char) entity+0x1c * 0x98 selects the 0x98-byte anchor
// record at table+0x3244. Publishes the context anchor
// {entry0 * 0.01, entry4 * 0.01, 0} at context+0x14..+0x1c, then
// entry0 = entity.x - context+0x20 + (float)entry0 (the raw int is
// re-read as a float after the store) and finally
// context+0x18 = entity.z - context+0x24 + context+0x18 (this last
// write clobbers the anchor y just published). Returns 0.
i32 SetHomingSlotAnchorEaxEcxDxStackAbi(i32 unused, void *table,
                                        void *context, i32 dead_argument);

} // namespace th10
