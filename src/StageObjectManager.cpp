#include "StageObjectManager.hpp"

#include "AsciiRenderModeDispatcher.hpp"

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

void LinkStageObjectNodeEaxEcxAbi(void *manager, void *node)
{
    u8 *bytes = static_cast<u8 *>(manager);
    u32 *list_head = reinterpret_cast<u32 *>(bytes + 0x434);
    u32 *node_words = static_cast<u32 *>(node);
    node_words[1] = *list_head;             // node->prev = old head
    u32 *previous = reinterpret_cast<u32 *>(*list_head);
    previous[2] = reinterpret_cast<u32>(node); // old head->next = node
    *list_head = reinterpret_cast<u32>(node);
    ++(*reinterpret_cast<u32 *>(bytes + 0x438));
}

i32 InitializeStageObjectManagerEbxAbi(void *manager)
{
    u8 *bytes = static_cast<u8 *>(manager);
    void *work = RequestManagerWorkSlotNative(
        g_RenderOwner, kStageObjectWorkName, 7);
    *reinterpret_cast<void **>(bytes + 0x458) = work;
    if (work == 0) {
        ReportStageObjectError(kStageObjectFailureText);
        return -1;
    }

    u8 *calc_node = static_cast<u8 *>(AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&StageObjectManagerCalcCallback)));
    *reinterpret_cast<u32 *>(calc_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(calc_node + 0x20) =
        reinterpret_cast<u32>(bytes);
    RegisterSchedulerCalcCallback(calc_node, &g_SchedulerHeap, 0x13);
    *reinterpret_cast<u32 *>(bytes + 8) =
        reinterpret_cast<u32>(calc_node);

    u8 *draw_node = static_cast<u8 *>(AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&StageObjectManagerDrawCallback)));
    *reinterpret_cast<u32 *>(draw_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(draw_node + 0x20) =
        reinterpret_cast<u32>(bytes);
    RegisterSchedulerDrawCallback(draw_node, &g_SchedulerHeap, 0x1b);
    *reinterpret_cast<u32 *>(bytes + 0xc) =
        reinterpret_cast<u32>(draw_node);

    *reinterpret_cast<u32 *>(bytes + 0x434) =
        reinterpret_cast<u32>(bytes + 0x10);
    return 0;
}

void DestroyStageObjectManagerStackAbi(void *manager)
{
    u8 *bytes = static_cast<u8 *>(manager);

    void *calc_node = *reinterpret_cast<void **>(bytes + 8);
    if (calc_node != 0)
        ReleaseSchedulerCallback(calc_node, &g_SchedulerHeap);
    void *draw_node = *reinterpret_cast<void **>(bytes + 0xc);
    if (draw_node != 0)
        ReleaseSchedulerCallback(draw_node, &g_SchedulerHeap);

    // Vtable slot 4 (+0x10) releases each node before the unlink.
    typedef void (*ReleaseThunk)(void *);
    u8 *node = *reinterpret_cast<u8 **>(bytes + 0x18);
    while (node != 0) {
        u8 *next = *reinterpret_cast<u8 **>(node + 8);
        void **vtable = *reinterpret_cast<void ***>(node);
        reinterpret_cast<ReleaseThunk>(vtable[4])(node);
        // Unlink: node->prev->next = node->next; node->next->prev =
        // node->prev (the 0x41c0d0 twin), then the node is freed.
        u32 *node_words = reinterpret_cast<u32 *>(node);
        u32 *previous = reinterpret_cast<u32 *>(node_words[1]);
        u32 *following = reinterpret_cast<u32 *>(node_words[2]);
        previous[2] = reinterpret_cast<u32>(following);
        if (following != 0)
            following[1] = node_words[1];
        FreeHeapBlock(node);
        node = next;
    }
    g_StageObjectManager = 0;
}

