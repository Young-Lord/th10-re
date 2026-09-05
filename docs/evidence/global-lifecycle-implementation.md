# Global Lifecycle Manager Implementation Plan

## Scope and evidence boundary

This document is an implementation-oriented recovery of TH10
`resources/th10.exe` entries `0x0041f850`, `0x0041fac0`, `0x0041fb50`,
`0x0041fd00`, and `0x0041fdd0`. `GlobalLifecycleManager` remains a
conservative behavioral name. Its size (`0x3f0`), the offsets below, and all
listed control flow are direct instruction evidence. Names for external
subsystems and opaque objects are not type recovery.

The object is separate from the main-chain context. `0x0041fd00` creates it
and the main-chain state machine stores the returned pointer at main-chain
context `+0x768`.

## Entry ABI

| Address | Input ABI | Output ABI | Direct role |
| --- | --- | --- | --- |
| `0x0041f850` | `ESI = GlobalLifecycleManager *` | `EAX = ESI`; plain `ret` | Register-entry constructor. |
| `0x0041fac0` | `EBX = GlobalLifecycleManager *` | `EAX = 0`; plain `ret` | Register-entry initializer. |
| `0x0041fb50` | `[ESP+4] = GlobalLifecycleManager *` | no caller-used result; `ret 4` | In-place teardown only. |
| `0x0041fd00` | no explicit input | `EAX = manager` or `0` | Heap factory. |
| `0x0041fdd0` | `[ESP+4] = GlobalLifecycleManager *` | `EAX = 1`; `ret 4` | Draw/frame callback body. |

`0x0041fef0` is the callback bridge for `0x0041fdd0`: `push ecx; call
0x0041fdd0; ret`. The scheduler supplies callback owners in `ECX`; therefore
the semantic C++ callback may be a `__thiscall` method even though its internal
body is stack-argument ABI.

Do not expose `0x0041f850` and `0x0041fac0` directly as ordinary member
functions. Their register-only entry ABIs require tiny assembly/compiler
specific bridges if their literal entry points must be called. A semantic C++
reimplementation can use ordinary private methods instead.

## Object layout and final constructor state

The `0x3f0` byte allocation is fully cleared by `0x0041f850`, then bit `0x2`
is set at `+0x00`, and the pointer is published to global `0x00477820`.
Consequently the final constructor state is exactly:

| Offset | Final value after `0x0041f850` | Evidence / safe meaning |
| --- | --- | --- |
| `+0x00` | `0x00000002` | lifecycle/callback flag word; bit 1 is consumed by calc callback `0x0041feb0`. |
| `+0x04` | `0` | unknown. |
| `+0x08` | `0` | calc callback node pointer, assigned by initializer. |
| `+0x0c` | `0` | draw callback node pointer, assigned by initializer. |
| `+0x10..+0x387` | zero bytes | opaque control/state storage. |
| `+0x388` | `0` | separately allocated, manager-owned pointer when non-null. |
| `+0x38c..+0x3ef` | zero bytes | includes callback stage/frame fields below. |
| global `0x00477820` | manager pointer | published singleton-like pointer. |

The apparent writes before the full-object clear are not final initialization:
`+0x10 = 0x004703e4`, `+0x14..+0x20 = 0`, a word at `+0x3b4 = 0xffff`, and
bit clearing in several offsets inside `+0x30..+0x3ab` are all overwritten by
the subsequent `rep stosd` zero of the entire object. They must not be copied
into a semantic constructor.

Known post-construction offsets:

| Offset | Directly observed use |
| --- | --- |
| `+0x08` | owning callback node for target `0x0041feb0`. |
| `+0x0c` | owning callback node for target `0x0041fef0`. |
| `+0x10` | base of an opaque thread-control region passed to `0x0044c150`. |
| `+0x14` | `_beginthreadex` result/OS thread handle. |
| `+0x18` | `_beginthreadex` thread-ID output. |
| `+0x1c` | written zero immediately before thread creation; unknown. |
| `+0x20` | written one immediately before thread creation; unknown. |
| `+0x28` | written `0x0041f990`, the startup thread entry. |
| `+0x388` | freed via `0x00452422` if non-null, then cleared. |
| `+0x3dc` | receives one result produced by draw/frame stage one. |
| `+0x3e0` | receives configuration/query result in startup thread. |
| `+0x3e4` | startup stage, initially zero; frame callback changes `1 -> 2`. |
| `+0x3e8` | draw stage, initially zero; frame callback changes `1 -> 2`. |
| `+0x3ec` | incremented once on every `0x0041fdd0` invocation. |

