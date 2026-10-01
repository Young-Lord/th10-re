#include "TimelineRenderObjects.hpp"

#include <string.h>

#include "EntityHelpers.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "ManagerWork.hpp"
#include "StageEffectHelpers.hpp"
#include "TimelineRenderObjectSetup.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

char g_TimelineDecryptScratch[256]; // TH10 DAT_00497d40

extern void *g_MainChainRenderOwner;
extern float g_MainChainStartupScale; // TH10 DAT_00476f78, alias g_AsciiOverlayInitialRate
extern void ResetPooledRenderOwnerNodeEsiEdiAbi(void *owner, void *node);
extern void *AllocateAsciiManagerMemory(u32 bytes);

// Typed view over the 0x732460-byte render owner (TH10 DAT_00491c10);
// every accessor is a named field of LargeRenderOwnerLayout.
struct RenderOwnerAccess {
    explicit RenderOwnerAccess(const void *owner)
        : base(static_cast<LargeRenderOwnerLayout *>(
              const_cast<void *>(owner))) {}

    OwnerLink *first_list_a() const
    {
        return base->first_list_a;
    }

    OwnerLink *second_list_head() const
    {
        return base->first_list_b;
    }

    OwnerLink **last_list_a() const
    {
        return &base->last_list_a;
    }

    u32 *pool_cursor() const
    {
        return &base->pool_cursor;
    }

    u8 *pool_active(u32 index) const
    {
        return &base->pooled_node_active[index];
    }

    u8 *pool_node(u32 index) const
    {
        return reinterpret_cast<u8 *>(&base->pooled_nodes[index]);
    }

    i32 *next_handle() const
    {
        return &base->node_id_counter;
    }

    i32 *live_object_count() const
    {
        return &base->live_object_count;
    }

    LargeRenderOwnerLayout *base;
};

void *NodeFromLink(OwnerLink *link)
{
    return link == 0 ? 0 : link->self_node;
}

i32 ReadHandle(const void *node)
{
    return static_cast<const VmRecord *>(node)->entity_id;
}

void WriteHandle(void *node, i32 handle)
{
    static_cast<VmRecord *>(node)->entity_id = handle;
}

u32 &NodeFlags(void *node)
{
    return static_cast<VmRecord *>(node)->flags;
}

u16 &NodeKind(void *node)
{
    return static_cast<VmRecord *>(node)->state_word;
}

u32 &NodeKindField(void *node)
{
    return static_cast<VmRecord *>(node)->render_kind;
}

void *FindNodeByHandle(const void *owner, i32 handle)
{
    RenderOwnerAccess access(owner);
    for (OwnerLink *link = access.first_list_a(); link != 0; link = link->next) {
        void *const node = NodeFromLink(link);
        if (ReadHandle(node) == handle)
            return node;
    }
    for (OwnerLink *link = access.second_list_head(); link != 0; link = link->next) {
        void *const node = NodeFromLink(link);
        if (ReadHandle(node) == handle)
            return node;
    }
    return 0;
}

// TH10 0x004492a0: soft release. Sets the recycling flag bit 26
// (0x4000000) on the record and on every direct child while the container
// field at +0x18 is zero.
void MarkNodeForRelease(void *node)
{
    NodeFlags(node) |= 0x4000000U;
    VmRecord &vm = *reinterpret_cast<VmRecord *>(node);
    if (vm.parent_link != 0)
        return;
    for (OwnerLink *link = static_cast<OwnerLink *>(vm.first_child);
         link != 0; link = link->next) {
        NodeFlags(NodeFromLink(link)) |= 0x4000000U;
    }
}

void MarkNodeKind(void *node, u16 kind)
{
    NodeKind(node) = kind;
    VmRecord &vm = *reinterpret_cast<VmRecord *>(node);
    if (vm.parent_link != 0)
        return;
    for (OwnerLink *link = static_cast<OwnerLink *>(vm.first_child);
         link != 0; link = link->next) {
        NodeKind(NodeFromLink(link)) = kind;
    }
}

u32 WrapPoolCursor(u32 value)
{
    u32 wrapped = (value + 1U) & 0x80000fffU;
    if (static_cast<i32>(wrapped) < 0)
        wrapped = ((wrapped - 1U) | 0xfffff000U) + 1U;
    return wrapped;
}

