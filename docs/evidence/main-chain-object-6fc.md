# Main Chain Object 6fc And Timeline Gate (0x40ae50..0x40b9f0)

Module: `src/MainChainObject6fcAndGate.cpp`. Covers the DAT_004776fc
main-chain object lifecycle, the preset scene-object spawn family over the
0x474430 configuration table, and the timeline-gate state helpers.

## Object lifecycle

- `0x0040ae50` `InitMainChainObject6fcEaxAbi` — native EAX = the 0x18-byte
  object: zero all six dwords, set flag bit 1 on the first, publish
  DAT_004776fc.
- `0x0040ae70` `RegisterMainChainObject6fcRecordsEbxAbi` — native EBX =
  the 0x18-byte object. Requests the manager-work resource (kind 7) for
  the object and stores it at +0x10 (failure: 0x46cf74 diagnostic, return
  -1). Registers two disabled scheduler records with the object as
  argument: calc 0x40b050 (always ready) at priority 0x17 and draw
  0x40b060 (always ready) at priority 0x20; elements land at +8 / +0xc.
  Returns 0.
- `0x0040af00` `DestroyMainChainObject6fcInPlaceEaxAbi` — native EAX =
  object, one stack argument (ret 4, the object again through +8/+0xc).
  Under the scheduler lock, removes the +8 and +0xc chain elements
  (0x449f60), then clears DAT_004776fc.
- `0x0040af90` `CreateMainChainObject6fc` — `operator new(0x18)` + init +
  register; on failure destroys and frees. Returns the object or null.

## Preset scene-object spawns (0x474430 table)

Table entry (0x10 bytes): u16 preset id, void init(ECX = entity, EDX =
param), u32 field_398, u32 field_39c. Shared body: allocate a pool VM
(0x449950), set the 0x40000000 preset flag at vm+0x35c and kind 0xf at
vm+0x20, apply the preset clone (0x4489c0 family), link the entity and
assign the id (0x4489d0), resolve the handle, run the entry's init callback
and copy its two parameter dwords into +0x398/+0x39c.

- `0x0040b070` `SpawnPresetSceneObjectFromTableEaxStackAbi` — native EAX =
  table index, stack (out_record, param, unused; ret 0xc). The native
  reuses the out_record slot as the assigned-id scratch and pre-loads
  out_record+0x10 — a dead store against the id write, preserved in the
  evidence notes. Publishes the resolved entity through *out_record.
- `0x0040b110` `SpawnPresetSceneObjectFromTableAltEaxStackAbi` — same spawn
  without the out_record+0x10 pre-load.
- `0x0040b1b0` `SpawnPresetSceneObjectByHandleStackAbi` — handle-based
  variant (ret 0x1c): stack (out_handle, param, u16 clone id, init edx
  param, init fn, field_398, field_39c); *out_handle receives the resolved
  entity (0 when the resolve fails).

## Timeline gate helpers

- `0x0040b460` `GetTimelineInnerFlag1EaxAbi` — bit 1 (0x2) of the +0x74
  flags of the inner timeline state.
- `0x0040b470` `GetTimelineInnerFlag2EaxAbi` — bit 2 (0x4).
- `0x0040b510` `GetTimelineGateFlag1EaxAbi` — bit 1 (0x2) of the +0x20
  gate word of the gate state.
- `0x0040b530` `InitTimelineGateStateEaxAbi` — native EAX = the 0x28-byte
  gate state: zero it, set flag bit 1, publish DAT_00477700.
- `0x0040b9f0` `PollTimelineGateAutoAdvanceEdiAbi` — native EDI = gate
  state (calc-gate poll). When the inner timeline controller (gate+0x18)
  still reports active (0x40bd20), publishes the step length selector:
  DAT_00491fb8 = (DAT_00491ff4 & 0x1000) ? 2 : 15, and returns 1.
  Otherwise increments the gate frame counter and applies the skip ladder:
  return 1 when the inner +0x74 flags have bit 2, the gate word has bit 1,
  the inner flags lack bit 1, or the 0x474e30 input bank lacks bit 8; when
  the counter is not a multiple of 12 return 6; else return 1.
