# Text scan helpers

Covers TH10 0x0044c000 (including the trailing chunk bodies at
0x0044c070 / 0x0044c080 / 0x0044c0b0) — module
`src/TextScanHelpers.{hpp,cpp}`. None of these carry direct
cross-references in the retail binary; the hint-text reader
(HintTextFile.cpp) uses an equivalent inlined form, so these are
reconstructed as standalone utilities from ZUN's text library.

## 0x0044c000 CopyShiftJisTextLineEsiEdxStackAbi

Native ESI = destination, EDX = source cursor, stack = max bytes (ret 4);
returns the advanced source cursor. Semantics:

1. The whole destination window is zero-filled up front (rep stos), so a
   short line is NUL-terminated by the fill.
2. Copy loop, entered once through the shared head and then via the
   0x44c023 back-edge: the current byte stops the line on `'\n'`, `'\r'`
   or a zero budget; otherwise it is stored and the *same* byte is
   re-tested as a Shift-JIS lead (0x81-0x9f, 0xe0-0xfc), in which case
   the trail byte is consumed and copied as well.
3. The budget decrements once per consumed byte — and a second time for
   a trail byte. The loop head only re-tests `budget != 0` before the
   next character, so a line ending on a double-byte lead with exactly
   one byte of budget wraps the counter to 0xffffffff; the copy still
   terminates because the next character is the line break. Preserved.
4. After the loop the trailing CR/LF run is consumed
   (`while (*src == '\n' || *src == '\r') ++src`).

## 0x0044c070 / 0x0044c080 / 0x0044c0b0 SumByteChecksumStride{1,2,4}

Native ECX = data, EDX = count; the sum accumulates in AL and is
zero-extended into AX on return (the high byte of AX is never
initialized — effectively a u8 result).

* Stride 1: every byte.
* Stride 2: the odd tail is dropped first (`count &= ~1`), then every
  even-index byte (indices 0, 2, ..., count-2).
* Stride 4: the tail is aligned to a multiple of four, then every fourth
  byte.

## Verification

Reference disassembly `build/reference/0044c000_sub_44C000.asm`; the
chunk carries no xrefs (checked via xref_query and immediate scans).
