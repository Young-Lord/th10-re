# Registration Draw Owner Evidence

## Scope and naming

This note documents the object made by `0x00413350`, its draw callback
`0x00413690`, its callback body `0x004135d0`, the directly called timing work
at `0x004134b0`, and the matching cleanup entries.  All address, ABI, size,
field-offset, ordering, and return statements below are instruction-level
facts from `resources/th10.exe`.  `RegistrationDrawOwner` is a descriptive
name.  The format string at `0x0046d198` is `%2.1ffps`, so describing its
visible output as an FPS overlay is supported; the original class name and the
precise renderer service at `0x00401690` are not recovered.

## Factory: `0x00413350`

### ABI and allocation contract

```text
input:  no register or stack input used
output: EAX = RegistrationDrawOwner *
stack:  plain ret
```

The factory preserves `EBX`, `EBP`, `ESI`, and `EDI`.  It asks allocator
`0x00452493` for `0x8c` bytes.  For a non-null result it clears all 35 dwords,
sets bit `0x2` in owner `+0x00`, and immediately publishes it at global
`0x00447708`.

It then asks the same allocator for a separate `0x24` byte callback record.
The binary contains null branches after both allocations, but neither produces
a usable nullable-result contract: the common tail dereferences the selected
owner and record unconditionally.  A source reconstruction should therefore
treat allocation failure as outside this executable's operational contract,
rather than silently returning a partial or null object.

### Draw scheduler integration

The record is initialized directly, rather than through `0x00449ed0`:

```text
record +0x00 = 0
record +0x04 = (prior bits with bit 0 clear) | 0x3
record +0x08 = 0x00413690       primary draw callback
record +0x0c = 0                registration hook
record +0x10 = 0                calculation follow-up
record +0x14 = { record, 0, 0 } intrusive link
record +0x20 = owner
```

It invokes `0x00449b70` with its nonstandard ABI:

```text
ESI      = CallbackRecord *record
EDI      = 47                 signed priority
[ESP+4] = *(CallbackScheduler **)0x00491be4
```

`0x00449b70` pops the scheduler argument (`ret 4`), locks the global scheduler
critical section, and links into the draw queue.  Draw dispatch calls the
primary callback only while bit `0x2` is set, with `ECX = record+0x20`.
Priority ordering is signed ascending and equal priorities are newest-first.
Thus this owner is a live, owned, enabled draw record at priority 47.  The
factory finally writes `owner+0x0c = record` and returns the owner in `EAX`.

The main-chain registration hook calls this factory at `0x00420207` and
discards its return value.  It runs before the hook returns and therefore
before the calculation record is linked.  No other direct caller was found in
the executable's direct-call disassembly.

## Owner layout

The exact allocation size and the following member offsets are supported.  No
class hierarchy, vtable, or semantic type for the unlisted bytes is evidenced.

```cpp
struct RegistrationDrawOwner {
    unsigned int flags;                    // +0x00; factory sets bit 0x2
    unsigned char unknown_0004[0x08];
    CallbackRecord *draw_record;           // +0x0c
    unsigned char unknown_0010[0x04];
    double last_tick;                      // +0x14
    unsigned int phase_count;              // +0x1c
    unsigned int frame_accumulator;        // +0x20
    double displayed_fps;                  // +0x24
    double elapsed_window;                 // +0x2c
    float sampled_fps;                     // +0x34
    unsigned char unknown_0038[0x54];
}; // sizeof == 0x8c
```

`0x00417bda` writes IEEE double zero to `+0x24` and `+0x2c` during a later
startup path.  The factory itself zeroes every field.  The names for the
timing values follow their arithmetic and the `%2.1ffps` use, not original
debug information.

## Callback ABI

`0x00413690` is the scheduler-facing thunk:

```text
input:  ECX = RegistrationDrawOwner *
output: EAX = 1
stack:  plain ret
```

It preserves `EDI`, copies `ECX` to `EDI`, and calls `0x004135d0`.  Therefore
`0x004135d0` is not a normal `__thiscall` entry:

```text
input:  EDI = RegistrationDrawOwner *
output: EAX = 1 on every path
stack:  plain ret
```