void *CreateStageObjectManagerEbxAbi(void *manager)
{
    u8 *record = AllocateHeapBlock(0x45c);
    if (record != 0) {
        InitStageObjectHeaderDefaultsEdxNative(record + 0x10);
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
    u8 *main_chain = static_cast<u8 *>(g_MainChainContext);
    const u32 state = *reinterpret_cast<const u32 *>(main_chain + 0x58);
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
// kind 1 (+0xc) are skipped by every caller.
u8 *NodeChainHead(void *manager)
{
    return *reinterpret_cast<u8 **>(static_cast<u8 *>(manager) + 0x18);
}

} // namespace

i32 StageObjectManagerDrawCallbackEcxAbi(void *manager)
{
    u8 *main_chain = static_cast<u8 *>(g_MainChainContext);
    if ((*reinterpret_cast<const u8 *>(main_chain + 0x58) & 4U) != 0U)
        return 1;
    typedef void (*NotifyThunk)(void *);
    for (u8 *node = NodeChainHead(manager); node != 0;
         node = *reinterpret_cast<u8 **>(node + 8)) {
        if (*reinterpret_cast<u32 *>(node + 0xc) == 1U)
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
    u8 *bytes = static_cast<u8 *>(manager);
    u32 *pos_slot = reinterpret_cast<u32 *>(bytes + 0x440);
    pos_slot[0] = position[0];
    pos_slot[1] = position[1];
    pos_slot[2] = position[2];
    u32 *vel_slot = reinterpret_cast<u32 *>(bytes + 0x44c);
    vel_slot[0] = velocity[0];
    vel_slot[1] = velocity[1];
    vel_slot[2] = velocity[2];

    typedef i32 (*SpawnThunk)(void *, const u32 *, const u32 *, i32);
    i32 total = 0;
    for (u8 *node = NodeChainHead(manager); node != 0;
         node = *reinterpret_cast<u8 **>(node + 8)) {
        if (*reinterpret_cast<u32 *>(node + 0xc) == 1U)
            continue;
        void **vtable = *reinterpret_cast<void ***>(node);
        total += reinterpret_cast<SpawnThunk>(vtable[6])(
            node, position, velocity, argument);
    }
    return total;
}

i32 MarkStageObjectsPendingEaxAbi(void *manager)
{
    for (u8 *node = NodeChainHead(manager); node != 0;
         node = *reinterpret_cast<u8 **>(node + 8)) {
        if (*reinterpret_cast<u32 *>(node + 0xc) == 1U)
            continue;
        if (*reinterpret_cast<u8 *>(node + 0x50) == 0)
            *reinterpret_cast<u8 *>(node + 0x50) = 1;
    }
    return 0;
}

i32 SumStageObjectCounterVirtualEaxStackAbi(void *manager, u32 value,
                                            u32 argument)
{
    typedef i32 (*CountThunk)(void *, u32, u32);
    i32 total = 0;
    for (u8 *node = NodeChainHead(manager); node != 0;
         node = *reinterpret_cast<u8 **>(node + 8)) {
        if (*reinterpret_cast<u32 *>(node + 0xc) == 1U)
            continue;
        void **vtable = *reinterpret_cast<void ***>(node);
        total += reinterpret_cast<CountThunk>(vtable[8])(node, value,
                                                         argument);
    }
    return total;
}

i32 SpawnBossDropItemVmsEcxAbi(void *boss)
{
    u8 *bytes = static_cast<u8 *>(boss);
    const float drop_x =
        *reinterpret_cast<const float *>(bytes + 0x24) + 224.0f;
    const float drop_y =
        *reinterpret_cast<const float *>(bytes + 0x28) + 16.0f;
    *reinterpret_cast<float *>(bytes + 0x934) = drop_x;
    *reinterpret_cast<float *>(bytes + 0x938) = drop_y;
    *reinterpret_cast<u32 *>(bytes + 0x93c) =
        *reinterpret_cast<const u32 *>(bytes + 0x2c);

    // The wrapped angle lands in the +0x600 VM's +0x2c slot.
    *reinterpret_cast<float *>(bytes + 0x62c) = WrapAngleSumStackAbi(
        *reinterpret_cast<const float *>(bytes + 0x3c), 1.5707964f);

    u8 *first_vm = bytes + 0x600;
    *reinterpret_cast<u32 *>(first_vm + 0x35c) |= 4U;
    DispatchAsciiAnimationVmRenderMode(first_vm, g_RenderOwner);

    if (*reinterpret_cast<const float *>(bytes + 0x4c) == 0.0f) {
        *reinterpret_cast<float *>(bytes + 0xce0) = drop_x;
        *reinterpret_cast<float *>(bytes + 0xce4) = drop_y;
        *reinterpret_cast<u32 *>(bytes + 0xce8) =
            *reinterpret_cast<const u32 *>(bytes + 0x2c);
        u8 *second_vm = bytes + 0x9ac;
        *reinterpret_cast<u32 *>(second_vm + 0x35c) |= 4U;
        DispatchAsciiAnimationVmRenderMode(second_vm, g_RenderOwner);
    }
    return 0;
}

namespace {

// Shared rotated-box body for the 0x41e4d0 / 0x41f670 twins.
i32 RotatedBoxHit(const void *object, const float point[2], float radius)
{
    const u8 *bytes = static_cast<const u8 *>(object);
    const float dx = point[0] - *reinterpret_cast<const float *>(bytes + 0x24);
    const float dy = point[1] - *reinterpret_cast<const float *>(bytes + 0x28);
    const float angle = -*reinterpret_cast<const float *>(bytes + 0x3c);
    const float sine = static_cast<float>(std::sin(angle));
    const float cosine = static_cast<float>(std::cos(angle));
    const float rx = dx * cosine - sine * dy;
    const float ry = cosine * dy + sine * dx;
    const float half_width =
        *reinterpret_cast<const float *>(bytes + 0x40);
    const float half_height =
        *reinterpret_cast<const float *>(bytes + 0x44);
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
