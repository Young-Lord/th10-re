# Main Chain Registration Hook Dependency Evidence

## Scope

This document records the direct callees of the one-shot MainChain
registration hook at `0x004201b0`, rather than repeating the hook's own
control flow.  The direct calls considered here are `0x00420100`,
`0x004216f0`, `0x00413350`, `0x004462f0`, and `0x00437a00`.  The two thread
entry boundaries are included only to establish the safe `CreateThread`
wrappers needed by a C++ reconstruction.

Addresses, register/stack ABIs, allocation sizes, field offsets, constants,
and calls listed below are instruction-level facts from `resources/th10.exe`.
Names such as `VersionData`, `FontTable`, and `LargeRenderOwner` are
descriptive boundaries, not recovered original class names.

## Dependency Summary

| Entry | ABI at the hook call site | Observable role | C++ status |
| --- | --- | --- | --- |
| `0x00420100` | no explicit caller arguments; plain `ret` | Initializes version-data globals from `th10.dat` | ordinary external helper returning `0` or `-1`; hook must discard result |
| `0x004216f0` | no inputs; plain `ret` | Writes the fixed startup values of a global state aggregate | safe direct C++ global-reset function |
| `0x00413350` | no inputs; `EAX = Owner*`; plain `ret` | Allocates a `0x8c` owner and inserts its draw callback at priority `47` | safe C++ factory once expressed through scheduler API |
| `0x004462f0` | `EAX = LargeRenderOwner*`; plain `ret` | Resets a high-offset render block, mirrors values to globals, invokes D3D vtables | register adapter/thunk until large owner layout is recovered |
| `0x00437a00` | no explicit inputs; plain `ret` | Builds a 256-byte seed table and creates 14 GDI fonts | C++ implementation possible, but should remain a subsystem wrapper until the generated-table and font roles are named |

The hook itself deliberately ignores every callee return value.  In
particular, giving its C++ implementation error propagation would change the
observed behavior.

## `0x00420100`: Version Data Initialization

### ABI and result

`0x00420100` has no caller-provided stack arguments at `0x004201b2`, preserves
only the normal callee-saved `EDI`, and returns with a plain `ret`:

```text
EAX = 0   on completed load
EAX = -1  on either reported failure
```

The hook does not test `EAX`.  Therefore its call is semantically:

```cpp
(void)InitializeVersionData();  // Must run even though failure is ignored.
```

### Behavior and direct dependencies

1. It sets `EAX = 0x0046e000` (`"th10.dat"`) and `ECX = 0x00497990`, then
   calls `0x00434c30`.  That callee has a split-register ABI
   (`EAX` source path, `ECX` destination/global object) and returns success in
   `AL`.
2. On success, it formats the version name using format string
   `0x0046dff0` (`"th10_%.4x%c.ver"`) into a local `0x100`-byte buffer.
3. It passes that buffer to `0x0044b360` together with a null output pointer.
   The returned `EAX` and `EDX` are stored respectively at `0x0049238c` and
   `0x00492388`.
4. On either failure path it calls the reporting helper `0x0044b8e0` with
   `ECX = 0x00474f70` and one error-string argument, then returns `-1`.

This gives two confirmed global outputs but does not establish their types:

```text
0x00492388 = EDX returned by 0x0044b360
0x0049238c = EAX returned by 0x0044b360
```

The safe C++ boundary is an external `int InitializeVersionData();`.  It is
not yet safe to model `0x00497990`, the pair at `0x00492388`, or the resource
reader as concrete C++ structures.

## `0x004216f0`: Fixed Global Reset

### ABI

The entry has no input registers or stack arguments used by the function and
returns with a plain `ret`.  It uses a 12-byte local temporary area only to
stage constants before writing globals.  `EAX`, `ECX`, and `EDX` are clobbered;
no caller observes its return value.

### Exact effects

This is straight-line code from `0x004216f0` through `0x004218c0`.  It makes
no calls and has no branches.  Its visible writes include:

```text
0x491e94 = 0;          0x491e98 = 0;          0x491e9c = 1000.0f;
0x491ea0 = 0;          0x491ea4 = 0;          0x491ea8 = 0;
0x491eac = 0;          0x491eb0 = 1.0f;       0x491eb4 = 0;
0x491ed0 = 0;          0x491ed4 = 0;          0x491ed8 = 0;
0x491edc = 0.5235988f; 0x491f60 = 0;          0x491f64 = 0;
0x491f68 = 640;        0x491f6c = 480;        0x491f70 = 0;
0x491f74 = 1.0f;       0x491f78 = 1;
0x491e48 = 32;         0x491e4c = 16;         0x491e50 = 384;
0x491e54 = 448;        0x491e58 = 0;          0x491e5c = 1.0f;
0x491e60 = 0;          0x491db8 = 0;          0x491dbc = 0;
0x491dc0 = 0.
```

It also initializes the intervening `0x00491d7c..0x00491dc4` fields, including
`0x491dc4 = 0.5235988f`.  Object boundaries in that global region remain
unproven, so a conservative implementation should use a private
`ResetStartupGlobalState()` function with direct named fields only after their
owners are recovered.  This is the most immediately safe dependency to write
in C++.