void *AllocateRenderOwnerNode(void *owner)
{
    RenderOwnerAccess access(owner);
    u32 cursor = *access.pool_cursor();
    const u32 index = cursor;
    *access.pool_cursor() = WrapPoolCursor(cursor);

    if (*access.pool_active(index) != 0) {
        *access.pool_cursor() = WrapPoolCursor(*access.pool_cursor());
        if (*access.pool_active(index) != 0) {
            void *const node = AllocateAsciiManagerMemory(0x3ac);
            if (node == 0)
                return 0;
            memset(node, 0, 0x3ac);
            ResetPooledRenderOwnerNodeEsiEdiAbi(owner, node);
            return node;
        }
    }

    *access.pool_active(index) = 1;
    void *const node = access.pool_node(index);
    *access.pool_cursor() = WrapPoolCursor(*access.pool_cursor());
    return node;
}

void RegisterRenderOwnerNode(void *owner, void *node)
{
    RenderOwnerAccess access(owner);
    // The intrusive list node is the record's {link_self, link_next,
    // link_prev} trio at record+4.
    VmRecord &vm = *reinterpret_cast<VmRecord *>(node);
    vm.link_self = node;
    vm.link_next = 0;
    vm.link_prev = 0;
    if (access.first_list_a() == 0) {
        access.base->first_list_a =
            reinterpret_cast<OwnerLink *>(&vm.link_self);
    } else {
        OwnerLink *const tail = *access.last_list_a();
        VmRecord &tail_vm = *reinterpret_cast<VmRecord *>(
            reinterpret_cast<u8 *>(tail) - 0x04);
        if (tail_vm.link_next != 0) {
            vm.link_next = tail_vm.link_next;
            reinterpret_cast<VmRecord *>(
                static_cast<u8 *>(tail_vm.link_next) - 0x04)
                ->link_prev = &vm.link_self;
        }
        tail_vm.link_next = &vm.link_self;
        vm.link_prev = &tail_vm.link_self;
    }
    *access.last_list_a() = reinterpret_cast<OwnerLink *>(&vm.link_self);

    i32 handle = *access.next_handle() + 1;
    *access.next_handle() = handle;
    if (*access.next_handle() == 0)
        *access.next_handle() = handle + 1;
    WriteHandle(node, *access.next_handle());
}

void *CreateRegisteredRenderOwnerNode(void *owner)
{
    void *const node = AllocateRenderOwnerNode(owner);
    if (node == 0)
        return 0;
    RegisterRenderOwnerNode(owner, node);
    return node;
}

void ZeroRenderObjectVectors(void *node)
{
    VmRecord &vm = *reinterpret_cast<VmRecord *>(node);
    vm.delta_pos_x = 0.0f;
    vm.delta_pos_y = 0.0f;
    vm.delta_pos_z = 0.0f;
    vm.base_pos_x = 0.0f;
    vm.base_pos_y = 0.0f;
    vm.base_pos_z = 0.0f;
    vm.alt_pos_x = 0.0f;
    vm.alt_pos_y = 0.0f;
    vm.alt_pos_z = 0.0f;
}

void ClearRenderObjectNode(void *node)
{
    u32 *words = static_cast<u32 *>(node);
    for (i32 i = 0; i != 0xeb; ++i)
        words[i] = 0;
}

void InitializeRenderObjectTimerBlock(void *node)
{
    VmRecord &vm = *reinterpret_cast<VmRecord *>(node);
    u32 flags = vm.timer_flags;
    if ((flags & 1U) == 0) {
        vm.timer_cur = 0;
        vm.timer_prev = static_cast<i32>(0xfff0bdc1U);
        vm.timer_accum = 0.0f;
        vm.timer_rate = &g_MainChainStartupScale;
        vm.timer_flags = flags | 1U;
    }
    vm.timer_cur = 0;
    vm.timer_accum = 0.0f;
    vm.timer_prev = -1;
    NodeFlags(node) &= ~1U;
}

