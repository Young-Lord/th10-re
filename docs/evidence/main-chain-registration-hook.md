# Main Chain Registration Hook Evidence

## Scope

`0x004201b0` is the one-shot registration hook stored at `ChainElem + 0x0c`
by `0x00420470` for the calculation callback record whose primary callback is
`0x0041ff80` (`MainChainUpdate`).  This document uses descriptive names only;
it does not identify original source-level class or subsystem names.

The scheduler helper `0x00449ae0` calls a non-null `record+0x0c` before it
acquires the scheduler lock or links the record, then clears `record+0x0c` and
links the record regardless of the hook result.  Thus this entry is an
initialization hook, not a per-frame callback and not an insertion veto.

## Exact ABI and return behavior

The scheduler calls the hook as follows:

```text
ECX = *(void **)(record + 0x20)  // 0x00491c28 for this record
call *(void **)(record + 0x0c)
```

`0x004201b0` has no stack arguments and ends with a plain `ret`.  It preserves
`ESI`, does not dereference the incoming `ECX` value, and always returns
`EAX = 0`.  The scheduler exposes that zero to `RegisterMainChainCallbacks`,
which consequently continues registering the draw callbacks.

The entry does *not* preserve `ECX` despite initially pushing it.  It passes
the address of that saved stack slot as `CreateThread`'s `lpThreadId` argument;
the API overwrites it with the newly created thread ID on success.  Its final
`pop ecx` therefore places that thread ID in `ECX` on return.  The registration
helper does not use post-call `ECX`, so this does not alter its observed
behavior.  If `CreateThread` fails, Windows does not define a replacement
thread ID through that pointer, so the post-return `ECX` value is not a stable
contract.

A faithful ABI-facing declaration must therefore retain the `ECX` input and
the unusual `ECX` clobber.  `__fastcall` is only a convenient spelling for the
observed incoming register; this entry has no `EDX` or stack parameter.

```cpp
// Semantic sketch only. The incoming pointer is not read by this function.
int __fastcall InitializeMainChainCalculationCallback(void *record_context);
```

## Exact control flow

There are no conditional branches in `0x004201b0-0x00420269`.

1. Call `0x00420100` and discard its result.  That callee initializes version
   data from `th10.dat`; its `0` and `-1` results cannot change this hook's
   subsequent control flow.
2. Write `1.0f` (`0x3f800000`) to `0x00476f78`.
3. Write `0xff000000` to `0x004923a8`.  This is later consumed as the Direct3D
   `Clear` color by `MainChainDrawInitialize`, so an opaque black clear color
   is a directly supported behavioral description.
4. Call `0x004216f0`, a no-argument global-state reset routine.
5. Call imported `timeGetTime`, then store its full 32-bit result to
   `0x00491ff8` and its low 16 bits to both `0x004918b0` and `0x004918a8`.
6. Call imported `CreateThread` with the exact arguments below and store its
   returned `HANDLE` in `0x004977ac`; the return value is not checked.
7. Call `0x00413350`, which allocates a 0x8c-byte zeroed owner object, stores
   it at `0x00447708`, creates a scheduler record for `0x00413690`, and
   inserts that record in the draw list at priority `47` with the new object
   at record `+0x20`.  The hook discards the returned owner pointer.
8. With `ESI = 0x00474dd4`, call `0x0044c150`.  That callee operates on
   `ESI` rather than a normal C++ argument; it waits for and closes a thread
   handle at `[ESI+4]` when non-null, and clears associated fields.
9. Initialize three fields in the global object at `0x00474dd4`, then call
   `0x00453683` to create another thread.  Store its returned `HANDLE` in
   `0x00474dd8`.  This helper uses standard stack arguments and returns zero
   on failure; that result is also not checked by this hook.
10. Load the object pointer at `0x00491c10` into `EAX` and call `0x004462f0`.
    The callee consumes the object in `EAX` rather than a stack or `ECX`
    argument and writes a large set of its fields; no null check precedes the
    call.
11. Call `0x00437a00`, which performs further global initialization including
    writes to the two low-word timestamp globals from step 5.
12. Return zero.

## Direct global and field effects

| Location | Exact write/use | Confidence |
| --- | --- | --- |
| calculation record `+0x0c` | Read/called by scheduler before this entry; cleared by `0x00449ae0` after the call | exact scheduler behavior |
| calculation record `+0x20` | Supplied in `ECX`; this entry does not read it | exact |
| `0x00476f78` | `0x3f800000` / `1.0f` | exact value; semantic role unknown |
| `0x004923a8` | `0xff000000` | exact value; later Direct3D clear-color use confirmed |
| `0x00491ff8` | `timeGetTime()` result | exact |
| `0x004918b0` | low 16 bits of `timeGetTime()` result | exact |
| `0x004918a8` | low 16 bits of `timeGetTime()` result | exact |
| `0x004977ac` | first `CreateThread` return value (`HANDLE`) | exact; lifetime management not established here |
| `0x00447708` | written by direct callee `0x00413350` with its allocated 0x8c-byte owner | exact callee effect |
| `0x00474dd4 + 0x0c` | `1` | exact |
| `0x00474dd4 + 0x10` | `0` | exact |
| `0x00474dd4 + 0x18` | `0` | exact |
| `0x00474dd4 + 0x1c` | `0x0043ba90` | exact function pointer value |
| `0x00474dd8` | second thread helper's returned `HANDLE` | exact; helper can return null |
| `*(void **)0x00491c10` | loaded into `EAX` for `0x004462f0` | exact nonstandard input ABI; object type unresolved |

