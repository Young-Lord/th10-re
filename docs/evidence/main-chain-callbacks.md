# Main Chain Callback Evidence

## Scope and confidence

This document describes the five functions that bind the `MainChain` object at
`0x00491c28` into the callback scheduler:

| Address | Evidence name | Scheduler list | Priority | Confidence |
| --- | --- | --- | ---: | --- |
| `0x0041ff80` | `MainChainUpdate` | calculation | 1 | exact control flow and ABI |
| `0x00420000` | `MainChainDrawInitialize` | draw | 1 | exact control flow and ABI |
| `0x004200c0` | `MainChainDrawContinue` | draw | 40 | exact |
| `0x004200d0` | `MainChainDrawFinalize` | draw | 50 | exact control flow and direct callee ABI |
| `0x00420470` | `RegisterMainChainCallbacks` | n/a | n/a | exact control flow and scheduler-record setup |

The names above are local evidence names.  They do not establish original
class names, the semantic identities of all globals, or the identities of the
render-device virtual methods.

All five functions are plain near-returning x86 functions (`ret`, no immediate
stack adjustment).  The scheduler supplies the registered callback context in
`ECX`.  This is demonstrated by the calculation and draw schedulers at
`0x00449c00` and `0x00449d40`: immediately before invoking record `+0x08`,
they execute `mov ecx, [record + 0x20]`.  Thus the callback-facing ABI is:

```cpp
typedef int (__fastcall *MainChainCallback)(MainChainContext *context);
```

`__fastcall` is used only to communicate the observed first argument in `ECX`;
there is no `EDX` argument and no stack argument at these five entries.  A
callback record created by `0x00449ed0` is 0x24 bytes.  Relevant fields are
`+0x04` flags, `+0x08` function, `+0x0c` optional pre-insertion callback, and
`+0x20` callback context.  The dispatcher calls a record only when flag bit 1
(`0x2`) is set.

## `0x0041ff80`: calculation callback

### Inputs, outputs, and exact decision table

Input: `ECX = g_MainChainContext` (`0x00491c28`).  The function preserves
`ESI`, uses it for the context, and returns its result in `EAX`.

| Condition | Effect | Return value |
| --- | --- | ---: |
| `context[+0x3cc]` is negative and `context[+0x63c] == 0` | write `3` to `0x00491fb8` | continues |
| always | advance the transition object at `0x00492590` via `0x00421e00` | continues |
| always | update input slot 0 through `0x0044a5f0(ECX=0)` | continues |
| `0x00447700(0x00491c10)` is nonzero | none after the call | `4` |
| preceding call returned zero and `context[+0x648] == 0` | call `0x004218d0` with `EAX=context` | callee result unchanged |
| preceding call returned zero and `context[+0x648] == 2` | none | `4` |
| preceding call returned zero and `context[+0x648] != 0 && != 2` | none | `1` |

The `+0x648` branch is the exact instruction sequence
`setne; dec; and 3; inc`; its result is `4` only for value `2`, otherwise `1`
for every nonzero value.

The call at `0x0041ffe5` is not an ordinary member call: `MainChainAdvanceState`
at `0x004218d0` receives its context in `EAX`.  The comparison of its result
against one at `0x0041ffea` is semantically dead (`jnz +0`), so this callback
returns that result unmodified.

### Direct dependencies

| Address/global | Observed dependency | What is proven |
| --- | --- | --- |
| `0x00491fb8` | written with `3` | a shared 32-bit state/status value; its semantic name is unknown |
| `0x00492590` / `0x00421e00` | `EAX=0x00492590; call` | the callee reads a pointer at base `+0x5208` and decrements transition timing/state; it is not a normal stack argument call |
| `0x0044a5f0` | `ECX=0` | input slot 0 update; it samples keyboard/device state and updates per-button counters |
| `0x00491c10` / `0x00447700` | pushed as sole stack argument | a 33-entry (`0..0x20`) manager-owned queue/pool is serviced; nonzero return signals this callback to return `4` |
| `0x004218d0` | `EAX=context` | MainChain state advancement; returns the callback result verbatim |

The first conditional write is exact, but the meaning of `context+0x3cc`,
`context+0x63c`, and global `0x00491fb8` remains unproven.  They should retain
their conservative field/global names in C++ until callers establish semantics.

## `0x00420000`: draw initialization callback

Input: `ECX = g_MainChainContext`; output: always `EAX = 1`.  It preserves
`EBX`, `ESI`, and `EDI`.  No conditional path exists.

It loads the global pointer at `0x00491c10` into `EAX`, uses
`context+0x26c` as a draw/camera-work base, stores that address into
`context+0x384`, and writes `1` to `context+0x388`.

### Exact writes through `0x00491c10`

Provided `0x00491c10` is a valid object as required by this path, these are the
unconditional writes (all offsets are relative to its value):

| Offset | Value |
| ---: | --- |
| `+0x3ada70` | `0` |
| `+0x3ada64` | `0` |
| `+0x3ada69` | byte `0xff` |
| `+0x3ada68` | byte `3` |
| `+0x3ada6b` | byte `0xff` |
| `+0x3ada6c` | byte `0xff` |
| `+0x73245c` | `0` |
| `+0x732458` | `0x80808080` |
| `+0x3ada6e` | byte `0xff` |
| `+0x60` | `0` |
| `+0x5c` | `0` |
| `+0x3ada6a` | byte `0xff` |