bool BindTimelineRenderObjectFromManagerWork(ManagerWorkPartial *manager_work,
                                             u32 clone_index, void *node)
{
    VmRecord &vm = *reinterpret_cast<VmRecord *>(node);
    void **const table = reinterpret_cast<void **>(
        manager_work->output_pointer_list_011c);
    void *const table_entry = table[clone_index];
    if (table_entry == 0)
        return false;
    if (manager_work->active_cursor != 0)
        return false;

    vm.bound_script_id = static_cast<u16>(clone_index);
    vm.bound_file_id = *reinterpret_cast<const u16 *>(manager_work);
    vm.bound_resource = manager_work;
    NodeFlags(node) &= ~0x600U;
    vm.script_base = table_entry;
    vm.current_instruction = table_entry;
    InitializeRenderObjectTimerBlock(node);
    FinalizeTimelineRenderObjectSetup(node);
    ++(*RenderOwnerAccess(g_MainChainRenderOwner).live_object_count());
    return true;
}

void ApplyTimelineRenderObjectClone(void *destination, ManagerWorkPartial *source,
                                    u16 clone_field)
{
    VmRecord &vm = *reinterpret_cast<VmRecord *>(destination);
    ZeroRenderObjectVectors(destination);
    NodeFlags(destination) |= 0x40000000U;
    vm.anim_field_3a0 = 0x10;
    vm.anim_field_3a1 = 0x10;
    if (!BindTimelineRenderObjectFromManagerWork(source, clone_field, destination))
        ClearRenderObjectNode(destination);
}

void BindTimelineContinuationFromManagerWork(void *manager_work, u32 kind,
                                             void *node)
{
    VmRecord &vm = *reinterpret_cast<VmRecord *>(node);
    ManagerWorkPartial *const work =
        reinterpret_cast<ManagerWorkPartial *>(manager_work);
    void **const table = reinterpret_cast<void **>(
        work->output_pointer_list_011c);
    void *const table_entry = table[kind];
    if (table_entry == 0 || work->active_cursor != 0) {
        ClearRenderObjectNode(node);
        return;
    }

    ResetPooledRenderOwnerNodeEsiEdiAbi(g_MainChainRenderOwner, node);
    vm.bound_script_id = static_cast<u16>(kind);
    vm.bound_file_id = *reinterpret_cast<const u16 *>(manager_work);
    vm.bound_resource = manager_work;
    NodeFlags(node) &= ~0x600U;
    vm.script_base = table_entry;
    vm.current_instruction = table_entry;
    InitializeRenderObjectTimerBlock(node);
    FinalizeTimelineRenderObjectSetup(node);
    ++(*RenderOwnerAccess(g_MainChainRenderOwner).live_object_count());
}

void ConfigureTimelineContinuationRenderObject(void *node, void *manager_work,
                                               u32 kind, const float params[3])
{
    VmRecord &vm = *reinterpret_cast<VmRecord *>(node);
    vm.delta_pos_x = params[0];
    vm.delta_pos_y = params[1];
    vm.delta_pos_z = params[2];
    BindTimelineContinuationFromManagerWork(manager_work, kind, node);
}

// Shared body of the four native setup-script spawn creators
// (0x448d00/0x448e30/0x448f60/0x449090): allocate a 0x3ac pool VM record
// through the global render owner, publish kind + the 0x40000000 flag,
// bind the setup script, then register the record through the selected
// list helper (the id lands on record+0). The native first stack argument
// is dead; only the script index and kind are consumed.
void *SpawnSetupEffectVm(i32 script_id, u32 kind,
                         void (*link)(u32 *, void *))
{
    u8 *const vm = static_cast<u8 *>(
        AllocatePoolVmEsiAbi(g_MainChainRenderOwner));
    NodeKindField(vm) = kind;
    NodeFlags(vm) |= 0x40000000U;
    AssignPoolVmScriptEcxEaxAbi(vm, script_id);
    u32 id = 0;
    link(&id, vm);
    return vm;
}

void LinkEffectVmToListBBackLocal(u32 *out_id, void *vm)
{
    AttachEffectVmToListB(out_id, vm, g_MainChainRenderOwner);
}

void LinkEffectVmToListBFrontLocal(u32 *out_id, void *vm)
{
    AttachEffectVmToListBFront(out_id, vm, g_MainChainRenderOwner);
}

} // namespace

