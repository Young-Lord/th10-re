# Main Chain DirectInput Initialization

## Scope

This document records the semantic C++ reconstruction boundary for TH10
`0x0043b8d0`, plus its DirectInput enumeration callbacks at `0x0043baf0` and
`0x0043bb30`.  The facts below come from the original code in
`resources/th10.exe`; no source or assembly implementation is changed here.

Names such as `input_root`, `keyboard`, and `controller` describe proven COM
interface use.  They do not identify the original owning C++ class.

## Entry Boundary and Context Fields

`0x0043b8d0` takes its sole input in `EAX`, copies it to `ESI`, takes no stack
arguments, and returns with plain `ret`:

```cpp
// Binary boundary only.  EDX is neither an input nor a preserved value.
int TH10_FASTCALL InitializeMainChainInputFromEax(MainChainContext *context);
```

`__fastcall` is only a project spelling for the needed EAX boundary.  The
ordinary typed body should be separate, because this is not a normal MSVC
`__thiscall` method.

| Context offset | Proven use | Conservative C++ type |
| --- | --- | --- |
| `+0x0c` | `DirectInput8Create` output; `CreateDevice`; `EnumDevices`; `Release` | `IDirectInput8A *` |
| `+0x10` | system-keyboard device, format/cooperative/acquire calls | `IDirectInputDevice8A *` |
| `+0x14` | first successfully created game-controller device | `IDirectInputDevice8A *` |
| `+0x48` | `GetWindowLongA` and both cooperative-level calls | `HWND` |
| `+0x150` bit `0x8` | setup suppression after obtaining the instance handle | opaque flags |

The global instance passed by the background thread is `0x00491c28`.  The
initialization result is `0` on the normal path and `-1` on every explicit
failure path.  Its caller ignores that result and later derives availability
flags solely from the two device pointers.

## API Call Order

The exact normal sequence is:

1. `GetWindowLongA(context->window_0048, GWL_HINSTANCE)` with `GWL_HINSTANCE
   == -6`.
2. Test `context->input_setup_flags_0150 & 0x8`.  A set bit returns `-1`, but
   does not skip the preceding `GetWindowLongA` call.
3. `DirectInput8Create(instance, 0x800, IID_IDirectInput8A,
   &context->input_root_000c, 0)`.
4. `input_root->CreateDevice(GUID_SysKeyboard,
   &context->input_keyboard_0010, 0)`.
5. `keyboard->SetDataFormat(&c_dfDIKeyboard)`.
6. `keyboard->SetCooperativeLevel(context->window_0048, 0x16)`.
7. `keyboard->Acquire()`.  The original ignores this `HRESULT`.
8. `input_root->EnumDevices(4, CreateFirstEnumeratedController, 0, 1)`.
   Both `HRESULT` and enumeration context are ignored/null.  Raw `4` and `1`
   are compatible with `DI8DEVCLASS_GAMECTRL` and `DIEDFL_ATTACHEDONLY`.
9. If `context->input_controller_0014` is non-null, configure it:
   `SetDataFormat(0x00466704)`, `SetCooperativeLevel(hwnd, 0x0a)`,
   `SetProperty(0x2c, &0x00491c44)`, then
   `EnumObjects(ConfigureControllerObjectRange, 0, 0)`.  All four HRESULTs
   are ignored.  The `0x2c` global is written immediately before `SetProperty`.

The vtable offsets establish the standard DirectInput operations: root
`Release +0x8`, `CreateDevice +0xc`, `EnumDevices +0x10`; device `Release
+0x8`, `SetProperty +0xc`, `EnumObjects +0x10`, `SetDataFormat +0x2c`,
`SetCooperativeLevel +0x34`, and `Acquire +0x1c`.

## Failure Cleanup Is Deliberately Asymmetric

The implementation must not normalize all input fields at entry or use a
single RAII rollback for every failure.  That would change observable
partially initialized state and therefore the background thread's later
pointer-presence flags.

| Failure or exit | Root `+0x0c` | Keyboard `+0x10` | Controller `+0x14` | Return |
| --- | --- | --- | --- | --- |
| setup-suppression flag | unchanged | unchanged | unchanged | `-1` |
| `DirectInput8Create < 0` | explicitly zeroed, no release | unchanged | unchanged | `-1` |
| keyboard `CreateDevice < 0` | release then zero | unchanged | unchanged | `-1` |
| keyboard `SetDataFormat < 0` | release then zero | release then zero | unchanged | `-1` |
| keyboard `SetCooperativeLevel < 0` | release then zero | release then zero | unchanged | `-1` |
| `Acquire < 0` | retained | retained | optional setup still runs | `0` |

