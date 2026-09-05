# Timeline Record Interpreter Evidence

`0x0040bd80` consumes a variable-length stream of records from state `+0x54`.
Each record is `{ u16 trigger_tick, u8 opcode, u8 payload_bytes, payload[] }`.
The normal advance is `record + 4 + payload_bytes`, with no bounds checks.

The embedded timer at `+0x18` is advanced through the native shared rate
logic. Due records dispatch opcodes 0 through 17. Opcode 0 returns `-1`;
opcode 7 installs a continuation, releases owner slot `record[+4]+0x1d`,
stores `record+8` at `+0x70`, and advances with the normal payload step;
opcode 12 clears
`0xec` bytes of state after loading a replacement stream, initializes its three
timer blocks, and the common tail skips the replacement stream's first record.
Opcodes 13 and 14 create kinds 0 and 5 ASCII overlays with priority 49.

`0x0040bd20` is the outer controller. It calls `0x0040bd80`, returns one when
the interpreter returns any nonzero value, and otherwise advances a distinct
embedded rate timer at state `+0x00..+0x10`. This is separate from the record
timer at `+0x18..+0x28` advanced by the interpreter itself.

`0x0040c480` and `0x0040c4d0` resolve a slot handle through `0x4491c0`, write
kind 2 or 3 to object `+0x304`, and only when object `+0x18` is zero traverse
the singly linked child list at `+0x14`.

## Stream Replacement

`0x0040b480` copies the requested path into `DAT_00497c38`, loads through the
packed archive helper, frees any prior gate-state buffer at `+0x14`, and stores
the new pointer there. Opcode 12 uses this result, zeroes `0xec` bytes of
timeline state, reinitializes all three timer blocks, sets flags/color, and
positions the record pointer past the replacement stream header dword.

## Continuation Chain

Opcode 7 calls `0x0040c540` with `(480.0f, 392.0f)`, which creates a kind-6
render object through `0x00448d50` when `AsciiManager+0x89a4` is zero. The
interpreter stores `record+8` at state `+0x70`, clears the owner work slot at
`record[+4]+0x1d` through `0x004477d0`, sets flags bit 2, registers
`0x0040c3a0` through `0x0044c1c0`, then advances the record pointer with the
normal `4 + payload_bytes` step before returning zero.

`0x0040c3a0` loads `DAT_00477700+0x18` and runs `0x0040c3c0`, which copies the
continuation path into the shared scratch buffer, calls `0x00447280` with slot
`record[+4]+0x1d`, stores the returned manager work at `state+0x80[index]`,
clears flags bit 2, and releases the continuation handle through `0x00409e50`
(`ReleaseTimelineContinuationHandle`).

## Render-Object Cluster

Timeline source slots at state `+0x80` hold manager-work pointers, not render
handles. `0x00448d00` always allocates a fresh owner node, stores the kind at
node `+0x20`, and clones through `0x00449870` -> `0x0043e7e0` using the
manager-work pointer directly. Failure zeroes `0xeb` dwords of the node.

`0x00448d50` copies the three float parameters to node `+0x340..+0x348`, then
binds through `0x0043e710`. Both clone paths initialize the embedded timer at
node `+0x5c..+0x6c`, call the shared `0x0043ee30` tail, and increment owner
`+0x4c`.

`0x0043ee30` (`FinalizeTimelineRenderObjectSetup`) interprets manager-work setup
bytecode at node `+0x390` until timer tick `+0x60` reaches the current
instruction limit at `pc+4`. Records are `{ u16 opcode, u16 step, u8 flags,
operands[] }` with PC advance `pc += step`. Opcodes `0x0001`/`0x0002`/`0xffff`
fail out; `0x0045`/`0x003f` restart on kind records (`0x0040`). The epilogue
scales position/anchor fields when scale operands equal the `0.0f` sentinel,
wraps phase at `+0x54/+0x58`, applies global offsets when bit `0x2000` is set,
samples gated anim blocks, builds polylines when flags equal `0x02400000`, and
advances the embedded timer at `+0x5c`.

Handle lookup/release helpers (`0x4491c0`, `0x4492a0`, `0x449630`) walk the
owner's two intrusive lists and preserve the native release-flag and kind
propagation rules.

## Text Submission

`0x00417010` decrypts opcode-3 payload bytes with key `0x77`, step `+7`, then
step `+0x10` per byte into scratch `DAT_00497d40`.

`0x00447a50` copies the format string locally, reads font size from object
`+0x3a0` defaulting to `0x11`, resolves the surface at
`object[+0x394]->+4`, passes selected flag `(object[+0x360]>>1)&1` into
`0x004479d0`, and sets object `+0x35c` bit 0. The dispatch at `0x004479d0`
routes to `0x00437db0` (shadow+fill) or `0x00437fe0` (selected tint) through
`GeneratedSurfaceText.cpp`, which preserves the font-size gate, generated-surface
memset/tint, scanline post-process, neighbour smooth, and upload tail. Imported
GDI/D3DX calls remain link boundaries.

## Audio Actions

`0x00420a90` copies the path, rewrites the extension to `.wav`, and queues BGM
opcode 1. `0x00420b10` optionally queues opcode 4, always queues opcode 2, and
sets `DAT_0047783c+0x1d892+mode_index`. `0x00420c30` compares
`DAT_00476f78` against `0.0f`, then against `1.0f` before optional division,
converts through the x87 helper, and queues opcode 5.
