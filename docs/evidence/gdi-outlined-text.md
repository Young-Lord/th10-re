# GDI Outlined Text Renderer (TH10 0x437380)

Reconstruction: `src/GdiOutlinedText.cpp`
(`DrawGdiOutlinedTextEaxEdiStackAbi`).

## Native ABI

Usercall: EAX = owner object, EDI = NUL-terminated text (returns 0 early
when null), five stack arguments (ret 0x14): a1 anchor left, a2 anchor top,
a3 GDI object to select (the font the caller just created), a4 fill color,
a5 outline color (-1 disables the outline passes). Returns 1.

Call site 0x4372b0: thiscall wrapper that formats the text through
0x436890, deletes the previous font (IAT 0x466030), creates a new one via
IAT 0x466034 (CreateFontA family, 0x30 charset slot, face-name pointer),
then calls this renderer with EAX = the owner, EDI = text.

## Owner fields

- `+0x104` text extent width, `+0x108` text extent height (paired with the
  a1/a2 anchor for the right/bottom clip edges)
- `+0x114` memory DC (HDC)

## Body

1. `SelectObject(dc, a3)`; the previous object is saved for the final
   restore (the native stashes it in a slot that is disjoint from the
   rect scratch area).
2. `SetBkMode(dc, TRANSPARENT)` (constant 1, IAT 0x46601c).
3. Outline color path (a5 != -1): `SetTextColor(dc, a5)` (IAT 0x466018),
   then SIX `DrawTextA(dc, text, -1, rect, 0)` passes (IAT 0x466268) in
   that color: the base rect `{a1, a2, a1+w-2, a2+h-2}` plus five copies
   whose rectangles the native derives with the same +/-2 pixel shift
   arithmetic (an outline/bold effect by repeated draws).
4. `SetTextColor(dc, a4)`, then the final pass over the expanded rect
   `{a1+1, a2+1, a1+w+1, a2+h+1}`.
5. `SelectObject(dc, previous)` restore; return 1.

With a5 == -1 all six outline passes are skipped (the native jumps
straight to the fill-color block), leaving a single fill draw.

## Verification notes

- Disassembly 0x437380-0x43758a (525 bytes), 0x88 bytes of locals plus a
  five-arg stdcall-style epilogue (`ret 0x14`).
- Import resolution via the import directory: 0x466018 = GDI32
  SetTextColor, 0x46601c = GDI32 SetBkMode, 0x46602c = GDI32 SelectObject,
  0x466268 = USER32 DrawTextA (the `ebx` indirect calls in the body).
- Caveat: the exact per-field rect values of the six passes were derived
  from the esp-relative scratch stores; the base/expanded rect arithmetic
  ({a1,a2,a1+w-2,a2+h-2} and the +1 expansion) is solid, while the
  distribution of the +/-2 shifts across the five outline rects is
  reconstructed as (2,0), (0,2), (-2,0), (0,-2) offsets around the base
  plus one unshifted base pass. Glyph placement under format 0
  (DT_LEFT|DT_TOP) only depends on left/top, so visual behavior is
  insensitive to the right/bottom clip edges.
