#include "EntityHelpers.hpp"
#include "GameManagerObject.hpp"
#include "GameManagerState.hpp"
#include "Th10Types.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

// Main-chain render owner (TH10 DAT_00491c10); the VM pool and entity lists
// that SpawnManagerEntityFromScript / SetManagerSlotEntityStopWord touch are
// owned by it.
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

// The fixed float the manager rate-tracker block points at (TH10
// flt_00476f78).
extern float g_MainChainTimeScaleTarget; // TH10 flt_00476f78

} // namespace

// TH10 0x0042c5c0. Game-manager state setter used by the manager calculation
// controller for every mode transition (native: EAX = manager, ECX = state).
// It stores the target state at +0x1c, clears +0x20, and seeds/resets the
// +0x2b0..+0x2c0 rate-tracker block in the same idiom as
// ResetMainChainFrameStateBlock.
void *SetGameManagerState(void *game_manager, u32 state)
{
    GameManager &mgr = *reinterpret_cast<GameManager *>(game_manager);
    mgr.state = static_cast<i32>(state); // +0x1c
    mgr.sub_state = 0; // +0x20

    const u32 rate_flags = mgr.frame_timer.flags; // +0x2c0
    if ((rate_flags & 1U) == 0) {
        mgr.frame_timer.count = 0;      // +0x2b4
        mgr.frame_timer.prev = -999999; // +0x2b0
        mgr.frame_timer.accum = 0;      // +0x2b8
        mgr.frame_timer.rate = &g_MainChainTimeScaleTarget; // +0x2bc
        mgr.frame_timer.flags = rate_flags | 1U;
    }
    mgr.frame_timer.count = 0;
    mgr.frame_timer.accum = 0;
    mgr.frame_timer.prev = -1;
    return game_manager;
}

// TH10 0x0042c620. Sub-state step helper used by the per-state bodies
// (native: EAX = manager, ECX = sub-state). It stores the sub-state at
// +0x20 and performs the same rate-tracker seed/reset as
// SetGameManagerState.
void SetGameManagerSubState(void *game_manager, u32 sub_state)
{
    GameManager &mgr = *reinterpret_cast<GameManager *>(game_manager);
    mgr.sub_state = static_cast<i32>(sub_state); // +0x20

    const u32 rate_flags = mgr.frame_timer.flags; // +0x2c0
    if ((rate_flags & 1U) == 0) {
        mgr.frame_timer.count = 0;      // +0x2b4
        mgr.frame_timer.prev = -999999; // +0x2b0
        mgr.frame_timer.accum = 0;      // +0x2b8
        mgr.frame_timer.rate = &g_MainChainTimeScaleTarget; // +0x2bc
        mgr.frame_timer.flags = rate_flags | 1U;
    }
    mgr.frame_timer.count = 0;
    mgr.frame_timer.accum = 0;
    mgr.frame_timer.prev = -1;
}

// TH10 0x0042c670. Spawn a 0x3ac-byte pool VM record bound to the given
// manager script slot. Native (verified by disassembly): EDI = script id,
// stack = manager. It allocates from the render-owner pool
// (AllocatePoolVmEsiAbi), marks the record kind 0xf and flag bit
// 0x40000000 at +0x35c, assigns the script through 0x00449870, links the
// record into the owner entity lists and writes the assigned id at
// manager + 0x2c4 + 4*script_id.
void *SpawnManagerEntityFromScript(void *game_manager, u32 script_id)
{
    u32 *const manager_words = static_cast<u32 *>(game_manager);
    void *const vm_memory = AllocatePoolVmEsiAbi(g_MainChainRenderOwner);
    VmRecord &vm = *reinterpret_cast<VmRecord *>(vm_memory);
    vm.render_kind = 0xfU;
    vm.flags |= 0x40000000U;
    AssignPoolVmScriptEcxEaxAbi(vm_memory, static_cast<i32>(script_id));
    u32 assigned_id = 0;
    LinkEntityAndAssignIdEaxEsiAbi(&assigned_id, vm_memory);
    // Kept raw: script_id is a parameter and the evidence shows slots up to
    // 182, past the modeled 180-entry script_entity_handles array.
    manager_words[(0x2c4 + 4 * script_id) / 4] = assigned_id;
    return vm_memory;
}

