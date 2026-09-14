# Score-Screen Renderers (TH10 0x00431410 / 0x004329f0 / 0x00431ba0 / 0x00433230)

Implemented in `src/ScoreScreenRenderers.cpp/.hpp` as `RunManagerDrawBody9`,
`RunManagerDrawBodyB`, `RunManagerDrawBodyC` and `RunManagerDrawBodyF` (names
match the extern declarations in `TitleGameManagerLifecycle.cpp`).

## Dispatch context

The game-manager draw controller 0x0042d260 (dispatcher in
`TitleGameManagerLifecycle.cpp`) enters with the manager in EAX, copies it to
EDI and jumps through the table at 0x0042d2b4 over `manager+0x1c - 9`:

| Case | State | Target | Native ABI |
| ---- | ----- | ------ | ---------- |
| 0 | 0x9 | 0x00431410 | manager in EDI, plain `retn` (no stack arg) |
| 2 | 0xB | 0x004329f0 | manager in EDI, plain `retn` (no stack arg) |
| 3 | 0xC | 0x00431ba0 | `push edi` + stdcall, `retn 4` |
| 6 | 0xF | 0x00433230 | `push edi` + stdcall, `retn 4` |
| 7 | 0x10 | 0x00433b30 | `RunManagerDrawBody10` (ScoreFileFormats.cpp) |
| 1/4/5 | 0xA/0xE/0xF | none | return 1 |

All four bodies return 1 in EAX. Only 0x00431410 additionally requires the
sub-state at `manager+0x20` to be 2 or 3; the others gate internally
(0x004329f0 and 0x00433230 on sub-state == 2, 0x00431ba0 on sub-states
2 and 4).

## Shared record addressing

Every renderer reads the 24-byte high-score payload described by
`InsertScoreRecordEdi` (score `+0x10`, `DAT_00474c7c` byte `+0x14`,
`DAT_00474c90` byte `+0x15`, 9-byte name `+0x16`, `time_t` `+0x20`, slow-rate
float `+0x24`) through a base pointer `R` that sits eight bytes below the
payload start, so fields appear at `R+0x18` (score), `R+0x1c`/`R+0x1d` (the
two byte fields), `R+0x1e` (name), `R+0x28` (time) and `R+0x2c` (slow rate).
A record counts as occupied when the time dword at `R+0x28` is non-zero.

Record base per renderer:

- 0x004329f0: `save + 0x437c * (mgr+0x24) + 0xf0 * (mgr+0xfc) + 0x18 * i`
  (menu shot-type cursor selects the 0x437c block, menu difficulty cursor the
  ten-record run).
- 0x00433230: `save + 0x437c * (DAT_00474c6c + 3 * DAT_00474c68) +
  0xf0 * DAT_00474c74 + 0x18 * i` (global shot-type/character/difficulty
  selectors).
- 0x00431410 reads the *clear pairs* instead: score dword at `+0x4dc` and
  cleared byte at `+0x4e1`, eight bytes per (difficulty, stage) pair keyed by
  the same 0x437c block (`48 * difficulty + 8 * stage`, stage 1..6).

Date/step constants: 18.0 `flt_470C0C`, 15.0 `flt_470C08`, 9.0
`flt_470C10`, 0.1 `flt_470C18`, 10.0 `flt_470C1C`, 160.0 `flt_470C24`,
80.0 `flt_470C28`, 16.0 `flt_470B48`. Stage-name table `off_474718`
("X", "test  ", "Stage 1".."Stage 6", "Extra  "), always indexed from
`0x47471c`, i.e. element `[stage_byte + 1]` with the signed byte — a negative
byte indexes before the table exactly as the native code does.

## 0x00431410 — `RunManagerDrawBody9` (state-9 separator rows)

Requires `manager+0x20` in {2, 3}, sets ASCII `text_mode` (+0x8988) to 1 and
returns early (color `+0x8974` = 0xFFFFFFFF, `text_mode` = 0) when the
`+0x2b4` timer is below 10 and the sub-state is not 3. Rows are drawn when
the timer reached 10 or sub-state is 3.

