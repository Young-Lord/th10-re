# Generated Font Table Evidence

## Scope and naming

This document covers the static initializer at `0x00437a00`, its paired
teardown at `0x00437d10`, and the DIBV4 surface constructor at
`0x00436af0` in `resources/th10.exe`.

`GeneratedFontTable` is a descriptive C++ boundary, not a recovered source
type.  The code initializes two independent resources:

1. a static DIBV4-backed surface at `0x00474cb0`, whose first 256 bytes are
   populated with generated values; and
2. fourteen separately-owned GDI `HFONT` handles at `0x004918a0` through
   `0x00491868`.

The table is therefore not an array of fonts, and `0x436af0` is not a font
creation helper.

## Entrypoints and ABI

| Address | Inputs | Result | Role |
| --- | --- | --- | --- |
| `0x00437a00` | no explicit inputs; plain `ret` | none | Builds the surface/table and creates 14 fonts. |
| `0x00437d10` | no explicit inputs; plain `ret` | none | Destroys the surface, then calls `DeleteObject` on each font slot. |
| `0x00436af0` | `EAX = GeneratedSurface*`, stack `(width, height, pixel_format_id)`; `ret 12` | `AL = 1` on success, `0` on failure | Rebuilds a DIBV4-backed GDI surface. |
| `0x00436a30` | `ESI = GeneratedSurface*`; plain `ret` | `AL = 1` only when a DC existed | Releases the GDI resources owned by that surface. |

`0x436af0` is a mixed-register ABI and should be put behind an adapter at a
binary boundary.  The semantic C++ function can take ordinary typed
arguments, but it must not be declared as a normal `thiscall` implementation
of the original entry.

## Static storage

### Surface at `0x00474cb0`

The initializer writes bytes `0x00474cb0..0x00474daf`; construction writes
through `+0x120`, proving a minimum object size of `0x124` bytes.

| Offset | Recovered field | Evidence |
| --- | --- | --- |
| `+0x000..+0x0ff` | `u8 generated[256]` | `0x437a5c` writes one byte per index `0..255`.  Other routines also treat this region as a table; it is not the DIB pixel buffer. |
| `+0x100` | `i32 pixel_format_id` | Set from the third stack argument on successful construction; reset to `-1` by `0x436a30`. |
| `+0x104` | `i32 width` | Set from the first stack argument. |
| `+0x108` | `i32 height` | Set from the second stack argument. |
| `+0x10c` | `i32 image_bytes` | Written from `BITMAPV4HEADER.bV4SizeImage`. |
| `+0x110` | `i32 pitch_bytes` | Written from the aligned scanline stride. |
| `+0x114` | `HDC memory_dc` | Returned by `CreateCompatibleDC(NULL)`. |
| `+0x118` | `HGDIOBJ previous_selection` | Returned by `SelectObject(memory_dc, bitmap)`. |
| `+0x11c` | `HBITMAP bitmap` | Returned by `CreateDIBSection`. |
| `+0x120` | `void* pixel_bits` | Written by `CreateDIBSection` through its `ppvBits` argument. |

The `generated` member is deliberately separate from `pixel_bits`: the DIB
allocation is created by GDI and its returned pointer resides at `+0x120`.

### Pixel-format descriptor table at `0x00474918`

`0x436af0` linearly scans 24-byte records until an `id == -1` sentinel.  The
six initialized records have this layout:

```cpp
struct PixelFormatDescriptor {
    i32 id;          // +0x00
    i32 bits_per_pixel; // +0x04
    u32 alpha_mask;  // +0x08
    u32 red_mask;    // +0x0c
    u32 green_mask;  // +0x10
    u32 blue_mask;   // +0x14
};
```

| `id` | bpp | alpha | red | green | blue |
| --- | --- | --- | --- | --- | --- |
| `0x16` | 32 | `0x00000000` | `0x00ff0000` | `0x0000ff00` | `0x000000ff` |
| `0x15` | 32 | `0xff000000` | `0x0000ff00` | `0x00ff0000` | `0x000000ff` |
| `0x18` | 16 | `0x00000000` | `0x00007c00` | `0x000003e0` | `0x0000001f` |
| `0x17` | 16 | `0x00000000` | `0x0000f800` | `0x000007e0` | `0x0000001f` |
| `0x19` | 16 | `0x00800000` | `0x00007c00` | `0x000003e0` | `0x0000001f` |
| `0x1a` | 16 | `0x00f00000` | `0x00000f00` | `0x000000f0` | `0x0000000f` |

There is no safe semantic claim beyond the bit-mask values for the IDs.  In
particular, the primary initializer tries `0x1a` and falls back to `0x15`; it
does not prove a source-level format enum name.

