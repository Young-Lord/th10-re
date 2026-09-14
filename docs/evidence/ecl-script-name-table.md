# Script-Name Table Registration (TH10 0x00450220)

Reconstruction: `src/EclScriptNameTable.cpp`
(`RegisterScriptFileNamesThisStackAbi`). This is the producer half of the
0x450470 `ResolveScriptTableIndexEaxAbi` boundary (declared in
EclScriptVm.hpp / EclScriptLibrary.cpp / EclEasedTransforms.cpp): it fills
the name table that 0x450470 searches.

## Native ABI

`__thiscall` ECX = the script-name registry object, one stack argument =
loaded file block (ret 4). Returns the file slot index used, or -1 when
validation fails (the slot pointer is nulled in that case).

## Registry object layout

- `+0x00` vtable
- `+0x04` count of registered file slots
- `+0x08` total name count
- `+0x0c + i*4` loaded file block pointers
- `+0x8c` name table: `{char *string, char *name}` pairs (8 bytes each),
  kept sorted ascending by string bytes

## SCPT v1 block layout

- `+0x00` magic `SCPT` (0x54504353) — required
- `+0x04` u16 version 1 — required (checked as a 16-bit compare)
- `+0x06` u16 name count
- `+0x10` u16 string count
- `+0x24` name-count dwords: big-endian offsets of the name strings
- then the string bytes (NUL terminated); the string cursor starts after
  BOTH tables (`file + 0x24 + name_count*4 + string_count*4`), matching
  the native `lea edi,[ecx+eax*1+0x24]` + `lea edi,[edi+eax*4]`

## Body

1. Stores the argument into the file slot at `[+0x04]`, re-reads it, and
   validates magic and version; on failure the slot is nulled and -1 is
   returned.
2. Extends the name count by the block's string count (stored into `+0x08`)
   and mallocs `new_total * 8` bytes through 0x452706 (CRT heap wrapper;
   the native mallocs fresh memory rather than reallocating — the old table
   is copied and then freed through 0x452422).
3. First registration (old table null): entries are appended in file order;
   each entry's `+4` is `file + be32(offset_table[i])` and `+0` is the
   running string cursor, advanced past each string via the inlined strlen.
4. Re-registration: `memcpy` of the old entries, free of the old block,
   then each new string is inserted at its sorted position (inline strcmp
   with the sbb sign idiom; the scan advances while
   `strcmp(new, existing) > 0`) with a one-entry (8-byte) tail shift.
5. The file-slot count at `+0x04` advances, and when the stored block's
   name count (`word +6`) is non-zero the registry's vtable slot 1 is
   invoked (thiscall) with `file + 0x24` (the name-offset table).
6. Returns the slot index.

## Verification notes

- Disassembly 0x450220-0x45046a (589 bytes), sits immediately before
  0x450470 in the binary.
- The stack-arg `esp` tracking confirms the two-table string base: the
  offset table has `word[6]` dwords and the string cursor additionally
  skips `word[0x10] * 4` bytes.
- Allocation helpers: 0x452706 = malloc over the CRT heap handle at
  0x477364 (cdecl, caller cleans); 0x452422 = free; 0x452493 = the
  `NativeAlloc` operator-new wrapper already modeled in EclScriptVm.cpp.
