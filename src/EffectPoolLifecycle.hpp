#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0041ac20 / 0x0041ac30. EAX = source pair, ECX = destination
// record; 2-dword copy to destination +0 / +8 (tip record accessors).
void CopyTipPairEaxEcxAbi(const void *src, void *dst);
void CopyTipPairToSlot8EaxEcxAbi(const void *src, void *dst);

// TH10 0x0041ac80. EAX = destination, two stack dwords (retn 8): plain
// 2-dword store into the tip record.
void SetTipPairFromStackEaxStackAbi(void *dst, u32 value_a, u32 value_b);

// TH10 0x0041acf0. ECX = 0x3f0 stage entity record. Clears flag bit 0
// at +0x6c/+0xb0/+0xfc/+0x128/+0x174/+0x1b0/+0x1fc/+0x228/+0x378,
// zeroes the 0x3ac script region, writes 0xffff into the +0x384 word
// and clears bit 0 of +0x3d8. Returns the record.
void *ResetStageEntityRecordEcxAbi(void *record);

// TH10 0x0041ad60. ECX = entity. Frees the radial ribbon buffer at
// +0x358 (allocated by 0x004452f0) and clears the pointer.
void FreeEntityRibbonBufferEcxAbi(void *entity);

// TH10 0x0041ad90. EBX = manager. Registers the effect-pool calc
// callback 0x0041ba00 (slot 0x15) and draw callback 0x0041ba30 (slot
// 0x19) at manager+8/+0xc. Returns 0.
i32 InstallEffectPoolCallbacksEbxAbi(void *manager);

// TH10 0x0041aed0. EBX = manager. Allocates the 0x21cec0 effect pool,
// installs the per-record reset (0x0041acf0) / ribbon-free (0x0041ad60)
// pair through the 0x45252d pool iterator over 0x3f0-byte records at
// pool+0x14 (0x896 count), zeroes the pool, publishes it at
// DAT_00477818 with flag bit 1, then installs the callbacks. On
// callback failure the pool is torn down (0x0041adf0) and freed;
// returns the pool or 0.
void *CreateEffectPoolManagerEbxAbi(void *manager);

// TH10 0x0041ba30. Gate over TickEffectPoolSlots: returns 1 while the
// main chain context (DAT_00477810) has flag bit 2 of +0x58 set,
// otherwise falls through to the slot tick.
i32 TickEffectPoolSlotsGuarded(void);

} // namespace th10
