# Main Chain Render-Side Dependency Evidence

## Scope and confidence

This document covers the render-side direct dependencies reached by
`MainChainDrawInitialize` (`0x00420000`) and `MainChainDrawFinalize`
(`0x004200d0`):

| Address/global | Conservative evidence name | Confidence |
| --- | --- | --- |
| `0x004215a0` | `UpdateMainChainCameraWork` | exact machine-level ABI, field offsets, and control flow |
| `0x00442f50` | `FlushRenderOwnerPendingVertices` | exact machine-level ABI, field offsets, and control flow |
| `0x00491c10` | `g_RenderOwner` | pointer global and accessed offsets only; concrete type unknown |
| `0x00491c30` | `g_D3D9Device` | `IDirect3DDevice9 *` is directly supported by imports and vtable slots |

The executable imports `Direct3DCreate9` from `d3d9.dll` and D3DX matrix
helpers from `d3dx9_31.dll`.  The slot numbers and argument counts below
match `IDirect3DDevice9`, including its three `IUnknown` slots.  This makes
the `0x00491c30` boundary materially stronger than a generic "render vtable"
inference.  It does **not** identify the owner or lifetime manager of that
device pointer.

All addresses and offsets are for `resources/th10.exe`.

## Direct dependency graph

```text
MainChainDrawInitialize (ECX = MainChainContext*)
  EDI = context + 0x26c
  -> UpdateMainChainCameraWork (EDI = CameraWork*)
       ESI = [g_RenderOwner]
       -> FlushRenderOwnerPendingVertices (ESI = RenderOwner*) if non-null
       -> D3DXMatrixLookAtLH / D3DXMatrixPerspectiveFovLH / D3DXVec3Normalize
       -> g_D3D9Device->SetTransform(2, CameraWork + 0x4c)
       -> g_D3D9Device->SetTransform(3, CameraWork + 0x8c)
  -> g_D3D9Device->Clear(0, null, 1, [0x4923a8], 1.0f, 0)

MainChainDrawFinalize (ECX supplied but unused)
  ESI = [g_RenderOwner]
  -> FlushRenderOwnerPendingVertices (ESI = RenderOwner*)
       -> g_D3D9Device state calls and DrawPrimitiveUP
```

`0x004215a0` and `0x00442f50` are internal register-ABI helpers, not normal
MSVC instance methods.  In particular, neither receives an implicit object in
`ECX` at its entry.  Calling either through an ordinary C++ prototype would
silently lose the required input register.

## `0x004215a0`: camera work update

### Exact entry ABI

| Item | Evidence |
| --- | --- |
| Required input | `EDI = CameraWork*` |
| Stack arguments | none read by this function |
| `ECX` / `EAX` input | not consumed as an input at entry |
| Callee-saved behavior | saves/restores `EBX`, `EBP`, and incoming `ESI`; it leaves the input value in `EDI` unchanged but does not save it itself |
| Return convention | `ret`; callers in this path ignore `EAX` |

`MainChainDrawInitialize` establishes this ABI at `0x0042000e-0x00420061`:
it computes `EDI = ECX + 0x26c`, stores that pointer in
`MainChainContext + 0x384`, and calls `0x004215a0`.  Thus only the location of
the work object is proven; `CameraWork` is a local reconstruction name.

At entry the helper loads `ESI = [0x00491c10]`.  If non-null, it calls
`0x00442f50` without changing `ESI`; that call therefore receives the global
owner pointer in `ESI`.

### Observed work-object layout

The fields below are direct reads/writes.  D3DX argument positions establish
the matrix and vector roles, but do not establish original field names.

