# Scene Trigger Manager (0x405d00..0x4067a0)

Module: `src/SceneTriggerManager.cpp`. The 0x3e0b54-byte effect manager
root (DAT_004776f0) owns 2001 0x7f0-byte trigger records at +0x60 (the
scene-trigger pool).

- `0x00405d00` `InitSceneTriggerRecordEcxAbi` — __thiscall ECX = one
  0x7f0-byte trigger record (the eh vector scalar ctor). Clears bit 1 of
  the record's +0x74 flag, then of the nine embedded-record flag dwords at
  +0x8+{0x6c..0x378}, wipes the 0x3ac-byte embedded region at +0x8, stores
  the 0xffff sprite sentinel at +0x38c, clears the +0x408/+0x41c timer-init
  flags, and clears the ten 0x34-stride flag dwords starting at +0x624.
  Returns the record.
- `0x00405de0` `DestroySceneTriggerRecordInPlaceEcxAbi` — __thiscall ECX =
  trigger record (the eh vector scalar dtor): releases the record's +0x360
  heap buffer (0x452422 CRT free) and clears the pointer. Other fields are
  left alone.
- `0x00405e20` `LoadEffectManagerTriggerSectionEbxAbi` — the registration
  half invoked by the 0x406060 creator (which is modeled as
  `CreateEffectManagerRoot` in `src/ManagerCreation.cpp`): native EBX =
  the effect manager root. Requests the
  manager-work resource (kind 7), records the work at root+0x3e0b50, seeds
  the section state word at +0x3e07a6 to 5, points root+0x10 at the record
  pool (+0x60), and registers the two scheduler callbacks (0x406770 calc at
  priority 0x14, 0x4067a0 draw at 0x1d, both registered disabled) with
  elements at root+8 / root+0xc. Returns 0, or -1 after the 0x46cd54
  diagnostic when the resource request fails.
- `0x00405ed0` `TeardownEffectManagerTriggerSectionEsiAbi` — native ESI =
  the effect manager root. Releases the trigger-section manager work
  through the resource-matched entity release (0x4493e0), wipes the
  0xf82bc-dword record pool region at +0x60, re-points root+0x10 at the
  pool and re-seeds the +0x3e07a6 word to 5.
- `0x004065c0` `BindEffectTriggerGroupsEcxEcxAbi` — __thiscall ECX = root:
  the group bind pass over the six group descriptors.
- `0x004066e0` `TickEffectTriggerGroupEcxEaxAbi` — EAX = root, ECX = group
  index: per-group tick.
- `0x00406770` `EffectTriggerGroupCalcCallbackEcxEcxAbi` and `0x004067a0`
  `EffectTriggerGroupDrawCallbackEcxEcxAbi` — the two scheduler callback
  bodies; TH10_FASTCALL thunk adapters are provided for chain
  registration.
