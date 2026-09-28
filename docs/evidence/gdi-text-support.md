# GDI text support (0x00436890-0x00437d10)

Module: `src/GdiTextSupport.cpp`

`0x00436890` builds the 0x2c-byte font-spec record the callers place on
their stack: `{HFONT result +0x00, width +0x04, height +0x08 (16),
string object at +0x0c (SSO buffer +0x10, size +0x20, capacity +0x24),
weight 600 +0x2c}`; the face name is a 13-byte constant at 0x0046f36c
assigned through `StringAssignRangeEcxAbi`.

`0x004372a0` (native ESI = owner, ECX = text, stack ret 0x14 = anchor
left/top, height, fill, outline) constructs the font with `CreateFontA`
(charset 1, pitch/family 0x30) and draws through
`DrawGdiOutlinedTextEaxEdiStackAbi` (0x00437380, registered), then
destroys the font and frees a heap face name (capacity >= 0x10).

`0x00436a30`/`0x004369e0` release/reset the GDI surface record (memory DC
+0x114, selected object +0x118, bitmap +0x11c, dimensions +0x100..0x110);
`0x00437d10` releases the shared record `byte_474cb0` and the fifteen
cached font handles 0x004918a0..0x00491868.