`+0x10` cannot yet be declared `CRITICAL_SECTION`, `HANDLE`, or a standard
library thread object. `0x0044c150` reads `+0x14` as a handle, waits/terminates
when necessary, closes it, then clears `+0x14` and `+0x28`; that proves a
thread-control relationship but not the full type.

## Initializer: exact direct effects

`0x0041fac0` has no failure branch and returns zero after the following steps.
It does not test allocation results returned by the callback allocator.

1. Allocate a `0x24` callback node with target `0x0041feb0`.
2. Clear callback-node flag bit `0x2`, set node `+0x20 = manager`, register it
   in scheduler global `0x00491be4` through `0x00449ae0` at priority `3`, and
   store the node at manager `+0x08`.
3. Allocate a second `0x24` callback node with target `0x0041fef0`.
4. Clear its flag bit `0x2`, set node `+0x20 = manager`, register it through
   `0x00449b70` at priority `2`, and store it at manager `+0x0c`.
5. Call `0x0044c150` with `ESI = manager + 0x10`; with the zeroed initial
   control region this is a no-op.
6. Prepare the thread-control fields: `+0x28 = 0x0041f990`, `+0x20 = 1`,
   `+0x1c = 0`.
7. Call CRT `_beginthreadex` at `0x00453683` with:
   `security = 0`, `stack_size = 0`, `start = 0x0041f990`, `argument =
   manager`, `initflag = 0`, `thread_id_out = manager + 0x18`.
8. Store the returned handle at `+0x14`; return zero whether that handle is
   null or non-null.

The callback node allocator itself dereferences a null allocation result before
returning. Thus an exact implementation cannot honestly offer normal nullable
callback-registration failure behavior without deliberately changing the
executable's failure semantics.

## Factory ownership and failure behavior

`0x0041fd00` calls allocator `0x00452493` with `0x3f0`. A non-null allocation
is passed in `ESI` to the constructor. The factory then invokes initializer
`0x0041fac0` regardless of whether allocation succeeded; this means the
allocation-null path is not a usable safe path because the initializer uses
`EBX = 0` as an object base.

The intended success/failure branch is nevertheless exact:

```cpp
GlobalLifecycleManager *CreateGlobalLifecycleManager() {
    GlobalLifecycleManager *manager =
        static_cast<GlobalLifecycleManager *>(GameAlloc(0x3f0));
    if (manager != 0) {
        ConstructEsiAbi(manager);
    }

    const int init_result = InitializeEbxAbi(manager);
    if (init_result != 0) {
        if (manager != 0) {
            TeardownInPlace(manager);
            GameFree(manager);
        }
        return 0;
    }
    return manager;
}
```

The current initializer always returns zero, so its cleanup branch is dormant
in the inspected binary. Retaining it in a semantic C++ factory preserves the
observed ownership contract. `0x0041fb50` alone never frees `manager`.

## Teardown: exact ordered effects

`0x0041fb50` uses SEH but its normal path has this externally relevant order:

1. Call `0x0044c150` with `ESI = manager + 0x10` to stop/close the recorded
   thread control.
2. For non-null manager `+0x08`, acquire callback-framework lock
   `0x00492274`, call `0x00449f60` with `ECX = node`, `EDX = scheduler global
   0x00491be4`, then release the lock.
3. Repeat the same removal sequence for non-null manager `+0x0c`.
4. Call `0x0041f930`, which frees and clears two global heap pointers at
   offsets `+0x3ad084` and `+0x3ad088` of global `0x00491c10` when non-null.
5. Free and clear global `0x00491c10 + 0x3ad070` if non-null.
6. Clear global `0x00477820`; then, if global `0x004776e0` is non-null, call
   `0x00401260` on it and free it through `0x004524a1`.
