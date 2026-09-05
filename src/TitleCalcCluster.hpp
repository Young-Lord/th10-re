#pragma once

#include "Th10Platform.hpp"
#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00424d90. Native ESI = the option-position manager (DAT_00477834);
// plain ret. Arms the three option-position option sub-records at +0x458/
// +0x460..+0x470, +0x474..+0x484, +0x488..+0x498 (first-use flag is the
// fifth dword of each block), releases the tracked entity at +0x329c and
// refreshes the life icons from DAT_00474c70.
void ResetOptionPositionRecordsEsiAbi(void *manager);

// TH10 0x0042a450. Native EBX = the game-mode object (DAT_00477838); plain
// ret. Mode 0 builds the selected run record (record + 0x1c + 4*slot) from
// the run globals and the 0x477834 manager; mode 1 commits it back (reverse
// copy, option rebuild, manager trail clears, 0x424d90 tail).
void ApplyOptionPositionStateEbxAbi(void *record);

// TH10 0x00417040. Native ESI = the ASCII HUD owner (DAT_0047770c); plain
// ret. Scores/score-display updater: advances the displayed score toward
// DAT_00474C44, publishes the best score, redraws the 9+9 score digit VMs
// and the life/power-of-life digit VMs, then checks the score-rank table.
void UpdateInGameScoreDisplayEsiAbi(void *hud_owner);

// TH10 0x00404450. Native one stack argument (the 0x2a78-byte secondary
// title-screen state), __stdcall ret 4. Re-enables its three scheduler
// records and initializes every per-stage player VM from the stage table.
void InitializeTitleSecondaryStateStackAbi(void *state);

// TH10 0x00417770. Native EAX = owner; plain ret. Walks the doubly linked
// record chain at owner+0x18, runs each record's vtable+0x10 method, unlinks
// it and frees the node; the tail node (next == 0) is processed too.
void ReleaseOwnerRecordChainEaxAbi(void *owner);

// TH10 0x00418a00. Native EAX = the shared frame-state block at DAT_00474c40;
// plain ret. While DAT_00474c58 > 0 shifts the +0x14 timer by -1.0 frames
// (0x44bf40); otherwise drains the power pool dword DAT_00474c4c toward the
// 5000 floor using the decrement in DAT_00474c50 (clamped to >= 18).
void TickTitleFrameStateEaxAbi(void *frame_state);

// TH10 0x00409f90. Native one stack argument (the DAT_00477704 HUD
// conditional state), __stdcall ret 4; the object stays allocated. Releases
// the four entity resource pools at 0x491c10+0x3accb0, runs vtable+0x14 on
// every record in the state+0x58 chain with argument 1, clears +0x64/+0x10
// and clears the scheduler-disabled bit on the two records at +8/+0xc.
void ReleaseAsciiHudConditionalState(void *state);

// TH10 0x00428e10. Native usercall: EAX = game-mode state, ECX = two-dword
// source. Copies the two dwords to +0x3cc/+0x3d0, publishes their
// signed*0.01f floats at +0x3c0/+0x3c4, and sets the four per-option
// flags at +0x332c/+0x33c4/+0x345c/+0x34f4 to 1. Returns the state.
void *PublishSelectedRunStatsEaxEcxAbi(void *state, const u32 *values);

// TH10 0x0042ab20. Native usercall: EAX = bucket index, ECX = owner.
// Walks the doubly linked record chain at owner+64+12*index (nodes
// {record, next}), unlinks each record (prev at record+0x6280, next at
// record+0x627c) and frees it with the shared delete. Returns the last
// next pointer.
void *FreeGameModeChainEntriesEaxEcxAbi(i32 index, void *owner);

// TH10 0x0042aa50. Native userpurge: ESI = bucket index, stack = owner.
// Allocates and zeroes the 0x6284-byte record (self pointers at +0x5470/
// +0x6278, list node at +0x6278 with next +0x627c / prev +0x6280) and
// links it at the tail of the owner's bucket chain
// (owner + 4*(3*index+15), first pointer at +4). Returns the node base.
void *AllocateGameModeChainEntryEsiStackAbi(i32 index, void *owner);

} // namespace th10
