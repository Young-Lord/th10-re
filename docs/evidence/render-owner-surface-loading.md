# Render-Owner Surface Loading — 0x00447c80 / 0x00446d70

Module: `src/RenderOwnerSurfaceLoading.cpp/.hpp`. Both entries had no
static xrefs (coverage-gap noted them as data/vtable-referenced only); the
neighborhood is otherwise covered by `LargeRenderOwner*.cpp`
(0x4462f0 `InitializeLargeRenderOwner` was already implemented there and is
unchanged) and `MainChainRenderAdapters.cpp`.

## 0x00447c80 `LoadRenderOwnerCachedSurfaceFromFile`

Native ECX = render owner, EDX = file name, stack = cache slot index;
`ret 4`. Cache arrays on the owner:

| offset | meaning |
| ------ | ------- |
| +0x3ad4e0 | primary surfaces[32] |
| +0x3ad560 | shadow surfaces[32] |
| +0x3ad5e0 | cached file buffers[32] |
| +0x3ad660 | cached file sizes[32] |
| +0x3ad6e0 | D3DXIMAGE_INFO descriptors[32] (stride 0x1c) |

Flow:

1. When a primary surface exists for the slot, `0x447f70`
   (`ReleaseLargeRenderOwnerCachedSurfacePair`) releases the pair.
2. Data source: the cached buffer (+0x3ad5e0) when present — its ownership
   moves into this call and the slot is cleared — otherwise the file name
   is copied through the `"%s"` sprintf helper 0x452ae8 into a stack buffer
   and loaded via `0x44b360` (`LoadMainChainFile(path, &size, 0)`). On load
   failure 0x44b8e0 formats the `"「%s」が読み込めないです。\r\n"` message
   (0x470150) against the 0x474f70 text context (native EDI) and the call
   returns -1.
3. The fixed 640x1024 (0x280 x 0x400) decode surface is created with the
   device wrapper slot 36 (`CreateOffscreenPlainSurface`, format
   DAT_00491d14, pool 3). **Native quirk:** when this creation fails the
   routine returns -1 without releasing the loaded buffer (leak preserved).
4. `D3DXLoadSurfaceFromFileInMemory` (IAT 0x4662c4) decodes the image into
   it with `D3DXIMAGE_INFO` output into the slot descriptor; the descriptor
   then supplies the real width/height.
5. The primary surface is created with device slot 28
   (`CreateRenderTarget(w, h, fmt, 0, 0, 1, &out, 0)`) falling back to slot
   36, and the shadow surface with slot 36.
6. Both receive the image via `D3DXLoadSurfaceFromSurface` (IAT 0x4662c0,
   filter 1); any failure releases the image surface and the buffer and
   returns -1.
7. Success releases the image surface (IUnknown Release) and the buffer and
   returns 0.

## 0x00446d70 `LoadGeneratedGlyphIntoSurface`

Native EAX = format selector, ECX = destination top, EDI = glyph record /
file image, stack = {dead first argument, record holder, source byte count,
embedded flag}; `ret 0x10`, always returns 0. The record holder (+0x00
wrapper object) creates the destination surface into the reused
source-size stack slot via vtable +0x48 `(this, 0, &out)`.

- **Embedded path** (flag != 0): the glyph record holds the data offset at
  +0x30; inside the data area the u16 selector sits at +0x06, the u16
  width/height at +0x08/+0x0a and the pixels at +0x10. The upload is
  `D3DXLoadSurfaceFromMemory(dest, 0, {0, top, w, h+top}, data+0x10,
  format, w * bpp, 0, {0, 0, w, h}, 1, 0)` using the selector tables at
  0x46f288 (`{0, 0x15, 0x19, 0x17, 0x14, 0x1a}`) and 0x46f2a0
  (`{4, 4, 2, 2, 3, 2}` bytes per pixel). Signatures verified against the
  vendored October 2006 DXSDK `d3dx9tex.h` (the 10-parameter
  `D3DXLoadSurfaceFromMemory` with `pSrcPalette` explains the native push
  order).
- **File-in-memory path** (flag == 0): the created surface's descriptor is
  fetched through vtable +0x30 and the image at EDI with the stack byte
  count is uploaded via `D3DXLoadSurfaceFromFileInMemory` with destination
  rectangle `{0, top, width, height}`.

The selector is remapped before the byte-count store: when the gate byte
DAT_00491d78 has bit 0 set, a selector whose format-table entry is 0x15 or
0 becomes 5 and 0x14 becomes 3. The holder receives the source size
(+0x08) and the remapped format's byte count (+0x0c), and the
post-processing helper 0x4465b0 (boundary, native EAX = holder) runs. The
destination surface is released through IUnknown::Release on both paths.

## Status

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` passes. CSV rows
appended for 0x00447c80 and 0x00446d70.
