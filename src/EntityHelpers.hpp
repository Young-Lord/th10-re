#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Entity list walk and state-word helpers over the two manager lists at
// manager+0x72dad4 / +0x72dadc (nodes {entity, next}; entity id at +0).
// Child propagation runs while entity+0x18 == 0 over the list at +0x14.

// TH10 0x004491c0. Native inputs EDX = manager, ECX ignored.
u8 *FindEntityEdxStackAbi(void *manager, u32 id);

// TH10 0x004492a0. Soft release: entity+0x35c |= 0x4000000.
void ReleaseEntityById(void *manager, u32 id);

// TH10 0x004492f0. Native ESI = float3; position published verbatim.
void SetEntityPositionDirectEsiAbi(void *manager, u32 id,
                                   const float position[3]);

// TH10 0x00449350. Native ESI = float3; position published with the
// +224/+16 game-area offset.
void SetEntityPositionOffsetEsiAbi(void *manager, u32 id,
                                   const float position[3]);

// TH10 0x00449470. Native EAX = &idSlot, ESI = value: resolve the entity
// and set its u16 state word at +0x304 (used both as a kill code and as
// the option sprite kind selector).
void SetEntityStateWordEaxEsiAbi(u32 *id_slot, i32 value);

// TH10 0x00449950. Native ESI = manager: allocate a 0x3ac-byte VM record
// from the 4096-slot pool, falling back to the heap.
void *AllocatePoolVmEsiAbi(void *manager);

// TH10 0x00449870. Native stack args (vm, script id): reset the VM scratch
// region, set the flag and animation fields, and bind the effect script
// (0x43e7e0 remains a boundary; it wipes the record when the script is
// missing).
void AssignPoolVmScriptEcxEaxAbi(void *vm, i32 script_id);

// TH10 0x004489d0. Native EBX = entity, EDX = manager, EAX = &outId: link
// the entity into the active list and assign the next id (wrapping to 1).
void LinkEntityAndAssignIdEaxEsiAbi(u32 *out_id, void *entity);

// TH10 0x00448a50. List-A front-insertion twin of 0x4489d0 (prepend at
// +0x72dad4; the tail is touched only when the list is empty).
void LinkEntityFrontAndAssignIdEaxEsiAbi(u32 *out_id, void *entity);

// TH10 0x00449210. Hard stop: state word 1 by id.
void StopEntityById(void *manager, u32 id);

// TH10 0x00409e50. Native EAX = &handle; state word 1 (expire).
void ExpireEntityHandleEaxAbi(u32 *handle);

// TH10 0x0040c4d0. Native EAX = &handle; state word 3 (fire/consume).
void FireEntityHandleEaxAbi(u32 *handle);

// TH10 0x00428d70. Native ECX = {x, y}, stack = half extents; returns 1
// when the box is fully outside the fixed playfield rect.
i32 IsOutsidePlayfieldBox(const float position[2], float half_x,
                          float half_y);

} // namespace th10
