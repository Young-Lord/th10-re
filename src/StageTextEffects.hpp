#pragma once

#include "Th10Types.hpp"

namespace th10 {

// In-game floating digit-text effect owner (TH10 DAT_00477840, 0xb884 bytes).
// Layout:
//   +0x0010  resource-state pointer (seeded from AsciiManager +0x8994); its
//            +0x118 selects the 0x44-byte glyph-entry table
//   +0x0014  u32 ring write counter for the 0x2d0 live slots (wraps to 0)
//   +0x0018  0x3ac-byte ASCII animation VM record (see AsciiAnimationVm)
//   +0x03c4  0x40-byte text-effect records; 0x2d0 slots are addressed by the
//            ring counter, the updater/drawer scan 0x2d3 of them (quirk)
// Record layout (base = owner + 0x3c4 + slot * 0x40):
//   +0x00  text bytes (decimal digits written low-digit-first)
//   +0x0c  float x, +0x10 float y, +0x14 float z (world position)
//   +0x18  u32 param (republished to VM +0x2fc on draw)
//   +0x1c  i32 frame mirror (written by the tick, read as scratch)
//   +0x20  i32 frame counter
//   +0x24  float frame accumulator
//   +0x28  pointer to the rate table (defaults to float 1.0 at 0x476f78)
//   +0x2c  u32 flags (bit0 = timing fields initialized)
//   +0x38  u8 active flag, +0x39 u8 character count

// TH10 0x0042b780. Native __usercall with the owner in EDI (the draw-chain
// callback thunk 0x0042b9b0 copies the ECX callback argument there). Draws
// every active record as a vertical digit stack through the owner VM,
// scaling the glyphs by the distance between the record and the camera.
void DrawTextEffectRecordsEdiAbi(void *owner);

// TH10 0x0042b6f0. Native EAX = owner (the update-chain callback thunk
// 0x0042b9a0 moves the ECX callback argument into EAX). Ticks every active
// record: the y position rises by 0.5 and the frame counter advances either
// smoothly (rate >= 1.01: accumulator += 1.0, frames++) or through the
// accumulator (accumulator += rate, frames = trunc(accumulator)). Records
// deactivate after 60 frames. Always returns 1.
i32 TickTextEffectRecordsEaxAbi(void *owner);

// TH10 0x0042b9c0. Native: ECX = owner, EAX = value, EDI = Float3 position,
// stack = param (`ret 4`). Grabs the next ring slot, formats `value` as
// decimal digits written low-digit-first (negative values store the single
// byte '\n', zero stores an empty string), and seeds the record.
void AddTextEffectNumberEcxEaxEdiStackAbi(void *owner, i32 value, u32 param,
                                          const void *position);

// TH10 0x0042b430. Native EBX = owner. Every field store and flag clear in
// the native body is followed by a full 0xb884-byte zeroing pass that erases
// them (dead stores, quirk preserved): the observable behavior is zeroing
// the owner, setting bit 1 of the flag word and publishing DAT_00477840.
void InitializeTextEffectOwnerEdxAbi(void *owner);

// TH10 0x0042b4d0. Native EAX = owner. Registers the disabled tick record
// (callback thunk 0x0042b9a0, calculation priority 15) and draw record
// (thunk 0x0042b9b0, draw priority 0x27), then resets the owner VM and
// rebinds it to glyph entry 0xc4 of the resource from AsciiManager +0x8994.
// Returns 0.
i32 RegisterTextEffectOwnerSchedulerRecordsEaxAbi(void *owner);

// TH10 0x0042b660. Allocates the 0xb884-byte owner, initializes it in
// place (0x0042b430) and registers its scheduler records (0x0042b4d0);
// on registration failure the owner is destroyed (0x0042b570) and freed.
// Returns the owner or 0. Called from the game-mode entry chain
// (0x0040a350 / 0x00417870).
void *CreateTextEffectOwner();

} // namespace th10
