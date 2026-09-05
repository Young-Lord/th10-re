# Transition Sound Adapter Lifecycle

This note is limited to the construction/destruction boundary of the
descriptive `TransitionSoundAdapter` family in `resources/th10.exe`.  The
class name is not recovered from the binary.  It documents the three requested
entry points and the allocator APIs they call, so a C++ implementation can
preserve ownership without pretending that their register ABIs are ordinary
constructors.

## Object boundary and related entry points

The proven allocation size for the derived, one-buffer object is `0x78` bytes.
The two successful construction paths at `0x0044c8f0` and `0x0044cbf0` allocate
that size through `0x00452493`, place the result in `ESI`, and call
`0x0044d6a0`.  The object starts with base vtable `0x004705d8` during
construction and ends with derived vtable `0x004705d4`.

| Address | Role | Receiver / arguments | Return and cleanup |
| --- | --- | --- | --- |
| `0x0044ce80` | common/base constructor | `ESI = object`, `EAX = source IDirectSoundBuffer* array`, `EDI = unsigned count`, two stack dwords | `EAX = object`; callee `ret 8` |
| `0x0044d080` | non-deleting base destructor | `ECX = object` | plain `ret` |
| `0x0044cf00` | base scalar deleting wrapper | `ECX = object`, one stack deletion flag | runs `0x44d080`; frees object when flag bit 0 is set; `ret 4` |
| `0x0044d6a0` | derived one-slot constructor | `ESI = object`, four stack dwords | `EAX = object`; callee `ret 0x10` |
| `0x0044d730` | derived non-deleting destructor | `ECX = object` | assigns derived vtable then tail-calls `0x44d080` |
| `0x0044d710` | derived scalar deleting wrapper | `ECX = object`, one stack deletion flag | runs `0x44d730`; frees object when flag bit 0 is set; `ret 4` |

`0x0044ce80` and `0x0044d6a0` therefore require explicit ABI thunks if the
recovered C++ must be called from the original binary.  A normal MSVC C++
constructor is a useful semantic implementation, but cannot be exported at
either address directly: both constructors receive `this` in `ESI`, and the
base form also uses `EAX` and `EDI` as formal inputs.

## Confirmed layout used by lifetime code

```cpp
struct TransitionSoundAdapterLayout {
    void **vtable;                         // +0x00
    IDirectSoundBuffer **slots;            // +0x04, heap-owned pointer array
    void *field_0008;                      // +0x08, constructor argument
    void *owned_auxiliary;                 // +0x0c, released by destructor
    unsigned long slot_count;              // +0x10
    unsigned char fields_0014_to_005b[0x48];
    unsigned long field_005c;              // +0x5c, derived constructor clears
    unsigned long field_0060;              // +0x60, derived constructor clears
    unsigned long field_0064;              // +0x64, derived constructor clears
    unsigned long field_0068;              // +0x68, derived constructor clears
    void *field_006c;                      // +0x6c, fourth derived argument
    unsigned char fields_0070_to_0077[0x08];
};
```

Only names describing directly observed ownership are semantic claims.  In
particular, the stored values at `+0x08` and `+0x6c` have no established
ownership rule.

## Common constructor: `0x0044ce80`

The routine first writes the base vtable, then computes the byte count with a
32-bit `lea count * 4`, calls `0x00452493`, and stores the result at `+0x04`.
It does not test that allocation before copying.

It copies `source[i]` to `slots[i]` for every unsigned `i < count`, writes the
two stack arguments to `+0x08` and `+0x0c`, and writes `count` to `+0x10`.
There is no `AddRef` in this loop.  The later destructor's unconditional
`Release` proves that the entries are transferred COM references, not ordinary
borrowed pointers, unless the caller had acquired an independent reference
before this entry.

Next it calls `0x0044d110` with `EAX = object`, stack arguments `slots[0]` and
zero.  Its HRESULT is ignored.  The constructor then calls the slot vtable at
`+0x34` with argument zero for every slot, which is the known
`IDirectSoundBuffer::SetCurrentPosition(0)` call, writes zero at `+0x30`, and
returns the object.  Thus `count == 0`, a null allocation, or a null entry is
not a supported constructor input even though portions of the copying loop are
conditionally skipped.

The raw implementation also has no checked multiplication: a sufficiently
large count can wrap `count * 4` before allocation while the subsequent copy
still iterates to the original count.  A semantic C++ factory may reject that
input; a codegen-compatibility path must retain this precondition rather than
silently substitute a bounded container.

## Derived one-slot constructor: `0x0044d6a0`

This constructor is not merely a conventional three-argument specialization.
With the entry stack denoted by `S`, its four dwords are:

| Input | Destination |
| --- | --- |
| `[S+4]` | newly allocated `slots[0]` |
| `[S+8]` | object `+0x08` |
| `[S+0xc]` | object `+0x0c` |
| `[S+0x10]` | object `+0x6c` |

Its exact sequence is:

1. Write base vtable `0x004705d8`.
2. Allocate four bytes through `0x00452493`; store the returned pointer at
   `+0x04`; immediately write argument one to that allocation.
3. Store arguments two and three at `+0x08` and `+0x0c`, set `+0x10 = 1`, then
   call `0x0044d110` for slot zero with zero as its second stack argument.
   The return value is ignored.
