#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00418810. EAX = title-state playfield origin {x, y, z}.
// Publishes the shifted origin {x+224, y+16, z} into the three float
// globals at DAT_00497d80 (guarded by the once-byte at DAT_00497d8c)
// and returns its address.
float *PublishTitlePlayfieldOriginEaxAbi(const float *origin);

// TH10 0x004189e0. EAX = title state. Zeroes the four input counters
// at +0x4c..+0x58.
void ClearTitleInputCountersEaxAbi(void *state);

// TH10 0x00418a90. EAX = title state. Advances the menu item counter
// at +0x50, clamped to 9.
void AdvanceTitleMenuItemIndexEaxAbi(void *state);

// TH10 0x00418ad0 / 0x00418af0 / 0x00418b10. EAX = title state, ECX or
// EDX = toggle source. XOR-toggles flag bit 3 / bit 1 / bit 0 of +0x60
// by the parity of the argument.
void ToggleTitleFlagBit3EaxEcxAbi(void *state, i32 source);
void ToggleTitleFlagBit1EaxEcxAbi(void *state, i32 source);
void ToggleTitleFlagBit0EaxEdxAbi(void *state, i32 source);

// TH10 0x00418b30. EAX = title state. Flag bit 1 read of +0x60.
i32 ReadTitleFlagBit1EaxAbi(const void *state);

// TH10 0x00418c10. EAX = title state. Zeroes the cursor pair at
// +0x48/+0x4c.
void ClearTitleCursorPairEaxAbi(void *state);

// TH10 0x00418d40. EDX = 0x1a8 title sub-record. Zeroes 0x6a dwords,
// sets flag bit 1, threads sixteen self-referencing list heads at
// +0x18/+0x24/... /+0xcc (0xc stride) and publishes the record at
// DAT_00477814. Returns the record.
void *InitializeTitleStateListsInPlaceEdxAbi(void *record);

// TH10 0x00418e30. EBX = manager. Registers the hint calc callback
// 0x004198a0 (slot 0x19) and draw callback 0x004198b0 (slot 0x2c) at
// manager+8/+0xc, clears manager+0x10 and, when the hint gate byte
// DAT_00491d6a is set, parses hint/hint_auto.txt and
// hint/hint_user.txt. Returns 0.
i32 InstallHintTextCallbacksEbxAbi(void *manager);

// TH10 0x00419090. EBX = manager. Allocates the 0x1a8 hint sub-record,
// initializes it in place, then installs the hint callbacks. On
// callback failure the record is torn down (0x00418ee0) and freed;
// returns the record or 0.
void *CreateHintTextOwnerEbxAbi(void *manager);

} // namespace th10
