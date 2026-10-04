#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0041c0b0. EAX = stage object manager, ECX = node. Push-front
// into the doubly linked node list: head list_head_0434
// (manager+0x434), count node_count_0438 (manager+0x438); the node
// links are StageObjectHeader::list_prev_0004 (+0x004) and
// list_next_0008 (+0x008).
void LinkStageObjectNodeEaxEcxAbi(void *manager, void *node_memory);

// TH10 0x0041c120. EBX = manager. Requests manager work slot 7 for
// bullet.anm into bullet_anm_work_0458 (+0x458), installs the manager
// calc callback 0x0041c480 (slot 0x13) and draw callback 0x0041c4e0
// (slot 0x1b) into calc_element (+0x8) / draw_element (+0xc), and
// seeds list_head_0434 (+0x434) with &list_sentinel_0010
// (manager+0x10). Returns -1 when the work slot is unavailable, else 0.
i32 InitializeStageObjectManagerEbxAbi(void *manager);

// TH10 0x0041c1c0. Stack = manager (retn 4). Releases both scheduler
// callbacks (calc_element / draw_element), then walks the node chain
// from list_sentinel_0010.list_next_0008 (manager+0x18): each node is
// released through its vtable slot 4 (+0x10), unlinked through its
// list_prev_0004 / list_next_0008 links, and freed. Clears the manager
// global DAT_0047781c; the native performs no list_head_0434 /
// node_count_0438 fixup here.
void DestroyStageObjectManagerStackAbi(void *manager);

// TH10 0x0041c290. EBX = manager. Allocates the 0x45c manager, runs
// the header default initializer on list_sentinel_0010
// (manager+0x10), then zeroes the whole 0x45c block (the native wipes
// the header defaults it just wrote), publishes it at DAT_0047781c and
// initializes the callbacks. On failure the manager is destroyed and
// freed; returns it or 0.
void *CreateStageObjectManagerEbxAbi(void *manager);

// TH10 0x0041c480. ECX = manager (calc callback). Gates on the main
// chain state word at DAT_00477810+0x58: bits 0 or 2 (or bit 10) short
// circuit to 1. Bit 1 saves the frame-time scale dword, zeroes it,
// ticks the manager slots (0x0041c330) and restores the scale.
// Returns the tick result (or 1 on the gate paths).
i32 StageObjectManagerCalcCallbackEcxAbi(void *manager);

// TH10 0x0041c4e0. ECX = manager (draw callback). Skipped while the
// main chain has flag bit 2 of +0x58; otherwise walks the node chain
// from list_sentinel_0010.list_next_0008 (manager+0x18) and calls
// vtable slot 3 (+0xc) on every node whose state_000c (+0x00c) is not
// 1. Returns 1.
i32 StageObjectManagerDrawCallbackEcxAbi(void *manager);

// TH10 0x0041c760. EAX = manager, EBX = velocity vec3, stack =
// {position vec3, argument} (retn 8). Stores the position into
// tween_target_x/y/z_0440/444/448 and the velocity into
// broadcast_vel_x/y/z_044c/450/454, then walks the node chain from
// list_sentinel_0010.list_next_0008 (manager+0x18) and sums vtable
// slot 6 (+0x18) over every node whose state_000c is not 1. Returns
// the sum.
i32 BroadcastStageObjectSpawnEaxEbxStackAbi(void *manager,
                                            const u32 velocity[3],
                                            const u32 position[3],
                                            i32 argument);

// TH10 0x0041c7d0. EAX = manager. Walks the node chain from
// list_sentinel_0010.list_next_0008 (manager+0x18) and sets the
// pending byte done_latch_0050 (+0x050) for every node whose
// state_000c is not 1. Returns 0.
i32 MarkStageObjectsPendingEaxAbi(void *manager);

// TH10 0x0041c880. EAX = manager, stack = {value, argument} (retn 8).
// Walks the node chain from list_sentinel_0010.list_next_0008
// (manager+0x18) and sums vtable slot 8 (+0x20) over every node whose
// state_000c is not 1. Returns the sum.
i32 SumStageObjectCounterVirtualEaxStackAbi(void *manager, u32 value,
                                            u32 argument);

// TH10 0x0041d7c0. ECX = boss record. The boss record shares the kind
// A layout (StageObjectKindA, 0xd58) with its VM records at +0x600
// (vm1_0600) and +0x9ac (vm2_09ac): header.position_x_0024 /
// position_y_0028 (+0x024/+0x028) offset by (+224, +16) seed
// vm1_0600.base_pos_x/y (+0x934/+0x938) and vm2_09ac.base_pos_x/y
// (+0xce0/+0xce4), header.position_z_002c (+0x02c) is stored as the
// depth, header.angle_003c (+0x03c) is wrapped by pi/2 into
// vm1_0600.rotation_z (+0x62c), vm1_0600.flags bit 2 (+0x35c) is raised
// (vm1 only — native 0x41d802; the vm2 path 0x41d83f..0x41d863 writes no
// flags) and the render mode dispatched; the second VM (vm2_09ac) is only
// dispatched while header.zvel_004c (+0x04c) is zero. Returns 0.
i32 SpawnBossDropItemVmsEcxAbi(void *boss_memory);

// TH10 0x0041e4d0 / 0x0041f670 (identical twins). ECX = stage object
// (header.position_x_0024/position_y_0028, angle_003c, half extents
// depth_0040/alpha_0044), stack = {point vec2, radius} (retn 8).
// Rotated-box hit test of the point expanded by the radius against the
// object's oriented box: returns 2 on hit, 0 on miss.
i32 StageObjectRotatedBoxHitA_Thiscall(void *object, const float point[2],
                                       float radius);
i32 StageObjectRotatedBoxHitB_Thiscall(void *object, const float point[2],
                                       float radius);

// TH10 0x0041f820. EAX = stage entity. Sets flag bit 4 (+0x10) and
// clears flag bit 5 (+0x20) of the +0x35c state word (the focus /
// unfocus transition pair).
void SetStageEntityFocusFlagEaxAbi(void *entity);

// TH10 0x0042ba70. EAX = stage entity, stack = value (retn 4). Stores
// the value at +0x4c and raises flag bit 3 (0x8) of the +0x35c state
// word. Unreferenced in the binary (no direct xrefs); kept as an
// isolated semantic body.
void SetStageEntityField4cRaiseFlag3EaxStackAbi(void *entity, u32 value);

// TH10 0x0041f850. ESI = 0x3f0 stage entity record. Installs the
// stage-entity vtable (0x004703e4), clears the kind flags, zeroes the
// 0x3ac script region, writes 0xffff into the +0x378 word, then zeroes
// the whole 0x3f0 record again, sets flag bit 1 and publishes it at
// DAT_00477820. Returns the record.
void *InitializeStageEntityRecordEsiAbi(void *record);

} // namespace th10