## `0x00413350`: Draw Owner Factory

### ABI and allocation behavior

The factory takes no arguments, returns the allocated owner in `EAX`, and
uses a plain `ret`.  It first requests `0x8c` bytes from `0x00452493`.
On a non-null allocation it clears all 35 dwords, sets owner `+0x00` bit `1`,
and stores the result to `0x00447708`.

It then allocates a `0x24` callback record directly, initializes it as a draw
record, and invokes scheduler insertion `0x00449b70` with the nonstandard
arguments:

```text
ESI = CallbackRecord *       // 0x24-byte allocation
EDI = 47                     // signed draw priority
[ESP+4] = *(void **)0x491be4 // scheduler
```

The record is filled as follows:

```text
+0x04 |= 0x3                 // owned and enabled
+0x08 = 0x00413690           // primary callback
+0x0c = 0
+0x10 = 0
+0x20 = owner
```

After insertion it writes `owner + 0x0c = record` and returns `owner`.
The direct allocator-null paths are not usable failure contracts: execution
later dereferences the null owner or null record.  A C++ implementation should
not silently turn this into a nullable factory without an explicit behavioral
decision.

### Owner/callback boundary

`0x00413690` is called by the draw scheduler using `ECX = owner`; it moves
that value into `EDI` and calls `0x004135d0`.  The latter reads owner `+0x20`
and `+0x34`, and ultimately increments owner `+0x20`.  Thus only these owner
facts are established by the registration path:

```cpp
struct RegistrationDrawOwner {
    unsigned int flags;       // +0x00, bit 1 set by factory
    unsigned char unknown[8];
    CallbackRecord *record;   // +0x0c
    unsigned char unknown2[0x10];
    unsigned int counter;     // +0x20, read and incremented by callback
    unsigned char unknown3[0x10];
    float field_0034;         // +0x34, read by callback
    unsigned char unknown4[0x54];
}; // size 0x8c
```

The exact callback body also reads unrelated globals and therefore should not
be given a semantic name based solely on this factory.

The safe next C++ implementation is `CreateRegistrationDrawOwner()` using
`CallbackSchedulerApi::Create` and `AddToDrawChain(..., 47)`, with an opaque
`RegistrationDrawOwner` layout.  It must preserve that this extra callback is
registered before the hook returns.

## `0x004462f0`: Large Render-Owner Initialization

### ABI

At `0x00420253` the hook loads `EAX = *(void **)0x00491c10` then calls this
entry.  `0x004462f0` consumes that pointer directly from `EAX`; it has no
stack parameters and is not a normal `__thiscall` entry.  It saves/restores
the incoming `ECX`, preserves `EBX`, `EBP`, `ESI`, and `EDI`, ends in a plain
`ret`, and has no stable useful return value for the hook.

The direct call assumes the pointer is non-null and valid through at least
`+0x3adac7`.  No null check is present.

### Writes and D3D interactions

The function initializes the 21 dwords at owner offsets
`+0x3ada78..+0x3adac4` with zero, `128.0f` (`0x43000000`), `1.0f`, and
`-128.0f` (`0xc3000000`) in the exact positions below:

```text
zero:   80, 84, 88, 94, 9c, a8, ac, bc   (all offsets relative to 0x3ada00)
128.0: 8c, a4, b4, b8
1.0:   98, b0, c0, c4
-128:  78, 7c, 90, a0
```

It copies those values into globals in `0x00497930..0x0049798c`; the mappings
are direct one-for-one loads/stores, for example:

```text
owner+0x3ada78 -> 0x497930
owner+0x3ada7c -> 0x497934
owner+0x3ada80 -> 0x497938
owner+0x3ada8c -> 0x497948
owner+0x3adac4 -> 0x49798c
```

It also reads `*(void **)0x00491c30` and issues vtable calls at byte offsets
`0x68` and `0x190`.  The first consumes the staged arguments present around
`0x00446442`; the second is called with five stack values originating from the
object and zero constants.  The body also calls vtable slots `+0x2c` and
`+0x30` on `*(void **)(owner + 0x3ada74)`, copies 20 dwords from
`owner + 0x3ada78` to an output returned by the `+0x2c` call, then releases
that interface through `+0x30`.

This is sufficient to prove a render-facing reset sequence, but insufficient
to assign the large owner or all COM slots a safe source-level type.  The
right C++ boundary is therefore a narrow adapter:

```cpp
// Requires EAX = owner at the actual binary boundary.
void TH10_FASTCALL InitializeLargeRenderOwnerFromEax(void *owner);
```

The wrapper may call a future typed implementation after the owner tail and
the interface at `0x491c30` have been recovered.  It must not be declared as
an ordinary no-argument helper or a `thiscall` method today.

## `0x00437a00`: Generated Table and Font Initialization

### ABI and top-level behavior

The entry has no explicit inputs at the hook site, preserves `ESI`, and ends
with a plain `ret`.  It returns no result used by the hook.

It first invokes `0x00436af0` with:

