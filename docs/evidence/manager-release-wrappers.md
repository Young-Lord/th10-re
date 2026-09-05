# Manager Release Wrapper Family

Implemented in `src/ManagerReleaseWrappers.cpp/.hpp`. Each native entry
takes its object in ESI (usercall), null-checks it, runs the object's
in-place destructor, and releases the outer allocation with the shared
main-chain delete 0x004524a1 (`FreeMainChainObject`):

| address | wrapper | destructor |
|---|---|---|
| 0x004148e0 | `ReleaseAsciiHudOwnerEsiAbi` | 0x004145f0 (semantic `DestroyAsciiHudOwnerInPlace`) |
| 0x00425090 | `ReleasePlayerStateBlockEsiAbi` | 0x00424ed0 (semantic `DestroyPlayerStateBlockInPlace`) |
| 0x00406140 | `ReleaseEffectManagerRootEsiAbi` | 0x00405f70 (semantic `DestroyEffectManagerRootInPlace`) |
| 0x00405730 | `ReleaseGameContextEsiAbi` | 0x00405620 (semantic `DestroyGameContextInPlace`) |
| 0x0041afb0 | `ReleaseBulletManagerEsiAbi` | 0x0041adf0 (semantic `DestroyBulletManagerInPlace`) |
| 0x0042b6d0 | `ReleaseMainChainObject840EsiAbi` | 0x0042b570 (semantic `DestroyMainChainObject840InPlace`) |
| 0x0040d730 | `ReleaseAsciiHudConditionalStateEsiAbi` | 0x0040d530 (native EAX = state, semantic `DestroyAsciiHudConditionalStateEax`) |

## Destructor family (reconstructed 2026-09-05)

All six destructors are implemented in `src/ManagerReleaseWrappers.cpp`
(declarations exported through `ManagerReleaseWrappers.hpp`; the previous
boundary externs in this file and in `TitleGameManagerLifecycle.cpp`
resolve to them). Every one follows the shared idiom: the two scheduler
chain records at +8/+0xc are removed via 0x00449f60 under the
DAT_00492274 lock with the DAT_0049231c activity-depth byte (records are
not cleared), then object-specific teardown and the DAT-published global
clear.

- **0x00424ed0 `DestroyPlayerStateBlockInPlace`** (DAT_00477834): clears
  the block global, then branches on mode-flag bit 0 (DAT_00474ca0):
  keep-alive path releases resource-matched entities for +0x10
  (0x4493e0, EAX = DAT_00491c10) and republishes +0x45c into the player
  shot entry cache (DAT_00491bf0); full path releases the large
  render-owner slot at owner+0x3AD08C (0x00447810 + shared delete
  0x004524a1 + slot clear), CRT-frees +0x45c (0x00452422) and clears
  DAT_00491bf0. The +0x36c buffer is CRT-freed and cleared on both paths.
- **0x00405f70 `DestroyEffectManagerRootInPlace`** (DAT_004776f0):
  releases resource-matched entities for the +0x3E0B50 word, clears the
  root global, then runs the eh vector destructor iterator (0x004525ff)
  over 2001 (0x7D1) 0x7F0-byte records at +0x60; the scalar dtor
  0x00405de0 frees each record's +0x360 buffer through the CRT free and
  clears the slot (modeled inline).
- **0x00405620 `DestroyGameContextInPlace`** (DAT_004776ec): only the two
  scheduler-record removals plus the global clear.
- **0x0041adf0 `DestroyBulletManagerInPlace`** (DAT_00477818): clears the
  manager global, then the eh vector destructor iterator over 2198
  (0x896) 0x3F0-byte records at +0x14; the scalar dtor 0x0041ad60 frees
  each record's +0x358 buffer through the CRT free (modeled inline).
- **0x0042b570 `DestroyMainChainObject840InPlace`** (DAT_00477840): reads
  the +0x370 buffer, clears the global, CRT-frees the buffer through
  0x00452422 and clears the slot.
- **0x0040d530 `DestroyAsciiHudConditionalStateEax`** (native EAX =
  DAT_00477704): first calls the 0x00409f90 sub-block release
  (semantic `ReleaseAsciiHudConditionalState` from
  `src/TitleCalcCluster.cpp`), removes the two records, frees each
  non-null 0x0c..0x88 word of the +0x54 sub-object through the CRT free
  (slots not cleared), tears the sub-object down via 0x0040d680 (native
  ESI = sub-object: plants vtable 0x46D0F0 at +0, CRT-frees +0x8C and
  clears it), releases the sub-object with the shared delete, zeroes
  +0x54, then — when mode flags & 9 are clear — releases the four large
  render-owner slots at owner+0x3AD090..0x3AD09C (indices 9..12 of the
  native signed slot guard, each released with 0x00447810 + shared
  delete + clear). Finally DAT_00477704 is cleared. The native leaves
  zero in EAX on both return paths; no caller consumes it, so the C++
  body is void.

Also implemented here:

- `0x0040d510` `EnableAsciiHudConditionalRecordsEaxAbi` — identical body
  to 0x00409e20 (re-enable scheduler records at +8/+0xc, word at
  record+4 |= 2; returns the +0xc record or the manager).
- `0x00409eb0` `ReleaseEclSelectMenuNamesEsiAbi` — frees each non-null
  filename pointer in the ECL menu's +0x34 array (count at +0x38) via
  the CRT free (0x00452422), then frees the array; the +0x34 slot is
  cleared twice (native quirk, preserved).

All baselines pass; CSV rows appended; IDA names applied.

## VM record scalar destructor (TH10 0x00401ff0)

`DestroyTitleScreenVmRecordInPlace` (native __thiscall ECX = the
0x3ac-byte VM record): releases the record's +0x358 buffer through the
CRT free (0x00452422) and clears the slot. This is the scalar destructor
the eh vector destructor iterators of 0x00402440 and 0x004145f0 pass;
those iterators now call this semantic body.

## Large render-owner slot release (TH10 0x00447810)

`ReleaseLargeRenderOwnerSlotEdiAbi` (native usercall EDI = the slot
block): when the +0x108 buffer exists, first releases every entity whose
+0x308 resource equals the block (`ReleaseEntitiesUsingResourceEaxEdxAbi`
over the render owner's list-A/list-B), then walks the 16-byte-stride
entry array at +0x120 (count at +0x10c): each non-null entry object is
Released through its +0x08 vtable method and its +0x04 pointer freed via
the CRT free; finally the +0x120 / +0x118 / +0x11c / +0x12c / +0x108
buffers are CRT-freed and cleared.