It then calls `0x004215a0` with the work base in `EDI`.  That callee updates
camera/view and projection matrices, commits them to the render object at
`0x00491c30` through vtable offset `+0xb0`, and copies two work values into
the `0x00491c10` object at `+0x5c/+0x60`.  It also calls `0x00442f50` first
when `0x00491c10` is non-null; this is part of the exact dependency chain.

After the helper returns, this callback makes two virtual calls:

1. `(*(void (__thiscall **)(void *, void *))(*(void **)context[+0x08] +
   0xbc))(context[+0x08], context[+0x384] + 0xcc);`
2. `(*(void (__thiscall **)(void *, int, int, int, int, unsigned int, int))
   (*(void **)0x00491c30 + 0xac))(0x00491c30, 0, 0, 1, 0x004923a8, 1.0f, 0);`

The C++ signatures above express the pushed values and receiver placement, not
semantic type recovery.  The first object type, both virtual method identities,
and the semantic meaning of `0x004923a8` are not yet known.

## `0x004200c0`: draw continuation callback

This function is exactly:

```cpp
int __fastcall MainChainDrawContinue(MainChainContext *) { return 1; }
```

It does not read `ECX`, has no side effects, and has no dependencies beyond
the scheduler that invokes it.

## `0x004200d0`: draw finalization callback

The scheduler still supplies `ECX = g_MainChainContext`, but this function
does not consume it.  It preserves `ESI`, loads `ESI = *(void **)0x00491c10`,
then calls `0x00442f50` with that implicit register input.  Afterwards it
writes zero to globals `0x00491e64` and `0x00491e68`, returns `1`, and restores
`ESI`.

`0x00442f50` proves its nonstandard dependency ABI by directly reading
`[ESI+0x3adac8]`.  When that count is nonzero, it invokes render-object virtual
methods at `0x00491c30` vtable offsets `+0x10c`, `+0x164`, and `+0x14c`, then
copies `[ESI+0x72dacc]` to `[ESI+0x72dad0]`, clears
`[ESI+0x3adac8]`, and increments `[ESI+0x58]`.  Therefore a C++ wrapper for
`0x00442f50` must not be declared as a conventional no-argument function:
its required input is in `ESI`.

The two cleared globals are externally read by startup/window-related code and
also written by other subsystems, but this callback alone does not identify
their types or meanings.

## `0x00420470`: registration

The function has no inputs, preserves `EBX`, `EBP`, `ESI`, and `EDI`, and
returns `EAX`.  It first initializes:

```text
*(u32 *)0x00491fb4 = 0xfffffffe
*(u32 *)0x00491fb8 = 0
*(u32 *)0x00491fc0 = 0
```

It calls `0x00449ed0(callback)` to allocate/initialize each 0x24-byte record.
For every record it sets flag bit `0x2` at `+0x04` and stores
`0x00491c28` at `+0x20`.

| Callback | Additional record setup | Registration helper | `EDI` priority |
| --- | --- | --- | ---: |
| `0x0041ff80` | `+0x0c = 0x004201b0` | `0x00449ae0(0x00491be4)` | 1 |
| `0x00420000` | none | `0x00449b70(0x00491be4)` | 1 |
| `0x004200c0` | none | `0x00449b70(0x00491be4)` | 40 |
| `0x004200d0` | none | `0x00449b70(0x00491be4)` | 50 |

`0x00449ae0` inserts into the manager list rooted at argument `+0x18`, while
`0x00449b70` inserts into the list rooted at argument `+0x3c`; inspection of
the dispatchers shows these are calculation and draw lists respectively.  Both
helpers take the manager pointer as one stack argument, receive the record in
`ESI` and priority in `EDI`, and return the optional `record+0x0c` callback's
result (or zero).  The registration function tests only the first insertion
result: a nonzero result ends registration immediately and becomes its return
value.  If it is zero, all three draw records are inserted and it explicitly
returns zero.  Results from the draw insertions are discarded.

The calculation record's pre-insertion callback `0x004201b0` is not one of the
five requested callbacks.  It is invoked by `0x00449ae0` with the record
context in `ECX` before the record is linked, so it is an initialization hook,
not the per-frame update callback.

## Reconstruction constraints

The documented behavior is suitable for C++ reconstruction, but exact C++
types should remain conservative until the scheduler record, render object,
`0x00491c10` owner, and the two virtual interfaces are independently recovered.
In particular, model the `EAX` ABI of `0x004218d0` and the `ESI` ABI of
`0x00442f50` with narrow wrappers or compiler-specific thunks; representing
either as an ordinary C++ call would silently change the binary contract.

## Verification sources

- `resources/th10.exe`, `objdump -D -Mintel`, exact ranges `0x0041ff80-0x0041fff1`, `0x00420000-0x004200b1`, `0x004200c0-0x004200c6`, `0x004200d0-0x004200f7`, and `0x00420470-0x0042053c`.
- Scheduler/register helpers `0x00449ae0`, `0x00449b70`, `0x00449c00`, and `0x00449d40`.
- Direct callees `0x004215a0`, `0x00421e00`, `0x00442f50`, `0x00447700`, and `0x0044a5f0`.