7. Free and clear global `0x00491c10 + 0x3ad06c` if non-null.
8. Call `0x0042b1e0` with `EBX = global 0x0047783c`; if that global is
   non-null, free each non-null pointer at its `+0x00` and `+0x04` through
   `0x00452422`, then free the container through `0x004524a1`. Clear global
   `0x0047783c` afterward.
9. Free manager `+0x388` through `0x00452422` when non-null and clear it.
10. Write `0x004703e4` at manager `+0x10`, then call `0x0044c150` again with
    `ESI = manager + 0x10`.

The scheduler removal helper frees its node when the node owns itself (flag
bit `0x1`, as nodes created at initialization do). The manager fields `+0x08`
and `+0x0c` are not explicitly cleared by the destructor after removal; they
become stale pointers and the object must not be reused.

## Draw/frame callback body: `0x0041fdd0`

The callback begins with the manager pointer on the stack. It always returns
one and has three independent direct effects:

* If `manager + 0x3e4 == 1`, it obtains a global configuration/result object
  through `0x00449950`, marks that object's `+0x35c` bit `0x40000000`, calls
  `0x00449870` with the manager `+0x3e0` value and that object, calls
  `0x004489d0` against global `0x00491c10`, stores its output at `+0x3dc`, and
  increments `+0x3e4` to two.
* If `manager + 0x3e8 == 1`, it lazily fills global `0x004776e0 + 0x89a4`
  through `0x00448d50` when that slot is null, then increments `+0x3e8` to
  two. The constants passed to the helper are `480.0f`, `392.0f`, `0.0f`, and
  integer `6`.
* It increments manager `+0x3ec` unconditionally.

The semantic purpose and exact types of the called global services remain
unknown. C++ should keep them behind named external wrappers rather than
declaring fabricated object classes.

## Recommended C++ boundary

The first C++ implementation should preserve layout and ownership while
placing unresolved machine-specific calls behind a narrow adapter layer:

```cpp
namespace th10 {

struct GlobalLifecycleManager;

GlobalLifecycleManager *GameAllocManager(u32 bytes);       // 0x00452493
void GameFreeManager(void *pointer);                        // 0x004524a1
void GameFreeOwnedBuffer(void *pointer);                    // 0x00452422
void StopManagerThreadControlEsiAbi(void *control);         // 0x0044c150
CallbackRecord *CreateCallbackRecord(CallbackTarget target); // 0x00449ed0
int RegisterCalculationEsiEdiAbi(CallbackRecord *, int, CallbackScheduler *);
int RegisterDrawEsiEdiAbi(CallbackRecord *, int, CallbackScheduler *);
void RemoveCallbackEcxEdxAbi(CallbackRecord *, CallbackScheduler *);

GlobalLifecycleManager *CreateGlobalLifecycleManager();
void TeardownGlobalLifecycleManager(GlobalLifecycleManager *);
void DestroyGlobalLifecycleManager(GlobalLifecycleManager *); // teardown + free

} // namespace th10
```

`DestroyGlobalLifecycleManager` is a semantic ownership wrapper, not a direct
name for `0x0041fb50`: it must call in-place teardown and then `GameFreeManager`.
The direct destructor wrapper must remain separately available for the factory
failure path and for any caller that owns allocation separately.

Initial C++ should use `u8 unknown_0010[0x378]` or a private opaque
thread-control wrapper for `+0x10..+0x387`. It should not claim a standard
thread object or add a C++ destructor until external free sites and full
unwind behavior are recovered. Likewise, no implementation should assume that
the manager owns every global released by teardown; the direct evidence proves
only that this teardown coordinates those releases.

## Remaining uncertainty

* The semantic identity of the thread entry `0x0041f990`, and whether the
  `+0x1c`/`+0x20` pair represents cancellation/running state, is unresolved.
* The global services called by `0x0041fdd0` and teardown have known ABI and
  effects at their call sites but incomplete types and names.
* The manager's callback node fields are stale after teardown; their clearing
  responsibility is not established.
* The factory's allocator-null behavior is internally unsafe, so normal
  `new`-style null/error semantics cannot be asserted from this binary.
