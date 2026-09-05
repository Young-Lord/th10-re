# ManagerWork Service Evidence

## Scope and naming

This note records the directly observable behavior of the manager-work service
at `0x00447700` and its direct helpers `0x004473c0` and `0x00447810` in
`resources/th10.exe`.  `ManagerWorkOwner`, `ManagerWork`, and `WorkRecord`
are behavioral names only.  They do not identify the original source classes.

The three functions form three distinct ownership layers:

1. `0x00447700` scans 33 owner slots and conditionally services one work
   object.
2. `0x004473c0` advances one active work object's chained batch.
3. `0x00447810` releases allocations and virtual-interface objects owned
   *inside* one work object.  It never frees that outer work allocation.

## `0x00447700`: service scan

### ABI and return polarity

The entry saves `EBX`, `ESI`, and `EDI`, loads its only input from
`[ESP+0x0c]`, and returns with `ret 4` on every path:

```text
input:  stack[0] = ManagerWorkOwner *owner
output: EAX = 0 on a completed scan or when the selected work service is
        nonzero; EAX = -1 when the selected work service returns zero
stack:  callee removes one 4-byte argument
saved:  EBX, ESI, EDI
```

This is a one-argument x86 `__stdcall` boundary.  It has no null-owner guard:
the initial address calculation and slot read dereference the supplied owner.

The return value is a status polarity, not a conventional success boolean.
At `0x0041ffb8`, `MainChainUpdate` treats any nonzero return as its own return
code `4`; it therefore continues only when this service returns zero.

### Owner layout

`0x00447709` adds `0x3ad06c` to the owner pointer and iterates by four bytes
until exactly 33 slots have been visited.

```cpp
struct ManagerWorkOwnerPartial {
    unsigned char unknown_0000[0x3ad06c];
    ManagerWork *work_slots[0x21]; // +0x3ad06c .. +0x3ad0ec
};
```

The range and element count are direct facts.  No total owner size, slot
index meaning, or synchronization rule is established here.

### Exact scan behavior

For each index `0..32`:

* A null `work_slots[index]` is skipped.
* With non-null `work`, a nonzero `work+0x128` takes the literal cleanup path:
  call `0x00447810` with `EDI = work`, call `0x004524a1(work)`, and write zero
  to the slot.
* With `work+0x128 == 0` and `work+0x124 == 0`, the slot is skipped.
* With `work+0x128 == 0` and `work+0x124 != 0`, it calls
  `0x004473c0(owner, work)`.  Its result is normalized solely by zero/nonzero:
  a nonzero callee result returns `0` from the service; a zero result returns
  `-1` immediately.

The cleanup branch falls through after zeroing its slot and executes
`mov ecx, [slot]; mov [ecx+0x128], 0`.  Taken literally, that dereferences
address `0x128`.  This cannot be modeled as a normally safe C++ cleanup
operation.  It may be dead under the program's runtime invariants, reflect an
intentional trap, or depend on behavior outside this local range; none is
proven.  A semantic reconstruction must keep this branch isolated and must
not silently label `+0x128` as a normal deletion request.

`0x004524a1` is the outer-allocation release used after `0x00447810` by this
service and by independent owner destruction sites.  Those callers clear the
owner slot after the release, confirming that the owner holds the outer
allocation pointer; `0x00447810` itself does not perform that release.

## Shared work-object layout

The following fields are directly used by one or both helpers.  Names denote
observed behavior, not source-level types.

```cpp
struct WorkRecordPartial {
    void *virtual_object;   // +0x00, called through vtable slot +0x08
    void *owned_allocation; // +0x04, released through 0x00452422
    unsigned char unknown_08[0x08];
}; // record stride is 0x10

struct ManagerWorkPartial {
    unsigned char unknown_0000[0x108];
    void *chain_head;            // +0x108; also cleanup gate and owned release
    int work_record_count;       // +0x10c; number of 0x10-byte records
    unsigned char unknown_0110[0x08];
    void *output_records_44;     // +0x118; freed by cleanup
    void *output_pointer_list;   // +0x11c; freed by cleanup
    WorkRecordPartial *records;  // +0x120; count records, stride 0x10
    int chain_cursor_or_count;   // +0x124; service-active field
    int cleanup_or_state;        // +0x128; exact meaning unproven
    void *owned_012c;            // +0x12c; freed by cleanup
};
```

`chain_head` points to a relative-linked sequence: `0x004473c0` begins with
that pointer and advances using a signed/unsigned dword displacement at each
node's `+0x38`; zero terminates the chain.  Each visited node contributes its
dwords `+0x00` and `+0x04` to two accumulators.  The node layout and meaning
of those aggregates are not established.

The `+0x124` field is both the activity gate tested by `0x00447700` and the
cursor/count manipulated by `0x004473c0`.  It must not yet be named a simple
"count": the helper compares its loop index to `field - 1`, later increments
it on one completion path, and clears it on other terminal paths.

## `0x004473c0`: advance one work chain

### ABI

The function ends with `ret 8`; it saves `EBX`, `EBP`, `ESI`, and `EDI`.
After its initial `push ecx`, it loads `EBP` from the second stack argument.
The proven boundary is:

```text
input:  stack[0] = ManagerWorkOwner *owner
        stack[1] = ManagerWork *work
output: EAX = work on non-error terminal paths
        EAX = 0 if its inner processor returns a negative result
stack:  callee removes two 4-byte arguments
saved:  EBX, EBP, ESI, EDI
```

