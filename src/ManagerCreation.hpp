#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00406060. Allocates the 0x3e0b54-byte effect manager root,
// runs the eh vector constructor over 2001 records of 0x7f0 bytes at
// +0x60 (scalar ctor 0x405d00, thiscall ctor 0x405de0 - boundaries),
// then memsets the WHOLE object to zero (native order quirk: the wipe
// happens after the construction), publishes it to DAT_004776f0, and
// calls the 0x405e20 loader; a failed load destroys and frees the object
// (dtor 0x405f70) and returns null.
void *CreateEffectManagerRoot();

// TH10 0x00414830. Allocates the 0x9ed0-byte ASCII HUD owner, runs its
// constructor 0x413810 (boundary), calls the 0x413980 loader; a failed
// load destroys (0x004145f0 DestroyAsciiHudOwnerInPlace) and frees the
// object and returns null.
void *CreateAsciiHudOwner();

// TH10 0x00425020. Allocates the 0x4478-byte player state block, runs
// its constructor 0x4246c0 (boundary, native EAX = object), then the
// player initializer 0x004247f0 (semantic InitializePlayerObject, native
// EBX = object); a failed init destroys (0x00424ed0
// DestroyPlayerStateBlockInPlace) and frees the object, returning null.
void *CreatePlayerStateBlock();

// TH10 0x00413810. Native stdcall (EAX/stack = the 0x9ed0 object):
// constructs the six 0x3ac-record glyph VM arrays (counts 10/10/9/4/2/7
// at +0x10/+0x24c8/+0x4980/+0x6a8c/+0x793c/+0x8094) via the eh vector
// constructor (scalar ctor 0x402050, dtor 0x00401ff0), clears the nine
// +0x9ad4-family flag bits, wipes the +0x9a28 record, sets the +0x9d8c
// word to -1, clears +0x9e70, then memsets the WHOLE object (order
// quirk), sets bit 1 of the first dword, publishes DAT_0047770c and
// returns the object.
void *ConstructAsciiHudOwnerEaxAbi(void *object);

// TH10 0x004246c0. Native ESI = the 0x4478 player state object: clears
// the nine +0x80-family and three +0x470-family flag bits, wipes the
// +0x14 record, sets the +0x398 word to -1, runs the 128-step +0x4ac
// (stride 0x5c) and 33-step +0x3560 (stride 0x6c) flag-clear loops,
// clears +0x3320/+0x33b8/+0x3450/+0x34e8 and +0x431c, then memsets the
// WHOLE object (order quirk), publishes DAT_00477834 and returns it.
void *ConstructPlayerStateBlockEsiAbi(void *object);

} // namespace th10