4. Call slot zero's `SetCurrentPosition(0)`, set `+0x30`, `+0x5c`, `+0x60`,
   `+0x64`, and `+0x68` to zero, set the derived vtable `0x004705d4`, and
   store argument four at `+0x6c`.

The function does not validate the four-byte allocation, the buffer argument,
or either DirectSound call.  The factories that invoke it test only the outer
`0x78` allocation.  Consequently, a failure of the internal slot allocation
would fault in the original constructor rather than produce a usable failure
return.

## Base destructor: `0x0044d080`

The non-deleting destructor performs the following order, which is the C++
ownership contract to preserve:

1. Select the base vtable `0x004705d8`.
2. For each unsigned index below `slot_count`, if `slots[i]` is non-null, call
   vtable `+0x08` (`IUnknown::Release`) and then clear that array element.
3. If `slots` is non-null, release the pointer-array allocation through
   `0x004524a1` and clear `+0x04`.
4. If `owned_auxiliary` is non-null, conditionally close its handle:
   when `owned_auxiliary + 0x78 == 1`, call `KERNEL32!CloseHandle` on its
   `+0x8c` dword and write `-1` back to that dword.  Then free the auxiliary
   object through `0x004524a1` and clear `+0x0c`.

It does not clear `slot_count`, `+0x08`, the timer/configuration fields, or
`+0x6c`.  It is therefore a one-shot teardown, not a reconstructible/resettable
destructor.  With a nonzero retained `slot_count`, a second call reaches the
slot loop with `slots == NULL` and dereferences that null array pointer before
it can skip the already-cleared entries.  No idempotence contract exists.

The deleting wrappers establish two distinct outer-object responsibilities:
`0x0044cf00` frees after base teardown, while the derived wrapper `0x0044d710`
first restores the derived vtable via `0x0044d730` and then follows the same
free-on-bit-0 rule.  The whole-object free is not performed by `0x0044d080`.

## Direct allocator APIs

| Address | Boundary | Proven behavior relevant here |
| --- | --- | --- |
| `0x00452493` | one stack `size` argument, plain `ret` | passes `(size, 1)` to `0x004526da`; this is the allocation entry used for the slot array and `0x78` outer object |
| `0x004526da` | stack `(size, retry_flag)`, plain `ret` | rejects sizes greater than `0xffffffe0`; on allocation failure and nonzero retry flag, calls the CRT new-handler route at `0x00455737` and retries; returns null if unsuccessful |
| `0x0045265f` | one stack size argument, internal CRT heap boundary | for the normal heap route it calls `KERNEL32!HeapAlloc(DAT_00499340, 0, rounded_size)`; flags are zero, so this is not zero-initializing allocation |
| `0x004524a1` | one stack pointer argument, tail jump | enters `0x00452422`, the matching CRT deallocation route |
| `0x00452422` | one stack pointer argument, plain `ret` | accepts null; for the normal heap route calls `KERNEL32!HeapFree(DAT_00499340, 0, pointer)` |

`0x00452493` may return null despite its retry flag.  That fact matters for
factories that test an outer allocation, but the two constructors themselves
dereference the inner slot allocation without checking it.  The allocation
memory is not cleared by the direct `HeapAlloc` route; all fields claimed
initialized above have explicit stores.

## C++ implementation plan

Use a semantic C++98 class for ownership and expose separate x86 thunks only
where native callers require a nonstandard register contract.

```cpp
class TransitionSoundAdapter {
public:
    // Takes ownership of one COM reference per non-null entry.
    TransitionSoundAdapter(IDirectSoundBuffer *const *source,
                           unsigned long count,
                           void *field_0008,
                           void *owned_auxiliary);
    ~TransitionSoundAdapter(); // non-deleting semantic teardown
};

class SingleTransitionSoundAdapter : public TransitionSoundAdapter {
public:
    // Preserve all four observed inputs, including the +0x6c value.
    SingleTransitionSoundAdapter(IDirectSoundBuffer *buffer,
                                 void *field_0008,
                                 void *owned_auxiliary,
                                 void *field_006c);
};
```

The implementation should use the game allocator wrappers for both the outer
object and `slots`, not `new[]`, `delete[]`, `std::vector`, or a `ComPtr` that
would add references.  Constructor initialization must be explicit because
the original allocation is uninitialized.  Teardown must Release all slots
before releasing the slot array, then perform the auxiliary handle-close/free
sequence.  Keep the `0x0044d110` and `SetCurrentPosition(0)` operations behind
typed DirectSound helper calls, but do not convert their ignored HRESULTs into
new constructor failure behavior without a deliberate behavioral decision.

For native entry replacement, keep a thin assembly-independent compiler thunk
boundary that marshals `ESI/EAX/EDI` and the exact stack parameters into these
semantic constructors.  The class logic should remain ordinary C++; the ABI
marshalling is a distinct implementation problem and should not be embedded in
the class declarations as false `__thiscall` signatures.

## Verification basis

* `resources/th10.exe`, disassembly `0x0044ce80-0x0044cf1b`,
  `0x0044d080-0x0044d107`, `0x0044d6a0-0x0044d73a`, and the construction
  callers `0x0044c8f0-0x0044cbae` and `0x0044cbf0-0x0044ce7a`.
* Allocator paths `0x00452422-0x004524a5`, `0x0045265f-0x00452705`, and the
  PE import table for `HeapAlloc`, `HeapFree`, and `CloseHandle`.
* Existing adapter-operation evidence in `transition-sound-adapter.md`.
