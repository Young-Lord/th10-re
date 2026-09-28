#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00405860. Native stack arg = node (DAT_004776ec), ret 4; returns
// -1 when the one-shot latch was already set. The register ABI remains a
// thunk boundary.
i32 TickRespawnDeathEffectStackAbi(void *node);

// TH10 0x004054b0. Native EDX = integer lives, stack = percent; ret 4.
void RefreshHudLivesDisplayEdxStackAbi(i32 lives, i32 percent);

// TH10 0x00408030. Native ESI = enemy slot; returns 1 when activated.
i32 ActivateStageEnemyEsiAbi(void *enemy_slot);

// TH10 0x004086b0. Native ECX = the float2 position, stack = (margin_x,
// margin_y; ret 8); returns 1 when outside the playfield rect
// x = (-192, 192), y = (0, 448) and 0 when inside.
i32 CheckStageEffectPositionInFieldEcxEcxStackAbi(const float *position,
                                                  float margin_x,
                                                  float margin_y);

// TH10 0x00408100. Native EAX = enemy manager, EBX = player position,
// stack = (radius, spawn-fx flag, unused-only flag); ret 0xc; returns 0.
void ScanIntroActivations(void *enemy_manager, const float player_position[3],
                          float radius, i32 spawn_fx, i32 require_unused);

// TH10 0x0041c800. Native EAX = manager, EBX = target vec3, stack = (float
// radius, flag); ret 8; returns the sum of the virtual results.
i32 BroadcastEntranceTweenEaxEbxStackAbi(void *manager,
                                         const float target[3], float radius,
                                         i32 flag);

// TH10 0x0041c850. Native EAX = manager; returns 1.
i32 BroadcastBulletClearEaxAbi(void *manager);

} // namespace th10
