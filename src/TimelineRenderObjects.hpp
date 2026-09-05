#pragma once

#include "Th10Types.hpp"
#include "TimelineTextSubmission.hpp"

namespace th10 {

struct ManagerWorkPartial;

// TH10 0x004491c0. Looks up a render-owner handle in either owner list.
void *ResolveTimelineHandle(void *owner, i32 handle);

// TH10 0x00449450. Resolves the slot handle and clears the slot on failure.
void *RefreshTimelineTextHandle(void **slot);

// TH10 0x004492a0. Marks the resolved object and any direct children for release.
void ReleaseTimelineHandle(void *owner, i32 handle);

// TH10 0x00449630. Releases the destination slot handle and clears it.
void SetTimelineObjectPhase(void **slot);

// TH10 0x004091c0 body used by continuation cleanup. Marks kind 1 on the handle
// referenced through the supplied pointer.
void ReleaseTimelineContinuationHandle(i32 *handle_slot);

// TH10 0x00417010. Decrypts a timeline record payload string in place through
// the shared scratch buffer at 0x00497d40.
const char *DecryptTimelineRecordText(const u8 *payload);

// TH10 0x00448d00. Setup-script spawn: pool VM + script bind, register in
// owner list A (tail). Returns the record whose first dword is its id.
i32 *SpawnSetupEffectVmListABack(i32 script_id, u32 kind);

// TH10 0x00448e30. Same spawn, registered at the front of owner list A.
i32 *SpawnSetupEffectVmListAFront(i32 script_id, u32 kind);

// TH10 0x00448f60. Same spawn, registered at the tail of owner list B.
i32 *SpawnSetupEffectVmListBBack(i32 script_id, u32 kind);

// TH10 0x00449090. Same spawn, registered at the front of owner list B.
i32 *SpawnSetupEffectVmListBFront(i32 script_id, u32 kind);

// TH10 0x00448d50. Creates the continuation render object used by opcode 7.
void *CreateTimelineContinuationRenderObject(void *manager_work, u32 kind,
                                             const float params[3]);

// TH10 0x00449870 preset clone used while seeding the five timeline text slots.
void ApplyTimelineRenderObjectPresetClone(ManagerWorkPartial *work, void *node,
                                          u16 clone_field);

// TH10 0x00449950 plus 0x004489d0. Allocates an owner node, applies the preset
// clone, and returns the published handle.
i32 CreateTimelinePresetTextSlotNode(void *owner, ManagerWorkPartial *text_work,
                                     u16 clone_field);

} // namespace th10
