# Transition Sound Adapter (`0x0044d4e0`)

This note records the DirectSound transition adapter used by the transition
root at `0x00492590` in `resources/th10.exe`.  `TransitionSoundAdapter` is a
descriptive name only.  The binary proves an adapter with a dynamically owned
array of `IDirectSoundBuffer*`; it does not prove the original C++ class name.

## `0x0044d4e0` ABI and direct callers

The volume entry has a custom register/stack boundary, not a normal C++ member
function ABI:

```text
input:  EAX = TransitionSoundAdapter*
        [ESP+4] = signed 32-bit input volume
stack:  callee removes the one stack argument (`ret 4`)
output: EAX is incidental; HRESULT is discarded
```

It neither validates the adapter, its slot-array pointer, nor slot zero.  Its
receiver load is exactly:

```asm
mov eax, [eax+0x04] ; IDirectSoundBuffer** slots
mov esi, [eax]      ; slots[0]
mov edi, [esi]      ; slots[0]->lpVtbl
```

The complete direct-call set is:

| Call site | Caller behavior | Input-volume source |
| --- | --- | --- |
| `0x00421e47`, `0x00421e75`, `0x00421ea8`, `0x00421ed9` | `0x00421e00` transition tick | four active fade-phase quotient formulas |
| `0x0043de5a` | transition/state dispatcher `0x0043ddf0` | owner field `+0x52c4` |
| `0x0044d4a4` | adapter start/configuration method `0x0044d440` | literal `0`, after clearing phase/countdown fields |

The four internal phase helpers at `0x0044d740`, `0x0044d790`, `0x0044d7d0`,
and `0x0044d810` contain equivalent calls but have no direct callers in the
current program reference graph.  They are not evidence of additional
executed call sites.

## Typed DirectSound calls

Slot zero is invoked through these exact `IDirectSoundBuffer` vtable entries:

```cpp
typedef long HRESULT;
typedef long LONG;

struct IDirectSoundBuffer;

typedef HRESULT (__stdcall *IDirectSoundBuffer_SetVolume)(
    IDirectSoundBuffer *self, LONG volume);       // vtable +0x3c, index 15
typedef HRESULT (__stdcall *IDirectSoundBuffer_Stop)(
    IDirectSoundBuffer *self);                    // vtable +0x48, index 18
```

`0x0044d4e0` calls `SetVolume` and ignores its `HRESULT`.  When a phase-1
transition expires, `0x00421e24-0x00421e30` loads the same `slots[0]` receiver
and calls `Stop`, also ignoring its `HRESULT`.  The adapter helper at
`0x0044d5b0` is another slot-zero stop operation: it clears adapter `+0x30`,
then calls `slots[0]->Stop()` and returns that `HRESULT`.

The use of these two vtable slots is sufficient to type the calls as the
DirectSound methods above.  It does not prove that the concrete object has no
additional interface methods beyond the `IDirectSoundBuffer` prefix.

## Proven adapter and slot layout

The transition control consumed at root `+0x5208` is this adapter object.  Its
constructor at `0x0044d6a0` receives the object in `ESI`, allocates exactly
four bytes for one slot, stores its first input as slot zero, and writes count
one at `+0x10`.  This is a derived single-buffer constructor: it first assigns
base vtable `0x4705d8`, then replaces it with `0x4705d4` after initialization.

```cpp
struct TransitionSoundAdapterPartial {
    void **vtable;                              // +0x00, assigned 0x4705d4
    IDirectSoundBuffer **slots;                 // +0x04, heap array
    void *field_0008;                           // +0x08, constructor input 2
    void *owned_auxiliary;                      // +0x0c, constructor input 3
    unsigned long slot_count;                   // +0x10, constructor writes 1
    unsigned char unknown_0014[0x08];
    long transition_phase;                      // +0x1c
    unsigned char unknown_0020[0x10];
    long playback_active;                       // +0x30
    // At least through +0x77; constructor is passed a 0x78-byte allocation.
};
```

The transition timer fields used by `0x00421e00` are `+0x14` (remaining
ticks), `+0x18` (duration), and `+0x1c` (phase).  Their names express the
transition use only; the same adapter is also used for non-transition sound
operations.  `+0x20` and `+0x24` retain the start/configuration arguments
after `0x0044d440`; `+0x2c` is cleared there; `+0x30` is the observed playback
active flag.  No complete semantic definition of those fields is established.

### Ownership and access boundaries

`0x0044c8f0` and `0x0044cbf0` each create a DirectSound buffer through an
interface vtable call at `+0x0c` (`CreateSoundBuffer`), receive the output in a
local `IDirectSoundBuffer*`, allocate `0x78` bytes, and pass that pointer as
constructor argument one to `0x0044d6a0`.  Neither path releases the output
after successful construction.  This is direct transfer-of-the-creation
reference evidence: the adapter owns the initial COM reference placed in
`slots[0]`; the constructor itself does not call `AddRef`.

The common base constructor at `0x0044ce80` establishes the general slot-array
case.  Its nonstandard inputs are `ESI = adapter`, `EAX = source pointer
array`, `EDI = count`, plus two stack fields stored at `+0x08` and `+0x0c`;
it allocates `count * 4` bytes at `+0x04`, copies every source pointer into the
new array, writes the count to `+0x10`, runs the `0x0044d110` slot-zero setup,
then initializes every slot with `SetCurrentPosition(0)`, and leaves `+0x30`
zero.  It also does not call `AddRef`.  The slot-zero setup is unconditional,
so a zero count is not a safe construction input despite the copy loop itself
allowing it.  Therefore copied source entries are ownership transfers into
this adapter family, not borrowed aliases, unless a separate caller explicitly
increments a reference before construction.  `0x0044d6a0` is the proven
one-entry specialization of that model, not evidence that the base object can
never contain more than one slot.

