# Main Chain Background Thread Evidence

## Scope

This document records the second thread created by the MainChain registration
hook: its CRT entry at `0x0043ba90`, the DirectInput initialization body at
`0x0043b8d0`, and the resulting updates to `0x00491ff4`.  It also includes the
two DirectInput enumeration callbacks at `0x0043baf0` and `0x0043bb30`, because
they are direct parts of the initialization body required for a C++ recovery.

Names are descriptive.  The documented facts do not identify the original
source-level class or attach a semantic name to `MainChainContext+0x150`.

## Thread Creation and ABI

The registration hook calls CRT `__beginthreadex` at `0x00453683`, not
`CreateThread` directly, with these logical arguments:

```cpp
__beginthreadex(
    0,                         // security
    0,                         // stack size
    MainChainBackgroundThread, // 0x0043ba90
    (void *)0x00491c28,        // argument
    0,                         // creation flags
    (unsigned *)0x00474ddc);   // thread ID destination
```

The returned handle is stored at `0x00474dd8`.  Neither the hook nor the
thread checks a failure result from `__beginthreadex`.

`0x0043ba90` itself never reads `[ESP+4]`, so the supplied MainChain context
argument is unused.  The function ends in a plain `ret`, rather than `ret 4`.
That is correct for its actual boundary: MSVC's `__beginthreadex` wrapper at
`0x004535ee` loads the start function from its `_ptiddata`, pushes the stored
argument, calls it, and immediately pushes `EAX` for CRT thread termination.
There is no caller-side argument adjustment between those two operations, so
the start routine uses the C calling convention.

The final `EAX` from `0x0043ba90` is the newly stored value of
`0x00491ff4`, not a forced zero.  The suitable source-level boundary is:

```cpp
// TH10 0x0043ba90.  Argument is deliberately unused.
unsigned int __cdecl MainChainBackgroundThread(void *unused);
```

Using `LPTHREAD_START_ROUTINE`/`__stdcall` directly for this entry would
misdescribe the original cleanup convention.  A separate `__stdcall` adapter
is only appropriate if a later C++ implementation chooses to call Win32
`CreateThread` instead of preserving the CRT `__beginthreadex` boundary.

## Thread Entry `0x0043ba90`

The entry is straight-line code from `0x0043ba90` through `0x0043bae2`:

```cpp
unsigned int __cdecl MainChainBackgroundThread(void *)
{
    g_MainChainRuntimeFlags &= ~0x600u;

    // Actual ABI of the callee is EAX = &g_MainChainContext.
    (void)InitializeMainChainInputFromEax(&g_MainChainContext);

    const unsigned int keyboard_bit =
        g_MainChainContext.input_keyboard_0010 != 0 ? 0x200u : 0;
    g_MainChainRuntimeFlags =
        (g_MainChainRuntimeFlags & ~0x200u) | keyboard_bit;

    const unsigned int controller_bit =
        g_MainChainContext.input_controller_0014 != 0 ? 0x400u : 0;
    g_MainChainRuntimeFlags =
        (g_MainChainRuntimeFlags & ~0x400u) | controller_bit;
    return g_MainChainRuntimeFlags;
}
```

The compiler's XOR/AND/XOR sequence is exactly the two clear-and-replace
operations above; it does not merely OR capability bits into the old value.
All bits other than `0x200` and `0x400` survive the entry unchanged.  The
initial `AND 0xfffff9ff` clears both target bits before the input body runs.

The input body's `EAX` return (`0` or `-1`) is ignored.  Therefore its errors
do not directly select the two bits: each bit reflects pointer non-nullness
after the body, including any pre-existing or partially cleaned state left by
its individual error path.

`0x0044a5f0` later tests flag `0x200` to choose the keyboard-interface path.
This establishes that bit as a keyboard-input availability/path selector.  No
currently recovered caller assigns a stronger role to `0x400`; the directly
supported description is an optional-controller pointer-presence bit.

## Main Context Input Fields

The `EAX` input of `0x0043b8d0` is always the global MainChain context at
`0x00491c28`.  The following offsets are direct facts:

