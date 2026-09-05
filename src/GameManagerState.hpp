// Game-manager state helpers and state-machine controllers, exported across
// the manager-state module files. All names mirror the TH10 symbols; offsets
// and ABI notes live beside each implementation.
#ifndef TH10_GAMEMANAGERSTATE_HPP
#define TH10_GAMEMANAGERSTATE_HPP

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0042c5c0 (manager in EAX, state in ECX).
void *SetGameManagerState(void *game_manager, u32 state);

// TH10 0x0042c620 (manager in EAX, sub-state in ECX). Writes the sub-state
// at +0x20 and re-seeds the +0x2b0..+0x2c0 rate-tracker block exactly like
// SetGameManagerState.
void SetGameManagerSubState(void *game_manager, u32 sub_state);

// TH10 0x0042c670 (script id in EDI, manager on the stack).
void *SpawnManagerEntityFromScript(void *game_manager, u32 script_id);

// TH10 0x0042c770 (manager, slot, stop word).
i32 SetManagerSlotEntityStopWord(void *game_manager, u32 slot, u16 value);

// TH10 0x004497d0. Resolve the entity id stored at *id_slot, then scan the
// entity's inline node chain at +0x10 for a child whose kind at +0x38a
// equals `kind`; on match write child[0] to *out_handle, else write zero.
u32 *ResolveChildEntityByKind(u32 *id_slot, i32 kind, u32 *out_handle);

// TH10 0x00449470. Resolve the entity id stored at *id_slot, write `value`
// to the u16 stop word at +0x304 (propagating to the +0x14 child chain when
// the +0x18 count is zero).
void SetEntityStateWordByHandleSlot(u32 *id_slot, i32 value);

// TH10 0x00449250. Resolve the entity id, write `value` to the +0x304 stop
// word, run the animation body 0x0043ee30 on it, and propagate both to the
// +0x14 child chain when the +0x18 count is zero.
void SetEntityStopWordByIdAndRun(u32 id, u32 value);

// TH10 0x004495e0. Resolve the entity id stored at *id_slot and clear bit 2
// of the u32 flag word at +0x35c (propagating to the +0x14 child chain when
// the +0x18 count is zero).
void ClearEntityFlag2ByHandleSlot(u32 *id_slot);

// TH10 0x0044bea0. Shift a menu cursor record by `delta` with wrap at the
// +0x08 maximum, stepping over the disabled entries listed at +0x90 (count
// at +0xd4); the wrap style flag sits at +0xd0. Cursor record fields are
// word-indexed from the record base (e.g. manager + 0x24).
i32 ShiftManagerSelector(void *cursor_record, i32 delta);

// TH10 0x0042c6d0 (manager in EDI, slot in ESI). Resolve the slot entity at
// manager + 0x2c4 + 4*slot, write stop word 1 at +0x304 (propagating to the
// +0x14 child chain when the +0x18 count is zero) and clear the slot.
void ReleaseManagerSlotEntity(void *game_manager, u32 slot);

// TH10 0x0042c750 (manager in EAX, slot in ECX). Cursor-change side effect
// on the slot entity: runs 0x00449250 with stop word 3 on its id.
void Call42C750(void *game_manager, u32 slot);

// TH10 0x0044be70. Cursor finalize: decrement the step count at +0x8c
// (clamped at zero), reload the cursor value and maximum from the parallel
// +0x0c / +0x4c per-step arrays at that index, and clear the disabled count
// at +0xd4. Native takes the record base in EAX.
void Call44BE70(void *cursor_record);

// TH10 0x0040ace0. Input poll on the fixed 0x474e30 bank: true when the
// mask matches either of the u16 words at base + 0x04 / + 0x06 (native EAX
// mask, ECX base).
bool PollMenuInputState(u32 mask);

// TH10 0x0044be20. Push one step onto the menu cursor record: store the
// current value and maximum into the per-step arrays at +0x0c / +0x4c at
// the +0x8c step count, advance the count (capped at 15) and clear the
// disabled-row count at +0xd4. Returns the record base.
void *RunManagerCursorHandle(void *cursor_record);

// TH10 0x0040ad20. Set the cursor value on a record (native ECX = value,
// EDX = record): when the +0x08 maximum is zero write the value verbatim,
// otherwise clamp it to [0, maximum - 1].
void Call40AD20(u32 raw_value, void *cursor_record);

// TH10 0x0043dc90. Reserve one entry in one of the twelve channels of the
// boundary context (native ECX = context, EDI = channel kind, stack = arg).
// The channel-key table lives at context + 0x620 (negative = free), the
// per-channel counts at + 0x650, the 0x200-byte entry blocks at + 0x680 and
// the per-kind scratch word at + 0x408. New channels also store the signed
// table value at word_4749ce[8*kind] into that scratch word. Returns the
// channel index.
i32 ReserveContextChannel(void *context, u32 kind, u32 arg);

} // namespace th10

#endif