```text
EAX = 0x00474cb0
stack arguments = 0x400, 0x40, 0x1a
```

and retries with final argument `0x15` only when `AL == 0`.  `0x436af0` is a
split ABI helper (`EAX` plus three stack arguments, `ret 12`) that initializes
the global object at `0x00474cb0`; the exact object type is still unresolved.

Next, the function advances a 16-bit state at `0x004918b0` 256 times using
the exact recurrence:

```text
x = uint16_t((x ^ 0x9630) - 0x6553)
x = uint16_t((x >> 14) + (x << 2))
0x4918b0 = x
0x474cb0[i] = uint8_t(x >> 9)
```

It increments `0x004918b4` once and stores it back, then makes fourteen calls
through import `0x00466034`, which is `CreateFontA`.  All calls use the same
fixed arguments except their final height:

```text
nHeight = 0x20, 0x22, ..., 0x3c
nWidth = 0
nEscapement = 0
nOrientation = 0
fnWeight = 0x190
fdwItalic = 0
fdwUnderline = 0
fdwStrikeOut = 0
fdwCharSet = 0x80
fdwOutputPrecision = 0
fdwClipPrecision = 0
fdwQuality = 4
fdwPitchAndFamily = 0x11
lpszFace = 0x0046f36c
```

Each call's `EAX` is stored in a descending four-byte global slot from
`0x004918a0` through `0x00491868`.  The selector sequence and later cleanup
at `0x00437d10` (which calls imported `DeleteObject` on all fourteen slots)
prove these are independently owned `HFONT` handles.  The precise display
role of each height remains unresolved.

A safe C++ reconstruction is an `InitializeGeneratedTableAndFonts()` function
that owns the 256-byte table and 14 `HFONT` slots, while keeping the exact
font parameters in a private helper.  It should preserve the unconditional
sequence: returned handles are stored without registration-time checks and
the paired teardown calls `DeleteObject` on the same slots.

## Thread Entry Boundaries

### First thread: `0x0043d080`

The hook directly calls imported `CreateThread` with:

```cpp
CreateThread(0, 0, (LPTHREAD_START_ROUTINE)0x0043d080,
             (void *)0x00492590, 0, &thread_id);
```

The entry at `0x0043d080` does not read its stack parameter and ends with a
plain `ret`, rather than `ret 4`.  It iterates `i = 0..36`, calls `0x0044b360`
for the pointer in `0x00474b40[i]`, and stores each result at
`0x004977c0[i]`.  If any result is zero it reports an error through
`0x0044b810`; otherwise it waits until `0x004977b4` becomes nonzero when its
initial value was zero.  The explicit `CreateThread` result is stored at
`0x004977ac` and never checked by the hook.

For C++, use an `LPTHREAD_START_ROUTINE`-compatible adapter, but do not infer
a useful parameter type from the unused `0x00492590` argument.

### Second thread: `0x0043ba90`

The hook uses helper `0x00453683` to call `CreateThread` with start address
`0x0043ba90`, parameter `0x00491c28`, and thread-ID output `0x00474ddc`.
The helper returns the handle (or zero) to `0x00474dd8`, which the hook also
does not check.

`0x0043ba90` ignores its stack parameter and ends with a plain `ret`.  It
calls `0x0043b8d0` with `EAX = 0x00491c28`, then sets bits `0x200` and
`0x400` of `0x00491ff4` to the non-nullness of `0x00491c38` and `0x00491c3c`
respectively.  It first clears both bits.  This gives a narrow C++ thread
entry boundary independent of the unknown body of `0x0043b8d0`.

## Recommended C++ Reconstruction Order

1. Implement `ResetStartupGlobalState()` from `0x004216f0`, keeping its
   aggregate opaque apart from proven fields.
2. Implement `CreateRegistrationDrawOwner()` through the existing callback
   scheduler C++ boundary, including priority `47` and the `0x8c` owner.
3. Implement the hook's timestamp writes and both `CreateThread` calls using
   Win32 types; retain ignored failure results and its always-zero return.
4. Add typed C++ wrappers for version and input initialization only at their
   current opaque boundaries.
5. Leave `0x004462f0` behind an EAX adapter until the large render-owner tail
   and all COM method types are independently recovered.

This order adds readable C++ behavior without disguising any of the
register-oriented or untyped dependencies as normal source-level methods.

## Verification Sources

- `resources/th10.exe`, `objdump -D -Mintel`, ranges
  `0x00420100-0x0042026a`, `0x004216f0-0x004218c0`,
  `0x00413350-0x00413683`, `0x004462f0-0x004464c8`, and
  `0x00437a00-0x00437d0d`.
- Thread boundary ranges `0x0043d080-0x0043d0eb`,
  `0x0043ba90-0x0043bae3`, and helper `0x00453683-0x0045370d`.
- PE import table: `0x00466034 = CreateFontA`, `0x00466030 = DeleteObject`,
  `0x00466118 = CreateThread`, and `0x00466270 = timeGetTime`.
- Existing scheduler ABI evidence:
  `docs/evidence/callback-scheduler.md`.
