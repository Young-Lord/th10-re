#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0041c0b0. EAX = stage object manager, ECX = node. Push-front
// into the doubly linked node list: head at manager+0x434, count at
// manager+0x438, node next at +4 / prev at +8.
void LinkStageObjectNodeEaxEcxAbi(void *manager, void *node);

// TH10 0x0041c120. EBX = manager. Requests manager work slot 7 for
// bullet.anm, installs the manager calc callback 0x0041c480 (slot
// 0x13) and draw callback 0x0041c4e0 (slot 0x1b) at manager+8/+0xc,
// and seeds the node list head at manager+0x434 with manager+0x10.
// Returns -1 when the work slot is unavailable, else 0.
i32 InitializeStageObjectManagerEbxAbi(void *manager);

// TH10 0x0041c1c0. Stack = manager (retn 4). Releases both scheduler
// callbacks, then walks the node chain at manager+0x18: each node is
// released through its vtable slot 4 (+0x10), unlinked, and freed.
// Clears the manager global DAT_0047781c.
void DestroyStageObjectManagerStackAbi(void *manager);

// TH10 0x0041c290. EBX = manager. Allocates the 0x45c manager, runs
// the header default initializer at manager+0x10, then zeroes the
// whole 0x45c block (the native wipes the header defaults it just
// wrote), publishes it at DAT_0047781c and initializes the callbacks.
// On failure the manager is destroyed and freed; returns it or 0.
void *CreateStageObjectManagerEbxAbi(void *manager);

// TH10 0x0041c480. ECX = manager (calc callback). Gates on the main
// chain state word at DAT_00477810+0x58: bits 0 or 2 (or bit 10) short
// circuit to 1. Bit 1 saves the frame-time scale dword, zeroes it,
// ticks the manager slots (0x0041c330) and restores the scale.
// Returns the tick result (or 1 on the gate paths).
i32 StageObjectManagerCalcCallbackEcxAbi(void *manager);

// TH10 0x0041c4e0. ECX = manager (draw callback). Skipped while the
// main chain has flag bit 2 of +0x58; otherwise walks the node chain
// at manager+0x18 and calls vtable slot 3 (+0xc) on every node whose
// +0xc kind is not 1. Returns 1.
i32 StageObjectManagerDrawCallbackEcxAbi(void *manager);

// TH10 0x0041c760. EAX = manager, EBX = velocity vec3, stack =
// {position vec3, argument} (retn 8). Stores the position at
// manager+0x440 and the velocity at manager+0x44c, then walks the node
// chain at manager+0x18 and sums vtable slot 6 (+0x18) over every node
// whose +0xc kind is not 1. Returns the sum.
i32 BroadcastStageObjectSpawnEaxEbxStackAbi(void *manager,
                                            const u32 velocity[3],
                                            const u32 position[3],
                                            i32 argument);

// TH10 0x0041c7d0. EAX = manager. Walks the node chain at +0x18 and
// sets the pending byte at node+0x50 for every node whose +0xc kind is
// not 1. Returns 0.
i32 MarkStageObjectsPendingEaxAbi(void *manager);

// TH10 0x0041c880. EAX = manager, stack = {value, argument} (retn 8).
// Walks the node chain at +0x18 and sums vtable slot 8 (+0x20) over
// every node whose +0xc kind is not 1. Returns the sum.
i32 SumStageObjectCounterVirtualEaxStackAbi(void *manager, u32 value,
                                            u32 argument);

// TH10 0x0041d7c0. ECX = boss record. Seeds the two item-drop VM
// position records (+0x934 / +0xce0) from the boss position offset by
// (+224, +16), stores the +0x2c depth, wraps the +0x3c angle by pi/2
// into the +0x600 VM's +0x2c, raises VM flag bit 2 (+0x35c) and
// dispatches the render mode; the second VM at +0x9ac is only spawned
// while the +0x4c selector is zero. Returns 0.
i32 SpawnBossDropItemVmsEcxAbi(void *boss);

// TH10 0x0041e4d0 / 0x0041f670 (identical twins). ECX = stage object
// (position +0x24/+0x28, angle +0x3c, half extents +0x40/+0x44),
// stack = {point vec2, radius} (retn 8). Rotated-box hit test of the
// point expanded by the radius against the object's oriented box:
// returns 2 on hit, 0 on miss.
i32 StageObjectRotatedBoxHitA_Thiscall(void *object, const float point[2],
                                       float radius);
i32 StageObjectRotatedBoxHitB_Thiscall(void *object, const float point[2],
                                       float radius);

// TH10 0x0041f820. EAX = stage entity. Sets flag bit 4 (+0x10) and
// clears flag bit 5 (+0x20) of the +0x35c state word (the focus /
// unfocus transition pair).
void SetStageEntityFocusFlagEaxAbi(void *entity);

// TH10 0x0041f850. ESI = 0x3f0 stage entity record. Installs the
// stage-entity vtable (0x004703e4), clears the kind flags, zeroes the
// 0x3ac script region, writes 0xffff into the +0x378 word, then zeroes
// the whole 0x3f0 record again, sets flag bit 1 and publishes it at
// DAT_00477820. Returns the record.
void *InitializeStageEntityRecordEsiAbi(void *record);

} // namespace th10
