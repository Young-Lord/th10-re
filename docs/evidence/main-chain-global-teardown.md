# Main Chain Global Teardown Evidence

## Scope

This document recovers `0x004203f0`, called
`DestroyAllMainChainObjects` in local reconstruction code.  The name is a
behavioral description, not evidence for an original symbol.  The function
tears down five independently allocated globals in a fixed order.  It does
not receive a C++ object pointer, does not directly write any of the five
global pointer slots, and returns zero through a plain `ret`.

All observations below are from `resources/th10.exe`.

## Entry ABI and exact top-level order

`0x004203f0` preserves `ESI`.  For each non-null global it performs:

1. push the loaded object pointer;
2. invoke an in-place teardown entry that returns with `ret 4`;
3. push the same saved pointer;
4. call `0x004524a1` and discard its one stack argument in the caller.

The fixed sequence is:

| Order | Global | Local evidence name | Teardown entry | Global clear performed by | Final allocation release |
| ---: | --- | --- | --- | --- | --- |
| 1 | `0x00477810` | title-screen object | `0x00417c80` | `0x0041807a` | `0x004524a1` |
| 2 | `0x0047784c` | game-manager object | `0x0042cb60` | `0x0042cd25` | `0x004524a1` |
| 3 | `0x00477820` | global-lifecycle manager | `0x0041fb50` | `0x0041fc29` | `0x004524a1` |
| 4 | `0x00477700` | transition object | `0x0040b7b0` | `0x0040b90a` | `0x004524a1` |
| 5 | `0x00477838` | opaque manager object | `0x004294a0` | `0x004295d3`, only if the destroyed pointer is still the global value | `0x004524a1` |

Thus the ownership boundary is explicit: the five teardown entries dispose
internal resources and sever their global publication; `0x004203f0` owns the
outer `GameFree` step.  Calling an in-place teardown entry and then retaining
or reusing its allocation is unsupported by the observed code.

Equivalent conservative C++ pseudocode is:

```cpp
void DestroyAllMainChainObjects() {
    if (g_TitleScreen != 0) {
        TeardownTitleScreen(g_TitleScreen);
        GameFree(g_TitleScreen);
    }
    if (g_GameManager != 0) {
        TeardownGameManager(g_GameManager);
        GameFree(g_GameManager);
    }
    if (g_GlobalLifecycleManager != 0) {
        TeardownGlobalLifecycleManager(g_GlobalLifecycleManager);
        GameFree(g_GlobalLifecycleManager);
    }
    if (g_TransitionObject != 0) {
        TeardownTransitionObject(g_TransitionObject);
        GameFree(g_TransitionObject);
    }
    if (g_OpaqueMainChainManager != 0) {
        TeardownOpaqueMainChainManager(g_OpaqueMainChainManager);
        GameFree(g_OpaqueMainChainManager);
    }
}
```

This must remain ordered source code.  A generic unordered container or a
single bulk reset would lose both the observed destruction order and the
callee-owned global reset timing.

## Direct teardown calls

### `0x00417c80`: title-screen object

Input is `[ESP+4] = title`; it has SEH setup and returns `ret 4`.  The entry
performs a broad title/UI-domain teardown: it first calls `0x0042b1e0` using
the global at `0x0047783c`, resets shared display/state globals including
`0x00474ca0` and `0x00476f78`, removes callback records at title `+0x08` and
`+0x0c`, and releases numerous auxiliary global objects.  Its exact object
names beyond the established title-screen role are not recovered.

The global-reset fact is unambiguous: after both title callback-record removal
blocks, it writes zero to `0x00477810` at `0x0041807a`, before its final
state-dependent calls and return.  Therefore the outer free at `0x00420402`
does not derive its pointer from the global again; it uses the `ESI` copy
loaded before teardown.

There is one important interaction with item 5.  When shared state
`0x00491fb8` is neither `14` nor `15`, this teardown loads `0x00477838`,
calls `0x004294a0`, and frees it.  `0x004294a0` clears `0x00477838` when its
argument is that currently published object.  Consequently the final step of
`0x004203f0` sees null and skips it.  For states `14` and `15`, that early
path is skipped and the fifth top-level step performs the same destruction.
This avoids a double free without requiring a special case in
`0x004203f0` itself.

### `0x0042cb60`: game-manager object

Input is `[ESP+4] = game_manager`; it has SEH setup and returns `ret 4`.
The teardown stops its thread-control region at object `+0x5ab0`, removes
calculation and draw callback records at `+0x0c` and `+0x10` under the
callback-framework lock, frees owned render/resource slots rooted in the
large global object at `0x00491c10`, destroys 0x32 subordinate objects from
the manager's `+0x59e4` array, and releases owned storage at `+0x5aac` when
present.

At `0x0042cd25` it writes zero to `0x0047784c`, then restores the
thread-control marker at `+0x5ab0` and calls `0x0044c150` again.  The outer
allocator release is intentionally separate and occurs at `0x0042041b`.

### `0x0041fb50`: global-lifecycle manager

Input is `[ESP+4] = lifecycle_manager`; it has SEH setup and returns `ret 4`.
It first stops/closes the manager thread-control region at `+0x10`, removes
the callback records at `+0x08` and `+0x0c` under the callback lock, then
releases manager-associated global resources and its optional owned buffer at
`+0x388`.  Its broader ownership details are recovered separately in
`global-lifecycle-implementation.md`.

At `0x0041fc29` it clears `0x00477820` before freeing further global helpers.
The object itself is not freed by this entry; `0x00420434` performs that
release after the entry returns.