// TH10 0x0042c770. Write the SI stop-word value to the record addressed by
// manager + 0x2c4 + 4*slot: resolve the stored id through the owner entity
// lists (0x004491c0), store the value at record + 0x304, and when the
// record's container field at + 0x18 is zero repeat over its child list at
// + 0x14.
i32 SetManagerSlotEntityStopWord(void *game_manager, u32 slot, u16 value)
{
    // Kept raw: slot is a parameter and can address slots past 180.
    u32 *const manager_words = static_cast<u32 *>(game_manager);
    u8 *const entity = FindEntityEdxStackAbi(
        g_MainChainRenderOwner,
        manager_words[(0x2c4 + 4 * slot) / 4]);
    if (entity == 0)
        return 0;
    VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
    vm.state_word = value;
    if (vm.parent_link == 0) {
        u32 **child = static_cast<u32 **>(vm.first_child);
        for (; child != 0; child = reinterpret_cast<u32 **>(child[1])) {
            VmRecord &child_vm = *reinterpret_cast<VmRecord *>(
                reinterpret_cast<u8 *>(child[0]));
            child_vm.state_word = value;
        }
    }
    return 1;
}

// TH10 0x0043ee30: runs the entity's animation/script body (native ABI is a
// register pass; modeled here as an explicit entity argument).
extern void RunEntityAnimationBody(void *entity); // TH10 0x0043ee30

// TH10 0x004497d0. Resolve the entity id stored at *id_slot, then walk the
// inline node chain embedded at entity + 0x10 (node[0] = child entity,
// node[1] = next node) looking for a child whose signed kind at +0x38a
// equals `kind`. A match stores the child's word 0 into *out_handle; no
// match stores zero. A zero id additionally rewrites *id_slot to zero
// (native: id pointer in ESI, kind in EBX, out pointer in EDI).
u32 *ResolveChildEntityByKind(u32 *id_slot, i32 kind, u32 *out_handle)
{
    u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner, *id_slot);
    if (entity == 0)
        *id_slot = 0;
    u8 *node = entity + 0x10;
    while (node != 0) {
        u8 *const child = *reinterpret_cast<u8 **>(node);
        VmRecord &child_vm = *reinterpret_cast<VmRecord *>(child);
        const i32 child_kind = static_cast<i32>(
            static_cast<i16>(child_vm.bound_script_id));
        if (child_kind == kind) {
            *out_handle = static_cast<u32>(child_vm.entity_id);
            return out_handle;
        }
        node = *reinterpret_cast<u8 **>(node + 4);
    }
    *out_handle = 0;
    return out_handle;
}

// TH10 0x00449470. Resolve the entity id stored at *id_slot and write the
// value into the u16 stop word at +0x304, recursing over the +0x14 child
// chain when the +0x18 count is zero (native: id pointer in EAX, value in
// SI; the owner is the fixed render-owner global).
void SetEntityStateWordByHandleSlot(u32 *id_slot, i32 value)
{
    u8 *const entity =
        FindEntityEdxStackAbi(g_MainChainRenderOwner, *id_slot);
    if (entity != 0) {
        VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
        vm.state_word = static_cast<u16>(value);
        if (vm.parent_link == 0) {
            u32 **child = static_cast<u32 **>(vm.first_child);
            for (; child != 0; child = reinterpret_cast<u32 **>(child[1])) {
                VmRecord &child_vm = *reinterpret_cast<VmRecord *>(
                    reinterpret_cast<u8 *>(child[0]));
                child_vm.state_word = static_cast<u16>(value);
            }
        }
    }
}

// TH10 0x00449250. Resolve the entity id, write the value into the +0x304
// stop word, run the animation body 0x0043ee30 on the entity, and propagate
// both to the +0x14 child chain when the +0x18 count is zero (native: id on
// the stack, value in DI, owner in EDX).
void SetEntityStopWordByIdAndRun(u32 id, u32 value)
{
    u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner, id);
    if (entity != 0) {
        VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
        vm.state_word = static_cast<u16>(value);
        RunEntityAnimationBody(entity);
        if (vm.parent_link == 0) {
            u32 **child = static_cast<u32 **>(vm.first_child);
            for (; child != 0; child = reinterpret_cast<u32 **>(child[1])) {
                u8 *const child_entity =
                    reinterpret_cast<u8 *>(child[0]);
                VmRecord &child_vm =
                    *reinterpret_cast<VmRecord *>(child_entity);
                child_vm.state_word = static_cast<u16>(value);
                RunEntityAnimationBody(child_entity);
            }
        }
    }
}

