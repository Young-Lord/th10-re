# Stage Object Manager (`0x0041c0b0` family)

Batch B reconstruction evidence for the stage object manager, its node list
callbacks, and the stage-entity accessors. Bodies live in
`src/StageObjectManager.cpp`; the vtable slot targets (0x41c030 table
family) are documented in `docs/evidence/stage-object-vtable.md`.

| Address | Name | Contract |
|---------|------|----------|
| 0x0041c0b0 | `LinkStageObjectNodeEaxEcxAbi` | Push-front into the doubly linked node list: head at manager+0x434, count at manager+0x438, node next at +4 / prev at +8. |
| 0x0041c120 | `InitializeStageObjectManagerEbxAbi` | Requests manager work slot 7 for bullet.anm, installs the manager calc callback 0x0041c480 (slot 0x13) and draw callback 0x0041c4e0 (slot 0x1b) at manager+8/+0xc, and seeds the node list head at manager+0x434 with manager+0x10. Returns -1 when the work slot is unavailable, else 0. |
| 0x0041c1c0 | `DestroyStageObjectManagerStackAbi` | Stack = manager (`retn 4`). Releases both scheduler callbacks, then walks the node chain at manager+0x18: each node is released through its vtable slot 4 (+0x10), unlinked, and freed. Clears the manager global `DAT_0047781c`. |
| 0x0041c290 | `CreateStageObjectManagerEbxAbi` | Allocates the 0x45c manager, runs the header default initializer at manager+0x10, then zeroes the whole 0x45c block (the native wipes the header defaults it just wrote), publishes it at `DAT_0047781c` and initializes the callbacks. On failure the manager is destroyed and freed; returns it or 0. |
| 0x0041c480 | `StageObjectManagerCalcCallbackEcxAbi` | Gates on the main chain state word at `DAT_00477810`+0x58: bits 0 or 2 (or bit 10) short circuit to 1. Bit 1 saves the frame-time scale dword, zeroes it, ticks the manager slots (0x0041c330) and restores the scale. Returns the tick result (or 1 on the gate paths). |
| 0x0041c4e0 | `StageObjectManagerDrawCallbackEcxAbi` | Skipped while the main chain has flag bit 2 of +0x58; otherwise walks the node chain at manager+0x18 and calls vtable slot 3 (+0xc) on every node whose +0xc kind is not 1. Returns 1. |
| 0x0041c760 | `BroadcastStageObjectSpawnEaxEbxStackAbi` | Stores the position at manager+0x440 and the velocity at manager+0x44c, then walks the node chain and sums vtable slot 6 (+0x18) over every node whose +0xc kind is not 1. Returns the sum (`retn 8`). |
| 0x0041c7d0 | `MarkStageObjectsPendingEaxAbi` | Walks the node chain and sets the pending byte at node+0x50 for every node whose +0xc kind is not 1. Returns 0. |
| 0x0041c880 | `SumStageObjectCounterVirtualEaxStackAbi` | Walks the node chain and sums vtable slot 8 (+0x20) over every node whose +0xc kind is not 1 (`retn 8`). |
| 0x0041d7c0 | `SpawnBossDropItemVmsEcxAbi` | Seeds the two item-drop VM position records (+0x934 / +0xce0) from the boss position offset by (+224, +16), stores the +0x2c depth, wraps the +0x3c angle by pi/2 into the +0x600 VM's +0x2c, raises VM flag bit 2 (+0x35c) and dispatches the render mode; the second VM at +0x9ac is only spawned while the +0x4c selector is zero. Returns 0. |
| 0x0041e4d0 | `StageObjectRotatedBoxHitA_Thiscall` | ECX = stage object (position +0x24/+0x28, angle +0x3c, half extents +0x40/+0x44), stack = {point vec2, radius} (`retn 8`). Rotated-box hit test of the point expanded by the radius against the object's oriented box: returns 2 on hit, 0 on miss. |
| 0x0041f670 | `StageObjectRotatedBoxHitB_Thiscall` | Identical twin of 0x0041e4d0 (kind B table entry). |
| 0x0041f820 | `SetStageEntityFocusFlagEaxAbi` | Sets flag bit 4 (+0x10) and clears flag bit 5 (+0x20) of the +0x35c state word (the focus / unfocus transition pair). |
| 0x0041f850 | `InitializeStageEntityRecordEsiAbi` | Installs the stage-entity vtable (0x004703e4), clears the kind flags, zeroes the 0x3ac script region, writes 0xffff into the +0x378 word, then zeroes the whole 0x3f0 record again, sets flag bit 1 and publishes it at `DAT_00477820`. Returns the record. |
| 0x0042ba70 | `SetStageEntityField4cRaiseFlag3EaxStackAbi` | EAX = stage entity, stack = value (`retn 4`). Stores the value at +0x4c and raises flag bit 3 (0x8) of the +0x35c state word. The binary holds no direct references to this entry (no code or data xrefs); the body is reconstructed from the isolated reference disassembly `build/reference/0042ba70_*.asm`. |

## Reconstructed in this batch

`0x0042ba70` (`SetStageEntityField4cRaiseFlag3EaxStackAbi`) is new. Its full
native body is:

```asm
42ba70: mov  ecx,[esp+0x4]
42ba74: mov  [eax+0x4c],ecx
42ba77: or   dword ptr [eax+0x35c],0x8
42ba7e: ret  0x4
```

The +0x4c / +0x35c field pairing matches the 0x3f0 stage-entity record used
by 0x0041f820 and 0x0041d7c0, so the entry is grouped with the stage-entity
accessors despite having no callers.
