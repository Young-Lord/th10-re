// Semantic reconstruction of TH10 0x0040dae0 (in-place destructor of an ECL
// script object) and 0x0040cc50 (its scalar deleting destructor wrapper).
//
// This is the counterpart of CreateEclScriptObjectEaxStackAbi in
// EclScriptLibrary.cpp: the record's embedded list node (the
// work.list_self_0130 / list_next_0134 / list_prev_0138 triple at
// record+0x116c, next at +0x1170, prev at +0x1174) is removed from the
// conditional state's object list, the entity ids published in
// work.published_ids_00c0 (record+0x10fc) are soft-released on the render
// owner, and the player-block back references are dropped.

#include "EclScriptObjectTeardown.hpp"

#include "ConditionalStateObject.hpp"
#include "EclScriptObject.hpp"
#include "EntityHelpers.hpp"
#include "PlayerRecord.hpp"

namespace th10 {

namespace {

extern void FreeMainChainObject(void *object);      // TH10 0x004524a1
extern void *g_MainChainRenderOwner;                // TH10 DAT_00491c10
extern void *g_AsciiHudConditionalState;            // TH10 DAT_00477704
extern void *g_PlayerStateBlock;                    // TH10 DAT_00477834

// Byte-level store for the foreign images that stay RAW: the neighbour
// list nodes and the malloc'd name-list nodes.
void StoreU32To(void *address, u32 value)
{
    u8 *const bytes = static_cast<u8 *>(address);
    bytes[0] = static_cast<u8>(value);
    bytes[1] = static_cast<u8>(value >> 8);
    bytes[2] = static_cast<u8>(value >> 16);
    bytes[3] = static_cast<u8>(value >> 24);
}

} // namespace

// TH10 0x0040dae0. Stack argument = record, ret 4.
void DestroyEclScriptObjectInPlaceStackAbi(void *record)
{
    EclScriptObject &obj = *static_cast<EclScriptObject *>(record);
    EclScriptWork &work = obj.work;

    // Plant the live vtable first (the native destructor prologue restores
    // the final-class vtable before any release work).
    obj.vtable_0000 = reinterpret_cast<void *>(0x46d0c0U);

    // Unlink the embedded node from the conditional state's list. Only the
    // +0x60 count is decremented here even though creation bumps both
    // +0x60 and +0x64 (native quirk; the +0x64 counter is never undone).
    // The native dereferences the DAT_00477704 holder without a null
    // check; the reconstruction keeps that direct read.
    u8 *const state_bytes = static_cast<u8 *>(g_AsciiHudConditionalState);
    ConditionalState &cond =
        *reinterpret_cast<ConditionalState *>(state_bytes);
    void *const node = &work.list_self_0130;
    if (cond.script_list_head_0058 == node) // +0x58
        cond.script_list_head_0058 = work.list_next_0134;
    if (cond.script_list_tail_005c == node) // +0x5c
        cond.script_list_tail_005c = work.list_prev_0138;

    // The neighbour nodes are foreign records' embedded list nodes (raw
    // next at node+4, prev at node+8), so their fields stay RAW.
    if (work.list_next_0134 != 0)
        StoreU32To(static_cast<u8 *>(work.list_next_0134) + 8U,
                   reinterpret_cast<u32>(work.list_prev_0138));
    if (work.list_prev_0138 != 0)
        StoreU32To(static_cast<u8 *>(work.list_prev_0138) + 4U,
                   reinterpret_cast<u32>(work.list_next_0134));

    work.list_next_0134 = 0;
    work.list_prev_0138 = 0;
    --cond.script_count_0060; // +0x60 (the +0x64 aux count never unwinds)

    // Published-id slot clear: the index is work.published_id_index_1450
    // (record+0x248c) and is unbounded natively (the constructor even seeds
    // 0xffffffff there), so the indexed store into the ConditionalState
    // published_ids stays RAW — see the bounds note in
    // src/ConditionalStateObject.hpp.
    if ((work.flags_1444 & 0x8000U) != 0U) {
        const u32 slot = static_cast<u32>(work.published_id_index_1450);
        StoreU32To(state_bytes + 0x10U + slot * 4U, 0U);
    }

    // Soft-release the ten published entity ids through the render owner.
    // The native inlines 0x004492a0 (list-A/list-B scan over
    // owner+0x72dad4/+0x72dadc, flag 0x4000000 at entity+0x35c, propagated
    // to the +0x14 child list while entity+0x18 is clear), which the
    // reconstructed ReleaseEntityById models. Every slot is cleared after
    // the scan (native `*v6++ = 0` at 0x40dbe5).
    if (g_MainChainRenderOwner != 0) {
        for (u32 index = 0; index != 10U; ++index) {
            const u32 id = work.published_ids_00c0[index];
            if (id != 0U)
                ReleaseEntityById(g_MainChainRenderOwner, id);
            work.published_ids_00c0[index] = 0U;
        }
    }

    // Player-block back references: the primary slot at +0x3504 also owns
    // the byte flag at +0x3508; the 128-entry shot table at +0x4e8 (stride
    // 0x5c, shot+0x4c homing target) is scanned unconditionally and is
    // reached through the typed PlayerShotRecord fields.
    u8 *const player_ptr = static_cast<u8 *>(g_PlayerStateBlock);
    if (player_ptr != 0) {
        PlayerRecord &player =
            *reinterpret_cast<PlayerRecord *>(player_ptr);
        if (player.homing_target == &obj) {
            player.homing_target = 0;
            player.homing_target_latch = 0U;
        }
        for (u32 index = 0; index != 0x80U; ++index) {
            if (player.shots[index].homing_target == &obj)
                player.shots[index].homing_target = 0;
        }
    }

    // Destruction vtable planted before the name-list release, as in the
    // native epilogue.
    obj.vtable_0000 = reinterpret_cast<void *>(0x46d0d8U);

    // Free the script-name list (alloc_list_1034): each malloc'd node owns
    // the name buffer in node[0] and the next node in node[1]; the node
    // layout is foreign, so it stays RAW. Both allocations are released
    // through the shared delete.
    u32 *name_node = static_cast<u32 *>(obj.alloc_list_1034);
    while (name_node != 0) {
        const u32 next = name_node[1];
        FreeMainChainObject(reinterpret_cast<void *>(name_node[0]));
        FreeMainChainObject(name_node);
        name_node = reinterpret_cast<u32 *>(next);
    }
}

// TH10 0x0040cc50. ECX = record, stack = delete flags (ret 4).
void *ReleaseEclScriptObjectDeletingEcxStackAbi(void *record /* ECX */,
                                                u32 delete_flags)
{
    DestroyEclScriptObjectInPlaceStackAbi(record);
    if ((delete_flags & 1U) != 0U)
        FreeMainChainObject(record);
    return record;
}

} // namespace th10