// TH10 0x004495e0. Resolve the entity id stored at *id_slot and clear bit 2
// of the +0x35c u32 flag word, recursing over the +0x14 child chain when
// the +0x18 count is zero (native: id pointer in EAX).
void ClearEntityFlag2ByHandleSlot(u32 *id_slot)
{
    u8 *const entity =
        FindEntityEdxStackAbi(g_MainChainRenderOwner, *id_slot);
    if (entity != 0) {
        VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
        vm.flags &= ~2U;
        if (vm.parent_link == 0) {
            u32 **child = static_cast<u32 **>(vm.first_child);
            for (; child != 0; child = reinterpret_cast<u32 **>(child[1])) {
                VmRecord &child_vm = *reinterpret_cast<VmRecord *>(
                    reinterpret_cast<u8 *>(child[0]));
                child_vm.flags &= ~2U;
            }
        }
    }
}

// TH10 0x0044bea0. Menu cursor shift: adds the delta to the current value,
// wraps at the +0x08 maximum (or clamps to 0 / maximum - 1), then keeps
// shifting while the result collides with an entry in the disabled list at
// +0x90 whose count is at +0xd4. Returns the new cursor value. Native: the
// record base in EAX, delta on the stack.
i32 ShiftManagerSelector(void *cursor_record, i32 delta)
{
    i32 *const record = static_cast<i32 *>(cursor_record);
    const i32 maximum = record[2];
    if (maximum <= 0)
        return record[0];
    const i32 disabled_count = record[0xd4 / 4];
    const i32 wrap_style = record[0xd0 / 4];
    i32 *const disabled = record + 0x90 / 4;

    for (;;) {
        i32 value = record[0] + delta;
        record[0] = value;
        while (value >= maximum) {
            value = (wrap_style != 0) ? value - maximum : maximum - 1;
            record[0] = value;
        }
        while (value < 0) {
            value = (wrap_style != 0) ? value + maximum : 0;
            record[0] = value;
        }
        i32 index = 0;
        while (index < disabled_count) {
            if (disabled[index] == record[0])
                break;
            ++index;
        }
        if (index >= disabled_count)
            return record[0];
    }
}

// TH10 0x0042c6d0. Resolve the entity id stored at manager + 0x2c4 + 4*slot
// and write stop word 1 at +0x304 (recursing over the +0x14 child chain
// when the +0x18 count is zero), then clear the slot id. Used when a menu
// step leaves the currently spawned script entity behind (native: manager in
// EDI, slot in ESI).
void ReleaseManagerSlotEntity(void *game_manager, u32 slot)
{
    // Kept raw: slot is a parameter and can address slots past 180.
    u32 *const manager_words = static_cast<u32 *>(game_manager);
    u32 *const slot_word = &manager_words[(0x2c4 + 4 * slot) / 4];
    u8 *const entity =
        FindEntityEdxStackAbi(g_MainChainRenderOwner, *slot_word);
    if (entity != 0) {
        VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
        vm.state_word = 1;
        if (vm.parent_link == 0) {
            u32 **child = static_cast<u32 **>(vm.first_child);
            for (; child != 0; child = reinterpret_cast<u32 **>(child[1])) {
                VmRecord &child_vm = *reinterpret_cast<VmRecord *>(
                    reinterpret_cast<u8 *>(child[0]));
                child_vm.state_word = 1;
            }
        }
    }
    *slot_word = 0;
}

// TH10 0x0042c750. Forwarder to 0x00449250 that passes the slot entity's id
// (manager + 0x2c4 + 4*slot) and the fixed stop word 3 (native: manager in
// EAX, slot in ECX).
void Call42C750(void *game_manager, u32 slot)
{
    // Kept raw: slot is a parameter and can address slots past 180.
    u32 *const manager_words = static_cast<u32 *>(game_manager);
    SetEntityStopWordByIdAndRun(
        manager_words[(0x2c4 + 4 * slot) / 4], 3);
}

