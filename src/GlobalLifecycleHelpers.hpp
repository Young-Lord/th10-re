#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0041fac0. EBX = GlobalLifecycleManager. Allocates the calc
// callback node for 0x0041feb0 (registered on the calc scheduler with
// slot 3) and the draw callback node for 0x0041fef0 (draw scheduler,
// slot 2), binds the manager at node+0x20 and stores both at
// manager+8/+0xc. Then stops the thread control block at +0x10, seeds
// startup_thread_proc (+0x28), thread_running (+0x20) = 1,
// close_requested (+0x1c) = 0 and starts the CRT worker through
// _beginthreadex with the manager as the argument (handle at +0x14,
// id at +0x18). Returns 0.
i32 InstallGlobalLifecycleCallbacksEbxAbi(void *manager);

// TH10 0x0041fdd0. Stack = GlobalLifecycleManager (retn 4). Advances
// the loading-screen entity stages: startup_stage (+0x3e4) == 1
// creates the preset text slot node (kind 15, +0x40000000 flag,
// preset clone 0), links it and parks it at +0x3dc; draw_stage
// (+0x3e8) == 1 creates the continuation render object (kind 6) into
// DAT_004776e0+0x89a4 when that slot is still empty. Always bumps
// draw_frame_count (+0x3ec) and returns 1.
i32 CreateLoadingScreenEntitiesStackAbi(void *manager);

// TH10 0x0041ff00. EAX = owner. Raises flag bit 1 (0x2) on the three
// records at owner+0xc / +0x10 / +0x89a8 with no null checks (the
// native dereferences all three unconditionally).
void MarkPauseChainFlagsUncheckedEaxAbi(void *owner);

// TH10 0x0041fd00 is the CreateGlobalLifecycleManager factory; its
// semantic body lives in GlobalLifecycleManager.cpp (source-level
// factory preserving the ESI constructor / EBX initializer split).
// TH10 0x00420100 is InitializeVersionData; its semantic body lives in
// VersionData.cpp. Both are registered as boundaries here so the batch
// stays complete without duplicating the bodies.

} // namespace th10
