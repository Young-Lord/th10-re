# Stage Host Teardown: `0x0040a1a0`

Reconstruction: `src/StageHostTeardown.cpp/.hpp`
(`DestroyStageHostObjectStackAbi`).

## Identity

The 0x688-byte stage-host object (allocated by the factory `0x0040a3c0` via
`operator new(0x688)` + constructor `0x0040a040`, published through
`DAT_004776f8`, and driven by the continuation worker `0x0040a350` — the
thread entry referenced as `0x0040a340` in `src/EclSelectMenu.cpp`) is
destroyed here:

- `0x0040a3c0`: init failure path — `0x0040a1a0(host)` then free, return 0.
- `0x0040a410`: scalar deleting destructor — `0x0040a1a0(ECX=host, stack
  flags)`; stack bit 0 requests the outer free.
- `0x0040a430`: guarded delete helper.

## Native ABI

One stack argument (ret 4) = the host. The native carries an SEH frame
(handler `0x004657e9`); no caller exercises the unwind path, so the
reconstruction follows the project convention of omitting it.

## Body (order preserved)

1. `0x00409eb0(ESI = host)` — release the ECL select-menu name table
   (`ReleaseEclSelectMenuNamesEsiAbi`, `src/ManagerReleaseWrappers.cpp`).
2. Scheduler records at `host+0x8` and `host+0xc`: each non-null one is
   removed through `0x00449f60` under the scheduler critical section
   (`DAT_00492274`) with the `DAT_0049231c` activity byte incremented —
   exactly `CallbackSchedulerApi::RemoveSynchronized` with
   `g_CallbackScheduler` (`DAT_00491be4`).
3. Destroy-and-free every published manager global, in native order, each
   through its in-place destructor followed by the shared delete
   `0x004524a1`:

   | Global | Destructor |
   | ------ | ---------- |
   | `DAT_0047770c` | `0x004145f0` |
   | `DAT_00477834` | `0x00424ed0` |
   | `DAT_004776f0` | `0x00405f70` |
   | `DAT_00477704` | `0x0040d530` (EAX) |
   | `DAT_004776ec` | `0x00405620` |
   | `DAT_00477818` | `0x0041adf0` |
   | `DAT_00477840` | `0x0042b570` |

   All seven are already reconstructed (`ManagerReleaseWrappers.cpp`,
   `TitleGameManagerLifecycle.cpp`); the wrappers
   `Release*EsiAbi` model the destructor+free pairs.
4. `DAT_004776f8 = 0` (the published host holder is cleared first), then
   the host's `+0x620` buffer is released through the CRT free
   (`0x00452422`) and the slot cleared.
5. The embedded thread control block at `host+0x10`: plant the
   `ThreadControl` vtable `0x4703e4`, then `0x0044c150` with ESI =
   `host+0x10` (`StopThreadControl`), matching the pattern in
   `DestroyGlobalLifecycleManagerInPlace`.
