# Transition Tick (`0x00421e00`)

This document records the complete directly-observable transition-tick design
in `resources/th10.exe`.  Names such as `TransitionControl` describe observed
roles, not an asserted original class name.

The entry is called by `0x0041ff80` as:

```asm
mov eax, 0x00492590
call 0x00421e00
```

It is therefore **not** a normal `__thiscall`, `__stdcall`, or `__fastcall`
entry.  Its sole input is a root pointer in `EAX`; it takes no stack arguments,
returns with plain `ret`, preserves `ESI` and `EDI`, and has no meaningful
result value.  A source-level implementation needs a small EAX-register thunk
if the native entry itself must be replaced.

## Proven layout

Only the following fields are read by the tick:

```cpp
struct TransitionRootPartial {
    unsigned char unknown_0000[0x5208];
    struct TransitionControl *control; // +0x5208
};

struct TransitionControl {
    unsigned char unknown_0000[0x04];
    IDirectSoundBuffer **buffer_slot; // +0x04; dereferenced to obtain receiver
    unsigned char unknown_0008[0x0c];
    long remaining_ticks;             // +0x14, decremented once per tick
    long duration_ticks;              // +0x18, signed IDIV divisor
    long phase;                       // +0x1c
};
```

`TransitionControl` is only proven through `+0x1f`; its total size and the
ownership/lifetime of `buffer_slot` are not established here.  Neither the
root pointer nor `control` is validated after the initial `control != NULL`
test.  In particular, a nonzero active phase with `duration_ticks == 0` reaches
an x86 `idiv` and faults; a faithful reconstruction must not silently define a
zero-duration policy without separate evidence.

The interface identification follows directly from the called virtual-table
indices and argument layouts:

| Vtable offset | `IDirectSoundBuffer` entry | Native invocation | Tick use |
| --- | --- | --- | --- |
| `+0x3c` (index 15) | `HRESULT STDMETHODCALLTYPE SetVolume(LONG volume)` | push volume; push receiver; call | apply an eased DirectSound volume |
| `+0x48` (index 18) | `HRESULT STDMETHODCALLTYPE Stop()` | push receiver; call | stop only when phase 1 expires |

These are COM `__stdcall` calls with an explicit receiver on the stack.  The
binary ignores both `HRESULT` values.  The common `IDirectSoundBuffer` vtable
prefix independently corroborates the identification: nearby users call
`GetStatus` at `+0x24`, `Lock` at `+0x2c`, `Play` at `+0x30`,
`SetCurrentPosition` at `+0x34`, `SetVolume` at `+0x3c`, `SetPan` at `+0x40`,
`Stop` at `+0x48`, `Unlock` at `+0x4c`, and `Restore` at `+0x50`.  The code
does not establish whether the concrete object is exactly
`IDirectSoundBuffer` or a compatible extended interface.

## Tick behavior

The tick reads `root->control` once for the null check, then reloads it before
each test in this order: phases `1`, `2`, `4`, and `3`.  Since a matching phase
is either left unchanged or changed to zero, at most one phase action is taken
per entry.

For every matching phase it first performs signed 32-bit decrement and writes
the result to `remaining_ticks`.  If the result is not strictly positive, it
writes `phase = 0`.  There is no volume call in that expiration path except
for phase 1, which also calls `(*buffer_slot)->Stop()` after storing zero.

When the decremented count remains positive, the tick computes the following
signed x86 quotient, then invokes `0x0044d4e0` with `EAX = control` and the
quotient-derived stack argument:

| Phase | Stack argument passed to `0x0044d4e0` while active | Expiry behavior |
| ---: | --- | --- |
| `1` | `(remaining_ticks * 5000) / duration_ticks - 5000` | set zero; call `Stop()` |
| `2` | `(remaining_ticks * -5000) / duration_ticks` | set zero |
| `4` | `(remaining_ticks * 1000) / duration_ticks - 1000` | set zero |
| `3` | `(remaining_ticks * -1000) / duration_ticks` | set zero and return immediately |

The multiplication is `imul` and division is `cdq; idiv`: arithmetic is
signed 32-bit with the native overflow/fault behavior, truncating quotient
toward zero.  Phase 3's early return is observably equivalent to the shared
epilogue for the current implementation, but should be retained if preserving
instruction-level control flow matters.

## Volume helper (`0x0044d4e0`)

`0x0044d4e0` is another mixed-ABI entry:

```text
input: EAX = TransitionControl*, [ESP+4] = signed long input_volume
output: no meaningful return value
stack: callee consumes input_volume (`ret 4`)
```

It always dereferences `control->buffer_slot` and calls `SetVolume`; no null
checks protect either pointer.  Its direct global dependency is signed dword
`0x00497854`.

* When `*(long *)0x00497854 == 0`, it calls
  `SetVolume(-10000)` directly.
* Otherwise, let `g = *(long *)0x00497854`, `x = input_volume`, and
  `t = 1.0f - g * 0.01f`.  It computes an x87 floating expression equivalent
  in intent to:

  ```cpp
  long volume = -5000 + msvc_x87_round_to_i32(
      (x + 5000) * (1.0f - t * t));
  ```

  and calls `SetVolume(volume)`.

The constants at `0x00470afc` and `0x00470b00` are IEEE-754 `1.0f` and
`0.01f`.  The conversion helper at `0x00463b2c` takes its operand in x87
`ST(0)`, returns a 64-bit integer in `EDX:EAX`, and the caller uses `EAX`.
It is the compiler's FPU conversion sequence, so a codegen-sensitive rewrite
must preserve the active x87 rounding semantics rather than assume a C cast is
identical for halfway values.

Initializer evidence at `0x0042e5a0` assigns `0x00497854` from the signed byte
at `0x00491d68`; that establishes its signed scalar representation but not a
safe source-level semantic name.  The helper is also called from `0x0043ddf0`
and `0x0044d440`, so it is reusable volume-transition machinery rather than
exclusive to `0x00421e00`.

## Safe C++ reconstruction boundary

The semantic implementation can use the DirectSound declaration from the
platform SDK while keeping the native register entry isolated:

```cpp
enum TransitionPhase {
    TransitionPhase_None = 0,
    TransitionPhase_StopFade = 1,
    TransitionPhase_FadeIn = 2,
    TransitionPhase_FadeOutShort = 3,
    TransitionPhase_FadeInShort = 4
};

struct TransitionControlPartial {
    unsigned char unknown_0000[0x04];
    IDirectSoundBuffer **buffer_slot;
    unsigned char unknown_0008[0x0c];
    long remaining_ticks;
    long duration_ticks;
    TransitionPhase phase;
};

void TickTransitionSemantic(TransitionRootPartial *root);

// Native boundary for 0x00421e00: load EAX with root, then call semantic code.
void TickTransitionEaxAbi(TransitionRootPartial *root);
```

`TickTransitionSemantic` should own the phase ordering, signed arithmetic, and
COM calls.  `TickTransitionEaxAbi` should be compiler-specific glue only.  Do
not model `buffer_slot` as a direct buffer pointer or declare either native
entry as a normal C++ member function: both choices erase directly observed
ABI/layout facts.

## Verification basis

* `resources/th10.exe`, disassembly ranges `0x00421e00-0x00421ee1` and
  `0x0044d4e0-0x0044d549`.
* Cross-checks in `0x0043ddf0`, `0x0044d440`, `0x0044d370`, `0x0044d300`, and
  `0x0044d110` establish the surrounding DirectSoundBuffer-compatible virtual
  table usage.
* Data bytes at `0x00470afc` and `0x00470b00`; initialization path
  `0x0042e5a0` for global `0x00497854`.
