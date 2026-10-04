#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Stage/background object family (TH10 0x41c030..0x41f670).
//
// The DAT_0047781c manager owns 0xd58-byte (kind A, table 0x46da60) and
// 0xd74-byte (kind B, table 0x46da10) objects. The object layout (shared
// 0x424-byte header with the manager list node at +0x04/+0x08, the
// per-kind descriptor images at +0x424, the embedded animation VM records)
// and the 0x45c-byte manager are modeled field-by-field in
// StageObjectObject.hpp / StageObjectManagerObject.hpp; the functions below
// take void* and cast to those typed views. The callback tables are
// null-terminated exactly like the binary blobs at 0x46da10 / 0x46da60 /
// 0x46dab0. All offsets referenced in the comments are native object
// offsets.

// TH10 0x0041c030. EDX = header block (either a bare object or the
// manager+0x10 sentinel). Preserves the native dead stores: the table
// pointer, the flag-bit clears and the mid-body stores are all erased by the
// trailing 0x424-byte memset; only the final timer defaults survive.
void *InitStageObjectHeaderDefaultsEdxAbi(void *block);

// TH10 0x0041c100. ESI = manager memory; zeroes 0x45c bytes, publishes the
// global pointer and returns it. The sub-init call is dead (erased by the
// memset), as in the binary.
void *CreateStageObjectManagerInPlaceEsiAbi(void *manager);

// TH10 0x0041c5b0. EBX = 0xd58-byte kind A object; installs table 0x46da60.
void *InitStageObjectKindA_EbxAbi(void *object);

// TH10 0x0041c680. EBX = 0xd74-byte kind B object; installs table 0x46da10.
void *InitStageObjectKindB_EbxAbi(void *object);

// TH10 0x0041c510. ESI = DAT_0047781c manager, EDI = caller context forwarded
// verbatim to the slot-1 init (the native leaks the caller's EDI), stack =
// kind (0 -> kind A, 1 -> kind B, anything else reaches the int3 padding in
// the binary). ret 4; returns the new object or null when the 0x100 cap at
// manager+0x438 is hit. An allocation failure keeps running and dereferences
// the null object (+0x54 store), as in the binary.
void *SpawnStageObjectEsiEdiStackAbi(void *manager, void *forwarded_edi,
                                     i32 kind);

// TH10 0x0041c8c0 (table 0x46da60 slot 1). Copies the 0x1dc-byte descriptor
// into the object, binds the two animation VMs and seeds the motion fields.
i32 TH10_STDCALL StageObjectSpawnDescriptorA(void *object,
                                             const void *descriptor);

// TH10 0x0041e5c0 (table 0x46da10 slot 1). Kind B twin with the 0x1f8-byte
// descriptor and the T2 field map.
i32 TH10_STDCALL StageObjectSpawnDescriptorB(void *object,
                                             const void *descriptor);

// TH10 0x0041d3d0 (table 0x46da60 slot 2). Kind A per-frame update: slot-0
// script tick, the ten-way feature flag dispatch, depth/ground handling,
// tip emission and VM position publication. Returns 1 to release the object.
i32 TH10_STDCALL StageObjectUpdateA(void *object);

// TH10 0x0041e700 (table 0x46da10 slot 2). Kind B per-frame update with the
// four-state entrance machine (states 2..5) and alpha interpolation.
i32 TH10_STDCALL StageObjectUpdateB(void *object);

// TH10 0x0041cfd0 (table 0x46da60 slot 15, feature flag 0x8000c00). Plays the
// cutoff effect ring entry, snapshots the motion into the descriptor copy and
// spawns a fresh kind A object; clears flag bits 0x8000000|0xc00.
void TH10_STDCALL StageObjectCutoffRespawnA(void *object);

// TH10 0x0041d170 (table 0x46da60 slot 12, feature flag 0x40). Distance-driven
// shrink: re-fires the terminal event every +0x11c distance units.
void TH10_STDCALL StageObjectDistanceShrinkA(void *object);

