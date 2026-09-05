# MainChainUpdate Dependency Evidence

## Scope

This document recovers the direct dependencies of `MainChainUpdate` at
`0x0041ff80` that are needed for a conservative C++ reconstruction.  It covers
the exact entry ABI and direct memory accesses of `0x00421e00`, `0x0044a5f0`,
and `0x00447700`, plus the globals accessed directly by the update callback.

Names such as `TransitionRoot`, `InputSlot`, and `ManagerWork` are evidence
names only.  They do not identify original source classes or establish that
the objects belong to a particular engine subsystem.

## `MainChainUpdate` call sequence

`0x0041ff80` is a scheduler callback.  Its incoming context is
`ECX = 0x00491c28` (`g_MainChainContext`), it preserves `ESI`, and it returns
an `i32` in `EAX`.  The exact direct dependency calls are:

| Call site | Target | Argument setup | Required result behavior |
| --- | --- | --- | --- |
| `0x0041ffa6` | `0x00421e00` | `EAX = 0x00492590` | result ignored |
| `0x0041ffad` | `0x0044a5f0` | `ECX = 0` | result ignored |
| `0x0041ffb8` | `0x00447700` | push `*(u32 *)0x00491c10` | nonzero result makes callback return `4` |
| `0x0041ffe5` | `0x004218d0` | `EAX = context` | returned unchanged |

The first and last calls use a custom `EAX` input convention.  In particular,
neither is a normal MSVC `__thiscall` call: no receiver is placed in `ECX` and
no stack argument is pushed by this caller.

Before these calls, the callback reads byte `context+0x3cc` and pointer-sized
field `context+0x63c`.  When the byte is signed-negative and the pointer is
zero, it writes `3` to `0x00491fb8`.  After the manager call, it reads
`context+0x648`: zero selects the `0x004218d0` call; value `2` returns `4`;
every other nonzero value returns `1`.

## `0x00421e00`: transition tick

### Exact ABI

At entry, the function immediately executes `mov edi, eax`.  It does not read
an argument from the stack and returns with plain `ret`.  The proven ABI is:

```text
input:  EAX = pointer to root object (MainChainUpdate supplies 0x00492590)
output: EAX is caller-clobbered; caller ignores it
stack:  unchanged by callee (plain ret)
saved:  ESI, EDI
```

This is an internal custom register ABI, not a standard C++ calling
convention.  `__fastcall` must not be used to describe this entry: MSVC
`__fastcall` would place a first argument in `ECX`, not `EAX`.

### Proven layout and behavior

The root object is read only at `+0x5208`:

```cpp
struct TransitionRootPartial {
    u8 unknown_0000[0x5208];
    void *control; // +0x5208
};
```

When `control == 0`, the function returns without further accesses.  Otherwise
it repeatedly reloads the pointer and tests a dword at `control+0x1c` against
phases `1`, `2`, `4`, and `3`, in that order.  For each matching phase it
decrements `control+0x14`; a result not greater than zero stores phase `0`.

For phases `1`, `2`, and `4`, a still-positive countdown calls `0x0044d4e0`
with `EAX = control` and one stack `i32` argument.  The exact values passed
are respectively:

| Phase | Stack argument |
| ---: | --- |
| `1` | `((remaining * 5000) / *(i32 *)(control + 0x18)) - 5000` |
| `2` | `(remaining * -5000) / *(i32 *)(control + 0x18)` |
| `4` | `((remaining * 1000) / *(i32 *)(control + 0x18)) - 1000` |

For phase `1`, countdown completion additionally calls a virtual method at
`(**(void ***)(control+0x04))[0x48/4]` with the object loaded from
`*(void **)(control+0x04)` pushed as its sole argument.  The virtual method's
identity and the semantic meanings of phases/countdown/divisor remain
unproven.

Phase `3` only decrements `+0x14` and writes zero to `+0x1c` at completion.
The function does not validate the divisor at `+0x18`; a C++ reconstruction
must preserve the possibility of a zero-divisor fault unless independent
evidence establishes an invariant.

`0x0044d4e0` itself is also a mixed ABI helper: it consumes the control pointer
from `EAX`, one `i32` from the stack, and returns with `ret 4`.  It uses
`control+0x04` to reach a virtual interface and calls virtual slot `+0x3c`.
That helper need not be represented in the first C++ version of
`MainChainUpdate`; it is an implementation detail of the transition wrapper.

## `0x0044a5f0`: input-slot update

