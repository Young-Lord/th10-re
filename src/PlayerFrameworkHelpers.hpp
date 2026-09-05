#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0040ac90. Native EAX = controller, stack = requested state; ret 4.
void RequestGameStateTransitionEaxStackAbi(void *controller, i32 state);

// TH10 0x00412e70. Native thiscall ECX = score block 0x474c40, stack =
// amount; drains amount/10 from the maximum-score dword with a 5000 floor.
void AddMaximumScorePenalty(i32 amount);

// TH10 0x00413790. Native EAX = HUD owner, stack = count; ret 4.
void RefreshLifeIconsEaxStackAbi(i32 count);

// TH10 0x00424650. Native ESI = text manager, stack = text ptr + position
// ptr (forwarded to the text-object creation as the source vec3); ret 8.
void ShowCautionText(const float position[3]);

// TH10 0x00426610. Native ECX = player, EAX = target {x, y}; returns the
// burst angle in ST0.
float ComputeDeathBurstAngleEcxEaxAbi(void *player, const float target[2]);

// TH10 0x0041bb00. Native EAX = manager [0x477818], ECX = source {x,y,z},
// stack (kind, color, angle, speed); ret 0x10. Always returns zero.
void SpawnExplosionParticleEaxEcxEfxAbi(void *manager, void *position,
                                        i32 kind, u32 color, float angle,
                                        float speed);

// TH10 0x004231d0. Native EDI = game state manager, stack = param.
void RunGameOverPathBStackAbi(void *game_state_manager, i32 param);

// TH10 0x0043e7e0. Native EAX = effect run context, ECX = script index,
// EDX = vm record; wipes the record when the script is missing.
void BindEffectScriptContextEaxEcxDxAbi(void *context, i32 script_index,
                                        void *vm);

} // namespace th10