| Context offset | Global address | Exact use | Conservative type |
| --- | --- | --- | --- |
| `+0x0c` | `0x00491c34` | DirectInput8 creation output; released on several failure paths; used to enumerate controller devices | `IDirectInput8A *` |
| `+0x10` | `0x00491c38` | Keyboard `CreateDevice` output; configured, acquired, and sampled later | `IDirectInputDevice8A *` |
| `+0x14` | `0x00491c3c` | First enumerated controller creation output; optionally configured | `IDirectInputDevice8A *` |
| `+0x48` | `0x00491c70` | Passed to `GetWindowLongA` and both `SetCooperativeLevel` calls | `HWND` |
| `+0x150` | `0x00491d78` | Bit `0x8` suppresses DirectInput setup after `GetWindowLongA` | opaque flags |

The `+0x0c`, `+0x10`, and `+0x14` labels are interface-compatible types proven
by their GUIDs, vtable slots, and later uses.  They do not prove that the
whole surrounding `MainChainContext` is an input-manager class.

## Input Initialization `0x0043b8d0`

### ABI and result

`0x0043b8d0` receives its only input in `EAX`, moves it to `ESI`, has no stack
arguments, and ends with a plain `ret`.  It preserves incoming `ESI`; `EBX`
and `EDI` are preserved on the paths that use them.  It returns `0` after the
normal path and `-1` for each explicit early error.

This is not a normal C++ instance-method ABI.  Preserve an EAX adapter at the
binary-facing edge and put the typed body behind it:

```cpp
// Binary-facing adapter: EAX = context, no stack arguments.
int TH10_FASTCALL InitializeMainChainInputFromEax(MainChainContext *context);
```

`__fastcall` is only a convenient spelling for the EAX input in this project;
the original entry does not consume `EDX`.

### Exact control flow

1. Call `GetWindowLongA(context->window_0048, GWL_HINSTANCE)` (`-6`) and keep
   the returned `HINSTANCE` in `EAX`.
2. Test `context+0x150` bit `0x8`.  If set, return `-1`.  The preceding window
   API call still occurs on this path.
3. Call `DirectInput8Create(hinstance, 0x800, IID_IDirectInput8A,
   &context->input_root_000c, 0)`.  The IID is at `0x00467aac`.
4. On failure, write zero to `context+0x0c`, report the string at
   `0x0046f9e0`, and return `-1`.
5. Call `input_root->CreateDevice(GUID_SysKeyboard, &context+0x10, 0)`.
   `GUID_SysKeyboard` is the value at `0x0046793c`.
6. On failure, release `context+0x0c` through vtable `+0x8`, clear it, report
   `0x0046f9e0`, and return `-1`.  The code does not clear `context+0x10` on
   this particular failure path.
7. Call `keyboard->SetDataFormat(c_dfDIKeyboard)` through vtable `+0x2c` with
   the descriptor at `0x004664fc`.
8. On failure, release/clear keyboard at `+0x10`, release/clear input root at
   `+0x0c`, report `0x0046f9b0`, and return `-1`.
9. Call `keyboard->SetCooperativeLevel(context->window_0048, 0x16)` through
   vtable `+0x34`.  `0x16` is passed as a raw fixed constant.
10. On failure, release/clear keyboard, call `0x00421d20` to release/clear the
    input root at `+0x0c`, report `0x0046f97c`, and return `-1`.
11. Call `keyboard->Acquire()` through vtable `+0x1c`, ignoring its HRESULT.
    Report the success message at `0x0046f954`.
12. Call `input_root->EnumDevices(4, 0x0043baf0, 0, 1)` through vtable `+0x10`.
    Its HRESULT is ignored.
13. If `context+0x14` is non-null after enumeration, configure that device as
    described in the optional-controller section below and report
    `0x0046f934`.
14. Return zero.

The raw Japanese strings include the ASCII prefixes `"DirectInput"`,
`"DirectInput SetDataFormat"`, and `"DirectInput SetCooperativeLevel"` at the
failure addresses above.  The report receiver is `ECX = 0x00474f70` in every
listed reporting call.

### Cleanup Matrix

| Failure point | `+0x0c` input root | `+0x10` keyboard | `+0x14` controller | Result |
| --- | --- | --- | --- | --- |
| flag `+0x150 & 8` | untouched | untouched | untouched | `-1` |
| `DirectInput8Create` | explicitly zeroed | untouched | untouched | `-1` |
| `CreateDevice(keyboard)` | released then zeroed | untouched | untouched | `-1` |
| `SetDataFormat` | released then zeroed | released then zeroed | untouched | `-1` |
| `SetCooperativeLevel` | released then zeroed by `0x00421d20` | released then zeroed | untouched | `-1` |
| `Acquire` | retained | retained | handled below | `0` |

