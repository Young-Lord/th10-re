# Game-Manager Gate VM Slots (0x401d20)

Module: `src/GateVmSlots.cpp`.

The 0x491c28 game-manager slot owns a bank of 0x118-byte records at +0x154
(the per-record camera work / script bind blocks).

- `0x00401d20` `AcquireGateVmRecordSlotEbxAbi` — native EBX = record index,
  ESI = the 0x491c28 slot. Computes `record = slot + index * 0x118 + 0x154`,
  parks it at slot+0x384, refreshes the camera work over the record
  (native EDI = the 0x118-byte camera work record; modeled as
  `UpdateMainChainCameraWorkEdiAbi`), then invokes the slot's +0x8 object
  vtable slot +0xbc (__thiscall, stack = record+0xcc) to virtually bind the
  record's script area, and finally publishes the index at slot+0x388.
- `AcquireGameManagerGateVmRecordSlotEbxAbi` (helper, not a separate native
  entry) binds the fixed slot base for callers without the ESI.

The vtable-modeling typedef uses TH10_STDCALL; the native __thiscall ECX =
object convention is documented at the call site. Register-ABI specifics
remain a thunk boundary.