The destructor chain is equally explicit:

| Routine | Register ABI | Slot behavior |
| --- | --- | --- |
| `0x0044d080` | `ECX = adapter` | for every `i < slot_count`, if `slots[i] != NULL`, call vtable `+0x08` (`IUnknown::Release`), then set `slots[i] = NULL`; free `slots`; then free `owned_auxiliary` with its conditional handle close |
| `0x0044d730` | `ECX = adapter` | selects the derived vtable then tail-calls `0x0044d080` |
| `0x0044cf00` | `ECX = adapter`, `[ESP+4]` deletion flag | runs `0x0044d080`; frees the `0x78` object when flag bit 0 is set |

The constructor also calls `SetCurrentPosition(0)` on `slots[0]` through
vtable `+0x34`.  Bulk operations use every slot without per-slot null checks:
`0x0044d550` calls `Stop()` then `SetCurrentPosition(0)` for each slot and
clears `+0x1c`; `0x0044d600` calls `SetCurrentPosition(0)` for each slot.
Thus the destructor permits null slots, while normal operational paths require
each slot below `slot_count` to be non-null.

The transition path is deliberately narrower than the owning adapter: it
always selects slot zero.  It neither releases it nor clears the slot.  A C++
reconstruction should preserve that borrowing boundary rather than make the
transition control own an `IDirectSoundBuffer*` directly.

## Volume curve and x87 conversion

`DAT_00497854` is a signed dword, initialized at `0x0042e5a0` by sign
extending byte `DAT_00491d68`.  Let `g` be that value and `x` be the stack
input.  If `g == 0`, the adapter directly calls:

```cpp
slots[0]->SetVolume(-10000L);
```

Otherwise `0x0044d4ea-0x0044d52d` computes in x87 extended precision:

```text
t = 1.0f - (float)g * 0.01f
v = (float)(x + 5000) * (1.0f - t * t)
volume = ftol2_truncate_to_i32(v) - 5000
```

The `x + 5000` add occurs in signed 32-bit integer arithmetic before the x87
conversion; overflow wraps at the instruction level.  The float constants are
`1.0f` at `0x00470afc` and `0.01f` at `0x00470b00`.

`0x00463b2c` is the MSVC x87 `float`-to-integer helper, not a plain C++ cast.
It duplicates `ST(0)`, stores a float copy, executes `FISTP qword` under the
current x87 control word, reloads that integer, compares the residual, and
adjusts `EDX:EAX` toward zero when the initial rounded value crossed the input.
For finite in-range values, the observable low-32-bit result used here is
truncation toward zero.  The intermediate `FISTP` still makes the active x87
control word, 80-bit intermediates, exceptional values, and out-of-range
behavior part of the native contract.  Do not replace it with a cast, SSE
conversion, `floor`, or `round` in a codegen-sensitive implementation.

In particular, a semantic implementation may explicitly constrain its input
to finite signed-32-bit-convertible values and use truncation toward zero, but
an object-code-oriented replacement should retain an x87 helper/thunk with
the original FPU environment.  The binary uses only `EAX` from the helper and
discards the surviving x87 value with `fstp st(0)`.

## Conservative C++ boundary

The semantic layer can express DirectSound normally while keeping the native
mixed ABI at a separate thunk:

```cpp
struct TransitionSoundAdapterPartial {
    void **vtable;
    IDirectSoundBuffer **slots;
    void *field_0008;
    void *owned_auxiliary;
    unsigned long slot_count;
    long remaining_ticks;
    long duration_ticks;
    long transition_phase;
    // remaining fields intentionally omitted
};

void ApplyTransitionVolumeSemantic(TransitionSoundAdapterPartial *adapter,
                                   long input_volume)
{
    IDirectSoundBuffer *const buffer = adapter->slots[0];
    // Preserve the x87 helper boundary for native-equivalent conversion.
    buffer->SetVolume(/* native curve result */);
}

// Native entry 0x0044d4e0: EAX=adapter, one stack LONG, callee `ret 4`.
void ApplyTransitionVolumeEaxAbi(TransitionSoundAdapterPartial *adapter,
                                 long input_volume);
```

Do not declare `ApplyTransitionVolumeEaxAbi` as a conventional C++ member
function: neither its receiver register nor its stack cleanup matches one.
The adapter destructor is the appropriate owner of the `slots` allocation and
each transferred COM reference; transition code is only a consumer of
`slots[0]`.

## Verification basis

* `resources/th10.exe`, disassembly `0x0044d4e0-0x0044d549`,
  `0x0044d080-0x0044d107`, `0x0044d440-0x0044d4ce`,
  `0x0044d550-0x0044d63c`, and `0x0044d6a0-0x0044d72b`.
* Call/reference graph for `0x0044d4e0`, `0x0044d6a0`, and `0x0044d080`.
* Construction paths `0x0044c8f0` and `0x0044cbf0`; transition callers
  `0x00421e00` and `0x0043ddf0`.
* Conversion helper `0x00463b2c-0x00463ba0` and initializer
  `0x0042e5a0-0x0042e60f`.
