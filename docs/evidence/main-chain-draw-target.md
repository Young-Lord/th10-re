# Main Chain Draw Target Evidence

## Result

`MainChainContext + 0x08` is an `IDirect3DDevice9 *`-compatible COM interface
pointer.  In `MainChainDrawInitialize` (`0x00420000`) its vtable slot `+0xbc`
(index 47, zero based) is `IDirect3DDevice9::SetViewport`, with a
`D3DVIEWPORT9 *` pointing at `context->draw_work_pointer + 0xcc`.

This identifies the interface and the call precisely.  It does **not** prove
that this field aliases the separately stored `IDirect3DDevice9 *` at
`0x00491c30`, although both pointers have the same recovered interface type.

## Exact `0x00420000` call

At `0x00420066-0x0042007e`, after `0x004215a0` returns:

```text
EDX = [ESI + 0x384] + 0xcc
EAX = [ESI + 0x008]
ECX = [EAX]                    ; IDirect3DDevice9 vtable
push EDX                        ; D3DVIEWPORT9 const *
push EAX                        ; COM receiver
call dword ptr [ECX + 0xbc]
```

`ESI` is the scheduler-supplied `MainChainContext *`.  The call has no
conditional guard: a null field or invalid vtable would fault.  Its `EAX`
`HRESULT` result is immediately ignored.  The caller subsequently writes `1`
to `context + 0x388`; that write is unconditional and therefore is not a
success check for `SetViewport`.

The effective ABI is the x86 COM/stdcall form below.  The source declaration
uses the ordinary C++ member spelling, but the observed instruction sequence
pushes `this` explicitly because the binary calls the raw vtable entry.

```cpp
typedef long HRESULT;

struct D3DVIEWPORT9 {
    unsigned long X;      // +0x00
    unsigned long Y;      // +0x04
    unsigned long Width;  // +0x08
    unsigned long Height; // +0x0c
    float MinZ;           // +0x10
    float MaxZ;           // +0x14
};

struct IDirect3DDevice9;

// vtable byte offset 0xbc, index 47:
// HRESULT __stdcall slot47(IDirect3DDevice9 *self,
//                          const D3DVIEWPORT9 *viewport);
// In the Direct3D 9 declaration this is:
// HRESULT STDMETHODCALLTYPE IDirect3DDevice9::SetViewport(
//     const D3DVIEWPORT9 *viewport);
```

The slot position follows the complete `IDirect3DDevice9` ordering including
the three leading `IUnknown` methods.  It is consistent with the independently
identified `Clear` slot `+0xac` (index 43) used later in this same callback.

## Receiver and argument layout

Only this much receiver layout is required and supported:

```cpp
struct MainChainDrawTargetRef {
    IDirect3DDevice9 *device; // MainChainContext + 0x08
};
```

The device object itself is external COM state.  The only recovered field at
its address is the mandatory interface vtable pointer at device `+0x00`; no
in-process implementation fields should be invented for it.

`MainChainContext + 0x384` is a pointer to the current `0x118`-byte draw work
record.  The specific call proves a `D3DVIEWPORT9`-compatible 24-byte region
at work `+0xcc..+0xe3`.  The callback stores `context + 0x26c` into `+0x384`
before calling `0x004215a0`, so its normal path passes
`context + 0x338` to `SetViewport`.

The declared standard viewport member meanings above come from the D3D9 ABI.
This analysis does not independently show which function writes each member
of the work-local viewport, nor whether all six values are refreshed on every
frame.

## Cross-references and corroborating calls

The same field-relative pointer is used as the device receiver by the
following direct call sites in the main-chain code region:

| Site(s) | Vtable slot | Recovered call | Direct effect |
| --- | ---: | --- | --- |
| `0x00420079` | `+0xbc` | `SetViewport(work + 0xcc)` | applies the current draw-work viewport |
| `0x00420cb9`, `0x00420cfa` | `+0xe4` | `SetRenderState(0x1c, 1/0)` | toggles D3D render-state `0x1c` while tracking `context + 0x750` |
| `0x00420d39`, `0x00420d7a` | `+0xe4` | `SetRenderState(0x0e, 1/0)` | toggles D3D render-state `0x0e` while tracking `context + 0x754` |
| `0x00420daa` | `+0xe4` | tail call with caller-provided state/value | narrow forwarding wrapper through `context + 0x08` |