| Offset | Access | Directly established use |
| ---: | --- | --- |
| `+0x00`, `+0x04`, `+0x08` | read `float` | added componentwise to vectors from `+0x0c` and `+0x3c` |
| `+0x0c`, `+0x10`, `+0x14` | read `float` | one translated `D3DXMatrixLookAtLH` vector argument |
| `+0x18` | read as pointer to 3 floats | fourth `D3DXMatrixLookAtLH` argument |
| `+0x30`, `+0x34`, `+0x38` | write `float` | cross product of the vectors at `+0x18` and `+0x0c`, then normalized in place |
| `+0x3c`, `+0x40`, `+0x44` | read `float` | other translated `D3DXMatrixLookAtLH` vector argument |
| `+0x48` | read `float` | `D3DXMatrixPerspectiveFovLH` field-of-view argument |
| `+0x4c` | write 64 bytes | `D3DXMatrixLookAtLH` destination; then passed to `SetTransform(2, ...)` |
| `+0x8c` | write 64 bytes | `D3DXMatrixPerspectiveFovLH` destination; then passed to `SetTransform(3, ...)` |
| `+0xd4`, `+0xd8` | read signed `i32` | converted to float with signed-negative correction, then divided as `float(+0xd4) / float(+0xd8)` |
| `+0xe8`, `+0xec` | read `u32` | copied to `g_RenderOwner + 0x5c` and `+0x60` when that global is non-null |

The `D3DXMatrixPerspectiveFovLH` constants are exactly `zNear = 20.0f` and
`zFar = 1800.0f`.  Calling its `+0xd4 / +0xd8` ratio an aspect ratio is a
strong, but still semantic, inference.  The instruction-level fact is only
the signed conversion and division.

### Device calls from this helper

The three calls are direct `IDirect3DDevice9` methods.  Each call pushes the
receiver as the final stack argument, so the COM receiver is stack-based
`this` (`__stdcall`), not `ECX`.

| Site | Vtable offset / index | Exact stack-level call | Identified D3D9 method |
| --- | ---: | --- | --- |
| `0x0042165d` | `+0xb0` / 44 | `(device, 2, CameraWork + 0x4c)` | `SetTransform(D3DTS_VIEW, matrix)` |
| `0x0042166e` | `+0xb0` / 44 | `(device, 3, CameraWork + 0x8c)` | `SetTransform(D3DTS_PROJECTION, matrix)` |
| `0x004200a2` | `+0xac` / 43 | `(device, 0, 0, 1, [0x004923a8], 1.0f, 0)` | `Clear(0, NULL, D3DCLEAR_TARGET, color, 1.0f, 0)` |

The `Clear` call belongs to `MainChainDrawInitialize`, immediately after the
camera helper returns.  `0x004923a8` is read as the color argument; this
function alone does not prove its owner or a more descriptive name.

## `0x00442f50`: pending vertex flush

### Exact entry ABI and result

| Item | Evidence |
| --- | --- |
| Required input | `ESI = RenderOwner*` |
| Stack arguments | none read |
| `ECX` / `EAX` input | not consumed as an input at entry |
| Register preservation | does not write `ESI`, `EBX`, `EBP`, or `EDI`; clobbers `EAX`, `ECX`, and `EDX` |
| Return convention | `ret`; `EAX` is incidental and ignored at all scoped callers |

At `0x00442f50`, the first instruction reads
`[ESI + 0x3adac8]`; a zero value returns immediately.  This is conclusive
evidence for the `ESI` dependency.  The helper is called with this register
prepared by both `0x004215a0` and `MainChainDrawFinalize`; neither call passes
a C/C++ stack argument.

When the count is nonzero, the helper performs this exact transaction:

1. `g_D3D9Device->SetTextureStageState(0, 6, 0)`.
2. `g_D3D9Device->SetTextureStageState(0, 3, 0)`.
3. `g_D3D9Device->SetFVF(0x144)`.
4. `g_D3D9Device->DrawPrimitiveUP(4, count << 1,
   *(void **)(owner + 0x72dad0), 0x1c)`.
5. Copy `[owner + 0x72dacc]` to `[owner + 0x72dad0]`, clear the count, and
   increment `[owner + 0x58]`.

The D3D9 enum names are exact for these slot positions: `6` is
`D3DTSS_ALPHAARG2`, `3` is `D3DTSS_COLORARG2`, and primitive type `4` is
`D3DPT_TRIANGLESTRIP`.  `0x144` is consistent with
`D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1`; no code in this narrow path
validates the layout of the 28-byte vertex records, so the FVF expansion is
reported as a standard-header interpretation rather than a recovered TH10
vertex struct.