// TH10 0x0041d2c0 (table 0x46da60 slot 10, feature flag 0x10). Frame-time
// drift: scales the velocity and depth rate by the per-object multipliers
// while the +0x90/+0x94/+0x98 rate record runs, refreshes the +0x3c angle
// from the drifted velocity once it exceeds 7.5 on either axis, and clears
// feature flag 0x10 when the +0x90 tick count reaches the +0xb4 limit.
void TH10_STDCALL StageObjectDriftSlotA(void *object);

// TH10 0x0041e260 (table 0x46da60 slot 5; the 0x41c850 bullet-clear virtual).
// Radial spread: spawns ring VMs every 12 degrees until the ring passes the
// kind B depth; sets state +0xc = 1 on completion. Returns 0.
i32 TH10_STDCALL StageObjectSpreadA(void *object, i32 enable_explosion);

// TH10 0x0041f400 (table 0x46da10 slot 5). Kind B twin; also advances the
// depth per step and latches +0xc = 1.
i32 TH10_STDCALL StageObjectSpreadB(void *object, i32 enable_explosion);

// TH10 0x0041d880 (table 0x46da60 slot 6; the 0x41c800 region-hit virtual).
// Sweep: walks the angle 12 degrees per step while the step start stays below
// the object depth, box-testing each ray point against the 6.0f box around
// the argument vector, advancing the argument position to the first gap.
// ret 0xc.
i32 TH10_STDCALL StageObjectSweepBoxA(void *object, const float center[3],
                                      const float extent[3], i32 flag);

// TH10 0x0041dd80 (table 0x46da60 slot 7). Radial twin of slot 6: hit test is
// dx^2 + dy^2 against the squared radius argument, and every hit spawns a
// ring VM moving to the target. ret 0xc.
i32 TH10_STDCALL StageObjectSweepRadialA(void *object, const float target[3],
                                         float radius, i32 flag);

// TH10 0x0041eb00 (table 0x46da10 slot 6). Kind B box sweep twin.
i32 TH10_STDCALL StageObjectSweepBoxB(void *object, const float center[3],
                                      const float extent[3], i32 flag);

// TH10 0x0041efa0 (table 0x46da10 slot 7). Kind B radial sweep twin.
i32 TH10_STDCALL StageObjectSweepRadialB(void *object, const float target[3],
                                         float radius, i32 flag);

// TH10 0x0041f7a0. ECX = 2-float center, stack (dx, dy), ret 8. Returns 1
// when the box {center +/- d} leaves the playfield x=[-192,192) y=[0,448).
i32 TH10_STDCALL IsBoxOutsidePlayfieldEcxStackAbi(const float center[2],
                                                  float dx, float dy);

// TH10 0x0043dc90. ECX = effect manager (0x492590), EDI = kind, stack = value.
// Stores the value into the 128-entry ring of the kind's slot in the
// manager's 12-slot effect ring (+0x620 kinds, +0x650 counts, +0x680 ring).
void TH10_STDCALL QueueEffectRingValueEcxStackAbi(i32 kind, i32 value);

// Shared no-op slot bodies (0x41bf90..0x41c020 and friends).
i32 StageObjectSlotRet0(void *object);          // 0x41bf10 / 0x41bf20
void StageObjectSlotNoop(void *object);         // 0x41bf90..0x41c020 plain ret
void StageObjectSlotUnlinkThiscall(void *object); // 0x41bf30

// Unimplemented sibling slots, kept as boundaries (referenced by the two
// native tables but not dispatched by the reconstructed bodies above).
void StageObjectSlotBoundary(void *object);     // 0x41d7c0/0x41d870/
                                                // 0x41e4d0/0x41ea40/0x41eaf0/
                                                // 0x41f670
// TH10 0x0041ca80 (table 0x46da60 slot 0): the kind A instruction-queue
// interpreter (18 records of 0x18 bytes at object+0x460, 64-opcode jump
// table at 0x41cf78/0x41cf8c). Not part of this batch; boundary.
void StageObjectScriptTickA(void *object);

} // namespace th10
