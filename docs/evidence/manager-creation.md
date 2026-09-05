# Manager Creation Wrappers (TH10 0x00406060 / 0x00414830)

Implemented in `src/ManagerCreation.cpp/.hpp`.

## 0x00406060 `CreateEffectManagerRoot`

- `operator new(0x3e0b54)`; on success the eh vector constructor
  iterator (0x0045252d) runs over 2001 records of 0x7f0 bytes at +0x60
  with the ctor pair 0x00405d00 / 0x00405de0 (kept as raw addresses —
  the native pushes exactly these two).
- Native order quirk preserved: the full-object zero wipe runs AFTER the
  array construction (the constructed array is wiped again).
- Publishes the object to DAT_004776f0, then calls the 0x00405e20
  loader (boundary `LoadEffectManagerContent`); a failed load destroys
  the object (0x00405f70 semantic `DestroyEffectManagerRootInPlace`)
  and frees it with the shared delete (0x004524a1), returning null.

## 0x00414830 `CreateAsciiHudOwner`

- `operator new(0x9ed0)`; constructor 0x00413810 (native EAX = object,
  boundary); then the 0x00413980 loader (boundary); a failed load
  destroys via the semantic 0x004145f0 `DestroyAsciiHudOwnerInPlace`,
  frees, and returns null.

## Callers

0x0040a350 (both) and the 0x417870 result-screen region.

## Status

Baselines pass; CSV rows appended; IDA names applied.

## 0x00425020 `CreatePlayerStateBlock`

- `operator new(0x4478)`; constructor 0x004246c0 (boundary, native
  EAX = object); then the player initializer 0x004247f0 via the semantic
  `InitializePlayerObject`; a failed init destroys via the 0x00424ed0
  boundary `DestroyPlayerStateBlockInPlace`, frees, and returns null.