### `0x0040b7b0`: transition object

Input is `[ESP+4] = transition`; returns `ret 4`.  It removes callback records
at transition `+0x08` and `+0x0c` under the callback lock, invokes the
manager-work service cleanup with the render-owner global in `EDI`, frees a
set of auxiliary pointers in the `0x00491c10` object, and conditionally frees
transition `+0x14` through `0x00452422`.

It writes zero to both `0x00477700` and `0x004918a4` at `0x0040b90a` and
`0x0040b910`, respectively.  The first is the fifth-state-machine object's
published pointer; the latter is a related shared state word, not the outer
allocation.  `0x0042044d` releases the now-unpublished transition allocation.

### `0x004294a0`: opaque global manager

Input is `[ESP+4] = opaque_manager`; it has SEH setup and returns `ret 4`.
It frees object `+0x14`, invokes `0x0042ab20` for indices `0..7`, frees and
clears `+0x18` plus the eight pointer slots `+0x1c..+0x38`, and removes up to
three callback records at `+0x08`, `+0x1cc`, and `+0x0c` under the callback
lock.  It then destroys an internal fixed-element container at `+0xa0`.

The reset is guarded rather than unconditional:

```text
if (opaque_manager == *(void **)0x00477838)
    *(void **)0x00477838 = 0;
```

This exact identity check is why the title-screen teardown may safely perform
the early destruction described above.  The function does not free the outer
manager object; `0x00420466` does so when this is the final top-level step.

## Call sites and lifecycle meaning

## Process-Level Shutdown: `0x00420270`

This broader helper receives `MainChainContext*` in `EAX`, returns zero with
plain `ret`, and is called with `EAX = 0x00491c28` at `0x00438d95`. It runs
the following ordered process-level shutdown sequence:

1. Stop thread controls at `0x00474dd4` and `0x00492254` via `0x0044c150`,
   with `0x004977b4 = 2` between the two calls.
2. Release non-null version data at `0x0049238c` through `0x00452422`, then
   clear only that pointer; the length at `0x00492388` is retained.
3. Call `DestroyAllMainChainObjects`, then destroy the owner at `0x00447708`.
4. Release and clear the interface held at `*(0x00491c10 + 0x3ada74)`.
5. Queue BGM command `(root=0x00492590, path="dummy", opcode=4, slot=0)`,
   then destroy generated surface/fonts.
6. Unacquire/release/clear context `+0x10` then `+0x14`; release/clear
   context `+0x0c` without unacquiring it.
7. For global timer `0x00491d44`, execute an optional `timeKillEvent`, one
   `timeEndPeriod`, an in-place teardown that restores vtable `0x0046f810`
   and contributes two further `timeEndPeriod` calls, outer-free it, and
   clear the global.
8. Clear packed archive state at `0x00497990` through `0x00434d10`.

The resource-thread handle at `0x004977ac` is not joined or closed here.
The normal timer path therefore makes at most one `timeKillEvent` call and
exactly three `timeEndPeriod` calls. These intentionally unusual behaviors
are part of the observed lifecycle, rather than candidates for RAII cleanup.

The adjacent `0x004203a0` is a deleting wrapper: `EAX` contains the timer
object, bit 0 of its one stack argument requests the outer allocation release,
and it returns the original object in `EAX` with `ret 4`. Its in-place target
is `0x004203c0`, which receives the object in `ESI` and returns with plain
`ret`.

Evidence: `0x00420270-0x00420394`, timer in-place teardown
`0x004203c0-0x004203ef`, and caller `0x00438d95-0x00438d9a`.

There are two direct callers in the executable:

| Caller | Context | Behavior after return |
| --- | --- | --- |
| `0x00420270` | broader shutdown routine | continues tearing down thread controls, callback/render helpers, and other process-level state. |
| `0x0042194e` | `MainChainAdvanceState` request state `3` | executes the complete five-object teardown, releases state lock 5, and returns callback code `4`. |

The second caller proves that this is not solely application-exit cleanup: it
is also the state machine's failure/exit branch.  Conversely, ordinary state
transitions destroy only the involved title, game-manager, or transition
object and must not be replaced by this full routine.

## Reconstruction constraints

* Keep all five global pointers as separate ownership roots.  They are not
  fields of `MainChainContext`; the main context itself at `0x00491c28` is not
  freed by this routine.
* Preserve teardown-before-outer-free and the displayed order exactly.
* Do not add direct zero writes in `DestroyAllMainChainObjects`; the original
  order makes each callee responsible for unpublishing itself.  A defensive
  C++ wrapper may assert the expected postcondition after each call, but must
  not conceal a failed callee by clearing its global itself.
* `0x00477838` has a proven teardown layout but no sufficiently supported
  semantic class name.  Keep it opaque in C++ until its factory and consumers
  establish a stable role.
* The deeper global releases performed by the title teardown overlap with
  other subsystem domains.  Do not move them into the top-level routine based
  only on this call path.

## Verification sources

* `0x004203f0-0x0042046f`: exact top-level order and outer frees.
* `0x00417c80-0x004180d0`: title teardown, early `0x00477838` path, and
  `0x00477810` reset.
* `0x0042cb60-0x0042cd48`: game-manager teardown and reset.
* `0x0041fb50-0x0041fd00`: global-lifecycle teardown and reset.
* `0x0040b7b0-0x0040b917`: transition teardown and reset.
* `0x004294a0-0x0042960c`: opaque-manager teardown and guarded reset.
* `0x00420270-0x00420394` and `0x0042194e-0x0042195d`: the two direct callers.
