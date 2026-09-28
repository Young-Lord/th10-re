#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004415c0. Native EAX = row count, EDX = row base (stride 0x3ac),
// ECX = running VM-work cursor, stack = argument forwarded to
// InitializePlayerMainVmEsiStackAbi (0x00404f30). Per row: initialize the
// VM with the (byte-incremented) cursor, copy word +0x384 to +0x388.
u16 InitializePlayerVmRowBatchEaxEdxCcxAbi(u32 count, u8 *rows,
                                           u32 vm_cursor, i32 anm_argument);

// TH10 0x00441e40. Native EAX = target triple, EDX/ECX = sources,
// stack = third dword. Stores (ECX, EDX, stack) into the target.
void StoreInterpolationTripleEaxEdxCcxAbi(u32 *target, u32 value_edx,
                                          u32 value_ecx, u32 value_stack);

// TH10 0x00441e50. Native EDI = source triple, ESI = target, stack =
// scale float. Truncating float multiply per dword (x87 fistp semantics).
void ScaleInterpolationTripleEdiEsiAbi(const u32 *source, u32 *target,
                                       float scale);

// TH10 0x00441ef0. Native ECX = float[2] target, stack = angle, radius.
void PolarToCartesianEcxAbi(float *target, float angle, float radius);

// TH10 0x00441f50. Native EAX = camera-work record, DL = easing kind byte,
// ECX = duration, stack (ret 8) = two bytes stored at +0x208/+0x20c. Arms
// the tween sub-record at +0x218..0x228.
void BeginCameraTweenEaxStackAbi(void *record, u8 easing_kind, u32 duration,
                                 u8 arg_208, u8 arg_20c);

// TH10 0x00442050. Native EAX = record, EDX = from-RGB triple, ECX =
// to-RGB triple, stack (ret 8) = duration word + flag byte.
void BeginCameraColorTweenEaxAbi(void *record, const u8 *from_rgb,
                                 const u8 *to_rgb, u32 duration, u8 flag);

// TH10 0x00442130/0x00442150/0x004421a0/0x004421c0. Copy a three-dword
// vector from EAX to the ECX record at the given offsets.
void StoreOwnerVectorAt24EaxCcxAbi(const u32 *source, void *record);
void StoreOwnerVectorAt36EaxCcxAbi(const u32 *source, void *record);
void CopyVector3EaxCcxAbi(const u32 *source, u32 *target);
void StoreOwnerVectorAt12EaxCcxAbi(const u32 *source, void *record);

// TH10 0x004422f0. Native EAX = target, DL/CL = two bytes, stack = third.
void StoreRgbTripleEaxAbi(u8 *target, u8 second, u8 first, u8 third);

// TH10 0x0043c0-ish brightness helper (0x004423c0). (a2 * a1) >> 7 with a
// 255 clamp.
u32 ScaleBrightnessClamp255Abi(u8 multiplier, u8 value);

// TH10 0x004425a0. Native EAX = owner (DAT_00491c10 object), EDI = entity.
// Flushes and applies blend/z states from entity+0x35c when changed and
// increments owner+0x54.
u32 ApplyOwnerRenderModesEaxEdiAbi(void *owner, void *entity);

// TH10 0x00442f30. Native EAX = owner. Resets the vertex free-list head at
// owner+0x3ad858 and links the two sentinel nodes to each other.
void InitializeOwnerVertexFreeListEaxAbi(void *owner);

// TH10 0x00444b10 / 0x00444be0. Ribbon quad vertex builders (EAX = vertex
// count, ECX = entity, EBX = vertex buffer). Return -1 below 3 vertices.
i32 BuildHorizontalRibbonVerticesEaxCcxAbi(i32 count, void *entity,
                                           u8 *vertex_buffer);
i32 BuildVerticalRibbonVerticesEaxCcxAbi(i32 count, void *entity,
                                         u8 *vertex_buffer);

// TH10 0x004450e0. Native EAX = render object, ECX = owner, stack =
// two words. State 3 indexed strip draw through the raw device vtable.
i32 DrawRibbonStripIndexedEcxAbi(void *render_object, void *owner,
                                 u32 vertex_word_a, u32 vertex_word_b);

// TH10 0x00445880. Native thiscall (record): dispatches the ribbon strip
// draw with the fixed 33-word vertex budget.
i32 DrawRibbonStripFromWorkEcxAbi(void *record);

// TH10 0x004458b0. Native ECX = float[2] target, stack = angle, radius.
void PolarToCartesianStripEcxAbi(float *target, float angle, float radius);

} // namespace th10