| Vtable offset / index | Exact method boundary |
| ---: | --- |
| `+0x10c` / 67 | `HRESULT __stdcall SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value)` |
| `+0x14c` / 83 | `HRESULT __stdcall DrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT primitive_count, const void *vertices, UINT stride)` |
| `+0x164` / 89 | `HRESULT __stdcall SetFVF(DWORD fvf)` |

### Observed `g_RenderOwner` layout

`0x00491c10` points at a very large mutable object.  No class identity,
allocation site, or object size is established here.  The following narrow
prefix/late-field view is sufficient for this dependency only:

| Offset | Access | Conservative meaning |
| ---: | --- | --- |
| `+0x58` | increment after draw | submission/flush counter of unknown semantics |
| `+0x5c`, `+0x60` | written by camera helpers | two raw values sourced from `CameraWork + 0xe8/+0xec` |
| `+0x3adac8` | nonzero test, clear | pending-record count; used as `primitive_count = count << 1` |
| `+0x72dacc` | read | source/current vertex-buffer pointer |
| `+0x72dad0` | read as draw pointer, then overwritten | pending vertex-buffer pointer |

The adjacent label-sized helper at `0x00442f30` initializes the relevant late
fields as follows: it writes zero to `+0x3adac8`, then stores
`owner + 0x3adacc` in both `+0x72dacc` and `+0x72dad0`.  That supports the
buffer-pointer interpretation but does not establish capacity, ownership, or
the record producer.

## Conservative C++ boundary

The only interface that should be typed now is the device global:

```cpp
struct RenderOwner;  // Deliberately opaque: only listed offsets are known.

extern IDirect3DDevice9 *g_D3D9Device; // 0x00491c30
extern RenderOwner *g_RenderOwner;     // 0x00491c10

// Internal non-C++ entry points. These are documentation signatures, not
// legal direct C++ declarations for their original binary ABIs.
// 0x004215a0: EDI = CameraWork*, no stack args.
// 0x00442f50: ESI = RenderOwner*, no stack args.
```

For a source reconstruction that must invoke the original helpers, use narrow
MSVC x86 thunks rather than assigning a misleading member-function type.  For
example, a callable wrapper around the second helper can receive its normal
source-level pointer in `ECX` and explicitly preserve/load `ESI`:

```cpp
void __declspec(naked) __fastcall FlushRenderOwnerPendingVertices(
    RenderOwner *owner) {
    __asm {
        push esi
        mov  esi, ecx
        call 0x00442f50
        pop  esi
        ret
    }
}
```

The literal address is illustrative only; production reconstruction should
route it through the project mapping/linker boundary.  An analogous thunk for
`0x004215a0` must preserve caller `EDI`, load its source-level `CameraWork *`
into `EDI`, and make no claim that the helper is `__thiscall`.

Conversely, reimplementing the helper in C++ can use ordinary calls to
`IDirect3DDevice9::SetTransform`, `Clear`, `SetTextureStageState`, `SetFVF`,
and `DrawPrimitiveUP`.  That is behaviorally faithful at the direct interface
boundary, but it is not evidence that an MSVC C++ compilation will reproduce
the original register ABI or object code.

## Uncertainty and non-claims

- This evidence does not identify the original type/name of `g_RenderOwner`,
  the contents of its vertex records, or the producer that populates the
  pending count.
- `CameraWork` is a conservative layout name.  Eye/target/up terminology is
  inferred from the observed D3DX function parameter positions, not from TH10
  symbols.
- This document does not type the separate virtual call at
  `MainChainContext + 0x08`, vtable `+0xbc`; it is not a call through either
  global requested here.
- No claim is made that these helpers themselves are ready for direct C++
  replacement or object-code matching.  The goal is an exact external ABI and
  layout boundary before such reconstruction.

## Verification sources

- `resources/th10.exe`, disassembly ranges `0x00420000-0x004200b1`,
  `0x004200d0-0x004200f7`, `0x00421480-0x004216ea`, and
  `0x00442f30-0x00442fd5`.
- Import table entries `d3d9.dll!Direct3DCreate9` and
  `d3dx9_31.dll!D3DXMatrixLookAtLH`, `D3DXMatrixPerspectiveFovLH`, and
  `D3DXVec3Normalize`.
- `IDirect3DDevice9` slot ordering from the local Windows SDK-compatible
  declaration at `/usr/include/wine/windows/d3d9.h`.
