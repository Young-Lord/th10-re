# Effect Pool Lifecycle (`0x0041ac20` family)

Batch B reconstruction evidence for the effect-pool manager lifecycle and
its record accessors. Bodies live in `src/EffectPoolLifecycle.cpp`
(0x0041aed0 is additionally called from `StageEffectHost.cpp` /
`TitleSceneSetup.cpp` as the "bullet manager root" creation step).

| Address | Name | Contract |
|---------|------|----------|
| 0x0041ac20 | `CopyTipPairEaxEcxAbi` | EAX = source pair, ECX = destination record; 2-dword copy to destination +0 (tip record accessor). |
| 0x0041ac30 | `CopyTipPairToSlot8EaxEcxAbi` | Same 2-dword copy to destination +8. |
| 0x0041ac80 | `SetTipPairFromStackEaxStackAbi` | EAX = destination, two stack dwords (`retn 8`): plain 2-dword store into the tip record. |
| 0x0041acf0 | `ResetStageEntityRecordEcxAbi` | ECX = 0x3f0 stage entity record. Clears flag bit 0 at +0x6c/+0xb0/+0xfc/+0x128/+0x174/+0x1b0/+0x1fc/+0x228/+0x378, zeroes the 0x3ac script region, writes 0xffff into the +0x384 word and clears bit 0 of +0x3d8. Returns the record. Installed as the per-record reset of the effect pool. |
| 0x0041ad60 | `FreeEntityRibbonBufferEcxAbi` | ECX = entity. Frees the radial ribbon buffer at +0x358 (allocated by 0x004452f0) and clears the pointer. Installed as the per-record release of the effect pool. |
| 0x0041ad90 | `InstallEffectPoolCallbacksEbxAbi` | Registers the effect-pool calc callback 0x0041ba00 (slot 0x15) and draw callback 0x0041ba30 (slot 0x19) at manager+8/+0xc. Returns 0. |
| 0x0041aed0 | `CreateEffectPoolManagerEbxAbi` | Allocates the 0x21cec0 effect pool, installs the reset (0x0041acf0) / ribbon-free (0x0041ad60) pair through the 0x0045252d pool iterator over 0x3f0-byte records at pool+0x14 (0x896 count), zeroes the pool, publishes it at `DAT_00477818` with flag bit 1, then installs the callbacks. On callback failure the pool is torn down (0x0041adf0) and freed; returns the pool or 0. |
| 0x0041ba30 | `TickEffectPoolSlotsGuarded` | Gate over `TickEffectPoolSlots`: returns 1 while the main chain context (`DAT_00477810`) has flag bit 2 of +0x58 set, otherwise falls through to the slot tick. |

## Verification

Reference disassembly: `build/reference/0041ac20_*.asm` ..
`build/reference/0041ba30_*.asm`. The per-record update half of the pool is
documented in `docs/evidence/effect-pool-entity-update.md`.
