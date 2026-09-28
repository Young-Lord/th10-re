# Global Lifecycle Helpers (`0x0041fac0` family)

Batch B reconstruction evidence for the global lifecycle manager callback
installation and the loading-screen entity staging. Bodies live in
`src/GlobalLifecycleHelpers.cpp`; the factory 0x0041fd00 is a source-level
factory in `src/GlobalLifecycleManager.cpp` (see
`docs/evidence/global-lifecycle-manager.md`) and 0x00420100 lives in
`src/VersionData.cpp` (see `docs/evidence/version-data-initialization.md`).

| Address | Name | Contract |
|---------|------|----------|
| 0x0041fac0 | `InstallGlobalLifecycleCallbacksEbxAbi` | EBX = GlobalLifecycleManager. Allocates the calc callback node for 0x0041feb0 (registered on the calc scheduler with slot 3) and the draw callback node for 0x0041fef0 (draw scheduler, slot 2), binds the manager at node+0x20 and stores both at manager+8/+0xc. Then stops the thread control block at +0x10, seeds startup_thread_proc (+0x28), thread_running (+0x20) = 1, close_requested (+0x1c) = 0 and starts the CRT worker through `_beginthreadex` with the manager as the argument (handle at +0x14, id at +0x18). Returns 0. |
| 0x0041fd00 | `CreateGlobalLifecycleManager` | The factory; the original's ESI constructor / EBX initializer split is expressed as a source-level factory in `GlobalLifecycleManager.cpp`. |
| 0x0041fdd0 | `CreateLoadingScreenEntitiesStackAbi` | Stack = manager (`retn 4`). Advances the loading-screen entity stages: startup_stage (+0x3e4) == 1 creates the preset text slot node (kind 15, +0x40000000 flag, preset clone 0), links it and parks it at +0x3dc; draw_stage (+0x3e8) == 1 creates the continuation render object (kind 6) into `DAT_004776e0`+0x89a4 when that slot is still empty. Always bumps draw_frame_count (+0x3ec) and returns 1. |
| 0x0041ff00 | `MarkPauseChainFlagsUncheckedEaxAbi` | Raises flag bit 1 (0x2) on the three records at owner+0xc / +0x10 / +0x89a8 with no null checks (the native dereferences all three unconditionally). |
| 0x00420100 | `InitializeVersionData` | Initializes the version data block; the binary reports failures but its caller deliberately ignores them. |

## Build note

The file requires `Th10Platform.hpp` for `TH10_STDCALL` (the
`_beginthreadex` start-routine typedef and the 0x0041f990 startup thread
body declaration are `__stdcall` in the native ABI).

## Verification

Reference disassembly: `build/reference/0041fac0_*.asm`,
`build/reference/0041fdd0_*.asm`, `build/reference/0041ff00_*.asm`.
