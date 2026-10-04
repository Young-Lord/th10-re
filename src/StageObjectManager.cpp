#include "StageObjectManager.hpp"

#include "AsciiRenderModeDispatcher.hpp"
#include "StageObjectManagerObject.hpp"
#include "TitleScreenObject.hpp"

#include <cmath>
#include <string.h>

namespace th10 {

namespace {

// TH10 0x452493 / 0x452422 / 0x4524a1: operator new / free / delete.
u8 *AllocateHeapBlock(u32 bytes);
void FreeHeapBlock(void *pointer);

// TH10 0x0041c030: header defaults for the 0x10 sub-block.
void InitStageObjectHeaderDefaultsEdxNative(void *sub);

// TH10 0x00447280 / 0x0044b810: manager work request and the error
// logger on the reserved output object.
void *RequestManagerWorkSlotNative(void *owner, const char *name,
                                   i32 slot);
void ReportStageObjectError(const char *text);

// TH10 0x00449ed0 / 0x00449ae0 / 0x00449b70 / 0x00449f60: scheduler
// node alloc, calc/draw registration and release.
void *AllocSchedulerCallbackNode(void *callback);
void RegisterSchedulerCalcCallback(void *node, void *heap, u32 slot);
void RegisterSchedulerDrawCallback(void *node, void *heap, u32 slot);
void ReleaseSchedulerCallback(void *node, void *heap);

// Bound callbacks owned by other translation units.
void StageObjectManagerCalcCallback();   // TH10 0x0041c480
void StageObjectManagerDrawCallback();   // TH10 0x0041c4e0

// TH10 0x0041c330: the manager slot tick behind both callbacks.
i32 TickStageObjectManagerSlots(void *manager);

// TH10 0x0044bc10: angle + delta wrapped into [-pi, pi); result in st0.
float WrapAngleSumStackAbi(float angle, float delta);

extern u32 g_SchedulerHeap;       // TH10 DAT_00491be4
extern void *g_RenderOwner;       // TH10 DAT_00491c10
extern void *g_StageObjectManager; // TH10 DAT_0047781c
extern void *g_MainChainContext;   // TH10 DAT_00477810
extern void *g_StageEntityRecord;  // TH10 DAT_00477820
extern float g_FrameTimeScale;     // TH10 DAT_00476f78

const char kStageObjectWorkName[] = "bullet.anm"; // TH10 0x0046cd88
// TH10 0x0046cd54: failure message for the reserved logger.
const char kStageObjectFailureText[] = "";

const u32 kStageEntityVtable = 0x4703e4U; // TH10 off_004703e4

} // namespace

void LinkStageObjectNodeEaxEcxAbi(void *manager, void *node_memory)
{
    StageObjectManager &mgr =
        *reinterpret_cast<StageObjectManager *>(manager);
    StageObjectHeader &node =
        *reinterpret_cast<StageObjectHeader *>(node_memory);
    node.list_prev_0004 = mgr.list_head_0434;   // node->prev = old head
    mgr.list_head_0434->list_next_0008 = &node; // old head->next = node
    mgr.list_head_0434 = &node;
    ++mgr.node_count_0438;
}

i32 InitializeStageObjectManagerEbxAbi(void *manager)
{
    StageObjectManager &mgr =
        *reinterpret_cast<StageObjectManager *>(manager);
    void *work = RequestManagerWorkSlotNative(
        g_RenderOwner, kStageObjectWorkName, 7);
    mgr.bullet_anm_work_0458 = work;
    if (work == 0) {
        ReportStageObjectError(kStageObjectFailureText);
        return -1;
    }

    ChainElem *calc_element = static_cast<ChainElem *>(
        AllocSchedulerCallbackNode(
            reinterpret_cast<void *>(&StageObjectManagerCalcCallback)));
    calc_element->flags &= ~2U;
    calc_element->arg = manager;
    RegisterSchedulerCalcCallback(calc_element, &g_SchedulerHeap, 0x13);
    mgr.calc_element = calc_element;

    ChainElem *draw_element = static_cast<ChainElem *>(
        AllocSchedulerCallbackNode(
            reinterpret_cast<void *>(&StageObjectManagerDrawCallback)));
    draw_element->flags &= ~2U;
    draw_element->arg = manager;
    RegisterSchedulerDrawCallback(draw_element, &g_SchedulerHeap, 0x1b);
    mgr.draw_element = draw_element;

    mgr.list_head_0434 = &mgr.list_sentinel_0010;
    return 0;
}

void DestroyStageObjectManagerStackAbi(void *manager)
{
    StageObjectManager &mgr =
        *reinterpret_cast<StageObjectManager *>(manager);

    ChainElem *calc_element = mgr.calc_element;
    if (calc_element != 0)
        ReleaseSchedulerCallback(calc_element, &g_SchedulerHeap);
    ChainElem *draw_element = mgr.draw_element;
    if (draw_element != 0)
        ReleaseSchedulerCallback(draw_element, &g_SchedulerHeap);

    // Vtable slot 4 (+0x10) releases each node before the unlink.
    typedef void (*ReleaseThunk)(void *);
    StageObjectHeader *node = static_cast<StageObjectHeader *>(
        mgr.list_sentinel_0010.list_next_0008);
    while (node != 0) {
        // The next link is read before the release runs.
        StageObjectHeader *next = static_cast<StageObjectHeader *>(
            node->list_next_0008);
        void **vtable = *reinterpret_cast<void ***>(node);
        reinterpret_cast<ReleaseThunk>(vtable[4])(node);
        // Unlink: node->prev->next = node->next; node->next->prev =
        // node->prev (the 0x41c0d0 twin), then the node is freed.
        StageObjectHeader *previous = static_cast<StageObjectHeader *>(
            node->list_prev_0004);
        StageObjectHeader *following = static_cast<StageObjectHeader *>(
            node->list_next_0008);
        previous->list_next_0008 = following;
        if (following != 0)
            following->list_prev_0004 = previous;
        FreeHeapBlock(node);
        node = next;
    }
    g_StageObjectManager = 0;
}

void *CreateStageObjectManagerEbxAbi(void *manager)
{
    u8 *record = AllocateHeapBlock(0x45c);
    if (record != 0) {
        StageObjectManager &mgr =
            *reinterpret_cast<StageObjectManager *>(record);
        InitStageObjectHeaderDefaultsEdxNative(&mgr.list_sentinel_0010);
        // The native zeroes the whole block after the header defaults,
        // erasing them; preserved verbatim.
        memset(record, 0, 0x45c);
        g_StageObjectManager = record;
    }
    if (InitializeStageObjectManagerEbxAbi(record) == 0)
        return record;
    if (record != 0) {
        DestroyStageObjectManagerStackAbi(record);
        FreeHeapBlock(record);
    }
    return 0;
}

i32 StageObjectManagerCalcCallbackEcxAbi(void *manager)
{
    const TitleScreen &ts =
        *reinterpret_cast<const TitleScreen *>(g_MainChainContext);
    const u32 state = ts.flags;
    if ((state & 1U) != 0U || (state & 4U) != 0U ||
        (state & 0x400U) != 0U)
        return 1;
    if ((state & 2U) != 0U) {
        // The native parks the frame-time scale while the slot tick
        // runs, restoring the raw dword afterwards.
        const u32 saved =
            *reinterpret_cast<const u32 *>(&g_FrameTimeScale);
        *reinterpret_cast<u32 *>(&g_FrameTimeScale) = 0;
        const i32 result = TickStageObjectManagerSlots(manager);
        *reinterpret_cast<u32 *>(&g_FrameTimeScale) = saved;
        return result;
    }
    return TickStageObjectManagerSlots(manager);
}

namespace {

// TH10 0x0041c4e0 / 0x0041c760 / 0x0041c880 shared walk: nodes with
// state_000c == 1 (+0x00c) are skipped by every caller.
StageObjectHeader *NodeChainHead(void *manager)
{
    StageObjectManager &mgr =
        *reinterpret_cast<StageObjectManager *>(manager);
    return static_cast<StageObjectHeader *>(
        mgr.list_sentinel_0010.list_next_0008);
}

} // namespace

i32 StageObjectManagerDrawCallbackEcxAbi(void *manager)
{
    const TitleScreen &ts =
        *reinterpret_cast<const TitleScreen *>(g_MainChainContext);
    // Native test is a byte load at +0x58 (test byte ptr [eax+58h], 4).
    if ((*reinterpret_cast<const u8 *>(&ts.flags) & 4U) != 0U)
        return 1;
    typedef void (*NotifyThunk)(void *);
    for (StageObjectHeader *node = NodeChainHead(manager); node != 0;
         node = static_cast<StageObjectHeader *>(node->list_next_0008)) {
        if (node->state_000c == 1)
            continue;
        void **vtable = *reinterpret_cast<void ***>(node);
        reinterpret_cast<NotifyThunk>(vtable[3])(node);
    }
    return 1;
}

i32 BroadcastStageObjectSpawnEaxEbxStackAbi(void *manager,
                                            const u32 velocity[3],
                                            const u32 position[3],
                                            i32 argument)
{
    StageObjectManager &mgr =
        *reinterpret_cast<StageObjectManager *>(manager);
    // The native copies the raw position/velocity dwords into the
    // float cache fields, so the stores keep the bit patterns.
    *reinterpret_cast<u32 *>(&mgr.tween_target_x_0440) = position[0];
    *reinterpret_cast<u32 *>(&mgr.tween_target_y_0444) = position[1];
    *reinterpret_cast<u32 *>(&mgr.tween_target_z_0448) = position[2];
    *reinterpret_cast<u32 *>(&mgr.broadcast_vel_x_044c) = velocity[0];
    *reinterpret_cast<u32 *>(&mgr.broadcast_vel_y_0450) = velocity[1];
    *reinterpret_cast<u32 *>(&mgr.broadcast_vel_z_0454) = velocity[2];

    typedef i32 (*SpawnThunk)(void *, const u32 *, const u32 *, i32);
    i32 total = 0;
    for (StageObjectHeader *node = NodeChainHead(manager); node != 0;
         node = static_cast<StageObjectHeader *>(node->list_next_0008)) {
        if (node->state_000c == 1)
            continue;
        void **vtable = *reinterpret_cast<void ***>(node);
        total += reinterpret_cast<SpawnThunk>(vtable[6])(
            node, position, velocity, argument);
    }
    return total;
}

i32 MarkStageObjectsPendingEaxAbi(void *manager)
{
    for (StageObjectHeader *node = NodeChainHead(manager); node != 0;
         node = static_cast<StageObjectHeader *>(node->list_next_0008)) {
        if (node->state_000c == 1)
            continue;
        if (node->done_latch_0050 == 0)
            node->done_latch_0050 = 1;
    }
    return 0;
}

i32 SumStageObjectCounterVirtualEaxStackAbi(void *manager, u32 value,
                                            u32 argument)
{
    typedef i32 (*CountThunk)(void *, u32, u32);
    i32 total = 0;
    for (StageObjectHeader *node = NodeChainHead(manager); node != 0;
         node = static_cast<StageObjectHeader *>(node->list_next_0008)) {
        if (node->state_000c == 1)
            continue;
        void **vtable = *reinterpret_cast<void ***>(node);
        total += reinterpret_cast<CountThunk>(vtable[8])(node, value,
                                                         argument);
    }
    return total;
}

// The boss record shares the kind A layout (StageObjectKindA, 0xd58):
// the two VM records sit at +0x600 (vm1_0600) and +0x9ac (vm2_09ac).
i32 SpawnBossDropItemVmsEcxAbi(void *boss_memory)
{
    StageObjectKindA &boss =
        *reinterpret_cast<StageObjectKindA *>(boss_memory);
    const float drop_x =
        boss.header.position_x_0024 + 224.0f;
    const float drop_y =
        boss.header.position_y_0028 + 16.0f;
    boss.vm1_0600.base_pos_x = drop_x;
    boss.vm1_0600.base_pos_y = drop_y;
    // The native copies the +0x2c depth as a raw dword.
    *reinterpret_cast<u32 *>(&boss.vm1_0600.base_pos_z) =
        *reinterpret_cast<const u32 *>(&boss.header.position_z_002c);

    // The wrapped angle lands in the +0x600 VM's +0x2c slot.
    boss.vm1_0600.rotation_z = WrapAngleSumStackAbi(
        boss.header.angle_003c, 1.5707964f);

    boss.vm1_0600.flags |= 4U;
    DispatchAsciiAnimationVmRenderMode(&boss.vm1_0600, g_RenderOwner);

    if (boss.header.zvel_004c == 0.0f) {
        boss.vm2_09ac.base_pos_x = drop_x;
        boss.vm2_09ac.base_pos_y = drop_y;
        *reinterpret_cast<u32 *>(&boss.vm2_09ac.base_pos_z) =
            *reinterpret_cast<const u32 *>(&boss.header.position_z_002c);
        // Native 0x41d83f..0x41d863: the second-VM path goes straight to
        // the render-mode dispatch with NO +0xd08 (vm2 flags) write — the
        // flags |= 4 exists only on the vm1 path (0x41d802..0x41d811).
        DispatchAsciiAnimationVmRenderMode(&boss.vm2_09ac, g_RenderOwner);
    }
    return 0;
}

namespace {

// Shared rotated-box body for the 0x41e4d0 / 0x41f670 twins.
i32 RotatedBoxHit(const void *object, const float point[2], float radius)
{
    const StageObjectHeader &obj =
        *reinterpret_cast<const StageObjectHeader *>(object);
    const float dx = point[0] - obj.position_x_0024;
    const float dy = point[1] - obj.position_y_0028;
    const float angle = -obj.angle_003c;
    const float sine = static_cast<float>(std::sin(angle));
    const float cosine = static_cast<float>(std::cos(angle));
    const float rx = dx * cosine - sine * dy;
    const float ry = cosine * dy + sine * dx;
    const float half_width = obj.depth_0040;
    const float half_height = obj.alpha_0044;
    if (rx - radius > half_width)
        return 0;
    if (half_height * 0.5f < ry - radius)
        return 0;
    if (rx + radius < 0.0f)
        return 0;
    if (half_height * -0.5f > ry + radius)
        return 0;
    return 2;
}

} // namespace

i32 StageObjectRotatedBoxHitA_Thiscall(void *object, const float point[2],
                                       float radius)
{
    return RotatedBoxHit(object, point, radius);
}

i32 StageObjectRotatedBoxHitB_Thiscall(void *object, const float point[2],
                                       float radius)
{
    return RotatedBoxHit(object, point, radius);
}

void SetStageEntityFocusFlagEaxAbi(void *entity)
{
    u8 *bytes = static_cast<u8 *>(entity);
    u32 state = *reinterpret_cast<u32 *>(bytes + 0x35c);
    state = (state & ~0x20U) | 0x10U;
    *reinterpret_cast<u32 *>(bytes + 0x35c) = state;
}

// TH10 0x0042ba70. Native EAX = stage entity, stack = value (retn 4).
// Stores the value at +0x4c and raises flag bit 3 (0x8) of the +0x35c
// state word. The binary holds no direct references to this entry; the
// body is reconstructed from the isolated reference disassembly.
void SetStageEntityField4cRaiseFlag3EaxStackAbi(void *entity, u32 value)
{
    u8 *bytes = static_cast<u8 *>(entity);
    *reinterpret_cast<u32 *>(bytes + 0x4c) = value;
    *reinterpret_cast<u32 *>(bytes + 0x35c) |= 8U;
}

void *InitializeStageEntityRecordEsiAbi(void *record)
{
    u32 *words = static_cast<u32 *>(record);
    u8 *bytes = static_cast<u8 *>(record);
    words[4] = kStageEntityVtable;
    words[5] = 0;
    words[6] = 0;
    words[7] = 0;
    words[8] = 0;
    // Nine kind-flag clears ahead of the wipe (same layout as the
    // 0x41acf0 reset but offset by the 0x30-byte record head).
    static const u32 kFlagOffsets[] = {
        0x9cU, 0xe0U, 0x12cU, 0x158U, 0x1a4U, 0x1e0U, 0x22cU, 0x258U,
        0x3a8U
    };
    for (u32 index = 0; index != 9U; ++index)
        *reinterpret_cast<u32 *>(bytes + kFlagOffsets[index]) &= ~1U;
    memset(bytes + 0x30, 0, 0x3ac);
    *reinterpret_cast<u16 *>(bytes + 0x378) = 0xffffU;
    // The native wipes the whole 0x3f0 record again afterwards,
    // including the vtable it just installed; preserved verbatim.
    memset(bytes, 0, 0x3f0);
    *reinterpret_cast<u32 *>(bytes) |= 2U;
    g_StageEntityRecord = bytes;
    return bytes;
}

} // namespace th10
