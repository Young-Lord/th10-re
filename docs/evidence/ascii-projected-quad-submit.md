# Projected Glyph Quad Submission — 0x00442ad0

Module: `src/AsciiProjectedQuadSubmit.cpp/.hpp`
(`SubmitAsciiProjectedQuadEaxEcxStackAbi`). Native EAX = VM record, ECX =
render owner, stack = one flags byte; `ret 4`, always returns 0. The only
static caller is the projected-glyph wrapper at 0x443670. The entry
operates on the shared glyph scratch quad at 0x4978c0 (four 0x1c-byte
`{x, y, z, rhw, color, u, v}` vertices) and the render owner's draw caches,
the same structures used by 0x442670 / 0x4425a0 / 0x442fe0
(AsciiGlyphSubmission.cpp, which keeps local copies of the small helpers —
duplicated here for the same reason).

## Steps

1. **Camera offset**: every vertex x += owner+0x5c, y += owner+0x60 (the
   camera values mirrored into the render owner, `camera_value_005c/0060`
   in `MainChainRenderOwnerFrameState`).
2. **Pixel snap** (flags bit 0): each quad edge is rounded with the game's
   round-to-nearest-even control word (modeled as `RoundGlyphCoordinate`)
   and shifted by -0.5f (flt_4700e8); assignments:
   `v0.x = v2.x = round(v0.x)-0.5`, `v1.x = v3.x = round(v2.x)-0.5`,
   `v0.y = v1.y = round(v0.y)-0.5`, `v2.y = v3.y = round(v3.y)-0.5`.
3. **UVs** from the VM's texture-source record at [vm+0x394]:
   `u_left = [+0x28]+[vm+0x54] -> v0.u/v2.u`, `u_right = [+0x20]+[vm+0x54]
   -> v1.u/v3.u`, `v_top = [+0x24]+[vm+0x58] -> v0.v/v1.v`,
   `v_bottom = [+0x2c]+[vm+0x58] -> v2.v/v3.v`.
4. **Viewport cull** against DAT_00491fac's D3DViewport (+0xcc x, +0xd0 y,
   +0xd4 width, +0xd8 height); x/y/width/height convert through the
   `fild` + 2^32 (flt_470b98) unsigned fixup, and right/bottom are summed
   in integers before conversion. Native `fnstsw` mask quirks preserved:
   - `max_x >= vp_x` required, but unordered (NaN) passes (`test ah,0x5`
     parity idiom),
   - `max_y > vp_y` required — equality also rejects (`test ah,0x41`
     idiom), NaN passes,
   - `right >= min_x` and `bottom >= min_y` required (NaN passes).
5. **Texture bind**: when the record's +0x04 texture differs from the
   owner's +0x3ada64 cache, the owner flushes (0x442f50), caches the
   texture and issues `SetTexture(0, tex)` (device vtable 65). The
   +0x3ada6a texture-active flag is flushed and set to 1 when not already
   1.
6. **Color** (skipped with flags bit 1): `color = (vm+0x35c bit 15 set ?
   vm+0x300 : vm+0x2fc)`; when owner+0x73245c is nonzero the color is
   modulated per channel through the 0x4423c0 `(a*b)>>7` clamp helper with
   the owner's +0x732458..+0x73245b factors (identical to
   `ModulateColor`), and all four vertex colors are published (0x4978d0 /
   0x4978ec / 0x497908 / 0x497924).
7. **Submission**: the glyph render-state transition 0x4425a0 (blend mode
   cache +0x3ada68, sampler mode cache +0x3ada6e, ++owner+0x54) and the
   quad appended to the owner's vertex buffer as two triangles
   (0x442fe0: 6 vertices via the +0x72dacc cursor, +++0x3adac8 count).

## Status

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` passes. CSV row
appended for 0x00442ad0.
