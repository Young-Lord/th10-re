#include "LargeRenderOwnerFrameLoop.hpp"

#include "AsciiAnimationVm.hpp"
#include "AsciiRenderModeDispatcher.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "MainChainRender.hpp"
#include "TimelineRenderObjectSetup.hpp"

namespace th10 {

namespace {

// The pooled/bucket records are the canonical VmRecord layout; the local
// view names map onto: owner_link@0x4, child_head/first_child@0x10/0x14,
// chain_next@0x1c, render_kind@0x20, vertex_buffer@0x358, flags@0x35c,
// frame_callback@0x398, render_callback@0x39c.
typedef VmRecord OwnerNode;

inline OwnerLink *NodeLink(OwnerNode *node)
{
    return reinterpret_cast<OwnerLink *>(&node->link_self);
}

// The shared layout hosts the 19 kind-bucket sentinels in late_nodes[0..0x12]
// (the chain head is each sentinel's chain_next at +0x1c).
extern void ReleaseLargeRenderOwnerBuffer(void *pointer);
extern void FreeLargeRenderOwnerNode(void *node);
extern D3D9Device *g_MainChainD3D9Device;

float g_LargeRenderOwnerMirroredGlobals[22]; // TH10 DAT_00497930..8c

typedef i32 (TH10_STDCALL *D3DSetRenderStateFn)(D3D9Device *, u32, u32);
typedef i32 (TH10_STDCALL *D3DSetTransformFn)(D3D9Device *, u32, const void *);

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

void UnlinkOwnerLink(LargeRenderOwnerLayout *owner, OwnerLink *link)
{
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
}

void UnlinkChildLinks(OwnerNode *node)
{
    OwnerLink *const head = static_cast<OwnerLink *>(node->child_head);
    OwnerLink *const tail = static_cast<OwnerLink *>(node->first_child);
    if (head != 0)
        head->next = tail;
    if (tail != 0)
        tail->previous = head;
    node->child_head = 0;
    node->first_child = 0;
}

bool OwnerNodeIsPooled(LargeRenderOwnerLayout *owner, OwnerNode *node)
{
    u8 *const base = reinterpret_cast<u8 *>(owner);
    u8 *const node_bytes = reinterpret_cast<u8 *>(node);
    return node_bytes >= base + 0x68 && node_bytes < base + 0x3ac068;
}

u32 PooledNodeIndex(LargeRenderOwnerLayout *owner, OwnerNode *node)
{
    return static_cast<u32>(
        (reinterpret_cast<u8 *>(node) - (reinterpret_cast<u8 *>(owner) + 0x68)) /
        0x3ac);
}

void ReleaseOwnerNodeOwnedBuffer(OwnerNode *node)
{
    if (node->vertex_buffer != 0)
        ReleaseLargeRenderOwnerBuffer(node->vertex_buffer);
    node->vertex_buffer = 0;
}

void DestroyOwnerNodeInternal(LargeRenderOwnerLayout *owner, OwnerNode *node)
{
    UnlinkOwnerLink(owner, NodeLink(node));
    UnlinkChildLinks(node);
    ReleaseOwnerNodeOwnedBuffer(node);

    if (OwnerNodeIsPooled(owner, node)) {
        u8 *const active =
            reinterpret_cast<u8 *>(owner) + 0x3ac068 +
            PooledNodeIndex(owner, node);
        *active = 0;
        ResetAsciiAnimationVmRecord(node);
        return;
    }

    FreeLargeRenderOwnerNode(node);
}

void ResetKindBucketHeads(LargeRenderOwnerLayout *owner, OwnerNode **heads)
{
    for (u32 index = 0; index != 0x13; ++index) {
        heads[index] = &owner->late_nodes[index];
        owner->late_nodes[index].chain_next = 0;
    }
}

void ProcessOwnerListNode(LargeRenderOwnerLayout *owner, OwnerNode *node,
                          OwnerNode **heads)
{
    if ((node->flags & 0x04000000U) != 0) {
        DestroyOwnerNodeInternal(owner, node);
        return;
    }

    if (node->frame_callback != 0) {
        typedef void (TH10_FASTCALL *PreUpdateFn)(void *);
        reinterpret_cast<PreUpdateFn>(node->frame_callback)(node);
    }

    if (FinalizeTimelineRenderObjectSetup(node) != 0) {
        DestroyOwnerNodeInternal(owner, node);
        return;
    }

    OwnerNode *const previous_head = heads[node->render_kind];
    previous_head->chain_next = node;
    heads[node->render_kind] = node;
    node->chain_next = 0;
}

} // namespace

void DestroyRenderOwnerNodeOnSetupFailure(void *opaque_owner, void *opaque_node)
{
    DestroyOwnerNodeInternal(static_cast<LargeRenderOwnerLayout *>(opaque_owner),
                             static_cast<OwnerNode *>(opaque_node));
}

i32 UpdateLargeRenderOwnerListA(void *opaque_owner)
{
    LargeRenderOwnerLayout *const owner =
        static_cast<LargeRenderOwnerLayout *>(opaque_owner);
    OwnerNode *heads[0x13];
    ResetKindBucketHeads(owner, heads);

    for (OwnerLink *link = owner->first_list_a; link != 0; link = link->next) {
        ProcessOwnerListNode(owner, static_cast<OwnerNode *>(link->self_node),
                             heads);
        ++owner->frame_counter;
    }
    return 1;
}

i32 UpdateLargeRenderOwnerListB(void *opaque_owner)
{
    LargeRenderOwnerLayout *const owner =
        static_cast<LargeRenderOwnerLayout *>(opaque_owner);
    OwnerNode *chain_tail = reinterpret_cast<OwnerNode *>(
        reinterpret_cast<u8 *>(owner) + 0x7320a8);
    *reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(owner) + 0x7320c4) = 0;
    owner->frame_counter = 0;

