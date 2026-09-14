#pragma once

#include "Th10Types.hpp"

namespace th10 {

// The 0x491c28 game-manager slot owns three background effect VMs whose
// entity ids are published to DAT_00477824/28/2c, with the one-time spawn
// latch at slot+0x6fc (DAT_00492324) and the ANM manager-work at slot+0x3c8.

// TH10 0x00421180 (native stdcall `ret 8`: stack0 = slot, stack1 = float3
// position source). One-time spawn: while the +0x6fc latch is zero, allocates
// three 0x3ac pool VM records bound to scripts 0/1/2 of the slot's +0x3c8
// manager-work (kind 0xf, flag 0x40000000), links each into the render-owner
// entity list (0x004489d0) with the ids published to DAT_00477824/28/2c,
// latches +0x6fc = 1, then publishes the caller's float3 position into all
// three through 0x004492f0. Tail (both paths): while the render owner's first
// signed dword is negative (idle), stores 8 and the two 640x480 rects
// {0,0,640,480} at owner+0x2c..+0x48.
void SpawnGameManagerBackgroundVmsStackAbi(void *slot,
                                           const float position[3]);

// TH10 0x00421070 (native stdcall `ret 4`). Stop variant with state word 1:
// while the +0x6fc latch is 1, resolves each of the three published ids and
// writes the stop word at entity+0x304 (propagating over the +0x14 child
// chain when the +0x18 count is zero), zeroes the three id slots, latches
// +0x6fc = 0, and clears DAT_00491bec when set.
void LeaveGameManagerGateStackAbi(void *slot);

// TH10 0x00421300 (native stdcall `ret 4`). Stop variant with state word 2:
// identical body, latch written as 2.
void EnterGameManagerGateStackAbi(void *slot);

} // namespace th10
