# Game Context Lifecycle (0x4055c0 / 0x4056b0)

Module: `src/GameContextLifecycle.cpp`. The game-context object
(DAT_004776ec, 0x48 bytes) owns the scheduler records driving the popup
update (0x405840 -> the EDI-ABI popup updater 0x405750) and the
always-ready draw stub (0x405850).

- `0x004055c0` `RegisterGameContextSchedulerRecordsEbxAbi` — native EBX =
  the game-context object. Allocates two scheduler chain elements
  (0x449ed0), enables both (flags |= 2), points each element's argument at
  the context, registers the popup-update callback on the calculation chain
  at priority 0x11 and the draw stub on the draw chain at priority 0x22,
  and publishes the elements at context+8 / context+0xc. Returns 0.
- `0x004056b0` `CreateGameContextObject` — `operator new(0x48)` for the
  game-context object; the native wipes 0x12 dwords (dead store against the
  trailing flag set), sets bit 1 of the first dword and publishes
  DAT_004776ec, then calls the registration above. On a registration
  failure (nonzero result) or a null allocation the object is destroyed in
  place (0x405620) and freed; the null-allocation path still runs the
  registration (quirk preserved by the same call order).

The thunk bodies (0x405840 / 0x405850 adapters) stay ASM boundaries; the
semantic module only models the registration targets as
TH10_FASTCALL declarations.
