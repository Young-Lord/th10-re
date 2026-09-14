# ECL Script Object Teardown: `0x0040dae0` (+ deleting dtor `0x0040cc50`)

Reconstruction: `src/EclScriptObjectTeardown.cpp/.hpp`
(`DestroyEclScriptObjectInPlaceStackAbi`,
`ReleaseEclScriptObjectDeletingEcxStackAbi`).

## Identity

- Sole caller of `0x0040dae0`: `0x0040cc50`, the scalar deleting destructor
  (vtable `0x46d0c0` slot `+0x10`) of the 0x2518-byte ECL script object
  created by `CreateEclScriptObjectEaxStackAbi` (`0x0040cfb0`,
  `src/EclScriptLibrary.cpp`).
- The record's in-place constructor `0x0040d830` is a declared boundary;
  the per-frame update `0x0040dc80` is `RunEclScriptSetupStackAbi`.

## `0x0040cc50` (ECX = record, stack = delete flags, ret 4)

`0x0040dae0(record)`; if stack-arg bit 0 is set, free the record through
`0x004524a1`; returns the record.

## `0x0040dae0` (stack = record, ret 4)

1. Plant the live vtable `0x46d0c0` (destructor prologue restores the
   final-class vtable before any release work).
2. Unlink the embedded node at `record+0x116c` (`{record, next, prev}` at
   `{+0,+4,+8}`) from the conditional state (`DAT_00477704`) list: fix the
   `+0x58` head and `+0x5c` tail when they point at the node, fix
   `next->prev` / `prev->next`, clear both links, and decrement the
   `+0x60` count. Only `+0x60` is decremented even though creation bumps
   both `+0x60` and `+0x64` (native quirk; `+0x64` is never undone).
   The `DAT_00477704` holder is dereferenced without a null check.
3. Published-id slot: when `record+0x2480` bit `0x8000` is set (the same
   flag that gates slot registration on creation), clear
   `[state + 0x10 + record+0x248c * 4]`. The `+0x248c` read itself is
   unconditional in the native (garbage index otherwise) but the store is
   gated.
4. Soft-release the ten entity ids at `record+0x10fc` (stride 4): each
   nonzero id goes through the inlined `0x004492a0` — scan the
   render-owner entity lists at `+0x72dad4`/`+0x72dadc`, set flag
   `0x4000000` at `entity+0x35c`, and propagate to every child in the
   `+0x14` list while `entity+0x18` is clear. Reconstructed as
   `ReleaseEntityById` (`src/EntityHelpers.cpp`).
5. Player block (`DAT_00477834`): if `[player+0x3504] == record`, clear
   the slot and the byte flag at `+0x3508`; then scan the 0x80-entry table
   at `player+0x4e8` (stride 0x5c) clearing every entry equal to the
   record.
6. Plant the destruction vtable `0x46d0d8`, then free the script-name
   list at `record+0x1034`: each node owns the name buffer (`node[0]`)
   and the next node (`node[1]`), both released through `0x004524a1`.
   The record itself is not freed here (the deleting wrapper owns that).
