// Semantic reconstruction of TH10 0x0040dae0 (in-place destructor of an ECL
// script object) and 0x0040cc50 (its scalar deleting destructor wrapper).
//
// This is the counterpart of CreateEclScriptObjectEaxStackAbi in
// EclScriptLibrary.cpp: the record's embedded list node (+0x116c, next at
// +0x1170, prev at +0x1174) is removed from the conditional state's object
// list, the entity ids published at +0x10fc are soft-released on the render
// owner, and the player-block back references are dropped.

#include "EclScriptObjectTeardown.hpp"

#include "ConditionalStateObject.hpp"
#include "EntityHelpers.hpp"
#include "PlayerRecord.hpp"

namespace th10 {

namespace {

extern void FreeMainChainObject(void *object);      // TH10 0x004524a1
extern void *g_MainChainRenderOwner;                // TH10 DAT_00491c10
extern void *g_AsciiHudConditionalState;            // TH10 DAT_00477704
extern void *g_PlayerStateBlock;                    // TH10 DAT_00477834

u32 LoadU32From(const void *address)
{
    const u8 *const bytes = static_cast<const u8 *>(address);
    return static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8)
         | (static_cast<u32>(bytes[2]) << 16)
         | (static_cast<u32>(bytes[3]) << 24);
}

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
    u8 *const rec = static_cast<u8 *>(record);

    // Plant the live vtable first (the native destructor prologue restores
    // the final-class vtable before any release work).
    StoreU32To(rec, 0x46d0c0U);

    // Unlink the embedded node from the conditional state's list. Only the
    // +0x60 count is decremented here even though creation bumps both
    // +0x60 and +0x64 (native quirk; the +0x64 counter is never undone).
    // The native dereferences the DAT_00477704 holder without a null
    // check; the reconstruction keeps that direct read.
    u8 *const state_bytes = static_cast<u8 *>(g_AsciiHudConditionalState);
    ConditionalState &cond =
        *reinterpret_cast<ConditionalState *>(state_bytes);
    u32 *const node = reinterpret_cast<u32 *>(rec + 0x116cU);
    if (cond.script_list_head_0058 == node) // +0x58
        cond.script_list_head_0058 = reinterpret_cast<void *>(node[1]);
    if (cond.script_list_tail_005c == node) // +0x5c
        cond.script_list_tail_005c = reinterpret_cast<void *>(node[2]);

    const u32 next = node[1];
    if (next != 0U)
        StoreU32To(reinterpret_cast<void *>(next + 8U), node[2]);
    const u32 previous = node[2];
    if (previous != 0U)
        StoreU32To(reinterpret_cast<void *>(previous + 4U), node[1]);

    node[1] = 0U;
    node[2] = 0U;
    --cond.script_count_0060; // +0x60 (the +0x64 aux count never unwinds)

    // Published-id slot clear: the index is the record's +0x248c field and
    // is unbounded natively (the constructor even seeds 0xffffffff there),
    // so the +0x10 store stays RAW — see the bounds note in
    // src/ConditionalStateObject.hpp.
    if ((LoadU32From(rec + 0x2480U) & 0x8000U) != 0U) {
        const u32 slot = LoadU32From(rec + 0x248cU);
        StoreU32To(state_bytes + 0x10U + slot * 4U, 0U);
    }

    // Soft-release the ten published entity ids through the render owner.
    // The native inlines 0x004492a0 (list-A/list-B scan over
    // owner+0x72dad4/+0x72dadc, flag 0x4000000 at entity+0x35c, propagated
    // to the +0x14 child list while entity+0x18 is clear), which the
    // reconstructed ReleaseEntityById models.
    if (g_MainChainRenderOwner != 0) {
        for (u32 index = 0; index != 10U; ++index) {
            const u32 id = LoadU32From(rec + 0x10fcU + index * 4U);
            if (id != 0U)
                ReleaseEntityById(g_MainChainRenderOwner, id);
        }
    }

    // Player-block back references: the primary slot at +0x3504 also owns
    // the byte flag at +0x3508; the 0x80-entry table at +0x4e8 (stride
    // 0x5c) is scanned unconditionally.
    u8 *const player_ptr = static_cast<u8 *>(g_PlayerStateBlock);
    if (player_ptr != 0) {
        PlayerRecord &player =
            *reinterpret_cast<PlayerRecord *>(player_ptr);
        if (player.homing_target == rec) {
            player.homing_target = 0;
            player.homing_target_latch = 0U;
        }
        for (u32 index = 0; index != 0x80U; ++index) {
            if (player.shots[index].homing_target == rec)
                player.shots[index].homing_target = 0;
        }
    }

    // Destruction vtable planted before the name-list release, as in the
    // native epilogue.
    StoreU32To(rec, 0x46d0d8U);

    // Free the script-name list at +0x1034: each node owns the name buffer
    // in node[0] and the next node in node[1]; both are released through
    // the shared delete.
    u32 *name_node = reinterpret_cast<u32 *>(LoadU32From(rec + 0x1034U));
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