This matrix intentionally preserves the binary's asymmetric partial-state
behavior.  A semantic C++ implementation should not proactively clear all
three fields at function entry or add cleanup for `Acquire` failure without a
separate behavior-change decision.

## Optional Controller Enumeration

`EnumDevices` is invoked after keyboard acquisition with raw device class `4`
and flags `1`.  The callback at `0x0043baf0` is a two-stack-argument
`__stdcall` function:

```cpp
// TH10 0x0043baf0
int __stdcall CreateFirstEnumeratedController(
    const DIDEVICEINSTANCEA *instance, void *unused_context);
```

It first tests `context+0x14` (`0x00491c3c`).  If already non-null, it returns
zero, which stops further enumeration.  Otherwise it passes
`&instance->guidInstance` (implemented as `instance + 4`) to
`context+0x0c->CreateDevice`, with the output at `context+0x14` and null outer
unknown.  If that HRESULT is negative it returns one to continue enumeration;
otherwise it returns zero to stop after the first successful controller.

If an optional controller exists, `0x0043b8d0` makes these calls, ignoring all
their HRESULT values:

1. `controller->SetDataFormat(0x00466704)` through vtable `+0x2c`.
2. `controller->SetCooperativeLevel(context->window_0048, 0x0a)` through
   vtable `+0x34`.
3. Write `0x2c` to global `0x00491c44`, then call controller vtable `+0x0c`
   with `&0x00491c44`.
4. Call controller vtable `+0x10` with callback `0x0043bb30`, null context,
   and zero flags.

The last step is consistent with device-object enumeration.  Callback
`0x0043bb30` is also `__stdcall` with two stack arguments.  It reads dword
`instance+0x18`; if `(value & 3) == 0` it returns one without a device call.
Otherwise it constructs a 24-byte local property-range block and calls the
controller's vtable `+0x18` with raw property identifier `4` and that block.
The block has these exact dwords:

```text
+0x00 = 0x18     // structure size
+0x04 = 0x10     // header size
+0x08 = 4        // object selector
+0x0c = 2        // selector mode
+0x10 = -1000    // lower bound
+0x14 = 1000     // upper bound
```

It returns zero only when that property call returns a negative HRESULT;
otherwise it returns one.  The standard DirectInput names `DIPROP_RANGE` and
`DIPROPRANGE` are strongly indicated by the call shape and values, but the
above raw ABI is the required reconstruction constraint.

## C++ Reconstruction Boundary

The following split preserves the observed logic without pretending the raw
register entry is an ordinary method:

```cpp
struct MainChainInputFields {
    void *input_root_000c;
    void *input_keyboard_0010;
    void *input_controller_0014;
};

int InitializeMainChainInputSemantic(MainChainContext &context);

// Tiny binary-edge adapter only: load EAX, invoke raw entry or semantic body.
int TH10_FASTCALL InitializeMainChainInputFromEax(MainChainContext *context);

unsigned int __cdecl MainChainBackgroundThread(void *unused)
{
    g_MainChainRuntimeFlags &= ~0x600u;
    (void)InitializeMainChainInputFromEax(&g_MainChainContext);
    g_MainChainRuntimeFlags = (g_MainChainRuntimeFlags & ~0x200u) |
        (g_MainChainContext.input_keyboard_0010 != 0 ? 0x200u : 0u);
    g_MainChainRuntimeFlags = (g_MainChainRuntimeFlags & ~0x400u) |
        (g_MainChainContext.input_controller_0014 != 0 ? 0x400u : 0u);
    return g_MainChainRuntimeFlags;
}
```

The thread must deliberately use the global MainChain context rather than its
formal argument.  It must also ignore the input initializer's return code and
derive the two global bits from final pointer values.

## Verification Sources

- `resources/th10.exe`, `objdump -D -Mintel`, ranges
  `0x0043b8d0-0x0043bae3`, `0x0043baf0-0x0043bb24`, and
  `0x0043bb30-0x0043bb92`.
- CRT `__beginthreadex` implementation at `0x00453683-0x0045370d` and its
  start wrapper at `0x004535ee-0x00453676`.
- Direct users of `0x00491c38` in `0x0044a5f0`, including vtable `+0x24` for
  a 256-byte keyboard-state read and `+0x1c` recovery/acquisition behavior.
- MainChain registration hook at `0x004201b0-0x00420269` and existing
  `docs/evidence/main-chain-registration-hook.md`.
