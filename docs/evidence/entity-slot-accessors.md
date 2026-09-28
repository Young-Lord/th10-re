# Entity Slot Accessors (`0x00412a10` family)

Batch B reconstruction evidence for the small entity-record slot accessors
used by the ECL script library and entity update code. All bodies live in
`src/EntitySlotAccessors.cpp` (0x00413270's polar helper is also referenced
from `EnemyDeathEffects.cpp`).

| Address | Name | Contract |
|---------|------|----------|
| 0x00412a10 | `ResolveEntitySlotPointerEaxEcxAbi` | EAX = scheduler/VM context, ECX = slot index; returns the typed slot pointer for the ECL script library callers. |
| 0x00412d60 | `CopyVec2PairToSlotBaseEaxEcxAbi` | 2-dword copy from the EAX source pair to the ECX destination record at +0x0. |
| 0x00412d70 | `CopyVec2PairToSlot8EaxEcxAbi` | Same copy at destination +0x8. |
| 0x00412d80 | `CopyVec2PairToSlot10EaxEcxAbi` | Same copy at destination +0x10. |
| 0x00412d90 | `CopyVec2PairToSlot18EaxEcxAbi` | Same copy at destination +0x18. |
| 0x00412db0 | `RearmAnimTimerAt20EaxAbi` | Re-arms the animation timer at +0x20; the lazy timer seed mirrors the interpolator tail defaults. |
| 0x00412ed0 | `SetAnimDurationInvalidateCacheEaxEcxAbi` | Stores the animation duration (ECX) and invalidates the dependent cache field. |
| 0x00412f70 | `SetSlotPairFromStackEaxStackAbi` | Plain 2-dword store from the stack pair (`retn 8`). |
| 0x00413120 | `CopyVec3ToSlotCEaxEcxAbi` | Stores the EAX source vec3 into the ECX destination record at +0xc. |
| 0x00413170 | `WrapAngleIntoSlot1CEcxStackAbi` | Wraps the stack angle into -pi..pi and stores it at destination +0x1c (`retn 8`). |
| 0x004131a0 | `ReadEntityFlagBit3EaxAbi` | Reads bit 3 of the flag dword at record +0x60. |
| 0x00413220 | `CopyVec3EaxEcxAbi` | Plain 3-dword copy from EAX to ECX. |
| 0x00413270 | `SetPolarVec2SinCosScaledEaxStackAbi` | Native `fsincos` of the raw angle; the sine branch is scaled by the stack sine scale before the pair is stored. |

## ABI notes

These entries use the raw native register ABIs (EAX/ECX/EDX and stack
arguments with `retn N` tails); the semantic bodies take the same values as
plain C++ arguments. No thunks are required for the syntax-level
reconstruction; the register-ABI facts are preserved in the names and
comments so a future native-boundary pass can emit the register moves.

## Verification

Reference disassembly: `build/reference/00412a10_*.asm` ..
`build/reference/00413270_*.asm`. All 13 entries compile clean under
`g++ -m32 -std=c++98 -fsyntax-only`.