- Position: x = 296 when `DAT_00474c68` (character) is non-zero, else 168;
  y starts at 152 and steps 18 per row; z = 0.
- Six rows (stage 1..6), row colors:
  - not selected (`mgr+0x24 != stage-1`) → `0xFF808080`;
  - selected, pair byte cleared → `0xFFDFDFDF`;
  - selected and cleared → yellow `0xFFFFFF00`, except when sub-state is 3
    and the signed-mod-4 phase of `mgr+0x2b4` (native
    `and 0x80000003 / dec / or 0xFFFFFFFC / inc` idiom) is >= 2, which
    renders `0xFF000000` (blink).
- Cleared rows render `"%s  %.8d0"` (0x46effc) with the stored dword — the
  score without its trailing zero digit, the 0 printed literally. Uncleared
  rows render `"%s  ---------"` (0x46f008).
- Tail resets color and `text_mode`.

## 0x004329f0 — `RunManagerDrawBodyB` (extra-menu score list)

Sub-state 2 only. `text_mode` = 1. Rows at x = 48, y = 160 stepping 18.

- When `mgr+0x1d4 == 0`: ten rows (row number 1..10) colored
  `0xFFvvvvFF` with `v = 0xff - 0x10 * (row-1)` (alpha and blue channel stay
  0xff through the native OR of `0xFF0000FF`). Populated records use format
  0x46ee84 `"%2d  %s  %9ld%d  %.4d/%.2d/%.2d %.2d:%.2d  %s  %2.1f%%"` with
  args (row, name, score, `R+0x1d` byte, tm_year+1900, tm_mon+1, tm_mday,
  tm_hour, tm_min, `g_ScoreStageNames[R+0x1c byte + 1]`, slow double) via
  the 0x452baa localtime wrapper; empty records use 0x46ee50
  `"%2d  %s  %9ld%d  ----/--/-- --:--  Stage -  ---%%"` (score and digit byte
  printed even for untouched records).
- `mgr+0x1d4 != 0` skips straight to the totals.

Totals block at x = 324 (color reset to 0xFFFFFFFF first):

- y = 378: `"    %5d"` with the dword at `save + block + 0x4c8`.
- y = 396: `"%3d:%.2d:%.2d"` for the time dword at `+0x4cc`. The native
  signed magic-division chain (/60, /3600, /216000 — constants 0x88888889
  shift 5, 0x91a2b3c5 shift 11, 0x9b583739 shift 17) prints
  `t/216000 : (t/3600) % 60 : (t/60) % 60`; the middle field is hours % 60,
  not minutes or seconds — quirk preserved.
- y = 412: `"    %5d"` with the dword at `+0x4d0 + 4 * difficulty`.

Tail resets color and `text_mode`.

## 0x00431ba0 — `RunManagerDrawBodyC` (score-file viewer)

