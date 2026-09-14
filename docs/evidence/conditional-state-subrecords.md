# Conditional State Subrecords: `0x0040d280` (+ `0x0040d750`/`0x0040d810`/`0x0040d820`)

Reconstruction: `src/ConditionalStateSubrecords.cpp/.hpp`
(`InitializeConditionalStateSubrecordsEbxStackAbi`,
`TickConditionalStateRecordsFastAbi` and the two chain callbacks).

## Identity

- `0x0040d280` is called only by `0x0040d6b0` (`LoadStageEnemyDefinition`,
  a declared boundary in `src/TitleSceneSetup.cpp`), which allocates the
  0x68-byte conditional state, publishes it at `DAT_00477704`, and forwards
  its stack argument (the stage enemy script path).
- `0x0040d810`/`0x0040d820` are the scheduler callbacks registered by
  `0x0040d280`; `0x0040d750` is the shared tick body.
- `0x0040d680` (sub-object teardown) and `0x0040d530` (state destructor)
  are already reconstructed in `src/ManagerReleaseWrappers.cpp`.

## State layout (0x68 bytes, `DAT_00477704`)

| Offset | Meaning |
| ------ | ------- |
| +0x08  | ChainElem* frame ticker (calculation chain, priority 0x12) |
| +0x0c  | ChainElem* draw-chain no-op (priority 0x14) |
| +0x30  | effect-manager pool word (`DAT_004776f0` + 0x3e0b50) |
| +0x40  | i32 score copy of the frame counter |
| +0x44  | i32 frame counter |
| +0x48  | f32 rate accumulator |
| +0x4c  | const float* rate pointer (seeded `&flt_476F78` = 1.0f) |
| +0x50  | u32 block-init flag (bit 0) |
| +0x54  | 0x1098-byte script viewer (vtable `0x46d0b4`) |
| +0x58/+0x5c/+0x60 | ECL script object list head/tail/count |
| +0x10  | u32[] published-id slots indexed by `record+0x248c` |

## `0x0040d280` (EBX = state, stack = script path, ret 4, returns 0)

1. `[state+0x30] = [DAT_004776f0 + 0x3e0b50]` (no null check on the
   effect manager holder; native quirk preserved).
2. `operator new(0x1098)` for the script viewer; on success the tail words
   `+0x1090/+0x1094` are cleared, all 0x426 dwords zeroed, and vtable
   `0x46d0b4` planted; the native then dereferences the viewer vtable for
   the virtual call even when allocation failed. The virtual initializer
   is vtable slot `+0x08` = `0x0040cd20` (ECX = viewer, stack = path; it
   copies the path into the shared `DAT_00497c38` scratch and runs the
   stage-enemy-script load). Reconstructed as the boundary
   `InitScriptViewerPathEcxStackAbi`.
3. Two 0x24-byte scheduler records, built inline (not via `0x00449ed0`):
   the flags dword is `(heap-garbage & ~2) | 1`, so only OwnedByScheduler
   is guaranteed and the enabled bit is always clear. Callbacks
   `0x0040d810` / `0x0040d820`, argument = the state, registered through
   `0x00449ae0` (calculation, priority `0x12` in EDI) and `0x00449b70`
   (draw, priority `0x14`). Stored to `+0x8` / `+0xc` after registration.
4. Score/countdown block: when `+0x50` bit 0 is clear, seed it (set the
   bit), `+0x44 = 0`, `+0x40 = 0xfff0bdc1` (-999999), `+0x48 = 0.0f`,
   `+0x4c = &flt_476F78`. Then unconditionally `+0x44 = 0`, `+0x48 = 0`,
   `+0x40 = 0xffffffff` (-1) — the seed's -999999 score is always
   overwritten, mirroring the create-side pattern in
   `CreateEclScriptObjectEaxStackAbi`.

## `0x0040d750` (ECX = state, entered via `0x0040d810`)

1. Walk the list at `state+0x58`; each node lives at `record+0x116c` with
   `{record, next, prev}` at `{+0, +4, +8}`. Per record:
   - if `record+0x2480` bit `0x20000` is set, or the ECL per-frame update
     `0x0040dc80(record+0x103c)` returns nonzero, call vtable slot `+0x14`
     of `0x46d0c0` (`0x0040c5e0`, boundary `NotifyEclScriptObjectEcxStackAbi`)
     with argument 1 (the vtable pointer itself is null-checked first);
   - otherwise clear `record+0x2480` bit `0x400` (the run gate armed by
     `0x0040dc80`), allowing a re-run next frame.
2. Publish the frame counter: `[state+0x40] = [state+0x44]`.
3. Advance against the `+0x4c` rate pointer: when the rate is strictly
   inside the `0.99..1.01` band (NaN takes the fractional branch), both
   counters step by one (`+0x44 += 1`, `+0x48 += 1.0f`); otherwise
   `+0x48 += rate` and `+0x44 = 0x463b2c(+0x48)` (x87 conversion,
   round-half-away-from-zero). Returns 1.

`0x0040d820` (draw chain) is `mov eax,1; ret` — a keep-alive.

## Cross-checks

- The tick pattern is byte-compatible with
  `AdvanceAsciiOverlayTime` in `src/AsciiOverlayCallbacks.cpp` (same
  0x30/0x34/0x38/0x3c block over its own context).
- The list node layout and the `{record, next, prev}` linkage match the
  append in `CreateEclScriptObjectEaxStackAbi` and the unlink in
  `0x0040dae0` (reconstructed in `src/EclScriptObjectTeardown.cpp`).
- Note: the native always passes `record+0x103c` to `0x0040dc80`; the
  existing call site in `EclScriptLibrary.cpp`
  (`RunEclScriptSetupStackAbi(record + 0x1044U)`) is 8 bytes off relative
  to the native and should be revisited by that module's owner.
