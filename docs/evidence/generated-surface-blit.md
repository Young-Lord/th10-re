# Generated-Surface Texture Blitter — 0x00437750

Module: `src/GeneratedSurfaceBlit.cpp/.hpp`
(`BlendGeneratedSurfaceIntoTexture`). Native EAX = destination row, ECX =
destination column, EDX = source `GeneratedSurface`, stack = {texture,
columns, rows}; `ret 0xc`, boolean in AL. EDI/ECX survive into the kernel
bodies (destination pointer arithmetic), EBX carries the destination
column. Companion of the 0x437a00 static initializer
(`GeneratedFontTable.cpp`): this is where the GDI-rendered glyph bitmaps
are blended into locked D3D texture memory.

## Flow

1. Gate: `surface->bitmap` (+0x11c) must be set, otherwise return false.
2. Texture vtable +0x30 fills a D3DSURFACE_DESC-like descriptor whose first
   dword (pixel format) must equal `surface->pixel_format_id` (+0x100); on
   mismatch the blend is skipped but the texture is still unlocked and the
   call returns true.
3. Texture vtable +0x34 locks the texture: `{pitch, bits}` come back in the
   first two dwords of the lock record (the caller pre-zeroes +0x08/+0x0c
   and pre-fills +0x10/+0x14 with the surface dimensions; a `{0,0}` rect is
   passed). A failure returns false **without unlocking**.
4. Destination pixel `(row, column)` = `pitch * row + bits + column * bpp`;
   source = `surface->pixel_bits` (+0x120) with `surface->pitch_bytes`
   (+0x110). Both advance by `(stride - bpp * columns)` per row. The
   native reuses the consumed argument slots as scratch for the pitch and
   row counters; the kernel selector is `surface->pixel_format_id`:
   - **0x15 (A8R8G8B8, 4 bytes)**: each of the three stored channels is
     `src + ((dst - src) * src[3]) >> 8` (arithmetic shift, 8-bit
     truncation on store); the alpha byte is not written.
   - **0x19 (A1R5G5B5, 2 bytes)**: the word is copied only when its 0x8000
     alpha bit is set.
   - **0x1a (A4R4G4B4, 2 bytes)**: nibble blend — byte 0 high nibble
     (green) blends `dst + ((src-dst) * a) >> 4` against the destination,
     byte 0 low nibble (red) is written as `src * a >> 4` **without the
     destination baseline** (the `sub edx,eax` operand is the zeroed low
     nibble of the shifted result — native quirk preserved), and byte 1
     carries the accumulated alpha `min(15, ((dst>>2)&0x3c) + a)` in its
     high nibble with the blended blue in the low nibble.
   - any other id: no pixels are touched.
5. Texture vtable +0x38 unlocks; return true.

The `bpp` values agree with the bytes-per-pixel table at 0x46f2a0 used by
0x446d70 (`{4, 4, 2, 2, 3, 2}` for selectors 0..5, format ids
`{0, 0x15, 0x19, 0x17, 0x14, 0x1a}` at 0x46f288).

Degenerate-path note: when `columns <= 0` the native row advance reads a
stale saved-width slot (case 0x1a/0x15 skip the store that seeds it); the
reconstruction advances by the full stride in that case and does not
reproduce the stale-memory read.

## Status

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` passes. CSV row
appended for 0x00437750.
