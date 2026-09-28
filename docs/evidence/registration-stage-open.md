# Registration Stage Open (`0x00413350` family)

Batch B reconstruction evidence for the registration/stage-open manager
helpers. Bodies live in `src/RegistrationStageOpen.cpp`
(0x00413980 also has declarations referenced by `ManagerCreation.cpp`;
0x00414570 is called from `TitleGameManagerLifecycle.cpp`).

| Address | Name | Contract |
|---------|------|----------|
| 0x00413350 | `CreateRegistrationDrawOwner` | Allocates the 0x8c registration draw owner, zeroes it, publishes it at `DAT_00477708` with flag bit 1, allocates the 0x24 scheduler callback node for the draw callback 0x00413690, binds the owner at node+0x20 and registers it on the draw scheduler (slot 0x2f). Returns the owner (0 on allocation failure, with the node still registered against a null owner). |
| 0x004136c0 | `ReleaseRegistrationOwnerHandleChainEsiAbi` | ESI = owner. Releases the six entity handles at owner+0x40..+0x54 through `ReleaseEntityById` and clears them. |
| 0x00413740 | `SetManagerRegistrationFlagEaxAbi` | Sets flag bit 4 of +0x9eb4 and clears the +0x9ecc staging pointer. |
| 0x00413760 | `ReadManagerRegistrationFlagEaxAbi` | Flag bit 4 read of +0x9eb4. |
| 0x00413780 | `ReadManagerPostRegistrationFlagEaxAbi` | Flag bit 5 read of +0x9eb4. |
| 0x00413980 | `OpenStageScriptSequenceEbxAbi` | Requests manager work slot 6 for the stage script (fails with the logger on the reserved global when unavailable), opens the scene script resource, then installs the stage calc callback 0x00415ae0 (slot 0x18) and draw callback 0x00415af0 (slot 0x2b) at manager+8/+0xc. |
| 0x00414570 | `ReleaseRegistrationOwnerFromManagerEbxAbi` | Frees the registration owner at +0x9eb8 (handle chain first) and, unless `DAT_00474ca0` bit 0/3 is set, also releases the manager work buffer at `DAT_00491c10`+0x3ad0dc and the +0x9ebc script pointer, clearing +0x9e80. |

## Verification

Reference disassembly: `build/reference/00413350_*.asm` ..
`build/reference/00414570_*.asm`. The draw-owner creation/release cycle was
cross-checked against `docs/evidence/registration-draw-owner.md` and the
0x00413690 draw callback documented there.
