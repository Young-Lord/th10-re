# Generated Surface Implementation Evidence

## Scope

This document is a direct control-flow recovery of the generated GDI surface
constructor at `0x00436af0` and its cleanup entry at `0x00436a30` in
`resources/th10.exe`.  It is intended to support a semantic C++ adapter; it
does not claim that either binary entry uses C++ member-function ABI.

The object addressed by all recovered callers is the static storage at
`0x00474cb0`, but the entries operate on an arbitrary supplied base pointer.
Its first `0x100` bytes are unrelated generated-table storage.  Surface
metadata starts at `+0x100`:

```cpp
struct GeneratedSurface {
    u8 generated[0x100];       // +0x000; not initialized by these entries
    i32 pixel_format_id;       // +0x100
    i32 width;                 // +0x104
    i32 height;                // +0x108
    i32 image_bytes;           // +0x10c
    i32 pitch_bytes;           // +0x110
    HDC memory_dc;             // +0x114
    HGDIOBJ previous_selection;// +0x118
    HBITMAP bitmap;            // +0x11c
    void *pixel_bits;          // +0x120
};
```

The recovered object extent is `0x124` bytes.  `image_bytes` and
`pitch_bytes` are signed 32-bit storage because the original arithmetic and
stores are 32-bit; that does not establish a source-level signed type.

## Exact entry ABIs

| Address | Input convention | Stack cleanup | Return |
| --- | --- | --- | --- |
| `0x004369e0` | `EAX = GeneratedSurface*` | plain `ret` | unspecified | Initializes selected metadata fields. |
| `0x00436a20` | `ESI = GeneratedSurface*` | tail-jumps to `0x436a30` | `AL` from cleanup | Cleanup thunk. |
| `0x00436a30` | `ESI = GeneratedSurface*` | plain `ret` | `AL = 1` iff `memory_dc != NULL` on entry | Releases the attached GDI resources. |
| `0x00436af0` | `EAX = GeneratedSurface*`; stack `width`, `height`, `pixel_format_id` | `ret 12` | `AL = 1` after `CreateDIBSection` succeeds, otherwise `0` | Reconstructs a DIB-backed memory surface. |

`0x436af0` moves `EAX` to `ESI` and calls `0x436a30`; the cleanup entry
therefore receives its required pointer register.  The constructor preserves
`EBP`, `ESI`, `EDI`, and (after its local use) `EBX`; it is nevertheless a
mixed-register binary boundary, not a normal `__thiscall` target.

The cleanup preserves only `EDI` among the registers it uses.  It may clobber
`EAX`, `ECX`, and `EDX` through its work and imported calls.  A typed C++
implementation should use an ordinary function such as
`bool RebuildGeneratedSurface(GeneratedSurface&, i32, i32, i32)` and keep
register shims separate.

## Initialization and cleanup state

`0x004369e0` is an initializer, not cleanup.  With `EAX = surface`, it writes
only:

```cpp
surface.pixel_format_id = -1;
surface.width = 0;
surface.height = 0;
surface.memory_dc = NULL;
surface.bitmap = NULL;
surface.previous_selection = NULL;
surface.pixel_bits = NULL;
```

It deliberately leaves `generated`, `image_bytes`, and `pitch_bytes`
unchanged.  This matters because the constructor starts with cleanup rather
than this initializer: callers must provide a surface whose `memory_dc` field
is validly initialized to zero or to an owned DC.

At `0x00436a30`, the single guard is `surface.memory_dc != NULL`.  When that
field is null, the entry sets `AL = 0` and changes no field, including a
possibly non-null `bitmap`.

When it is non-null, calls occur in this fixed order, with no result checks:

```cpp
SelectObject(surface.memory_dc, surface.previous_selection);
DeleteDC(surface.memory_dc);
DeleteObject(surface.bitmap);
```

Only after all three calls, it writes:

```cpp
surface.width = 0;
surface.height = 0;
surface.memory_dc = NULL;
surface.bitmap = NULL;
surface.previous_selection = NULL;
surface.pixel_bits = NULL;
surface.pixel_format_id = -1;
return true;
```

It does not clear `image_bytes`, `pitch_bytes`, or `generated`.  More
importantly, cleanup does not delete a bitmap when `memory_dc` is null.  This
is the original ownership boundary, even though an RAII-only design would be
more defensive.

## Format records and original scan behavior

The constructor begins by releasing any DC-owned old surface, then zeros a
`0x6c`-byte local `BITMAPV4HEADER`.  It reads 24-byte records beginning at
`0x00474918`:

```cpp
struct PixelFormatRecord {
    i32 id;              // +0x00
    i32 bits_per_pixel;  // +0x04
    u32 alpha_mask;      // +0x08
    u32 red_mask;        // +0x0c
    u32 green_mask;      // +0x10
    u32 blue_mask;       // +0x14
};
```

The six defined IDs are `0x16`, `0x15`, `0x18`, `0x17`, `0x19`, and `0x1a`.
They are followed by a record whose first word at `0x004749a8` is `-1`.

There is an important raw-control-flow qualification.  The scan compares the
current record ID with the requested ID *before* testing whether it is the
sentinel.  Therefore:

- a requested ID of `-1` reaches the sentinel and returns `AL = 0`;
- each of the six defined IDs selects its matching record;
- an unrecognized non-`-1` request advances beyond the sentinel and continues
  reading 24-byte strides outside this descriptor table until an unrelated
  `-1` happens to be read or the process faults.

The apparent `test ecx, ecx` after address calculation is only an address
nonzero test; it is not a successful-lookup test.  A semantic adapter should
normally validate against the six known records to avoid reproducing an
out-of-bounds read, while documenting that this is a deliberate safety
boundary rather than literal behavior.  For binary-exact use, the original
input domain must be preserved.

## Header calculation and GDI calls

For a selected record, the original uses 32-bit signed instructions.  Let
`product` be the low 32 bits of signed `width * bits_per_pixel`.  The exact
calculation is equivalent to:

```cpp
i32 bytes_before_alignment = trunc_toward_zero(product / 8);
i32 pitch = trunc_toward_zero((bytes_before_alignment + 3) / 4) * 4;
i32 image_bytes = low_i32(pitch * height);
```

The executable's `imul`, correction before `sar`, and final `sar/shl` give
the stated truncation behavior for negative intermediates.  Ordinary signed
C++ multiplication can invoke undefined behavior on overflow, so a fidelity
adapter needs defined 32-bit wrap helpers if hostile dimensions are in scope.
Known callers use positive `0x400 x 0x40` dimensions.

The zeroed header is populated as follows:

```cpp
BITMAPV4HEADER header = {};
header.bV4Size = 0x6c;
header.bV4Width = width;
header.bV4Height = -height;             // top-down DIB
header.bV4Planes = 1;
header.bV4BitCount = (WORD)record.bits_per_pixel;
header.bV4SizeImage = image_bytes;
header.bV4RedMask = record.red_mask;
header.bV4GreenMask = record.green_mask;
header.bV4BlueMask = record.blue_mask;
header.bV4AlphaMask = record.alpha_mask;
if (pixel_format_id != 0x18 && pixel_format_id != 0x16)
    header.bV4V4Compression = BI_BITFIELDS; // 3
```

Masks are copied even in the two compression-zero cases.  No color-space,
gamma, endpoint, or palette fields are subsequently changed from zero.

The imported calls at `0x00466024`, `0x00466020`, and `0x0046602c` are made
with these exact effective arguments:

```cpp
void *bits;
HBITMAP bitmap = CreateDIBSection(
    NULL, reinterpret_cast<const BITMAPINFO*>(&header), DIB_RGB_COLORS,
    &bits, NULL, 0);
if (bitmap == NULL)
    return false;

ZeroMemory(bits, static_cast<size_t>(image_bytes));
HDC dc = CreateCompatibleDC(NULL);
HGDIOBJ previous = SelectObject(dc, bitmap);
```

`CreateDIBSection` is the only checked call.  A null `CreateCompatibleDC`
still proceeds to `SelectObject`; the function then stores the null DC and
reports success.  If that happens, later cleanup does not delete the stored
bitmap because its guard observes the null DC.  `SelectObject`'s result is
stored even when it is `NULL` or `HGDI_ERROR`.

The zero fill is performed immediately after a non-null bitmap and before DC
creation.  Its count is the 32-bit `image_bytes` result; no zero/negative or
overflow guard exists.  The normal positive-dimension C++ adapter may use
`memset(bits, 0, image_bytes)` only after independently validating that the
count is representable as a non-negative size.

On the successful path, stores occur in this order:

```cpp
surface.previous_selection = previous;
surface.pitch_bytes = pitch;
surface.pixel_bits = bits;
surface.image_bytes = image_bytes;
surface.memory_dc = dc;
surface.bitmap = bitmap;
surface.width = width;
surface.height = height;
surface.pixel_format_id = pixel_format_id;
return true;
```

On a `CreateDIBSection` failure, the opening cleanup has already run and no
surface metadata is written.  It neither retries nor cleans any partial GDI
resource because no later resource has been acquired at that branch.

## C++ implementation boundary

A semantic reconstruction can make the expected format input domain explicit
without claiming it is the original entrypoint:

```cpp
bool RebuildGeneratedSurface(GeneratedSurface &surface,
                             i32 width, i32 height, i32 format_id)
{
    DestroyGeneratedSurface(surface);
    const PixelFormatRecord *format = FindKnownFormat(format_id);
    if (format == NULL)
        return false;

    // Build the BITMAPV4HEADER and perform the exact successful-path order
    // described above.  Do not add DC/SelectObject failure cleanup here when
    // modeling the original normal-input behavior.
}
```

This adapter deliberately improves the malformed-format behavior.  For
ordinary recovered call sites, it retains the important original behavior:
opening conditional cleanup, `CreateDIBSection` as the only allocation
failure test, top-down DIB construction, bitmap zeroing before DC allocation,
and DC-first conditional destruction.

## Verification basis

- `resources/th10.exe`, x86 disassembly ranges `0x004369e0-0x00436a9a` and
  `0x00436af0-0x00436c92`.
- PE imports: `CreateCompatibleDC` at `0x00466020`, `CreateDIBSection` at
  `0x00466024`, `DeleteDC` at `0x00466028`, `SelectObject` at `0x0046602c`,
  and `DeleteObject` at `0x00466030`.
- Static descriptor data at `0x00474918-0x004749bf` and generated surface
  storage at `0x00474cb0`.