Sub-state 2 — replay list, 25 rows at x = 58, y = 80 stepping 15
(`flt_470C08`), colors from the shared `EntryColor` idiom
(`mgr+0x24` selected → `0xFFFFFF00`, else `0xFF808080`). Each `mgr+0x59e4`
slot holds a record whose `+0x18` points at a replay header (+0x00 name,
+0x0c time, +0x48 slow float, +0x50 chara, +0x54 shot, +0x58 rank,
+0x5c stage); populated rows use format 0x46efb0
`"No.%.2d %s %.2d/%.2d/%.2d %.2d:%.2d %s %s %s %2.1f%%"` (identical to
`RunManagerDrawBody10`'s `AppendScoreLine`, year printed as tm_year % 100),
empty rows format 0x46ef74
`"No.%.2d -------- --/--/-- --:-- ------- ------- --- ---%%"`.

Sub-state 4 — detail:

- Selected entry = `mgr+0x59dc`; the slot is dereferenced without a null
  check (quirk preserved). Header from the slot's `+0x18`.
- Line position x = 80; y = (10 - `mgr+0x2B8` float) * (15 * selected) * 0.1
  + 80 (`flt_470C1C`/`flt_470C18`/`flt_470C28`) while `mgr+0x2b4 < 10`,
  else 80. Rendered with format 0x46efb0, entry number selected+1.
- While the timer is below 10 the body stops after the detail line (tail:
  color 0xFFFFFFFF, `text_mode` 0).
- Otherwise seven per-stage rows at x = 220, y = 128 stepping 18, colors
  `EntryColor(mgr+0x24, stage-1)`. Sub-records live inside the record at
  `0x24 + 0x24 * (stage-1)`:
  - played flag `+0xb0 == 0` → `"%s  ---------"` (0x46f008);
  - stage < 6 and the pointer at `+0xd4` non-null → `"%s  %.8d%d"`
    (0x46ef68) with the pointed-to object's `+0xc` / `+0x1b4` dwords;
  - otherwise the header totals `header+0x10` / `header+0x60` in the same
    format (stages 6 and 7 always take this path).

Common tail resets color and `text_mode`; other sub-states return 1 without
touching the ASCII manager.

## 0x00433230 — `RunManagerDrawBodyF` (score list + name entry)

Sub-state 2 only. Rows at x = 48, y = 160 stepping 18 for the ten records of
`0xf0 * DAT_00474c74` in the current block.

- While `mgr+0x58ec != 0` (no pending name entry) the whole list fades with
  the `0xFFvvvvFF` scheme; otherwise the selected row (`mgr+0x24`) is white
  `0xFFFFFFFF` and the others `0xFF404040` (native
  `sete/dec/and 0xFF404041/dec` idiom). Formats 0x46ee84 / 0x46ee50 as in
  0x004329f0 (entry numbers 1..10, incremented before the push).
- `mgr+0x58ec != 0` then returns immediately — native quirk: the common tail
  is skipped, so `color` stays at the last row tint and `text_mode` stays 1.
- Name entry (when `mgr+0x58ec == 0`):
  - the 9-byte buffer at `mgr+0x58dc` renders white at x = 84 on the cursor
    row, y = 18 * (`mgr+0x24`) + 160;
  - the caret `"_"` renders yellow at x = 9 * (`mgr+0x58e8`) + 84, minus 9
    when the cursor is 8 (past the last character);
  - the alphabet template (pointer variable `off_4746d8`, 76 characters)
    renders as a 13-column grid from (212, 360) stepping 18 per cell,
    resetting x and stepping y by 16 (`flt_470B48`) on every 13th cell
    (index % 13 == 12). Cell colors use `mgr+0x58f4` with the shared
    `EntryColor` idiom. The last three cells are forced to the bytes
    0x81 / 0x7f / 0x80 (index == length-3 → 0x81; the final two use the
    native `0x7f + (index != length - 2)`). Note `RunManagerDrawBody10`
    (0x00433b30) documents 0x81/0x80/0x81 for its own grid; this body's
    disassembly yields 0x81/0x7f/0x80 as written here.
  - An empty template skips the grid but still hits the color reset.
- Tail (editor path only): color = 0xFFFFFFFF. `text_mode` is left at 1 —
  this body never clears it (quirk; the dispatcher's other bodies do).

## Boundaries and shared globals

- `localtime` — CRT boundary (the native calls the 0x452baa validated
  wrapper; the reconstruction uses CRT `localtime` like the sibling
  ScoreFileFormats module, including the unchecked `tm` dereference).
- `AddFormatText` (0x00401630) is the object-matched ASCII variadic entry;
  the native callers pass the Float3 position in EBX at the register-ABI
  boundary, which the C++ `const Float3&` parameter models.
- Globals: `g_AsciiManagerHost` (DAT_004776E0), `g_ScoreSaveState`
  (DAT_0047783C), `g_PlayerCharacter` (DAT_00474C68), `g_PlayerShotType`
  (DAT_00474C6C), `g_CurrentDifficulty` (DAT_00474C74), `g_ReplayCharacterNames`
  (off_4746DC), `g_ReplayRankNames` (off_4746F4), `g_ReplayStageNames`
  (off_474744), `g_ScoreStageNames` (off_474718), `g_AlphabetTemplate`
  (off_4746D8).
