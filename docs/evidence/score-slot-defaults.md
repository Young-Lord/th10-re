# Score save default records (0x0042acb0 / 0x0042add0)

Reconstruction in `src/ScoreFileLoad.cpp` (the two former extern
boundaries consumed by `InitializeScoreSaveStateEaxAbi`). All addresses
were read from the raw objdump listing of `resources/th10.exe`.

## 0x0042acb0 InitializeScoreStageSlotStackStackAbi

Native ABI: stack argument = slot, `ret 4`. Called seven times from
0x0042ae60 for `state+8 + i*0x437c`.

1. `rep stos` zeroes 0x10df dwords (0x437c bytes).
2. Word +0 = 0x5243 ("CR"), word +2 = 0, dword +8 = 0x437c.
3. Thirty 0x18-byte score entries from +0x14. The native keeps one
   cursor (`eax`) across five outer iterations that each re-seed the
   score at 0xf4240*10 = 1000000 and write six entries stepping down by
   0x186a0 (100000). Per entry: the dword *before* the record (cursor-4)
   holds the score (entry 0's score lands at +0x10), byte +0 = 1 (stage),
   byte +1 = 0, then the 9 bytes at 0x46e858 ("--------\0") are copied as
   dword+dword+byte into +2..+0xa, and dword +0xc = 0. The 4-byte gap
   between the trailing dword and the next score stays zero from the
   initial wipe.
4. Twenty-two 0x2d0-byte spell-card entries at +0x628 (loop index 1, 6,
   ... 106 stepping 5). Per entry: dword at cursor-4 = 0, then
   `movsx`-signed bytes read from the repeating `02 03 00 01` pattern at
   0x4743c0 feed three condition words:
   - [+0] and [+0x90] = byte[0x4743c0 + i1] (i1 = 1, 6, ...),
     [+0x8c] = i1;
   - [+0x11c] = i2 - 1, [+0x120] = byte[0x4743bf + i2] (i2 = 3, 8, ...;
     the native computes the base `0x4743c2 - 3` once before the loop, so
     the reads walk pattern indices 2, 7, 12, ...),
     [+0x1ac] = i2, [+0x1b0] = byte[0x4743c0 + i2];
   - [+0x23c] = i3 + 4, [+0x240] = byte[0x4743c4 + i3] (i3 = 0, 5, ...).
   No PRNG is involved in this function.

## 0x0042add0 InitializeClearDataRegionEdxAbi

Native ABI: EDX = region (0x448 bytes at state+0x1d86c), plain ret.

1. `rep stos` zeroes 0x112 dwords (0x448 bytes).
2. Word +2 = 0 first, then word +0 = 0x5453 ("ST"), dword +8 = 0x448.
3. Nine bytes from 0x46e354 ("        \0", eight spaces) are copied into
   +0xc..+0x14 as dword+dword+byte.
4. 0x200 iterations, each: read the dword at DAT_004918b0 (the LCG state
   u16 plus the neighbouring u16 at +2 — the dword read drags both in),
   `full = (dword ^ 0x9630) - 0x6553`, `rotated = (full << 2) +
   (low16(full) >> 14)`, store `rotated & 0xffff` back into the state
   word, increment the counter dword at +0x4918b4, and store the same
   rotated word into the output stream at +0x46, +0x48, ... The counter
   increment is a plain dword `inc`.

Both bodies are implemented as `InitializeScoreStageSlotStackAbi` /
`InitializeClearDataRegionEdxAbi` with a file-local `ScorePrngStep()`
sharing the exact LCG semantics (see also `LcgDrawRaw32Duplicated` in
src/EnemyDeathEffects.cpp for the sibling draw used by 0x40c9d0).
