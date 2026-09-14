# ECL Script VM Core (0x44e1a0 cluster)

## Scope

Reconstructs the enemy-script ("ECL") virtual machine core in th10.exe:
the per-tick instruction interpreter `0x0044e1a0`, its argument evaluators
(`0x0044ff00`, `0x0044ff80`, `0x0044fe40`, `0x0044fdb0`, `0x00450030`,
`0x00450070`), the context list walker `0x0044fd10`, the sub-context
spawner `0x004500d0`, the sub-frame builder `0x0044df70`, and the node
lookup `0x00450160`. Implemented in `src/EclScriptVm.cpp/.hpp`.

Note: the caller-side notes in `EclScriptLibrary.cpp` previously described
`0x44fd10` as an entity-list scan and `0x40e5f0` as a plain death sequence;
the disassembly shows both are part of this VM (see below).

## Data model (byte-verified)

Run context, one `operator new(0x1024)` allocation:

```
+0x0000 f32  time         compared against the instruction tick
+0x0004 ptr  ins          current instruction record (0 = finished)
+0x0008      chunk[4096]  value stack / variables / frame scratch
+0x1008 u32  stack_cursor value-dword cursor; 0 == empty; pops clamp at 0
+0x100C u32  arg_base     variable-slot base for the current instruction
+0x1010 u32  script_id    sub-script id (0x450160 lookup key)
+0x1014 ptr  manager      owning manager (vtable'd variable store)
+0x1018 u32  field_1018   written by opcode 20
+0x101C u8   rank_mask    rank gate, inherited from the parent (0x4500d0)
+0x1020 u32  flags        bit 0 toggled by opcodes 18/19
```

Manager fields: `+0x04` published "current context" slot (tail write =
`manager+8`), `+0x1030` root record pointer (fake list head, never freed by
the walker), `+0x1034` first node of a doubly-linked `{ctx, next, prev}`
list (0xc-byte nodes allocated by `0x4500d0`).

Instruction record: `+0x00 i32` tick, `+0x04 i16` opcode, `+0x08 u16`
variable mask, `+0x0A u8` rank mask, `+0x0B u8` argument count,
`+0x10 u32` size/arg-0 dword, `+0x14` first argument dword, `+0x14+size+4*i`
type byte of argument i (8-byte `{tag, value}` pairs are also observed in
the `0x44df70` copy loop).

Variable store (manager object, vtable through its first dword): slot
`+0x04` int fetch, `+0x08` pointer fetch, `+0x0c` float fetch (ST0, i64 id),
`+0x10` string fetch. These remain extern boundaries.

## Interpreter 0x0044e1a0 (EAX = ctx, stack float delta, ret 4)

Prologue (byte-verified): null `ins` returns -1; `fild` of the integer
instruction tick compared against `ctx->time` — parity branch `0x44fb96`
keeps the context waiting (modeled as `tick <= time -> return 0`); the rank
byte must intersect `ctx->rank_mask` or the advance tail `0x44fb77` runs.
Dispatch is an 88-entry jump table (`byte_44fcb4` / `jpt_44e205`) over the
signed opcode word at `ins+4`.

Byte-verified opcodes implemented:

- 10 return: pops `{ins, time}` (0x4506d0 top normalization first); empty
  stack (`0x44fbb0`) or null restored `ins` (`0x44fbb2`) ends the context.
- 11 call: `0x44df70` with the context as its own parent; failure ends it.
- 12/13/14 jump: displacement `ins+0x10`, new tick `ins+0x14` widened to
  float; 13 jumps on popped zero, 14 on non-zero.
- 15/16 start sub-script (`0x4500d0`): 15 uses id -1; 16 reads the dword
  after the byte-size arg 0 as the id and forwards arguments from index 1.
- 17 kill sub-script (`ctx->ins = 0`), 18/19 set/clear `ctx+0x1020` bit 0,
  20 set `ctx+0x1018` from arg 1 — all through `0x450160` lookups.
- 21: `0x450190` boundary (body not reconstructed).
- 40 push int (0x450690), 41 pop top (0x4506d0), 42 push int eval,
  43 store int variable (int-tagged stack entries convert through
  `__ftol2`), 44 push float eval, 45 store float variable.
