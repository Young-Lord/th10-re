# ManagerWork Stage Setup Adapters

## Scope and naming

This note records directly observable contracts for the three setup adapters
called by `ProcessSelectedManagerWorkStage` (`0x00447470`) and its output
copier in `resources/th10.exe`:

| Address | Behavioral name |
| --- | --- |
| `0x00446eb0` | `CreateTextureFromEncodedStageData` |
| `0x00446f40` | `CreateTextureAndUploadStagePixels` |
| `0x00447050` | `CreateEmptyStageTexture` |
| `0x00447940` | `CopyAndDeriveStageOutputRecord` |

`StageDescriptor`, `WorkRecord`, and `StageOutputRecord` are layout names
only. They do not identify the original C++ class or on-disk format.

## Shared texture selection

All three setup paths select an index into these read-only tables:

```text
0x0046f288: D3DFORMAT-like value by index
0x0046f2a0: bytes-per-pixel / pitch multiplier by index
```

When `(byte)0x00491d78 & 1` is clear, the caller's index is used unchanged.
When it is set, the selected format value is remapped as follows before either
table is indexed:

```text
format == 0 or 0x15 -> index 5
format == 0x14        -> index 3
otherwise             -> original index
```

The first eight table pairs are:

| Index | `0x46f288` | `0x46f2a0` |
| --- | ---: | ---: |
| 0 | 1 | 4 |
| 1 | 2 | 4 |
| 2 | 0 | 2 |
| 3 | 0x15 | 2 |
| 4 | 0x19 | 3 |
| 5 | 0x17 | 2 |
| 6 | 0x14 | 0x61 |
| 7 | 0x1a | 0x77 |

`0x00491c30` is passed as the `IDirect3DDevice9 *` argument to D3DX in every
adapter. These functions do not range-check the format index.

## Selected stage inputs

For a selected `ChainNode *node`, `0x00447470` chooses exactly one path:

```text
if (node->use_alternate_adapter_0034 != 0)
    CreateTextureAndUploadStagePixels(...);
else if (*(u8 *)(node + node->marker_source_relative_001c) == '@')
    CreateEmptyStageTexture(...);
else
    CreateTextureFromEncodedStageData(...);
```

The target is the 16-byte record at:

```text
WorkRecord *record = (WorkRecord *)(work->records_0120 + 16 * stage_index);
```

The record is subsequently used as a virtual object pointer at `+0`, an owned
allocation at `+4`, an opaque scalar at `+8`, and the selected
bytes-per-pixel/pitch multiplier at `+0xc`. Only the first and last are
written by all three adapters; ownership of `+4` and meaning of `+8` depend on
the chosen path and are not fully established here.

## `0x00446eb0`: encoded-memory texture path

### Native boundary

```text
input: EAX = node + 0x14
       EDX = node + 0x10
       ESI = WorkRecord *record
       stack[0] = ManagerWork *work       (not read in recovered body)
       stack[1] = node + 0x18
       stack[2] = node + 0x0c
output: EAX = 0 on success, -1 on D3DX failure
stack: callee removes three words (`ret 0xc`)
```

It creates `record->virtual_object` using
`D3DXCreateTextureFromFileInMemoryEx`. The recovered argument mapping is:

```cpp
D3DXCreateTextureFromFileInMemoryEx(
    g_D3DDevice_491c30,
    record->owned_allocation,       // record + 0x04
    record->unknown_0008,           // record + 0x08, byte count
    *(i32 *)(node + 0x0c),          // width
    *(i32 *)(node + 0x10),          // height
    0, 0, selected_format, 1, 1, -1,
    *(u32 *)(node + 0x18),          // color key
    0, 0,
    reinterpret_cast<IDirect3DTexture9 **>(record));
```

The literal values identify the D3DX boundary; the semantic values of the
node fields are not independently proven beyond their argument positions.
On a nonzero HRESULT it returns `-1` immediately and does not initialize
`record+0xc`. On success it calls `0x004465b0` with `EAX=record`, then writes
the selected `0x46f2a0` table value to `record+0xc`, and returns zero.

`0x004465b0` only dereferences the created object. It obtains a level/surface
view through virtual slots `+0x48`, `+0x30`, and `+0x34`, then releases both
temporary COM interfaces. Its detailed image-description output is outside
this adapter contract, so it should remain a separate opaque post-create
step in C++ until recovered.

## `0x00446f40`: raw-pixel upload path

### Native boundary

```text
input: EAX = node + 0x14
       ECX = node + 0x10
       EBX = WorkRecord *record
       stack[0] = node + 0x0c
       stack[1] = node + 0x30
output: EAX = 0 on success, -1 only when D3DXCreateTexture fails
stack: callee removes two words (`ret 8`)
```

