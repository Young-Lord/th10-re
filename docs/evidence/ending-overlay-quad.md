# Ending Overlay Quad — 0x0043bfa0

Module: `src/EndingOverlayQuad.cpp/.hpp`
(`DrawEndingOverlayQuadStackAbi`). Native EDI = four-dword record, stack =
four scalars; `ret 0x10`. No static xrefs were found for the entry; it sits
between the MIDI cluster (0x43b110) and the full-screen rectangle callbacks
0x43c1a0+ (AsciiOverlayCallbacks.cpp) and follows the same state-reset
idiom, so it belongs to the ending overlay draw family.

## Flow

1. `esi = DAT_00491c10`, then `0x442f50`
   (`FlushRenderOwnerPendingVertices`).
2. The 0x50-byte XYZRHW|DIFFUSE vertex block (FVF 0x44, stride 0x14) is
   assembled on the stack by a compiler-shuffled store sequence
   (0x43bfaf..0x43c097). The literal final cell values are reproduced
   cell-by-cell, with `rec[i]` = the EDI record's dwords:

   | vertex | x | y | z | rhw | color |
   | ------ - | - | - | - | --- | ----- |
   | v0 | rec+8 | rec+0xc | 0.0f | rec+0 | rec+4 |
   | v1 | 0.0f | 1.0f | arg1 | rec+8 | rec+4 |
   | v2 | 0.0f | 1.0f | 1.0f | rec+0 | rec+3* |
   | v3 | 0.0f | 1.0f | arg3 | rec+8 | 1.0f* |

   `*` = the raw bit pattern stored (rec+0xc dword and 0x3f800000
   respectively, read as floats by D3D). Two native quirks are preserved:
   arg2 is stored into a cell that a 1.0f write overwrites (dead), and
   arg4 lands in a tail local past the drawn vertices.
3. Texture-stage/render-state block (device vtable indices per the project
   convention — 67 = SetTextureStageState, 57 = SetRenderState, 89 =
   SetFVF, 83 = DrawPrimitiveUP):
   - `(0, 4, 2)`, `(0, 1, 2)`, `(0, 5, 0)`, `(0, 2, 0)` — texture stages
     switched to the untextured current-diffuse selection,
   - `SetRenderState(0x14, 6)` (D3DRS_DESTBLEND = INVSRCALPHA),
   - `SetFVF(0x44)`,
   - `DrawPrimitiveUP(5 /*triangle strip*/, 2, vertices, 0x14)`.
4. Render-owner cache reset (DAT_00491c10, order preserved): byte
   +0x3ada6a = 0xff, dword +0x3ada70 = 0, dword +0x3ada64 = 0, byte
   +0x3ada69 = 0xff, byte +0x3ada68 = 3, byte +0x3ada6b = 0xff.
5. Restore: a stage-state call that pushes only the stage argument (native
   quirk — one argument after `this`; modeled with its own slot type),
   then `(0, 1, 4)`, `(0, 5, 2)`, `(0, 2, 2)`.

## Status

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` passes. CSV row
appended for 0x0043bfa0.