### Font slots

The binary stores the handles in descending addresses.  This gives a logical
height-indexed sequence while preserving the original storage mapping:

| Height | Slot address |
| --- | --- |
| `0x20` | `0x004918a0` |
| `0x22` | `0x0049189c` |
| `0x24` | `0x00491898` |
| `0x26` | `0x00491894` |
| `0x28` | `0x00491890` |
| `0x2a` | `0x0049188c` |
| `0x2c` | `0x00491888` |
| `0x2e` | `0x00491884` |
| `0x30` | `0x00491880` |
| `0x32` | `0x0049187c` |
| `0x34` | `0x00491878` |
| `0x36` | `0x00491874` |
| `0x38` | `0x00491870` |
| `0x3a` | `0x0049186c` |
| `0x3c` | `0x00491868` |

This is an exact 14-slot range, not a conventional ascending C array at the
first call's address.  A readable reconstruction may use
`HFONT fonts_by_height[14]`, provided its binary adapter maps index `i` to
`0x4918a0 - 4*i`.

## `0x00436af0`: DIBV4 construction algorithm

The constructor always begins by calling `0x436a30`; a repeated successful
initialization first releases the prior DIB resources.

It locates the requested descriptor.  The search starts at `0x474918`, uses
24-byte strides, stops at the `id == -1` sentinel, and returns failure before
any GDI allocation when the requested ID is absent.  The apparent `lea` of a
candidate pointer is not a nullability check: for any non-`-1` request it
produces the first matching record or the sentinel record address.

For a matching descriptor it creates a zeroed local `BITMAPV4HEADER`:

```cpp
pitch = ((width * descriptor.bits_per_pixel + 7) / 8 + 3) & ~3;

BITMAPV4HEADER header = {};
header.bV4Size       = 0x6c;
header.bV4Width      = width;
header.bV4Height     = -height;       // top-down DIB
header.bV4Planes     = 1;
header.bV4BitCount   = (WORD)descriptor.bits_per_pixel;
header.bV4V4Compression = BI_BITFIELDS; // 3, except IDs 0x16 and 0x18 leave it at 0
header.bV4SizeImage  = pitch * height;
header.bV4RedMask    = descriptor.red_mask;
header.bV4GreenMask  = descriptor.green_mask;
header.bV4BlueMask   = descriptor.blue_mask;
header.bV4AlphaMask  = descriptor.alpha_mask;
```

The `BI_BITFIELDS` write is omitted only for IDs `0x16` and `0x18`; their
masks are still copied into the V4 header.  The arithmetic in the executable
uses signed divide-by-power-of-two correction sequences, but all demonstrated
callers supply positive dimensions.  The expression above is equivalent for
the positive dimensions under this subsystem's contract.

It then performs this exact GDI sequence:

```cpp
HBITMAP bitmap = CreateDIBSection(
    NULL, reinterpret_cast<const BITMAPINFO*>(&header), DIB_RGB_COLORS,
    &pixel_bits, NULL, 0);
if (bitmap == NULL) return false;

memset(pixel_bits, 0, header.bV4SizeImage);
HDC dc = CreateCompatibleDC(NULL);       // return value is not checked
HGDIOBJ old = SelectObject(dc, bitmap);  // return value is not checked
```

Finally it stores every field listed above and returns `AL = 1`.  Thus a null
`CreateCompatibleDC` result still flows into `SelectObject` and still reports
constructor success after a non-null `HBITMAP`; a defensive C++ rewrite must
not silently add a different failure path if behavioral fidelity is required.

The static initializer invokes this helper as:

```text
try:      EAX = 0x474cb0, width = 0x400, height = 0x40, id = 0x1a
fallback: EAX = 0x474cb0, width = 0x400, height = 0x40, id = 0x15
          only when the first call returns AL == 0
```

It does not inspect the fallback result.  The resulting `+0x100..+0x120`
metadata describes a `1024 x 64` top-down 16- or 32-bpp DIB, while the first
256 object bytes remain the generated table.

## Generated 256-byte table

After the DIB attempts, `0x437a00` reads the counter at `0x004918b4`, loops
exactly 256 times, and writes `0x474cb0[i]`.  The counter is incremented once
per table element and written back only after the loop.

For each iteration, where `x` is the low 16 bits of `0x004918b0`:

```cpp
x = u16((x ^ 0x9630u) - 0x6553u);
x = u16((x >> 14) + (x << 2));
g_random_state = x;
generated[i] = u8(x >> 9);
```

The register writes are 16-bit, so both assignments intentionally truncate.
The function increments `0x4918b4` by 256 even when both surface creation
attempts fail.  The same state and counter have many consumers elsewhere in
the executable, so they should be modeled as existing global PRNG state, not
as private font-table fields.