It calls:

```cpp
D3DXCreateTexture(
    g_D3DDevice_491c30,
    *(i32 *)(node + 0x0c),          // width
    *(i32 *)(node + 0x30),          // height
    1, 0, selected_format, 1,
    reinterpret_cast<IDirect3DTexture9 **>(record));
```

After a creation failure it conditionally releases a local COM pointer (which
is initialized to null in this entry) and returns `-1`. After creation it:

1. obtains level zero from the texture through virtual slot `+0x48`;
2. calculates `source_pitch` as signed `*(i16 *)(node + 0x18)` times the
   selected `0x46f2a0` table value, using the format index at
   `*(i16 *)(node + 0x16)`;
3. uses `node+0x20` as the source-memory pointer;
4. calls `D3DXLoadSurfaceFromMemory` on the obtained level-zero surface;
5. writes the selected multiplier to `record+0xc`; and
6. releases the level-zero surface.

The `D3DXLoadSurfaceFromMemory` HRESULT is ignored. Thus a C++ port must not
report an upload failure after texture creation if it is preserving original
control flow. The routine returns zero after this point regardless of that
upload result.

## `0x00447050`: empty texture path

### Native boundary

```text
input: ECX = node + 0x10
       EDX = node + 0x0c
       ESI = format-table index
       EDI = WorkRecord *record
output: EAX = 0 unconditionally
stack: no stack arguments; plain `ret`
```

It calls `D3DXCreateTexture` with `width=EDX`, `height=ECX`, mip levels `1`,
usage `0`, the selected format, pool/literal `1`, and `record` as the output
texture pointer. It ignores the HRESULT, writes the selected multiplier to
`record+0xc`, and returns zero. Therefore this path can leave a null or stale
`record->virtual_object` if creation fails; callers deliberately do not test
it before the following virtual calls.

## `0x00447940`: output copier

### Native boundary

```text
input: EAX = destination record index
       EDX = ManagerWork *work
       EBX = StageOutputRecord *source
output: none defined
stack: no arguments; plain `ret`
```

It copies exactly 17 dwords (`0x44` bytes) from `source` to:

```text
destination = (u8 *)work->output_records_0118 + 0x44 * EAX
```

It then overwrites six floats in that destination, with no denominator guard:

```cpp
destination->field_20 = destination->field_08 / destination->field_1c;
destination->field_28 = destination->field_10 / destination->field_1c;
destination->field_24 = destination->field_0c / destination->field_18;
destination->field_2c = destination->field_14 / destination->field_18;
destination->field_34 =
    (destination->field_10 - destination->field_08) / source->field_38;
destination->field_30 =
    (destination->field_14 - destination->field_0c) / source->field_3c;
```

The binary uses x87 `FDIV`; zero and exceptional inputs must retain its
floating-point behavior when exact numerical compatibility matters. The
remaining `0x44`-byte fields are copied verbatim and remain unnamed.

## C++ reconstruction boundary

Use ordinary C++ semantic adapters, keeping the unusual entry ABI in optional
tiny wrappers only:

```cpp
struct WorkRecordPartial {
    IDirect3DTexture9 *texture; // +0x00, used as the later virtual object
    const void *source_data;    // +0x04 for the encoded path
    i32 source_size;            // +0x08 for the encoded path
    i32 format_pitch_factor;    // +0x0c
};

enum StageSetupResult { StageSetup_Ok = 0, StageSetup_Failed = -1 };

StageSetupResult SetupEncodedStageTexture(
    WorkRecordPartial &record, const ChainNodeView &node);
StageSetupResult SetupRawStageTexture(
    WorkRecordPartial &record, const ChainNodeView &node);
void SetupEmptyStageTexture(
    WorkRecordPartial &record, const ChainNodeView &node);
void CopyAndDeriveStageOutputRecord(
    ManagerWorkPartial &work, i32 output_index,
    const StageOutputRecord &source);
```

`ProcessSelectedManagerWorkStage` should select these semantic functions,
return `-1` only when the first two report failure, then preserve the binary's
unconditional post-setup virtual calls. Do not collapse all three paths into a
single checked texture factory: their HRESULT policies and data-source
contracts differ materially.

## Evidence basis

* `resources/th10.exe`, `0x00446eb0-0x00447078` and
  `0x00447940-0x004479ce`.
* Direct caller `0x00447470-0x004476f7`.
* Import table: `0x004521a0` is `D3DXCreateTexture`, `0x004521a6` is
  `D3DXLoadSurfaceFromMemory`, and `0x004521ca` is
  `D3DXCreateTextureFromFileInMemoryEx` from `d3dx9_31.dll`.