Each handled creation/configuration error also reports a fixed string through
the receiver at `0x00474f70`.  The factory and keyboard-create paths use
`0x0046f9e0`; format failure uses `0x0046f9b0`; cooperative-level failure uses
`0x0046f97c`; successful keyboard setup reports `0x0046f954`; successful
controller creation/configuration reports `0x0046f934`.

The cooperative-level error invokes `0x00421d20`, whose ABI relies on the
still-live `ESI = context`; it performs exactly root `Release` then nulling.
It is semantically equivalent to a local `ReleaseAndClearInputRoot(context)`
helper, not a general context cleanup routine.

## Controller Device Callback `0x0043baf0`

The root callback has two stack arguments and returns `ret 8`, matching
`LPDIENUMDEVICESCALLBACKA`:

```cpp
BOOL TH10_STDCALL CreateFirstEnumeratedController(
    const DIDEVICEINSTANCEA *instance,
    void *unused_context);
```

It reads global `0x00491c3c`, rather than its supplied context argument.

```cpp
if (g_MainChainContext.input_controller_0014 != 0)
    return DIENUM_STOP;       // 0

const HRESULT result = g_MainChainContext.input_root_000c->CreateDevice(
    instance->guidInstance,
    &g_MainChainContext.input_controller_0014,
    0);
return result < 0 ? DIENUM_CONTINUE : DIENUM_STOP;
```

`instance + 4` is passed as the device GUID, proving it is
`DIDEVICEINSTANCEA::guidInstance`.  The callback continues after a failed
device creation and stops after the first success.  It does not release or
clear an existing controller pointer.

## Controller Object Callback `0x0043bb30`

`0x0043bb30` is likewise a two-argument `__stdcall` callback compatible with
`LPDIENUMDEVICEOBJECTSCALLBACKA`:

```cpp
BOOL TH10_STDCALL ConfigureControllerObjectRange(
    const DIDEVICEOBJECTINSTANCEA *object,
    void *unused_context);
```

It reads `object->dwType` at `+0x18`.  Objects for which `(dwType & 3) == 0`
are skipped and enumeration continues.  For every other object it makes the
following `DIPROPRANGE`-shaped local block:

```cpp
DIPROPRANGE range;
range.diph.dwSize       = 0x18;
range.diph.dwHeaderSize = 0x10;
range.diph.dwObj        = object->dwType;
range.diph.dwHow        = 2;       // DIPH_BYID
range.lMin              = -1000;
range.lMax              = 1000;

const HRESULT result = g_MainChainContext.input_controller_0014->SetProperty(
    4, &range);                    // raw property identifier 4
return result < 0 ? DIENUM_STOP : DIENUM_CONTINUE;
```

The standard names `DIPROP_RANGE` and `DIPROPRANGE` fit both the structure
and the operation, but the raw identifier and fields above are the direct
evidence.  In particular, one failed property assignment stops all further
object enumeration; it is not ignored like the controller-level calls.

## C++ Implementation Boundary

The semantic implementation should keep normal DirectInput interfaces and
typed callbacks inside the C++ body, while retaining a narrow EAX adapter for
the original call site:

```cpp
int InitializeMainChainInputSemantic(MainChainContext &context)
{
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(
        GetWindowLongA(context.window_0048, GWL_HINSTANCE));
    if ((context.input_setup_flags_0150 & 0x8U) != 0)
        return -1;

    HRESULT hr = DirectInput8Create(instance, 0x800, IID_IDirectInput8A,
        reinterpret_cast<void **>(&context.input_root_000c), 0);
    if (hr < 0) {
        context.input_root_000c = 0;
        ReportInputMessage(kCreateMessage);
        return -1;
    }

    // Continue with the ordered operations and the branch-specific releases
    // above.  Do not make Acquire/controller configuration new failure exits.
    return 0;
}

int TH10_FASTCALL InitializeMainChainInputFromEax(MainChainContext *context)
{
    return InitializeMainChainInputSemantic(*context);
}
```

The sample deliberately leaves `ReportInputMessage`, the fixed controller
data-format descriptor, and the `0x2c` property value as named external
boundaries.  Their surrounding owners and original source types remain
unrecovered.  `ComPtr`/RAII may be used only if its `detach`/reset placement
reproduces every row of the cleanup matrix; conventional scope rollback is
not behavior-preserving here.

## Verification Evidence

The control flow and ABI statements above were checked against these original
ranges:

```text
0x0043b8d0-0x0043ba86  initializer
0x0043baf0-0x0043bb21  controller-device callback
0x0043bb30-0x0043bb8f  controller-object callback
0x00421d20-0x00421d34  root release-and-clear helper
```
