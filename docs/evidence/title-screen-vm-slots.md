# Title Screen VM Record Slots (0x402050..0x402640)

Module: `src/TitleScreenVmSlots.cpp`. The title screen VM records are
0x3ac-byte records (same layout as the shared pool VM); the native eh
vector constructor/destructor pair drives them.

- `0x00402050` `InitTitleScreenVmRecordEcxAbi` — __thiscall ECX = one
  0x3ac-byte record; the eh vector constructor's scalar callback. Clears
  the nine busy-flag dwords (0x34-stride walk), wipes the whole 0xeb-dword
  record, stores the 0xffff sprite-index sentinel at +0x384 and returns the
  record. Shared by the spell/bullet manager pools (0x408910 constructs its
  8 + 5 record pools through this entry via the 0x45252d eh iterator).
- `0x004020b0` `ClearTitleScreenVmRecordFlagsEaxAbi` — EAX = record; clears
  only the nine busy-flag dwords (record recycling without a full reset).
- `0x00402160` `InitTitleScreenVmHostStackAbi` — stack = host (ret 4);
  initializes the 0x2b64-byte title VM host record and sets flag bit 1 of
  the first dword.
- `0x00402640` `CreateTitleScreenVmHostStackAbi` — stdcall (stack:
  stage-data name, priority base; ret 8): `operator new(0x2b64)` +
  InitTitleScreenVmHostStackAbi, then
  `CreateTitleScreenStateEaxEcxStackAbi(host, name, priority)`. On a
  nonzero result (or a null allocation) the host is destroyed in place,
  freed and null is returned. The native runs the state constructor even on
  a null host (quirk preserved by the same call order).

The stage-scene setup calls the 0x402640 entry to open the mode record
bank; `src/TitleSceneSetup.cpp` declares it under its older boundary alias.