- 50/52/54/56/58 int add/sub/mul/div/mod (operand order verified: result
  is `second <op> first` for 52/56/58, `a + b` for 50, `a * b` for 54).
- 62 compare quirk: pushes 1 only for an unordered (NaN) result, else 0
  (literal translation of `fucompp` + `test ah,0x44` + `jp`).
- 64/66/68/70 float compares: pushes `second >= / > / <= / < first`.
- 72 compare against `flt_470b04` (= 0.0f): pushes ordered not-equal.
- 73/74 logical or/and of popped ints, 75/76/77 xor/or/and.
- 78 decrement variable: stores n-1, pushes the pre-decrement value.
- 79/80 fsin/fcos of the popped value (int-tagged widen to float).
- 81 polar helper: args 3/2, angle wrapped through `0x44bc70`, pair from
  `0x4501b0`, stored into the arg 0/1 slots.
- 83 time shift `ctx->time -= (float)arg0`.
- 84/85 int/float negate, 86 `arg0slot = arg1^2 + arg2^2`.

Not yet byte-verified (native default = advance tail, documented in code):
0, 1, 2-9, 22-29, 30 (the `Val`/`Str`/`Block` debug-print path at
`0x44fa71`), 31-39, 46-49, 51/53/55/57 (float arithmetic, same pop pattern),
59/60/61/63/65/67/69/71 (remaining compares), 82, 87.

## Argument evaluation

`0x44ff00` (int) / `0x44ff80`+`0x44fe40` (float) / `0x44fdb0` (int from
record): mask-clear returns the raw dword (as int or float bits). Mask set:
`>= 30` reads `chunk[arg_base + value]` (float version) or the dword slot;
`-1` pops a typed entry (`'f'`-tagged values truncate through `__ftol2`);
`0.125` (float version) pops with `'i'`-tagged reinterpret; other negatives
fetch from the manager store. Stack pops clamp the cursor at 0.

## 0x44df70 / 0x4500d0 / 0x44fd10

`0x4500d0` allocates the 0x1024 context (zeroed cursor/arg-base dwords on
success only), the 0xc node, publishes `{id, manager}`, zeroes
`ctx->time/ins`, copies the rank byte from the current context at
`manager+4`, links the node at the list head, and tail-calls `0x44df70`
with the parent = current context.

`0x44df70` (frame builder, decompile-verified): entry cursor 0 seeds zero
dwords; otherwise the saved dword is popped and rewritten below the entry
cursor, followed by the parent pointer and instruction pointer, each write
guarded by `cursor + 4 < 4096`. Arguments `first+1 .. ins[0x0b]-1` copy as
pairs: tag `'f'`/`'g'` evaluate float (`0x44ff80`, raw dword as default),
others evaluate int (`0x44ff00`); the destination byte at tag+1 (`'f'`)
selects verbatim float storage over `__ftol2`. Finally the label name at
`ins+0x14` resolves through `0x450470` with the manager's root record; the
result arms `ctx->ins` (and zeroes `ctx->time`). Success restores the
previous current context and returns 0; failure clears the parent's
instruction pointer, leaves the new context published, and returns -1.

`0x44fd10` walks the node list publishing each node's context at
`manager+4`; the root record executes first and a failure aborts the walk
with -1 without freeing. Finished sub-contexts are freed and unlinked. The
tail writes `manager+4 = manager+8` and returns 0.

## LCG oddity

The inline draws inside `0x44e1a0` and `0x40c9d0` combine the two LCG
half-steps as `(h2 << 16) | h2` — duplicating the second half — while
`0x44bb90` combines `(h1 << 16) | h2`. Both preserved as written.
Side note: the existing `PrngUnitFloat` reconstruction in
`VmLeafHelpers.cpp` scales by 2^-32, but `0x44bb90` actually computes
`combined * 2^-31 - 1` (range [-1, 1)); flagged for its owners.

## Native boundaries kept

`0x450190`, `0x450690`, `0x4505b0`, `0x4501b0`, `0x450470`, the four
manager vtable variable-store slots, `operator new/delete` thunks, and the
register-ABI thunks for `0x44e1a0`/`0x44df70`/`0x4500d0`/`0x44fd10` at
their native call sites (`0x40dc80`, `0x42bfc0`, `0x412aa0`, `0x412a00`).
