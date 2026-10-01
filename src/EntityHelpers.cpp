#include "EntityHelpers.hpp"

#include "AsciiAnimationVm.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "VmRecord.hpp"
#include <string.h>

namespace th10 {

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

const u32 kSoftReleaseFlag = 0x4000000U; // entity+0x35c bit 26

inline void WriteUint(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

enum ChildMutation {
    ChildMutation_SoftRelease,
    ChildMutation_StateWord,
    ChildMutation_PositionDirect,
    ChildMutation_PositionOffset
};

// Propagates one of the mutations to every entity on the child list at
// +0x14 while the container field at +0x18 is zero; the original has no
// explicit bounds on the walk.
void PropagateToChildren(u8 *entity, ChildMutation mutation, u16 state_word,
                         const float *position)
{
    VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
    if (vm.parent_link != 0)
        return;
    const u32 *child = static_cast<u32 *>(vm.first_child);
    for (; child != 0; child = reinterpret_cast<const u32 *>(child[1])) {
        u8 *const child_entity = reinterpret_cast<u8 *>(child[0]);
        if (child_entity == 0)
            continue;
        VmRecord &child_vm = *reinterpret_cast<VmRecord *>(child_entity);
        switch (mutation) {
        case ChildMutation_SoftRelease:
            child_vm.flags |= kSoftReleaseFlag;
            break;
        case ChildMutation_StateWord:
            child_vm.state_word = state_word;
            break;
        case ChildMutation_PositionDirect:
            child_vm.delta_pos_x = position[0];
            child_vm.delta_pos_y = position[1];
            child_vm.delta_pos_z = position[2];
            break;
        case ChildMutation_PositionOffset:
            child_vm.delta_pos_x = position[0] + 224.0f;
            child_vm.delta_pos_y = position[1] + 16.0f;
            child_vm.delta_pos_z = position[2];
            break;
        }
    }
}

} // namespace

// TH10 0x00449470.
void SetEntityStateWordEaxEsiAbi(u32 *id_slot, i32 value)
{
    u8 *const entity = static_cast<u8 *>(
        FindEntityEdxStackAbi(g_MainChainRenderOwner, *id_slot));
    if (entity == 0)
        return;
    VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
    vm.state_word = static_cast<u16>(value);
    PropagateToChildren(entity, ChildMutation_StateWord,
                        static_cast<u16>(value), 0);
}

// TH10 0x00449950. Pool records live at manager+0x68 + index*0x3ac with
// used-flag bytes at manager+0x3ac068 and the cursor at manager+0x3ad068
// (4096 slots). The cursor advances past the accepted slot; a heap
// fallback does not set the used flag but still advances the cursor.
void *AllocatePoolVmEsiAbi(void *manager_memory)
{
    LargeRenderOwnerLayout &owner =
        *static_cast<LargeRenderOwnerLayout *>(manager_memory);
    u32 index = owner.pool_cursor % 4096U;
    u8 *vm = reinterpret_cast<u8 *>(&owner.pooled_nodes[index]);
    if (owner.pooled_node_active[index] != 0) {
        index = (index + 1) % 4096U;
        owner.pool_cursor = index;
        vm = reinterpret_cast<u8 *>(&owner.pooled_nodes[index]);
        if (owner.pooled_node_active[index] != 0) {
            // Heap fallback: blank-construct then reset. The original
            // calls the reset even for a null allocation.
            u8 *AllocateHeapBlock(u32 bytes); // 0x452493
            vm = AllocateHeapBlock(0x3ac);
            if (vm != 0) {
                memset(vm, 0, 0x3ac);
                static const u32 kLatchOffsets[9] = {
                    0x6c, 0xb0, 0xfc, 0x128, 0x174, 0x1b0, 0x1fc,
                    0x228, 0x378};
                for (u32 i = 0; i != 9; ++i)
                    *reinterpret_cast<u8 *>(vm + kLatchOffsets[i]) &=
                        0xfe;
                VmRecord &rec = *reinterpret_cast<VmRecord *>(vm);
                rec.sprite_entry_id = 0xffffU;
            }
            ResetAsciiAnimationVmRecord(vm);
            owner.pool_cursor = (owner.pool_cursor + 1) % 4096U;
            return vm;
        }
    }
    owner.pooled_node_active[index] = 1;
    owner.pool_cursor = (owner.pool_cursor + 1) % 4096U;
    return vm;
}

// TH10 0x00449870. Zero-list {0x334, 0x338, 0x33c, 0x340, 0x344, 0x348,
// 0x34c, 0x350, 0x354}, flag 0x40000000, script id u16 at +0x38a,
// animation fields 0x10 at +0x3a0/+0x3a1, then the script-bind boundary.
void AssignPoolVmScriptEcxEaxAbi(void *vm_memory, i32 script_id)
{
    u8 *const vm = static_cast<u8 *>(vm_memory);
    VmRecord &rec = *reinterpret_cast<VmRecord *>(vm);
    static const u32 kClearedOffsets[9] = {
        0x340, 0x344, 0x348, 0x334, 0x338, 0x33c, 0x34c, 0x350, 0x354};
    for (u32 index = 0; index != 9; ++index)
        WriteUint(vm, kClearedOffsets[index], 0);
    rec.flags |= 0x40000000U;
    rec.bound_script_id = static_cast<u16>(script_id);
    rec.anim_field_3a0 = 0x10;
    rec.anim_field_3a1 = 0x10;
    extern void *g_EffectScriptContext; // runtime-filled bind context
    BindEffectScriptContextEaxEcxDxAbi(g_EffectScriptContext, script_id,
                                       vm);
}

// TH10 0x004489d0. The node is entity+4 ({entity, next, prev}); the link
// goes to list 1 only, handling a tail whose next is non-empty, and the
// id counter at manager+0x732454 skips zero by wrapping to 1.
void LinkEntityAndAssignIdEaxEsiAbi(u32 *out_id, void *entity_memory)
{
    LargeRenderOwnerLayout &owner =
        *static_cast<LargeRenderOwnerLayout *>(g_MainChainRenderOwner);
    u8 *const entity = static_cast<u8 *>(entity_memory);
    OwnerLink *const node = reinterpret_cast<OwnerLink *>(entity + 4);
    node->self_node = entity;
    node->next = 0;
    node->previous = 0;
    if (owner.first_list_a == 0) {
        owner.first_list_a = node;
    } else {
        OwnerLink *const last = owner.last_list_a;
        OwnerLink *const last_next = last->next;
        if (last_next != 0) {
            node->next = last_next;
            last_next->previous = node;
        }
        last->next = node;
        node->previous = last;
    }
    owner.last_list_a = node;

    owner.node_id_counter += 1;
    if (owner.node_id_counter == 0)
        owner.node_id_counter = 1;
    reinterpret_cast<VmRecord *>(entity)->entity_id =
        static_cast<i32>(owner.node_id_counter);
    *out_id = static_cast<u32>(owner.node_id_counter);
}

// TH10 0x00448a50. List-A front-insertion twin of 0x4489d0: same node
// layout and id counter, but the new node is prepended at +0x72dad4 and
// the tail (+0x72dad8) is only touched when the list was empty.
void LinkEntityFrontAndAssignIdEaxEsiAbi(u32 *out_id, void *entity_memory)
{
    LargeRenderOwnerLayout &owner =
        *static_cast<LargeRenderOwnerLayout *>(g_MainChainRenderOwner);
    u8 *const entity = static_cast<u8 *>(entity_memory);
    OwnerLink *const node = reinterpret_cast<OwnerLink *>(entity + 4);
    node->self_node = entity;
    node->next = 0;
    node->previous = 0;
    if (owner.first_list_a == 0) {
        owner.last_list_a = node;
    } else {
        node->next = owner.first_list_a;
        owner.first_list_a->previous = node;
    }
    owner.first_list_a = node;

    owner.node_id_counter += 1;
    if (owner.node_id_counter == 0)
        owner.node_id_counter = 1;
    reinterpret_cast<VmRecord *>(entity)->entity_id =
        static_cast<i32>(owner.node_id_counter);
    *out_id = static_cast<u32>(owner.node_id_counter);
}

// TH10 0x004491c0. Walks the active list then the secondary list, matching
// the entity id stored at entity+0; returns null for id 0 or a miss.
u8 *FindEntityEdxStackAbi(void *manager_memory, u32 id)
{
    if (id == 0)
        return 0;
    const LargeRenderOwnerLayout &owner =
        *static_cast<const LargeRenderOwnerLayout *>(manager_memory);
    const OwnerLink *const list_heads[2] = {owner.first_list_a,
                                            owner.first_list_b};
    for (u32 list_index = 0; list_index != 2; ++list_index) {
        for (const OwnerLink *node = list_heads[list_index]; node != 0;
             node = node->next) {
            u8 *const entity = static_cast<u8 *>(node->self_node);
            if (entity != 0 && reinterpret_cast<const VmRecord *>(entity)
                                   ->entity_id == id)
                return entity;
        }
    }
    return 0;
}

// TH10 0x004492a0. Soft release, keeping the entity for recycling.
void ReleaseEntityById(void *manager, u32 id)
{
    u8 *const entity =
        static_cast<u8 *>(FindEntityEdxStackAbi(manager, id));
    if (entity == 0)
        return;
    VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
    vm.flags |= kSoftReleaseFlag;
    PropagateToChildren(entity, ChildMutation_SoftRelease, 0, 0);
}

// TH10 0x004492f0. Publishes the position verbatim.
void SetEntityPositionDirectEsiAbi(void *manager, u32 id,
                                   const float position[3])
{
    u8 *const entity =
        static_cast<u8 *>(FindEntityEdxStackAbi(manager, id));
    if (entity == 0)
        return;
    VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
    vm.delta_pos_x = position[0];
    vm.delta_pos_y = position[1];
    vm.delta_pos_z = position[2];
    PropagateToChildren(entity, ChildMutation_PositionDirect, 0, position);
}

// TH10 0x00449350. Publishes the position with the game-area offset.
void SetEntityPositionOffsetEsiAbi(void *manager, u32 id,
                                   const float position[3])
{
    u8 *const entity =
        static_cast<u8 *>(FindEntityEdxStackAbi(manager, id));
    if (entity == 0)
        return;
    VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
    vm.delta_pos_x = position[0] + 224.0f;
    vm.delta_pos_y = position[1] + 16.0f;
    vm.delta_pos_z = position[2];
    PropagateToChildren(entity, ChildMutation_PositionOffset, 0, position);
}

// Shared core of the two handle-based state-word helpers (0x409e50 /
// 0x0040c4d0): the manager is the global render owner inside these
// callees; unresolved handles are silently ignored.
static void SetEntityStateWordFromHandle(u32 *handle, u16 value)
{
    u8 *const entity = static_cast<u8 *>(
        FindEntityEdxStackAbi(g_MainChainRenderOwner, *handle));
    if (entity == 0)
        return;
    VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
    vm.state_word = value;
    PropagateToChildren(entity, ChildMutation_StateWord, value, 0);
}

// TH10 0x00449210.
void StopEntityById(void *manager, u32 id)
{
    u8 *const entity =
        static_cast<u8 *>(FindEntityEdxStackAbi(manager, id));
    if (entity == 0)
        return;
    VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
    vm.state_word = 1;
    PropagateToChildren(entity, ChildMutation_StateWord, 1, 0);
}

// TH10 0x00409e50.
void ExpireEntityHandleEaxAbi(u32 *handle)
{
    SetEntityStateWordFromHandle(handle, 1);
}

// TH10 0x0040c4d0.
void FireEntityHandleEaxAbi(u32 *handle)
{
    SetEntityStateWordFromHandle(handle, 3);
}

// TH10 0x00428d70. The box is outside when it does not intersect the fixed
// playfield rect; all NaN comparisons take the outside branch.
i32 IsOutsidePlayfieldBox(const float position[2], float half_x,
                          float half_y)
{
    return position[0] + half_x <= -192.0f ||
           position[0] - half_x >= 192.0f ||
           position[1] + half_y <= 0.0f ||
           position[1] - half_y >= 448.0f
        ? 1
        : 0;
}


// TH10 0x004493e0. Native EAX = manager, EDX = resource pointer.
void *ReleaseEntitiesUsingResourceEaxEdxAbi(void *manager, u32 resource)
{
    LargeRenderOwnerLayout &owner =
        *static_cast<LargeRenderOwnerLayout *>(manager);
    OwnerLink *const list_heads[2] = {owner.first_list_a,
                                      owner.first_list_b};
    void *last_node = 0;
    for (u32 i = 0; i < 2U; ++i) {
        OwnerLink *node = list_heads[i];
        while (node != 0) {
            OwnerLink *const next = node->next;
            u8 *const entity = static_cast<u8 *>(node->self_node);
            VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
            if (vm.bound_resource == reinterpret_cast<void *>(resource)) {
                vm.flags |= 0x4000000U;
            }
            node = next;
        }
        last_node = node;
    }
    return last_node;
}

} // namespace th10