### Exact ABI

The prologue copies `ECX` to `EBX`, computes `0x474e30 + ECX * 0x6a`, and ends
in plain `ret`.  `MainChainUpdate` supplies `ECX = 0` and no stack arguments.

```text
input:  ECX = signed/unsigned slot index used directly as multiplier
output: EAX is caller-clobbered; MainChainUpdate ignores it
stack:  no callee stack-pop (plain ret)
saved:  EBX, ESI, EDI
```

The function is suitably described as a one-argument `__fastcall` interface
only at a C++ boundary, because its sole observed input happens to be in
`ECX`.  There is no evidence that `EDX` is an input.  Callers must pass a
valid index: this routine performs no bounds check before addressing the slot
array.

### Slot storage

The per-slot base is exactly `0x00474e30 + 0x6a * slot`.  The following fields
are written unconditionally after raw input is converted to a 16-bit mask:

```cpp
struct InputSlotPartial {
    u16 current;       // +0x00
    u16 previous;      // +0x02
    u16 repeat_mask;   // +0x04
    u16 pressed_mask;  // +0x06 = current & (previous ^ current)
    u16 released_mask; // +0x08 = ~current & (previous ^ current)
    u16 hold_frames[16]; // +0x0a; increment/reset logic is exact
    u8 unknown_002a[0x40];
};
typedef char AssertInputSlotPartialSize[
    sizeof(InputSlotPartial) == 0x6a ? 1 : -1];
```

For each of the sixteen low-order bits, `hold_frames[i]` increments while the
new `current` bit is set, resets to zero otherwise, and on reaching `0x1a`
sets the corresponding bit in `repeat_mask` before subtracting eight.  The
fields named `current`, `previous`, `pressed_mask`, and `released_mask` are
behavioral descriptions directly supported by their boolean expressions;
`repeat_mask` is conservative.

### Global dependencies and sampling paths

| Address | Exact use in `0x0044a5f0` | Conservative interpretation |
| --- | --- | --- |
| `0x004924fc` | dword tested for zero | input sampling enable/object-presence gate |
| `0x00491ff4` | `test ah, 2`, i.e. dword bit `0x200` | selects one of two raw-input paths |
| `0x00491c38` | dereferenced only in the bit-`0x200` path, virtual slots `+0x24` and `+0x1c` | opaque input device/interface pointer |
| `0x00474e30` | base of stride-`0x6a` slot storage | global `InputSlotPartial` array, count unknown |

If `0x004924fc` is zero, the generated mask remains zero.  Otherwise the
non-`0x200` path calls the imported function pointer at `0x00466208` with a
256-byte local buffer.  The `0x200` path invokes virtual `+0x24` on the object
at `0x00491c38`, requesting `0x100` bytes, and invokes virtual `+0x1c` to
recover on an `HRESULT` of `0x8007001e` or any other failure.  The raw bytes
are folded into a 16-bit internal mask by fixed masks and shifts; identifying
the device API as a particular DirectInput version is plausible but not needed
for this reconstruction and is not asserted here.

The resulting mask is passed to `0x0044a190` in `ECX`, with two stack dwords
`0` then `slot`; that helper returns the final mask in `AX`.  It is responsible
for mapping/suppression beyond raw sampling.  `0x0044a5f0` stores that `AX` in
`InputSlotPartial::current` and then derives the state-change fields above.

## `0x00447700`: manager-work service scan

### Exact ABI and return polarity

The function reads its only input from `[ESP+0x0c]` after saving `EBX` and
`ESI`, and ends with `ret 4` on every return path:

```text
input:  stack[0] = manager pointer
output: EAX = 0 on normal full scan; 0 or -1 on early service return
stack:  callee removes one 4-byte argument (ret 4)
saved:  EBX, ESI, EDI
```

Thus its external C++ ABI is ordinary one-argument `__stdcall`, not an
`EAX`/`ECX` custom ABI.  `MainChainUpdate` pushes the current dword value of
`0x00491c10` as that argument.

The manager contains an exactly observed 33-pointer region:

```cpp
struct ManagerWorkOwnerPartial {
    u8 unknown_0000[0x3ad06c];
    void *work_slots[0x21]; // +0x3ad06c through +0x3ad0ec
};
```

The function scans indices `0..0x20`.  A null slot is skipped.  For a non-null
slot object, it reads dwords at `+0x128` and `+0x124`.

