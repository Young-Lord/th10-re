# Title State Accessors (`0x00418810` family)

Batch B reconstruction evidence for the title-state accessors and the hint
text owner lifecycle. Bodies live in `src/TitleStateAccessors.cpp`
(0x00418a90 and 0x00419090 are additionally called from
`TitleGameManagerLifecycle.cpp` / `TitleSceneSetup.cpp`).

| Address | Name | Contract |
|---------|------|----------|
| 0x00418810 | `PublishTitlePlayfieldOriginEaxAbi` | EAX = title-state playfield origin {x, y, z}. Publishes the shifted origin {x+224, y+16, z} into the three float globals at `DAT_00497d80` (guarded by the once-byte at `DAT_00497d8c`) and returns its address. |
| 0x004189e0 | `ClearTitleInputCountersEaxAbi` | Zeroes the four input counters at +0x4c..+0x58. |
| 0x00418a90 | `AdvanceTitleMenuItemIndexEaxAbi` | Advances the menu item counter at +0x50, clamped to 9. |
| 0x00418ad0 | `ToggleTitleFlagBit3EaxEcxAbi` | XOR-toggles flag bit 3 of +0x60 by the parity of the ECX/stack argument. |
| 0x00418af0 | `ToggleTitleFlagBit1EaxEcxAbi` | XOR-toggles flag bit 1 of +0x60 by the parity of the argument. |
| 0x00418b10 | `ToggleTitleFlagBit0EaxEdxAbi` | XOR-toggles flag bit 0 of +0x60 (toggle source in EDX). |
| 0x00418b30 | `ReadTitleFlagBit1EaxAbi` | Flag bit 1 read of +0x60. |
| 0x00418c10 | `ClearTitleCursorPairEaxAbi` | Zeroes the cursor pair at +0x48/+0x4c. |
| 0x00418d40 | `InitializeTitleStateListsInPlaceEdxAbi` | EDX = 0x1a8 title sub-record. Zeroes 0x6a dwords, sets flag bit 1, threads sixteen self-referencing list heads at +0x18/+0x24/.../+0xcc (0xc stride) and publishes the record at `DAT_00477814`. Returns the record. |
| 0x00418e30 | `InstallHintTextCallbacksEbxAbi` | Registers the hint calc callback 0x004198a0 (slot 0x19) and draw callback 0x004198b0 (slot 0x2c) at manager+8/+0xc, clears manager+0x10 and, when the hint gate byte `DAT_00491d6a` is set, parses hint/hint_auto.txt and hint/hint_user.txt. Returns 0. |
| 0x00419090 | `CreateHintTextOwnerEbxAbi` | Allocates the 0x1a8 hint sub-record, initializes it in place, then installs the hint callbacks. On callback failure the record is torn down (0x00418ee0) and freed; returns the record or 0. |

## Verification

Reference disassembly: `build/reference/00418810_*.asm` ..
`build/reference/00419090_*.asm`. The hint text file formats are documented
in `docs/evidence/hint-text-file.md`.
