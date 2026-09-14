# TH10 0x0042a200 — Replay View Record Loader

Module: `src/ReplayViewLoad.cpp/.hpp`.

Native ESI/stdcall mix; reconstructed as
`i32 LoadReplayViewRecord(void *view, const char *file_name)`. Called by
the replay screens `0x00428f60` (twice) and `0x004296f0`.

## Behavior

1. Copies `file_name` byte-by-byte (including the terminator) to
   `view + 0x1d4`.
2. When `DAT_00474cd0` bit `0x20` is set (direct-view mode): the loader
   boundary `0x0044b360` returns the whole mounted buffer into `view+0x14`
   and the container header starts at `+36`. The branch here is a boundary:
   the native passes the raw name with a 4-byte stack slot and a zero.
3. Otherwise: `sprintf(path, "replay/%s", file_name)`, then
   `0x44b4d0` (exists) must succeed and `0x44b6b0` (open) must return 0 —
   otherwise the function returns **-1**. `0x44b790` hands out the header
   block (stored to `view+0x14`); the container must start with `"t10r"`
   (`0x72303174`) and version word `5` at `+4`, otherwise `0x44b7e0` runs
   and **-1** is returned. A second `0x44b790` block is the packed image;
   `0x44b7e0` closes it.
4. `malloc(*(header+0x20))` → `view+0x1c0`. The packed image
   (`*(header+0x1c)` bytes) receives **two** `0x0044b0d0` XOR stream passes
   in sequence — key step `0xe1` with block size `0x400`, then key step
   `0x7a` with block size `0x80`, both spans equal to the packed size —
   before the shared `0x00435dc0` LZSS decoder writes the body. Both
   transforms are the semantic bodies from `src/PackedArchive.cpp`.
5. `view+0x18` = body cursor. The stage count at body `+0x4c` clamps to 6
   when it reaches 8 (the native only tests `>= 8`, so a count of 7 stays
   7). For each entry (base `body+100`, fixed part 452 bytes, then
   `*(entry+8)` extra bytes): the first u16 is the stage slot and the view
   records, at stride 0x24 per slot:
   - `+0xa0 + 36*slot = entry + 452` (fixed part),
   - `+0xa8 + 36*slot = entry + 452 + 6 * *(u32*)(entry+4)` (extra data),
   - `+0xb0 + 36*slot = entry` (raw entry).
6. In the non-direct path the packed block is `free`d (only when
   `DAT_00474cd0` bit `0x20` is clear and the block is non-null). Returns
   0.

## Native quirks preserved

- The `>= 8 → 6` clamp is not a min(): a stage count of exactly 7 loads
  all seven entries.
- The two XOR passes run back-to-back over the same buffer; the second
  pass transforms the already-transformed bytes.
- No null checks on `malloc` or on the direct-mode buffer before the
  header reads (the direct branch is guarded by the caller's flags).