- A nonzero `slot_object+0x128` enters a cleanup/free path using
  `0x00447810` and allocator release `0x004524a1`, then clears observed
  ownership/state fields.  The exact meaning of `+0x128` is not established.
- If `+0x128` is zero and `+0x124` is zero, the scan continues.
- If `+0x128` is zero and `+0x124` is nonzero, it calls `0x004473c0` with two
  stack arguments: manager first and the slot object second.  The current
  body of `0x004473c0` uses the second argument as its main object and returns
  `1` on its successful processing path, `0` on one failure path, and `-1` on
  other failure paths.

The early return normalizes only zero/nonzero from `0x004473c0`: it returns
`0` when that callee is nonzero, and `-1` when it is zero.  Consequently the
`test eax,eax; je` at `0x0041ffbd` means `MainChainUpdate` continues after a
successful/nonzero service result, and returns `4` only when the scan itself
returns nonzero (the observed early `-1` case).  A readable C++ interface
should preserve this polarity instead of naming the result a generic boolean.

`0x00447810` is not a conventional no-argument C++ function: it consumes the
slot object from `EDI`.  It has plain `ret` and releases internal fields from
that object.  This register dependency is internal to the cleanup path and
does not change the stack ABI of `0x00447700`.

## Direct MainChain globals

| Address | Width/access by `0x0041ff80` | Proven fact | Uncertainty |
| --- | --- | --- | --- |
| `0x00491c28` | callback context in `ECX` | MainChain partial object; reads `+0x3cc`, `+0x63c`, `+0x648` | total size and source-level type unknown |
| `0x00491fb8` | dword write `3` | shared state/status location | its domain and owner are unknown; other lifecycle code writes `4` |
| `0x00492590` | address materialized into `EAX` | root used by `0x00421e00`, with a pointer at `+0x5208` | object type and total extent unknown |
| `0x00491c10` | dword loaded and pushed | manager pointer passed to `0x00447700` | also used by rendering/lifecycle code; no single source-level type is proven |

## Conservative C++ boundary

The following interfaces are sufficient to express a readable
`MainChainUpdate` without claiming unsupported types:

```cpp
struct TransitionRootPartial;
struct ManagerWorkOwnerPartial;

// Address 0x00421e00.  The binary entry requires root in EAX.
// Implement this as a compiler-specific EAX thunk; do not declare the entry
// itself __thiscall or __fastcall.
void AdvanceTransitionEaxAbi(TransitionRootPartial *root);

// Address 0x0044a5f0.  The binary entry receives slot in ECX only.
void __fastcall UpdateInputSlotEcxAbi(i32 slot);

// Address 0x00447700.  Normal one-stack-argument ABI, ret 4.
i32 __stdcall ServiceManagerWork(ManagerWorkOwnerPartial *manager);

i32 __fastcall MainChainUpdate(MainChainContext *context) {
    if (static_cast<i8>(context->callback_state_byte) < 0 &&
        context->field_063c == 0) {
        g_MainChainStatus_491fb8 = 3;
    }

    AdvanceTransitionEaxAbi(&g_TransitionRoot_492590);
    UpdateInputSlotEcxAbi(0);

    if (ServiceManagerWork(g_ManagerWorkOwner_491c10) != 0)
        return 4;

    if (context->field_0648 != 0)
        return context->field_0648 == 2 ? 4 : 1;

    return AdvanceMainChainStateEaxAbi(context);
}
```

`AdvanceMainChainStateEaxAbi` denotes the separately recovered `0x004218d0`
entry, which also requires its context in `EAX`.  The globals in this example
are intentionally evidence names, not declarations to add directly to a
header.  In particular, `UpdateInputSlotEcxAbi` may use `__fastcall` at the
C++ wrapper boundary because its direct argument is in `ECX`; the two EAX-ABI
functions require explicit thunks or compiler-specific inline assembly.

## Verification basis

- `resources/th10.exe`, `objdump -D -Mintel`, ranges
  `0x0041ff80-0x0041fff1`, `0x00421e00-0x00421ee1`,
  `0x0044a5f0-0x0044a942`, and `0x00447700-0x00447787`.
- Direct helper ranges `0x0044d4e0-0x0044d54a`,
  `0x0044a190-0x0044a4cf`, `0x004473c0-0x004476f8`, and
  `0x00447810-0x00447902` were inspected only to establish ABI, return
  polarity, and the stated direct field accesses.
- Existing callback and global-lifecycle evidence was used solely for
  cross-checking global addresses and callback context registration.
