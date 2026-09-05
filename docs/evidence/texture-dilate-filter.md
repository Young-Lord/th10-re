# Texture Dilate Filter (TH10 0x004465b0)

Implemented as `DilateTextureTransparentPixelsEaxAbi` in
`src/TextureDilateFilter.cpp/.hpp`.

## Semantics

A single-pass "dilate into transparency" filter over a texture's level-0
surface: every fully transparent pixel is filled with the channel-wise
average of its adjacent opaque pixels. Kept per-pixel, in place, inside
one LockRect/UnlockRect pair.

## ABI and COM boundaries

- Native entry: EAX = pointer to a texture holder whose first dword is
  the texture COM object. All COM access stays at the observed vtable
  offsets: GetSurfaceLevel `+0x48` (level 0), GetDesc `+0x30`, LockRect
  `+0x34` (rect 0, flags 0), UnlockRect `+0x38`, Release `+0x08`. The
  function returns the surface Release result.
- Description consumption: Format at desc+0, Width at desc+0x18, Height
  at desc+0x1c; LockRect yields Pitch at +0 and bits at +4. (The native
  reads exactly these offsets of the returned descriptor.)

## Format switch (27-case jump table, only four bodies)

- `0 / 0x15` (A8R8G8B8): current pixel alpha byte == 0 triggers the fill.
  Neighbors: left (x>0, alpha at pixel+3 nonzero), right (x<width-1),
  above (y>0), below (y<height-1); B/G/R averaged independently and the
  division by the neighbor count runs only when count > 1 (identical
  result for count == 1, preserved as structured). Row pointer stepping
  uses `pitch/4 * 4` and `pitch/2 * 2` pixel arithmetics exactly as the
  native does.
- `0x19` (A1R5G5B5): fill when the pixel's alpha bit is clear (high byte
  signed-negative test). Neighbors count as opaque when bit 0x8000 is
  set; 5-bit channels extracted as `(w>>10)&0x1f` (R), `(w>>5)&0x1f` (G),
  `w&0x1f` (B); the recomposed word keeps the pixel's own alpha bit.
- `0x1a` (A4R4G4B4): fill when the alpha nibble is clear (byte+1 &
  0xF0 == 0); opaque neighbors via `(w & 0xF000) != 0`; 4-bit channels
  R=(w>>8)&0xF, G=(w>>4)&0xF, B=w&0xF; alpha nibble preserved.
- All other formats: no pixel work; UnlockRect + Release still run.

## Callers

`0x446b70`, `0x446c70`, `0x446d70`, `0x446eb0` (thin adapter variants).

## Status

Baselines pass (`scripts/compile-main-chain-cpp.sh`, g++ -m32 -std=c++98
syntax check, `git diff --check`). IDA name/comment applied at 0x4465b0.
