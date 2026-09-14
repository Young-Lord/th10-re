#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00428630. Native thiscall ECX = out dword (null at the only known
// caller, the ECL script setup damage pass), stack = {player state block
// (DAT_00477834), position pair, size pair}. Runs the enemy damage pass:
// 128 player-shot records plus 32 bomb/sub-effect damage boxes, returns the
// summed raw damage.
i32 ComputeEnemyDamageFromPlayerAttacksStackAbi(u32 *out_value, void *player,
                                                const float *position,
                                                const float *size);

// TH10 0x004059f0. Native thiscall ECX = game context (the DAT_004776ec
// object), EAX = enemy position pair. Returns the bomb/sub-effect damage
// contribution for one enemy (0 when no bomb is active or the enemy is out
// of range and no splash rule applies).
i32 ComputeBombAreaDamageThisAbi(const void *game_context,
                                 const float *position);

// TH10 0x00427c70. Native thiscall over the two-dword {prev, cur} counter:
// true when the counter advanced this frame and cur is a multiple of the
// interval.
bool IsTimerFrameMultiple(const u32 *timer_pair, i32 interval);

// TH10 0x0040c480. Native EAX = &entity-id slot (ECX ignored): resolve the
// entity over the DAT_00491c10 render-owner lists and set its +0x304 stop
// word to 2, propagating to the +0x14 child chain when the +0x18 count is 0.
void SetPlayerShotEntityHitState(u32 *id_slot);

// TH10 0x004243f0. Same shape as 0x0040c480 but against the DAT_00491c40
// owner and stop word 6.
void SetResultEntityStateSixByHandleSlot(u32 *id_slot);

} // namespace th10