The `+0xe4` calls are decisive type corroboration: index 57 of
`IDirect3DDevice9` is `SetRenderState(D3DRENDERSTATETYPE, DWORD)`, and the
two explicit arguments are pushed before the receiver.  `0x00420d90` also
loads its context from `[ESP+8]`, loads `[context+8]`, replaces the wrapper's
stack receiver with that pointer, and tail-jumps through that same slot.

Several other rendering paths independently call `+0xbc` through the global
`0x00491c30`, including `0x00401855`, `0x00401d4e`, `0x00403a62`, and
`0x00439419`.  Those paths reinforce the slot identity but are not proof that
`MainChainContext + 0x08 == [0x00491c30]`.

There is no statically resolved in-image callee for the `+0xbc` call.  The
vtable is supplied by the runtime-created D3D9 device, so the final function
address belongs to the installed Direct3D implementation rather than a fixed
TH10 `.text` address.  The executable imports `d3d9.dll!Direct3DCreate9`; its
startup code creates the global device through D3D9 API calls, but the present
evidence does not expose an assignment from that global storage into
`MainChainContext + 0x08`.

## Conservative C++ boundary for `DrawInitialize`

The following is sufficient to reconstruct this direct effect in ordinary C++
without pretending the surrounding internal helpers have normal member ABIs:

```cpp
struct MainChainDrawWork {
    unsigned char unknown_0000[0xcc];
    D3DVIEWPORT9 viewport; // +0xcc
};

struct MainChainContextDrawPartial {
    unsigned char unknown_0000[0x08];
    IDirect3DDevice9 *draw_target; // +0x08
    unsigned char unknown_000c[0x378];
    MainChainDrawWork *draw_work_pointer; // +0x384
    long draw_initialized;                // +0x388
};

// The original 0x004215a0 takes its work pointer in EDI.  A C++ rewrite can
// model its completed semantic effect before this call, or place it behind a
// dedicated ABI thunk.
static long DrawInitializeViewport(MainChainContextDrawPartial *context) {
    context->draw_target->SetViewport(&context->draw_work_pointer->viewport);
    context->draw_initialized = 1;
    return 1;
}
```

`SetViewport` returns `HRESULT`, while `MainChainDrawInitialize` returns `1`
regardless of that result.  A faithful C++ reconstruction must retain this
ignored-result behavior and must not add a null check or error path absent from
the original.

## Uncertainty and non-claims

- `draw_target` is an evidence name for a device interface pointer; it is not
  proof of ownership, reference-count responsibility, initialization site, or
  a distinct device from `g_D3D9Device`.
- No field layout is asserted for the concrete object behind the COM pointer.
  Only its vtable pointer and documented interface slots are relevant here.
- `draw_work_pointer` is proven at `MainChainContext + 0x384`, but the total
  `MainChainContext` and draw-work semantic layouts remain partial.
- The `D3DVIEWPORT9` type proves the required 24-byte argument representation;
  it does not prove the source-level name of the larger draw-work object.
- This document identifies the virtual call boundary only.  It neither claims
  object-code matching nor types the nonstandard-ABI camera helper at
  `0x004215a0`.

## Verification sources

- `resources/th10.exe`, `objdump -D -Mintel` ranges
  `0x00420000-0x004200b1`, `0x00420c90-0x00420db0`, and
  `0x00421480-0x004216ea`.
- `docs/evidence/main-chain-render-dependencies.md` for the separate
  `0x00491c30` D3D9 device evidence and the `+0x384` camera-work path.
- Local Windows SDK-compatible declaration:
  `/usr/include/wine/windows/d3d9.h` (`IDirect3DDevice9` vtable order,
  `D3DVIEWPORT9`, `SetViewport`, and `SetRenderState`).
