#pragma once

#include <stddef.h>

#include "CallbackScheduler.hpp"
#include "StageObjectObject.hpp"
#include "Th10Types.hpp"

namespace th10 {

// ---------------------------------------------------------------------------
// Stage/background object manager (TH10 DAT_0047781c; source aliases
// g_BulletListRoot / g_BulletListRootSlot / g_StageObjectManager /
// g_EntranceTweenManager), 0x45c bytes.
//
// Creator 0x41c290: operator new(0x45c), header defaults at +0x10 (0x41c030,
// dead — erased by the whole-object wipe), publish, then 0x41c120 requests
// manager work slot 7 ("bullet.anm", handle at +0x458) and installs calc
// 0x41c480 (priority 0x13) / draw 0x41c4e0 (priority 0x1b). The slot tick
// is the shared effect-node list ticker 0x41c330 (TickEffectNodeList:
// container +0x18 first node, +0x434 head, +0x438 count). In-place dtor
// 0x41c1c0 releases both records, walks the chain from the sentinel
// releasing each node through vtable slot 4, unlinks and frees, and clears
// the global.
//
// The sentinel header at +0x10 doubles as the list tail anchor: +0x434
// seeds with &manager+0x10 and push-front links every node in front of it.
// The broadcast region 0x440..0x458 caches the entrance-tween target
// (0x41c800) and the spawn position/velocity pair (0x41c760).
// ---------------------------------------------------------------------------

struct StageObjectManager {
    u32 flags_0000;               // +0x000 (no bit set; whole-object wipe)
    u8 gap0004[4];                // +0x0004
    ChainElem *calc_element;      // +0x0008 (0x41c480, priority 0x13)
    ChainElem *draw_element;      // +0x000c (0x41c4e0, priority 0x1b)
    StageObjectHeader list_sentinel_0010; // +0x0010 sentinel node (dead
                                          //   header defaults at creation;
                                          //   walk starts at its +0x18)
    StageObjectHeader *list_head_0434;    // +0x0434 push-front head (seeds
                                          //   with the sentinel address)
    u32 node_count_0438;          // +0x0438 spawn cap 256
    u32 spawn_id_cursor_043c;     // +0x043c id counter (0xffffffff -> 1)
    float tween_target_x_0440;    // +0x0440 entrance-tween target = spawn
    float tween_target_y_0444;    //   broadcast position (0x41c760)
    float tween_target_z_0448;
    float broadcast_vel_x_044c;   // +0x044c spawn broadcast velocity
    float broadcast_vel_y_0450;
    float broadcast_vel_z_0454;
    void *bullet_anm_work_0458;   // +0x0458 RequestManagerWork(7, owner,
                                  //   "bullet.anm"); failure path reports
                                  //   through 0x44b810 and returns -1
};

typedef char AssertStageObjectManagerSize[
    sizeof(StageObjectManager) == 0x45c ? 1 : -1];
typedef char AssertStageObjectManagerCalcOffset[
    offsetof(StageObjectManager, calc_element) == 0x8 ? 1 : -1];
typedef char AssertStageObjectManagerSentinelOffset[
    offsetof(StageObjectManager, list_sentinel_0010) == 0x10 ? 1 : -1];
typedef char AssertStageObjectManagerHeadOffset[
    offsetof(StageObjectManager, list_head_0434) == 0x434 ? 1 : -1];
typedef char AssertStageObjectManagerCountOffset[
    offsetof(StageObjectManager, node_count_0438) == 0x438 ? 1 : -1];
typedef char AssertStageObjectManagerCursorOffset[
    offsetof(StageObjectManager, spawn_id_cursor_043c) == 0x43c ? 1 : -1];
typedef char AssertStageObjectManagerTweenOffset[
    offsetof(StageObjectManager, tween_target_x_0440) == 0x440 ? 1 : -1];
typedef char AssertStageObjectManagerVelOffset[
    offsetof(StageObjectManager, broadcast_vel_x_044c) == 0x44c ? 1 : -1];
typedef char AssertStageObjectManagerWorkOffset[
    offsetof(StageObjectManager, bullet_anm_work_0458) == 0x458 ? 1 : -1];

} // namespace th10