// TH10 0x0044be70. Cursor finalize (native: record base in EAX). Steps the
// record back one entry: the count at +0x8c is decremented (clamped at 0)
// and used to reload the cursor value from the per-step array at +0x0c and
// the maximum from the parallel array at +0x4c; the disabled-row count at
// +0xd4 is cleared. The per-step arrays are filled when the menus arm the
// cursor for the next stage.
void Call44BE70(void *cursor_record)
{
    u32 *const record = static_cast<u32 *>(cursor_record);
    const u32 index = record[0x8c / 4];
    const u32 step_index = (index != 0) ? index - 1 : 0;
    record[0x8c / 4] = step_index;
    record[0] = record[(0x0c + 4 * step_index) / 4];
    record[0x08 / 4] = record[(0x4c + 4 * step_index) / 4];
    record[0xd4 / 4] = 0;
}

// TH10 0x0040ace0. Input poll over the fixed 0x474e30 bank: true when the
// mask matches either of the two u16 words at base + 0x04 (the 0x474e34
// gate byte family) or base + 0x06 (the 0x474e36 flag family).
bool PollMenuInputState(u32 mask)
{
    const u8 *const bank = reinterpret_cast<const u8 *>(0x474e30U);
    const u16 word_low = *reinterpret_cast<const u16 *>(bank + 0x04);
    const u16 word_high = *reinterpret_cast<const u16 *>(bank + 0x06);
    return (mask & word_low) != 0 || (mask & word_high) != 0;
}

// TH10 0x0044be20. Push one step: the cursor record stores its current
// value and maximum at the next +0x0c / +0x4c per-step array slot (indexed
// by the +0x8c count), advances the count capped at 15, and resets the
// disabled-row count at +0xd4. This mirrors Call44BE70 which pops a step
// back. Native takes the record base in EAX and returns it.
void *RunManagerCursorHandle(void *cursor_record)
{
    u32 *const record = static_cast<u32 *>(cursor_record);
    const u32 step_index = record[0x8c / 4];
    record[(0x0c + 4 * step_index) / 4] = record[0];
    record[(0x4c + 4 * step_index) / 4] = record[0x08 / 4];
    const u32 next_index = (step_index < 15U) ? step_index + 1 : 15U;
    record[0x8c / 4] = next_index;
    record[0xd4 / 4] = 0;
    return cursor_record;
}

// TH10 0x0040ad20. Set a menu cursor value with clamping: when the +0x08
// maximum is zero the raw value is written verbatim; otherwise it is clamped
// into [0, maximum - 1].
void Call40AD20(u32 raw_value, void *cursor_record)
{
    i32 *const record = static_cast<i32 *>(cursor_record);
    const i32 maximum = record[2];
    if (maximum == 0) {
        record[0] = static_cast<i32>(raw_value);
        return;
    }
    const i32 value = static_cast<i32>(raw_value);
    record[0] =
        (value < maximum) ? ((value < 0) ? 0 : value) : maximum - 1;
}

// TH10 0x0043dc90. Reserve a queue entry in the boundary context: walk the
// twelve channel keys at +0x620; a matching key appends `arg` to that
// channel's +0x680 entry block (count at +0x650, 0x200-byte stride, 0x80
// entries per block) and a free (negative) key claims the channel by
// storing the kind at +0x620, the signed lookup word_4749ce[8*kind] into
// the +0x408 per-kind scratch and the arg as its first entry. Returns the
// channel index; full channels or a full block return without queuing.
i32 ReserveContextChannel(void *context, u32 kind, u32 arg)
{
    u8 *const bytes = static_cast<u8 *>(context);
    u32 *const keys = reinterpret_cast<u32 *>(bytes + 0x620);
    u32 *const counts = reinterpret_cast<u32 *>(bytes + 0x650);
    u32 *const entries = reinterpret_cast<u32 *>(bytes + 0x680);

    i32 channel = 0;
    for (; channel < 12; ++channel) {
        const i32 key = static_cast<i32>(keys[channel]);
        if (key < 0)
            break;
        if (static_cast<u32>(key) == kind) {
            const u32 count = counts[channel];
            if (count >= 0x80U)
                return channel;
            entries[0x200 / 4 * channel + count] = arg;
            counts[channel] = count + 1;
            return channel;
        }
    }
    if (channel >= 12)
        return channel;

    keys[channel] = kind;
    const short table_value = *reinterpret_cast<const short *>(
        0x4749ceU + 8 * kind);
    *reinterpret_cast<i32 *>(bytes + 0x408 + 4 * kind) = table_value;
    entries[0x200 / 4 * channel] = arg;
    counts[channel] = 1;
    return channel;
}

} // namespace th10
