#include "MainChainRender.hpp"

#include <string.h>

#include "LargeRenderOwnerFrameLoop.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "CallbackScheduler.hpp"

namespace th10 {

namespace {

// The pooled records are the canonical VmRecord layout; +0x358 is the owned
// vertex-buffer pointer and +0x4 is the intrusive OwnerLink.
typedef VmRecord OwnerNode;

inline OwnerLink *NodeLink(OwnerNode *node)
{
    return reinterpret_cast<OwnerLink *>(&node->link_self);
}

struct ResetSensitiveRenderTargetSlots {
    u8 unknown_0000[0x3ad4e0];
    void *render_targets[32];
};

typedef char AssertResetSensitiveRenderTargetOffset[
    offsetof(ResetSensitiveRenderTargetSlots, render_targets) == 0x3ad4e0
        ? 1 : -1];

extern void ReleaseLargeRenderOwnerBuffer(void *pointer); // TH10 0x00452422
extern void FreeLargeRenderOwnerNode(void *node); // TH10 0x004524a1
extern void ReleaseLargeRenderOwnerComObject(void *object); // IUnknown::Release
extern void ResetPooledRenderOwnerNodeEsiEdiAbi(
    LargeRenderOwnerLayout *owner, VmRecord *node); // TH10 0x00401de0
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern D3D9Device *g_MainChainD3D9Device; // TH10 DAT_00491c30
extern ChainCallback GetLargeRenderOwnerCallback(u32 index);

typedef i32 (TH10_STDCALL *D3DSetVertexShaderFn)(D3D9Device *, void *);

const i32 kLargeRenderCallbackPriorities[20] = {
    26, 8, 9, 11, 13, 15, 16, 17, 18, 19,
    21, 23, 24, 26, 28, 33, 36, 41, 42, 45
};

void ReleaseNodeBuffer(OwnerNode *node)
{
    if (node->vertex_buffer != 0)
        ReleaseLargeRenderOwnerBuffer(node->vertex_buffer);
    node->vertex_buffer = 0;
}

void DestroyOwnerNode(LargeRenderOwnerLayout *owner, OwnerNode *node)
{
    OwnerLink *const link = NodeLink(node);
    if (link == owner->last_list_a)
        owner->last_list_a = link->previous;
    if (link == owner->first_list_a)
        owner->first_list_a = link->next;
    if (link == owner->last_list_b)
        owner->last_list_b = link->previous;
    if (link == owner->first_list_b)
        owner->first_list_b = link->next;

    if (link->next != 0)
        link->next->previous = link->previous;
    if (link->previous != 0)
        link->previous->next = link->next;
    link->next = 0;
    link->previous = 0;

    OwnerNode *const first_pool = &owner->pooled_nodes[0];
    OwnerNode *const end_pool = first_pool + 0x1000;
    if (node >= first_pool && node < end_pool) {
        const u32 index = static_cast<u32>(node - first_pool);
        owner->pooled_node_active[index] = 0;
        ReleaseNodeBuffer(node);
        ResetPooledRenderOwnerNodeEsiEdiAbi(owner, node);
        return;
    }

    ReleaseNodeBuffer(node);
    FreeLargeRenderOwnerNode(node);
}

void DestroyOwnerList(LargeRenderOwnerLayout *owner, OwnerLink *first)
{
    for (OwnerLink *link = first; link != 0; ) {
        OwnerNode *const node = static_cast<OwnerNode *>(link->self_node);
        OwnerLink *const next = link->next;
        DestroyOwnerNode(owner, node);
        link = next;
    }
}

void DestroyOwnerNodeVectorReverse(OwnerNode *nodes, u32 count)
{
    while (count != 0) {
        --count;
        ReleaseNodeBuffer(&nodes[count]);
    }
}

void RegisterLargeRenderOwnerCallbacks(LargeRenderOwnerLayout *owner)
{
    for (u32 index = 0; index != 20; ++index) {
        ChainElem *const record = CallbackSchedulerApi::Create(
            GetLargeRenderOwnerCallback(index));
        record->flags |= ChainElemFlag_Enabled;
        record->arg = owner;
        if (index < 2) {
            (void)CallbackSchedulerApi::AddToCalculationChain(
                g_CallbackScheduler, record, kLargeRenderCallbackPriorities[index]);
        } else {
            (void)CallbackSchedulerApi::AddToDrawChain(
                g_CallbackScheduler, record, kLargeRenderCallbackPriorities[index]);
        }
    }
}

} // namespace

void SetLargeRenderGlobalDefaults()
{
}

void DestroyLargeRenderOwnerInPlace(void *opaque_owner)
{
    LargeRenderOwnerLayout *const owner =
        static_cast<LargeRenderOwnerLayout *>(opaque_owner);
    DestroyOwnerList(owner, owner->first_list_a);
    DestroyOwnerList(owner, owner->first_list_b);
    DestroyOwnerNodeVectorReverse(owner->late_nodes, 0x14);

    if (owner->owned_3ad488 != 0)
        ReleaseLargeRenderOwnerBuffer(owner->owned_3ad488);
    owner->owned_3ad488 = 0;
    DestroyOwnerNodeVectorReverse(owner->pooled_nodes, 0x1000);
}

void *ConstructLargeRenderOwner(void *opaque_owner)
{
    LargeRenderOwnerLayout *const owner =
        static_cast<LargeRenderOwnerLayout *>(opaque_owner);

    // The native CRT constructs both node arrays before zeroing the entire
    // owner; normal-path state starts with this explicit whole-object clear.
    memset(owner, 0, sizeof(*owner));
    SetLargeRenderGlobalDefaults();
    *reinterpret_cast<i32 *>(static_cast<u8 *>(opaque_owner) + 0x0) = -1;
    *reinterpret_cast<i32 *>(static_cast<u8 *>(opaque_owner) + 0x4) = -1;
    *reinterpret_cast<u32 *>(static_cast<u8 *>(opaque_owner) + 0x3ada60) = 1;
    *(static_cast<u8 *>(opaque_owner) + 0x3ada6c) = 0xff;

    for (u32 index = 0; index != 0x1000; ++index)
        ResetPooledRenderOwnerNodeEsiEdiAbi(owner, &owner->pooled_nodes[index]);

    RegisterLargeRenderOwnerCallbacks(owner);
    (void)reinterpret_cast<D3DSetVertexShaderFn>(
        g_MainChainD3D9Device->vtable[92])(g_MainChainD3D9Device, 0);
    return owner;
}

void ReleaseResetSensitiveRenderSlots(void *owner)
{
    void **const slots = reinterpret_cast<ResetSensitiveRenderTargetSlots *>(
        owner)->render_targets;
    for (u32 index = 0; index != 32; ++index) {
        if (slots[index] != 0) {
            ReleaseLargeRenderOwnerComObject(slots[index]);
            slots[index] = 0;
        }
    }
}

} // namespace th10