## Font construction

Each call uses `CreateFontA` with a fixed specification and only varies
`nHeight`:

```cpp
static const char kFace[] = { 0x82, 0x6c, 0x82, 0x72, 0x20,
                              0x83, 0x53, 0x83, 0x56, 0x83, 0x4e, 0 };
CreateFontA(height, 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE,
            SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            PROOF_QUALITY, FIXED_PITCH | FF_MODERN,
            kFace);
```

Exact encoded face bytes at `0x0046f36c` are CP932
`"ＭＳ ゴシク"`; the final Japanese character normally written as
`"ＭＳ ゴシック"` is not present in the binary byte sequence.  `nWeight` is
`0x190` (`FW_NORMAL`), italic/underline/strikeout are zero, charset is
`0x80` (`SHIFTJIS_CHARSET`), output precision is `0`, clip precision is `0`,
quality is `4` (`PROOF_QUALITY`), and pitch/family is `0x11`
(`FIXED_PITCH | FF_MODERN`).

Every returned handle is stored unconditionally; no `CreateFontA` result is
checked and no partial-construction cleanup occurs in the initializer.
Repeated successful initialization overwrites those slots without first
deleting their old values, so it leaks the prior fonts.  This is separate from
the surface, whose prior GDI resources are released by the opening call to
`0x436a30`.

## `0x00437d10`: teardown and ownership

Teardown has two independent phases in this fixed order:

1. load `ESI = 0x474cb0` and call `0x436a30`;
2. call imported `DeleteObject` once for each 14 font-slot values, in the
   order `0x4918a0`, `0x49189c`, ..., `0x491868`.

`0x436a30` only proceeds when `surface.memory_dc != NULL`:

```cpp
if (surface.memory_dc != NULL) {
    SelectObject(surface.memory_dc, surface.previous_selection);
    DeleteDC(surface.memory_dc);
    DeleteObject(surface.bitmap);
    surface.width = 0;
    surface.height = 0;
    surface.memory_dc = NULL;
    surface.bitmap = NULL;
    surface.previous_selection = NULL;
    surface.pixel_bits = NULL;
    surface.pixel_format_id = -1;
    return true;
}
return false;
```

Notably, it leaves `image_bytes` and `pitch_bytes` unchanged.  It also does
not release a non-null bitmap if `memory_dc` is null, since the guard is on
the DC alone, and it does not modify `generated[256]`.  Those are binary
facts, not recommended resource-management policy.

`0x437d10` does **not** clear any of the fourteen font globals after
`DeleteObject`.  It also calls `DeleteObject` unconditionally, including
null or stale values after a failed/repeated initializer.  Consequently the
original pair is not generally idempotent.  A high-level RAII facade may make
normal ownership safer, but a matching boundary must preserve this cleanup
order, unchecked calls, and slot-write behavior.

There is an additional CRT/static cleanup thunk at `0x00465a20` that calls
only `0x436a30` for `0x474cb0`; it does not delete the font slots.  The
explicit `0x437d10` call from the main-chain teardown at `0x004202ef` is the
only recovered path here that destroys both classes of resource.

## Conservative C++ boundary

```cpp
struct GeneratedSurface {
    u8 generated[0x100];
    i32 pixel_format_id;
    i32 width;
    i32 height;
    i32 image_bytes;
    i32 pitch_bytes;
    HDC memory_dc;
    HGDIOBJ previous_selection;
    HBITMAP bitmap;
    void *pixel_bits;
};

struct GeneratedFontTable {
    GeneratedSurface surface;
    HFONT fonts_by_height[14]; // adapter maps index i to 0x4918a0 - 4*i
};
```

The structures intentionally do not assert that the fonts are physically
adjacent to the surface: they are not.  `GeneratedFontTable` is a logical
owner boundary only.  Keep the `EAX` constructor adapter and the binary
slot-address mapping outside the ordinary typed implementation.

## Verification sources

- `resources/th10.exe`, `objdump -D -Mintel`, ranges
  `0x00436a20-0x00436d26`, `0x00437a00-0x00437da4`, and
  `0x00420230-0x00420310`.
- PE import table: `0x00466020 = CreateCompatibleDC`,
  `0x00466024 = CreateDIBSection`, `0x00466028 = DeleteDC`,
  `0x0046602c = SelectObject`, `0x00466030 = DeleteObject`, and
  `0x00466034 = CreateFontA`.
- Static data: descriptor records at `0x00474918`, surface storage at
  `0x00474cb0`, and font handles at `0x004918a0..0x00491868`.