    for (OwnerLink *link = owner->first_list_b; link != 0; link = link->next) {
        OwnerNode *const node = static_cast<OwnerNode *>(link->self_node);

        if ((node->flags & 0x04000000U) != 0) {
            DestroyOwnerNodeInternal(owner, node);
            ++owner->frame_counter;
            continue;
        }

        if (node->frame_callback != 0) {
            typedef void (TH10_FASTCALL *PreUpdateFn)(void *);
            reinterpret_cast<PreUpdateFn>(node->frame_callback)(node);
        }

        if (FinalizeTimelineRenderObjectSetup(node) != 0) {
            DestroyOwnerNodeInternal(owner, node);
            ++owner->frame_counter;
            continue;
        }

        chain_tail->chain_next = node;
        chain_tail = node;
        node->chain_next = 0;
        ++owner->frame_counter;
    }
    return 1;
}

i32 DrawLargeRenderOwnerKindChain(void *opaque_owner, u32 kind)
{
    LargeRenderOwnerLayout *const owner =
        static_cast<LargeRenderOwnerLayout *>(opaque_owner);
    u8 *const owner_bytes = reinterpret_cast<u8 *>(owner);
    for (OwnerNode *node = static_cast<OwnerNode *>(
             *reinterpret_cast<void **>(owner_bytes + 0x72db00 +
                                        kind * 0x3ac));
         node != 0; node = static_cast<OwnerNode *>(node->chain_next)) {
        if ((node->flags & 0x04000000U) != 0)
            continue;

        if (node->render_callback != 0) {
            typedef void (TH10_FASTCALL *PreDrawFn)(void *);
            reinterpret_cast<PreDrawFn>(node->render_callback)(node);
        }
        DispatchAsciiAnimationVmRenderMode(node, owner);
    }
    return 1;
}

void InitializeLargeRenderOwner(void *opaque_owner)
{
    u8 *const owner = static_cast<u8 *>(opaque_owner);
    u32 *const block = reinterpret_cast<u32 *>(owner + 0x3ada78);

    block[0] = 0xc3000000U;
    block[1] = 0xc3000000U;
    block[2] = 0;
    block[3] = 0;
    block[4] = 0;
    block[5] = 0x43000000U;
    block[6] = 0;
    block[7] = 0;
    block[8] = 0x3f800000U;
    block[9] = 0xc3000000U;
    block[10] = 0;
    block[11] = 0x43000000U;
    block[12] = 0;
    block[13] = 0;
    block[14] = 0;
    block[15] = 0;
    block[16] = 0x3f800000U;
    block[17] = 0;
    block[18] = 0x43000000U;
    block[19] = 0x43000000U;
    block[20] = 0x3f800000U;
    block[21] = 0x3f800000U;

    for (u32 index = 0; index != 22; ++index)
        *reinterpret_cast<u32 *>(
            reinterpret_cast<u8 *>(g_LargeRenderOwnerMirroredGlobals) +
            index * sizeof(u32)) = block[index];

    (void)reinterpret_cast<D3DSetRenderStateFn>(
        GetD3DSlot(g_MainChainD3D9Device, 57))(
        g_MainChainD3D9Device, 0x50, 0x102);

    void *const viewport_iface = *reinterpret_cast<void **>(owner + 0x3ada74);
    void **const viewport_vtable = *reinterpret_cast<void ***>(viewport_iface);
    typedef void * (TH10_STDCALL *ViewportCreateFn)(void *, u32, u32, const u32 *);
    typedef void (TH10_STDCALL *ViewportReleaseFn)(void *);
    u32 enable = 1;
    void *const viewport_object = reinterpret_cast<ViewportCreateFn>(
        viewport_vtable[0x2c / 4])(viewport_iface, 0, 0, &enable);
    (void)reinterpret_cast<ViewportReleaseFn>(viewport_vtable[0x30 / 4])(
        viewport_object);

    (void)reinterpret_cast<D3DSetTransformFn>(
        GetD3DSlot(g_MainChainD3D9Device, 100))(
        g_MainChainD3D9Device, 0, viewport_iface);
}

} // namespace th10
