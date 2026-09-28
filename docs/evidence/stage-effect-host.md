# Stage Effect Host (0x40a040..0x40add0)

Module: `src/StageEffectHost.cpp`.

- `0x0040a040` `InitStageHostObjectEdxAbi` — native EDX = the 0x688-byte
  stage host. Initializes the host defaults (see the module header for the
  field walk).
- `0x0040a130` `RegisterStageHostSchedulerRecordsEbxAbi` — native EBX =
  stage host. Selects the default scene configuration and registers the
  host scheduler callbacks.
- `0x0040a3c0` `CreateStageHostObject` — `operator new(0x688)` + init +
  register; on a registration failure destroys (0x40a1a0), frees and
  returns null; otherwise returns the host (quirk order preserved: the
  native runs the registration even on a null host).
- `0x0040ada0` `SelectStageConfigTableEcxEaxAbi` — native EAX =
  configuration index, ECX = stage host. Publishes the selected scene
  configuration table (DAT_00477748 = 0x474788 + index * 0x30) and stores
  the index at host+0x3c and host+0x40.
- `0x0040add0` `ResetStageHostSubrecordDefaultsEaxAbi` — native EAX = the
  record to reset (usercall). The entry carries no cross-references in the
  canonical binary and stands alone between 0x40ada0 and 0x40ae00 (dead
  entry preserved by the linker): clears +0x8c, +0x0 and +0xd4, seeds
  +0xd0 with flag bit 1 and sets the +0x8 counter to 999 (0x3e7).
- `0x0040a350` `EnterGameModeSetupEsiAbi` — native ESI = the game manager.
  Full game-mode entry: creates the player state block, the effect manager
  root, the game context, the two remaining manager roots (0x41aed0 /
  0x42b660), the stage conditional state (0x40d6b0) from the manager's
  selected stage script, and the ASCII HUD owner; finally shifts the
  manager's +0x1ec cursor record by +1 and -1 to refresh it (seeding
  +0x1f4 from the conditional state's +0x54 sub-record cursor first) and
  sets the +0x30 state to 1. Returns 0.

Related: the 0x4086b0 play-field bounds test used by the enemy activation
sweep is documented in `docs/evidence/player-stage-helpers.md`.