It preserves the caller's `ESI`, but uses the FPU stack and does not preserve
its x87 state.  C++ should expose only the `ECX` callback to the scheduler;
an ABI thunk can put the pointer in `EDI` for a faithful internal wrapper.

## Timing work: `0x004134b0`

`0x004135d0` first calls `0x004134b0` with `ESI = owner`.  This helper uses
the same nonstandard input, returns `EAX=1` on all paths, and has no stack
arguments.  It calls `0x00439540` for a double-precision current tick.

Its control flow is exactly:

1. If current tick is less than the owner `+0x14` baseline (or unordered),
   replace the baseline with current tick; otherwise retain the old baseline.
2. Compute `delta = current_tick - last_tick`.  If `delta < 0.5`, return.
3. Store `sampled_fps (+0x34) = unsigned_32_to_double(+0x20) / delta`.  The
   signed-to-double sequence adds `2^32` when bit 31 of `+0x20` is set, so the
   divisor is explicitly an unsigned 32-bit interpretation.
4. If `sampled_fps <= 0.0`, clear `phase_count (+0x1c)` and skip the remaining
   sampling actions.
5. Otherwise increment `phase_count`.  When it becomes 2, call `0x00439540`
   and copy its returned double to all of `0x00492528`, `0x00492530`,
   `0x00492538`, and `0x00492540`.  When it becomes 4, first zero both
   `0x00492508` and `0x0049250c`, then make the same four timestamp writes.
6. If global `0x00477810` is non-null, inspect bit mask `0x14` at that
   object's `+0x58`.  When neither bit is set, add `60.0` to owner `+0x2c`.
   Then add either `60.0` to owner `+0x24` when `sampled_fps > 57.0`, or add
   the `sampled_fps` value when it is at most `57.0`.  Regardless of that
   branch, clear bit `0x80` at global-owner `+0x58`.
7. Clear `frame_accumulator (+0x20)` and return 1.

The function has no guard for a zero frame accumulator before the floating
division.  The original FPU behavior, including rounding and exception masks,
must remain a runtime concern for a codegen-faithful implementation.

## Draw callback body: `0x004135d0`

After timing work, the callback has two independent parts.

First, it skips rendering only when `*(int *)0x00491fb8 == 14`.  Otherwise it
loads `RenderOwner *` from `0x004776e0`.  A null render owner suppresses the
render-service calls but does not suppress the final owner counter increment.
For a non-null render owner it uses `sampled_fps` to choose and temporarily
store a color at `render_owner + 0x8974`:

| comparison path | stored 32-bit color |
| --- | ---: |
| `sampled_fps >= 30.0`, or unordered at the first comparison | `0xff5050ff` |
| first comparison says `< 30.0`, second comparison says `>= 40.0` or unordered | `0xffa0a0ff` |
| both ordered comparisons fall through | `0xffffffff` |

For ordinary ordered values the middle path is unreachable: reaching its
comparison already requires `sampled_fps < 30.0`, which cannot also satisfy
`sampled_fps >= 40.0`.  This records the literal control flow without
inventing a three-band color policy.

It then calls `0x00401690` with:

```text
stack argument 1: format string 0x0046d198, "%2.1ffps"
stack argument 2-3: sampled_fps widened to IEEE double
register EBX: address of local 3-float position { 590.0f, 470.0f, 0.0f }
register ESI: *(void **)0x004776e0
```

The exact C++ signature and renderer object type of `0x00401690` are not
established.  It is nonetheless direct evidence that the callback renders a
formatted sampled-FPS value at that three-float position while the temporary
color field is set.  Immediately after the call it reloads `0x004776e0` and
writes `-1` to that object's `+0x8974` field.

Finally, on every control-flow path, it performs:

```cpp
owner->frame_accumulator += 1 + *(unsigned char *)0x00491d66;
return 1;
```

The byte is zero-extended before addition.  The state-14 branch, null-render
owner branch, and normal render branch all join at this update.

## Cleanup and ownership

The main-chain destruction path at `0x004202b3` loads global `0x00447708`
into `EBX` and calls `0x00413450`.

```text
0x00413450 input: EBX = RegistrationDrawOwner *
0x00413450 output: no caller-observed EAX value
0x00413450 stack: plain ret
```