void *ResolveTimelineHandle(void *owner, i32 handle)
{
    if (handle == 0)
        return 0;
    return FindNodeByHandle(owner, handle);
}

void *RefreshTimelineTextHandle(void **slot)
{
    void *const node = ResolveTimelineHandle(g_MainChainRenderOwner,
                                             reinterpret_cast<i32>(*slot));
    if (node == 0)
        *slot = 0;
    return node;
}

void ReleaseTimelineHandle(void *owner, i32 handle)
{
    void *const node = ResolveTimelineHandle(owner, handle);
    if (node == 0)
        return;
    MarkNodeForRelease(node);
}

void SetTimelineObjectPhase(void **slot)
{
    ReleaseTimelineHandle(g_MainChainRenderOwner,
                          reinterpret_cast<i32>(*slot));
    *slot = 0;
}

void ReleaseTimelineContinuationHandle(i32 *handle_slot)
{
    void *const node = ResolveTimelineHandle(g_MainChainRenderOwner,
                                             *handle_slot);
    if (node == 0)
        return;
    MarkNodeKind(node, 1);
}

const char *DecryptTimelineRecordText(const u8 *payload)
{
    u8 key = 0x77;
    signed char step = 7;
    char *dst = g_TimelineDecryptScratch;
    const u8 *src = payload;
    u8 value;
    do {
        value = static_cast<u8>(*src ^ key);
        *dst++ = static_cast<char>(value);
        key = static_cast<u8>(key + step);
        step = static_cast<signed char>(step + 0x10);
        ++src;
    } while (value != 0);
    return g_TimelineDecryptScratch;
}

i32 *SpawnSetupEffectVmListABack(i32 script_id, u32 kind)
{
    return static_cast<i32 *>(
        SpawnSetupEffectVm(script_id, kind, LinkEntityAndAssignIdEaxEsiAbi));
}

i32 *SpawnSetupEffectVmListAFront(i32 script_id, u32 kind)
{
    return static_cast<i32 *>(
        SpawnSetupEffectVm(script_id, kind,
                           LinkEntityFrontAndAssignIdEaxEsiAbi));
}

i32 *SpawnSetupEffectVmListBBack(i32 script_id, u32 kind)
{
    return static_cast<i32 *>(
        SpawnSetupEffectVm(script_id, kind, LinkEffectVmToListBBackLocal));
}

i32 *SpawnSetupEffectVmListBFront(i32 script_id, u32 kind)
{
    return static_cast<i32 *>(
        SpawnSetupEffectVm(script_id, kind, LinkEffectVmToListBFrontLocal));
}

void *CreateTimelineContinuationRenderObject(void *manager_work, u32 kind,
                                             const float params[3])
{
    void *const node = CreateRegisteredRenderOwnerNode(g_MainChainRenderOwner);
    if (node == 0)
        return 0;
    NodeFlags(node) |= 0x40000000U;
    NodeKindField(node) = 0;
    ConfigureTimelineContinuationRenderObject(node, manager_work, kind, params);
    return node;
}

void ApplyTimelineRenderObjectPresetClone(ManagerWorkPartial *work, void *node,
                                          u16 clone_field)
{
    VmRecord &vm = *reinterpret_cast<VmRecord *>(node);
    ZeroRenderObjectVectors(node);
    NodeFlags(node) |= 0x40000000U;
    vm.anim_field_3a0 = 0x10;
    vm.anim_field_3a1 = 0x10;
    vm.bound_script_id = clone_field;
    if (!BindTimelineRenderObjectFromManagerWork(work, clone_field, node))
        ClearRenderObjectNode(node);
}

i32 CreateTimelinePresetTextSlotNode(void *owner, ManagerWorkPartial *text_work,
                                     u16 clone_field)
{
    void *const node = AllocateRenderOwnerNode(owner);
    if (node == 0)
        return 0;
    NodeKindField(node) = 0xf;
    NodeFlags(node) |= 0x40000000U;
    ApplyTimelineRenderObjectPresetClone(text_work, node, clone_field);
    RegisterRenderOwnerNode(owner, node);
    return ReadHandle(node);
}

} // namespace th10
