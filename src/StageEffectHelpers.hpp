#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00448db0. Native ECX = context forwarded to the pool allocator,
// EDI = source vec3, stack (unused, effect context, script index);
// ret 0xc. The assigned id lands in *(effect context).
void *SpawnStageEffectEdxEbxAbi(void *effect_context,
                                const float position[3], i32 script_index);

// TH10 0x405500. Native EAX = stage state; no return value.
void CleanupStageLifeFlags(void *stage);

// TH10 0x420a90. Native stack (source path, param); ret 8; returns 1.
void StartBgmTrack(const char *path, i32 param);

// TH10 0x00448ac0. Native EBX = vm, EDX = manager, EAX = &outId; links
// into manager list B (0x72dadc/0x72dae0).
void AttachEffectVmToListB(u32 *out_id, void *vm, void *manager);

// TH10 0x00448b40. List-B front-insertion twin of 0x448ac0 (prepend at
// +0x72dadc; the tail is touched only when the list is empty).
void AttachEffectVmToListBFront(u32 *out_id, void *vm, void *manager);

// TH10 0x424480. Native ESI = overlay target, stack (id, p2, p3, p4, p5);
// ret 0x14; returns 0 on success, -1 when the target slot is busy.
i32 CreateGameOverOverlay(void *target, i32 id, i32 p2, i32 p3, i32 p4,
                          i32 p5);

// TH10 0x41a120. Native EBX = source vec3, stack (list base, text);
// ret 8; returns the new object or null.
void *CreateTextEffect(void *list_base, const float position[3],
                       const char *text);

// TH10 0x41beb0. Native ECX = 8-byte block, stack (angle, speed); ret 8.
void InitMovementBlock(void *block, float angle, float speed);

} // namespace th10
