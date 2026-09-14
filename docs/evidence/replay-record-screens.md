# Replay Record Screens — 0x00422660 / 0x004224c0

Module: `src/ReplayRecordScreens.cpp/.hpp`.

## Subsystem

Draw sibling of the calculation-record dispatcher `0x004223f0` family: the
body reads the same 0x3ac-byte record as `0x004236f0` / `0x00422c80`
(mode at +4, timer +0x14, cursor +0x24, replay-present gate +0x1e8, 25
parsed replay records +0x1ec, typed name +0x2b4). It is reached through
the thin `push ecx` adapter at `0x00422aa0` (`call 0x422660; ret`) and
sets the ASCII manager state (`DAT_004776e0`): `text_mode` (+0x8988) = 1
on entry, `color` (+0x8974) = `0xffffffff` and `text_mode` = 0 on the
common epilogue, returning 1.

Dispatch on `mode-10` over the byte table at `0x422a7c` (jump table
`0x422a6c`): modes 10 and 17 → the replay list, 11 and 18 → the selected
replay detail, 12 and 19 → the score ranking, 13..16 (and out-of-range
values) → the bare epilogue.

## Record/entry layouts

- Parsed replay record (`+0x1ec[25]`): `+0x18` points at a replay header
  with `+0x00` name, `+0x0c` `time_t`, `+0x48` slow-rate float, `+0x50`
  chara, `+0x54` shot, `+0x58` rank, `+0x5c` stage.
- Score-save entry (`save + block*0x437c + 24*(row + 10*shot)`, save =
  `DAT_0047783c`, block = `3*DAT_00474c68 + DAT_00474c6c`): `+0x18`
  score, `+0x1c` stage byte, `+0x1d` byte, `+0x1e` 9-byte name, `+0x28`
  `time_t` (a zero time marks the empty record). Consistent with
  `InsertScoreRecordEdi` (payload at +0x10 of the table that already
  includes the +8).

## Mode 10/17 — replay list

25 rows at x = 48 (`0x42400000`), y = 64 stepping 15 (`flt_470c08`).
Row color is the branchless native sequence (`setnz/dec/and 0x7f7e80/
add 0xff808080`): the row selected by `+0x24` gets `0xffffff00`, others
`0xff808080`. Populated rows use
`"No.%.2d %s %.2d/%.2d/%.2d %s %s %s"` (0x46e19c) with `localtime`
fields (`tm_year % 100`, `tm_mon + 1`, `tm_mday`), the header name,
`off_4746dc[3*chara + shot]`, `off_474708[rank]` and
`off_474744[stage]`; empty rows print
`"No.%.2d ------------ --/--/-- ----- - St-"` (0x46e174).

## Mode 11/18 — selected replay detail

While the +0x14 timer is below 10 the detail line animates:
`y = (flt_470b4c - (cursor*flt_470c08 + flt_470bc8)) * timer * flt_470c18
+ (cursor*flt_470c08 + flt_470bc8)` (224.0 / 15.0 / 64.0 / 0.1); from
timer 10 the y settles at 224.0. The name-entry grid (0x4224c0) renders
at (102, y, 0). The detail line then reads the replay header through the
unchecked chain `DAT_00477838 -> +0x18` and prints
`"No.%.2d         %.2d/%.2d/%.2d %s %s %s"` (0x46e148; ten literal
spaces between the number and the date) with the slot number
(cursor+1), the header date, `off_4746dc[3*chara+shot]`,
`off_474708[rank]` and `off_474744[DAT_00474c7c]` at (48, y, 0).

## Mode 12/19 — score ranking

Prints `"            Score Ranking!!"` (0x46e12c, twelve leading
spaces) at (48, 64). The name-entry grid renders at (75, cursor*18 +
`flt_470c80`(96), 0) only while the +0x1e8 gate is clear; when the gate
is set the selected-row value is forced to -1 (no highlight). Ten rows
at x = 48, y = 96 stepping 18 (`flt_470c0c`) read the score-save block
described above (row index `row + 10*shot`): populated rows
(`[entry+0x28] != 0`) use `"%2d %s %.9ld%d %.2d/%.2d/%.2d %s"`
(0x46e108) with the row number, the entry name, the score printed as a
9-digit long, the signed +0x1d byte as `%d`, the date of
`localtime(entry+0x28)` and `off_47471c[signed entry[0x1c]]`; empty rows
use `"%2d %s %.9ld%d --/--/-- Stage -"` (0x46e0e8). Row color as above.

## 0x004224c0 — name-entry grid (native `retn 0x10`: record, x, y, z)

Draws the typed name (`record+0x2b4`) at the caller position, then the
caret `"_"` (0x46e1c4) in `0xffffff00` at `x + 9*cursor` — pulled back
one cell (9.0, `flt_470c10`) when the name cursor (`+0x1e0`) equals 8 —
then restores `0xffffffff` and renders the alphabet template
(`off_4746d8`) as a fixed 13-column grid starting at (112, 320)
(`0x42e00000`/`0x43a00000`), stepping x by 18 (`flt_470c0c`) and, every
13th cell (`index % 13 == 12`), resetting x and stepping y by 16
(`flt_470b48`). The last three cells are the space/delete/END markers
0x81 / 0x7f / 0x80 (native `0x7f + (index != length-2)` for the final
two). Cell color uses the grid cursor (`record+0xfc`) with the same
branchless trick. The same renderer is inlined by the manager draw body
`0x00433b30` (see `docs/evidence/score-file-formats.md`).

## Boundaries kept

`0x401630` `AsciiManager::AddFormatText` (already object-matched),
`localtime`, `0x452baa` CRT thunk, and the name-table pointer arrays
`off_4746d8`/`off_4746dc`/`off_474708`/`off_47471c`/`off_474744` (read
in place from .rdata).

## Verification

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` — 0 errors.
