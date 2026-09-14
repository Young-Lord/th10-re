#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Spell/bullet base vtable neighborhood (DAT_004776f4 object family). The
// in-place destructor 0x00408af0 lives in GameModeTeardown.cpp; the sibling
// draw entries 0x004091c0 (plain) / 0x00409270 (push ebx; mov ebx,ecx; call
// 0x4091c0) and the update thunk 0x00409220 (push edi; mov edi,ecx; call
// 0x00408d60) remain thunks.

// TH10 0x00408210. Native EBX = bullet manager (the DAT_004776f0 effect
// manager root), one stack argument (ret 4). Deletes every active bullet
// record whose AABB overlaps the manager's delete region and optionally
// explodes it. Returns 0.
i32 ClearStageRegionBulletsEbxStackAbi(void *manager, i32 explosion_flag);

// TH10 0x004084a0. Native ECX = bullet manager, EDI = {x, y} point, one
// stack argument (ret 4) = radius (squared inside). No direct callers remain
// in the binary (the coverage gap lists it as data-referenced only); the
// body sums the item score value of every bullet whose corner circles
// (radius = argument) contain the point.
i32 SumBulletCornerItemValueEcxEdiStackAbi(void *manager,
                                           const float point[2],
                                           float radius);

// TH10 0x00408d60 (via thunk 0x00409220, which moves its ECX into EDI).
// Per-frame update of the spell/bullet base while a spell card capture is
// running (DAT_004776f4+0x378c bit 0). Always returns 1.
i32 UpdateSpellCardStoryStateEcxAbi(void *base);

// TH10 0x00409280. Stdcall ret 0x10; native EAX = the spell/bullet base.
// Arms the spell-capture state: stores the spell name, registers it in the
// score-save record, rebinds the 13 ASCII VMs, spawns the five framework
// entities, and initializes the rank-specific VM/script pair. The native
// return value in EAX is dead (the caller clears it immediately).
void StartSpellCardPracticeEaxStackAbi(void *base, i32 spell_card_index,
                                       const char *spell_name,
                                       i32 payload_id);

// TH10 0x00409c00. Native EAX = the spell/bullet base. Tears the capture
// down: expires the three framework handles, releases the rank entity, and
// either tallies the capture bonus or resets the HUD conditional slot.
void FinishSpellCardPracticeEaxAbi(void *base);

} // namespace th10