When `EBX` is non-null, the helper reads `owner+0x0c`.  If non-null, it enters
the scheduler's global critical section, calls `0x00449f60` with
`ECX=record, EDX=*(CallbackScheduler **)0x00491be4`, then exits the lock.
`0x00449f60` unlinks the record and, because factory set bit `0x1`, frees the
`0x24` record.  `0x00413450` then clears global `0x00447708` and frees the
owner through `0x004524a1`.  It does not clear owner `+0x0c` before the owner
is released.

Two additional, ABI-distinct cleanup forms exist:

```text
0x00413300: EAX = owner; remove owner+0x0c record, clear 0x00447708, do not
            free owner; plain ret.
0x004133f0: EBX = owner; [ESP+4] bit 0 requests owner deallocation after
            record removal and global clear; returns EBX in EAX; ret 4.
```

Both use the same record removal operation and therefore transfer record
deallocation to the scheduler removal routine.  No cleanup entry invokes the
owner callback directly, and no scheduler path claims ownership of the owner
context.  This split is essential: removing the record only destroys the
record; the corresponding owner must be disposed by one of the owner cleanup
forms.

## Conservative C++ boundary

The semantic implementation should preserve object and record lifetimes while
keeping unresolved ABI details isolated:

```cpp
struct RegistrationDrawOwner {
    unsigned int flags;
    unsigned char unknown_0004[8];
    CallbackRecord *draw_record;
    unsigned char unknown_0010[4];
    double last_tick;
    unsigned int phase_count;
    unsigned int frame_accumulator;
    double displayed_fps;
    double elapsed_window;
    float sampled_fps;
    unsigned char unknown_0038[0x54];
};

RegistrationDrawOwner *CreateRegistrationDrawOwner();
int TH10_FASTCALL RegistrationDrawCallback(RegistrationDrawOwner *owner);
void DestroyRegistrationDrawOwner(RegistrationDrawOwner *owner);
```

`CreateRegistrationDrawOwner()` should allocate/zero `0x8c`, set flag `0x2`,
publish `0x00447708`, create an owning+enabled scheduler record, register it
in the draw chain at priority 47, and retain the record at `+0x0c`.
`RegistrationDrawCallback()` can implement the timing and FPS display in
ordinary C++, provided a narrow thunk adapts the scheduler's `ECX` call into
the `EDI`-based internal boundary where ABI precision is needed.  The draw
service should remain an opaque wrapper accepting a `RenderOwner *`, position,
format, and double value until `0x00401690` is independently typed.

`DestroyRegistrationDrawOwner()` should first deregister the stored
record, then clear the global pointer, then release the owner.  It must not
delete the owner indirectly through scheduler ownership, because the scheduler
owns only the record.  A source-level RAII owner is appropriate only if its
destructor preserves this explicit ordering and is not used after scheduler
teardown has invalidated `0x00491be4`.

The native `EBX`/plain-`ret` entry remains a separate thunk around this
ordinary C++ semantic function.

## Verification sources

- `resources/th10.exe`, `objdump -D -Mintel`, ranges
  `0x00413350-0x0041369a`, `0x00417bda-0x00417bf1`, and
  `0x004201b0-0x004202be`.
- `resources/th10.exe`, `objdump -s`, `0x0046d198` and
  `0x00470ba8-0x00470c68`.
- Scheduler behavior in `docs/evidence/callback-scheduler.md`, specifically
  `0x00449b70`, `0x00449d40`, and `0x00449f60`.

## 2026-09-14 correction and real body

The timing helper 0x004134b0 is now implemented as
`UpdateRegistrationDrawTiming` (src/RegistrationDrawOwner.cpp); see
docs/evidence/registration-draw-timing.md. Re-reading the raw listing
shows two parity-test misreads in the sections above:

- the baseline replacement runs only on the ordered "less" outcome (an
  unordered comparison keeps the old baseline), and
- the callback color comparison at 0x4135f5 routes only *unordered*
  results to the second comparison, so 0xff5050ff covers every ordered
  value, the 0xffa0a0ff band is unreachable, and NaN resolves to
  0xffffffff.