### First thread creation

The direct imported call is precisely equivalent to:

```cpp
HANDLE handle = CreateThread(
    0,                 // lpThreadAttributes
    0,                 // dwStackSize
    (LPTHREAD_START_ROUTINE)0x0043d080,
    (void *)0x00492590,
    0,                 // dwCreationFlags
    &thread_id);       // physically the saved incoming-ECX stack slot
);
*(HANDLE *)0x004977ac = handle;
```

`0x0043d080` and the object at `0x00492590` are direct address facts.  Their
semantic thread role and parameter type are not established by this hook.

### Second thread creation

Before calling `0x00453683`, the hook passes these six stack values:

```text
0, 0, 0x0043ba90, 0x00491c28, 0, 0x00474ddc
```

The helper forwards the first five as `CreateThread`'s attributes, stack size,
start address, parameter, and flags respectively.  It uses the sixth as the
caller-provided thread-ID storage if non-null, otherwise uses a local stack
slot.  Therefore the created thread has start address `0x0043ba90` and
parameter `0x00491c28`; its generated thread ID is stored at `0x00474ddc`.
The helper allocates and releases an internal 0x8c-byte thread wrapper around
the creation call, so that wrapper is not retained by this entry.

`0x0043ba90` immediately calls `0x0043b8d0` with `EAX = 0x00491c28` and then
updates bits `0x200` and `0x400` in global `0x00491ff4` from the nullness of
`0x00491c38` and `0x00491c3c`.  This makes the thread entry's direct global
effects observable, but does not prove a higher-level thread name.

## Global reset callee `0x004216f0`

The hook gives `0x004216f0` no explicit input.  It directly resets a broad
global block, including these exact values:

```text
0x491e94 = 0;       0x491e98 = 0;          0x491e9c = 1000.0f;
0x491ea0 = 0;       0x491ea4 = 0;          0x491ea8 = 0;
0x491eac = 0;       0x491eb0 = 1.0f;       0x491eb4 = 0;
0x491ed0 = 0;       0x491ed4 = 0;          0x491ed8 = 0;
0x491edc = 0.5235988f;                     0x491f60 = 0;
0x491f64 = 0;       0x491f68 = 640;        0x491f6c = 480;
0x491f70 = 0;       0x491f74 = 1.0f;       0x491f78 = 1;
0x491e48 = 32;      0x491e4c = 16;         0x491e50 = 384;
0x491e54 = 448;     0x491e58 = 0;          0x491e5c = 1.0f;
0x491e60 = 0;       0x491db8 = 0;          0x491dbc = 0;
0x491dc0 = 0
```

It also writes zeroes and constants through the nearby `0x00491d7c` through
`0x00491dc4` region.  The values are instruction-level facts, but the object
boundaries and semantic meaning of this aggregate are not recovered, so it
should remain a global reset helper in C++ rather than an asserted structure.

## Reconstruction constraints and uncertainty

- The `record_context` parameter is the MainChain global in the only known
  registration site, but the hook never dereferences it.  Do not make its
  behavior depend on a `MainChainContext` layout merely because the scheduler
  happens to provide one.
- `0x0044c150` requires `ESI = 0x00474dd4`; `0x004462f0` requires
  `EAX = *(void **)0x00491c10`.  Neither call may be represented as an
  ordinary no-argument C++ function without a register-adapter thunk.
- The code does not check either thread creation result.  Adding failure
  handling or treating the hook's zero result as thread-creation success would
  change the observed behavior.
- The global/object names in this document are descriptive.  Thread roles,
  the identities of `0x00476f78`, `0x00474dd4`, and the `0x00491c10` object,
  plus the field semantics of the reset block, remain unresolved.

## Verification sources

- `resources/th10.exe`, `objdump -D -Mintel`, ranges
  `0x00420100-0x00420269`, `0x004216f0-0x004218c0`,
  `0x00413350-0x004133e6`, `0x00453683-0x0045370d`, and
  `0x0043ba90-0x0043bae2`.
- Import table: `0x00466270 = timeGetTime` and `0x00466118 = CreateThread`.
- Scheduler registration helper `0x00449ae0`; see
  `docs/evidence/callback-scheduler.md` for the record ABI and insertion
  ordering.