Thus a readable external wrapper is `__stdcall`, even though the inner helper
called at `0x00447470` uses a nontrivial five-stack-argument interface.

### Control flow and state effects

The function walks the chain starting at `work+0x108`, accumulating node
`+0x00` and `+0x04` into two local integers and counting nodes.  It has three
terminal outcomes:

| Condition | `work+0x124` after return | `EAX` |
| --- | ---: | --- |
| The current node has no `+0x38` successor | `0` | `work` |
| The special inner processing call returns negative | `0` | `0` |
| A traversed chain reaches its normal completion path | old value plus `1` | `work` |

The special call occurs when the current zero-based chain index equals
`work+0x124 - 1`.  It calls `0x00447470` with the owner, work, both
accumulators, and current node supplied on the stack.  That helper validates
and interprets the node, allocates/populates data in the work object's
`+0x118`, `+0x11c`, and `+0x120` regions, and returns `1` on its observed
success path or negative on validation/allocation failures.  Its detailed
render/format semantics are outside this note.

Crucially, `0x004473c0` treats *any* negative `0x00447470` result as failure,
but returns the work pointer for zero or positive results.  The outer service
then separately treats this non-null pointer as its zero/success status.

## `0x00447810`: release owned work contents

### ABI

This entry has no stack arguments and ends in plain `ret`.  It reads the work
pointer from `EDI` throughout:

```text
input:  EDI = ManagerWork *work
output: no defined result; EAX caller-clobbered
stack:  unchanged
saved:  EBP
```

It is an internal register ABI.  Do not expose the original entry as a normal
C++ member function or a no-argument function.  A conventional C++ wrapper
can take `ManagerWork *work` and install it in `EDI` in a small ABI thunk.

### Cleanup order and ownership

The first operation tests `work+0x108`.  If it is null, the routine returns
without accessing or clearing any later field.  If it is non-null, it:

1. Calls `0x004493e0` with `EAX = *(void **)0x00491c10` and `EDX = work`.
   That helper walks two manager-owned linked lists and sets bit `0x04000000`
   in `entry+0x35c` for every entry whose `+0x308` equals `work`.
2. For `i` in `0 .. work_record_count-1`, processes
   `records[i]` (stride `0x10`): calls virtual vtable slot `+0x08` with the
   `virtual_object` pointer as its sole stack argument when non-null, clears
   that field, then releases `owned_allocation` through `0x00452422` and
   clears it.
3. Releases and clears, in this order, `+0x120`, `+0x118`, `+0x11c`, `+0x12c`,
   and finally `+0x108`, each through `0x00452422` when non-null.

It does **not** clear `+0x10c`, `+0x124`, or `+0x128`.  Its effective contract
is therefore "release owned subordinate resources once `chain_head` exists",
not a resettable, idempotent destructor.  It is followed by
`0x004524a1(work)` at all observed whole-work deletion sites.

The identity of the virtual slot `+0x08` is unproven.  It must be represented
as an opaque release callback rather than asserted to be COM `Release`, a
destructor, or a particular graphics API method.

## Direct dependency map

| Caller | Direct dependency | Exact observed boundary | Purpose established here |
| --- | --- | --- | --- |
| `0x00447700` | `0x00447810` | `EDI = work`, plain `ret` | release internals before outer free |
| `0x00447700` | `0x004524a1` | one stack pointer; caller repairs stack | releases outer work allocation |
| `0x00447700` | `0x004473c0` | two stack pointers, `ret 8` | advances selected active work |
| `0x004473c0` | `0x00447470` | five stack arguments, `ret 0x14` | validates/processes current chain stage |
| `0x00447810` | `0x004493e0` | `EAX = global manager`, `EDX = work`, plain `ret` | marks associated linked-list entries |
| `0x00447810` | vtable slot `+0x08` | one stack pointer | releases an opaque per-record interface object |
| `0x00447810` | `0x00452422` | one stack pointer; caller repairs stack | releases subordinate allocations |

## Safe C++ reconstruction boundary

The C++ model should preserve the semantic ownership split and isolate the
two custom-register entries behind wrappers:

```cpp
enum ManagerWorkServiceResult {
    ManagerWorkService_Continue = 0,
    ManagerWorkService_Stop = -1,
};

// Models 0x00447700's externally visible polarity.
ManagerWorkServiceResult __stdcall ServiceManagerWork(ManagerWorkOwnerPartial *owner);

// Models 0x004473c0.  Non-null is the outer service's continue result.
ManagerWorkPartial *__stdcall AdvanceManagerWork(
    ManagerWorkOwnerPartial *owner, ManagerWorkPartial *work);

// Semantic implementation; the original address requires EDI = work.
void ReleaseManagerWorkContents(ManagerWorkPartial *work);
```

`ReleaseManagerWorkContents` is implemented as readable C++; only the binary
call boundary needs an `EDI` thunk. The `+0x128` cleanup
branch must remain an explicitly unresolved policy: do not make it a normal
`delete work` path until a caller or initialization path proves why the
post-slot-clear dereference is safe.

## Evidence basis

* `resources/th10.exe`, ranges `0x004473c0-0x004476f8`,
  `0x00447700-0x00447787`, and `0x00447810-0x00447902`.
* Direct call-site cross-checks, notably `0x0041ffb8` and independent whole
  work cleanup callers in the executable's `0x00401000` and `0x004024d0`
  regions.
* `0x00447470-0x004476f8` and `0x004493e0-0x0044943b` were inspected only
  for the stated inter-function contracts and side effects.
