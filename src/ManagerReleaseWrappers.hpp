#pragma once

#include "Th10Types.hpp"

namespace th10 {

// The "release wrapper" family: each native entry takes its object in ESI
// (usercall), null-checks it, runs the object's in-place destructor, and
// releases the outer allocation with the shared main-chain delete
// (0x004524a1). Where the destructor is already reconstructed the wrapper
// calls the semantic body; otherwise the destructor stays a boundary.

// TH10 0x004148e0. Native ESI = the DAT_0047770c ASCII HUD owner;
// destructor 0x004145f0 (DestroyAsciiHudOwnerInPlace).
void ReleaseAsciiHudOwnerEsiAbi(void *owner);

// TH10 0x00425090. Native ESI = the DAT_00477834 player-state block;
// destructor 0x00424ed0.
void ReleasePlayerStateBlockEsiAbi(void *block);

// TH10 0x00424ed0. In-place destructor of the DAT_00477834 player-state
// block: scheduler-record removal at +8/+0xc under the scheduler lock,
// clears the block global, then the keep-alive path (mode-flag bit 0:
// resource-matched entity release for +0x10, shot-entry-cache republish
// from +0x45c) or the full path (large-slot release at owner+0x3AD08C,
// CRT-free +0x45c, cache clear); the +0x36c buffer is CRT-freed on both.
void DestroyPlayerStateBlockInPlace(void *object);

// TH10 0x00406140. Native ESI = the DAT_004776f0 effect manager root;
// destructor 0x00405f70 (declared above).
void ReleaseEffectManagerRootEsiAbi(void *root);

// TH10 0x00405f70. In-place destructor of the DAT_004776f0 effect manager
// root: scheduler-record removal at +8/+0xc, resource-matched entity
// release for +0x3E0B50, clears the root global, and the eh vector
// destructor iterator over 2001 0x7F0-byte records at +0x60 (each frees
// its +0x360 buffer).
void DestroyEffectManagerRootInPlace(void *root);

// TH10 0x00405730. Native ESI = the DAT_004776ec game context;
// destructor 0x00405620 (declared above).
void ReleaseGameContextEsiAbi(void *context);

// TH10 0x00405620. In-place destructor of the DAT_004776ec game context:
// scheduler-record removal at +8/+0xc and the global clear.
void DestroyGameContextInPlace(void *context);

// TH10 0x0041afb0. Native ESI = the DAT_00477818 bullet manager;
// destructor 0x0041adf0 (declared above).
void ReleaseBulletManagerEsiAbi(void *manager);

// TH10 0x0041adf0. In-place destructor of the DAT_00477818 bullet manager:
// scheduler-record removal at +8/+0xc, clears the manager global, and the
// eh vector destructor iterator over 2198 0x3F0-byte records at +0x14
// (each frees its +0x358 buffer).
void DestroyBulletManagerInPlace(void *manager);

// TH10 0x0042b6d0. Native ESI = the DAT_00477840 main-chain object;
// destructor 0x0042b570 (declared above).
void ReleaseMainChainObject840EsiAbi(void *object);

// TH10 0x0042b570. In-place destructor of the DAT_00477840 0x840-byte
// main-chain object: scheduler-record removal at +8/+0xc, clears the
// global, and CRT-frees the +0x370 buffer.
void DestroyMainChainObject840InPlace(void *object);

// TH10 0x0040d730. Native ESI = the DAT_00477704 ASCII HUD conditional
// state; destructor 0x0040d530 (native EAX = state, declared above).
void ReleaseAsciiHudConditionalStateEsiAbi(void *state);

// TH10 0x0040d530. Native EAX = the DAT_00477704 ASCII HUD conditional
// state: 0x409f90 sub-block release, scheduler-record removal at +8/+0xc,
// the +0x54 sub-object teardown (0x0c..0x88 pointer frees, 0x46D0F0
// vtable, +0x8C buffer free, allocation release), then the state-global
// clear or the four large-slot releases at owner+0x3AD090..0x3AD09C first.
void DestroyAsciiHudConditionalStateEax(void *state);

// TH10 0x0040d510. Native EAX = manager: identical body to 0x00409e20 —
// re-enables the scheduler records at +8/+0xc when present and returns
// the +0xc record (or the manager).
void *EnableAsciiHudConditionalRecordsEaxAbi(void *manager);

// TH10 0x00409eb0. Native ESI = the ECL select menu: frees each non-null
// filename pointer in the +0x34 array (count at +0x38) through the CRT
// free, then frees the array itself; the +0x34 slot is cleared twice
// (native quirk, preserved).
void ReleaseEclSelectMenuNamesEsiAbi(void *menu);

// TH10 0x00401ff0. Native __thiscall ECX = the 0x3ac-byte VM record:
// releases the record's +0x358 buffer through the CRT free and clears the
// slot. Scalar destructor used by the eh vector destructor iterators.
void DestroyTitleScreenVmRecordInPlace(void *record);

// TH10 0x00447810. Native usercall EDI = a large render-owner slot block:
// when its +0x108 buffer exists, first releases every entity using the
// block as its resource (0x004493e0 over the render owner's lists), then
// releases the 16-byte-stride COM-entry array (+0x120, count +0x10c:
// each entry's object is Released via its +0x08 vtable method and its
// +0x04 pointer CRT-freed), and finally the +0x120/+0x118/+0x11c/+0x12c/
// +0x108 buffers through the CRT free.
void ReleaseLargeRenderOwnerSlotEdiAbi(void *slot_block);

} // namespace th10
