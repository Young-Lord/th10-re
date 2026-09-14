# Score File Formats — Record Insert, Section Decoder, Score-Screen Draw

Implemented in `src/ScoreFileFormats.cpp/.hpp`.

## 0x00421fa0 — `InsertScoreRecordEdi(score_table)`

Native `__usercall` with the score table in EDI. Callers 0x00423570 (post-run
flow) and 0x00432cb0 (name-entry commit) compute

```text
EDI = (value of DAT_0047783c) + 8 + 0x437c * (DAT_00474C6C + 3 * DAT_00474C68)
```

i.e. one 0x437c-byte stage slot of the score-save state, holding the ten
24-byte high-score records of one character at 24-byte strides. Each record's
24-byte payload sits at slot +0x10:

| Offset | Meaning |
| ------ | ------- |
| `+0x10` | score dword |
| `+0x14` | byte from `DAT_00474C7C` |
| `+0x15` | byte from `DAT_00474C90` |
| `+0x16..+0x1e` | 9-byte name (written as 8 spaces + NUL, `asc_46E354`) |
| `+0x20` | `time_t` |
| `+0x24` | slow-rate float |

Behavior:

1. Scans the ten stored scores (`DAT_00474C74` selects the character block)
   against the current score `DAT_00474C44` with signed compares; the loop
   breaks at the first stored score `<=` the current one, otherwise returns
   -1.
2. Shifts records `rank..8` down one slot — only the 24-byte `+0x10` payload
   region moves; the 16 bytes ahead of each record are untouched.
3. Writes the new record (score reloaded from `DAT_00474C44`, the two byte
   globals, blank name, `time()`, and
   `100.0 - *(double*)(DAT_00477708+0x24) / *(double*)(DAT_00477708+0x2c) * 100.0`
   stored as float — unchecked deref of `DAT_00477708`, quirk).
4. Returns the rank 0..9 (the name-entry flow uses it as the initial grid
   cursor). The wired call site in `src/GameManagerStateBodies.cpp`
   (`RunManagerStateBodyF`) previously declared this address as a
   zero-argument `QueryNameInputInitialCursor(void)`; it now passes the
   natively computed table pointer.

## 0x00434dd0 — `DecodePackedSectionEcxStackAbi(name, section_list, out_buffer)`

Native: ECX = section name, stack0 = section-list object, stack1 = output
buffer, `retn 8`. Callers: 0x00435800 and `LoadScoreDisplayRecords`
(0x0044b360).

- Section-list object: `+0x00` entry array, `+0x04` entry count,
  `+0x0c` reader object (vtable `+0x18` = seek-by-offset
  `(self, offset, 0) -> bool`, vtable `+0x08` = read
  `(self, buffer, size) -> bool`). The reader-null check happens before
  everything else.
- 0x00434f30 (`FindSectionEntryByNameEaxEbxAbi`): `stricmp` linear search
  over 0x10-byte entries whose first dword is the name; returns the entry or
  0.
- Entry fields: `+0x00` name, `+0x04` start offset, `+0x08` declared
  (unpacked) size. The packed size is read as `entry[+0x14] - entry[+4]` —
  because the lookup strides 0x10 bytes, `+0x14` is the *next* entry's start
  offset, so sections are contiguous and the last entry reads one dword past
  the table (quirk preserved).
- Buffer policy: the caller's `out_buffer` is reused only when the packed
  and declared sizes are equal; otherwise (or with a null buffer) a
  `malloc(packed_size)` scratch block is used.
- Seek/read failure frees the block — including when it aliases the
  caller's `out_buffer` (quirk preserved) — and returns 0.
- The loaded bytes are unscrambled through `TransformPackedBytesInPlace`
  (0x0044b0d0) with the `DAT_00474bd8` key record selected by the name byte
  sum (computed in CL starting from the terminator's 0), then decompressed
  through `DecompressPackedBytes` (0x00435dc0) when the sizes differ. The
  scratch block is freed when it is not the caller's buffer.

## 0x00433b30 — `RunManagerDrawBody10(manager)`

Native stdcall stack argument, `retn 4`; always returns 1. Dispatches on
`manager+0x20` (sub-state):

- **2 — replay list.** Sets ASCII `text_mode` (`+0x8988`) to 1 and renders
  25 rows at x = 56, y = 80 stepping 15 (`flt_470C08`). Row color comes from
  the branchless native sequence (`setnz/dec/and 0x7F7E80/add 0xFF808080`):
  the row selected by `manager+0x24` gets `0xFF808080`, others
  `0xFFFFFF00`. Each `manager+0x59E4` slot holds a record whose `+0x18`
  points at a replay header (`+0x00` name, `+0x0c` time, `+0x48` slow float,
  `+0x50` chara, `+0x54` shot, `+0x58` rank, `+0x5c` final stage);
  `off_474744[stage]` supplies the stage name. A null slot renders
  `"No.%.2d -------- --/--/-- --:-- ------- ------- --- ---%%"`. Populated
  rows use
  `"No.%.2d %s %.2d/%.2d/%.2d %.2d:%.2d %s %s %s %2.1f%%"` with
  `localtime(header+0x0c)` fields (`tm_year % 100`, `tm_mon + 1`, mday,
  hour, min), the header name, `off_4746DC[3*chara + shot]`,
  `off_4746F4[rank]`, the stage name and the slow-rate float promoted to
  double.
- **3 — score detail.** Draws the selected line (`manager+0x59DC` + 1) with
  a blank name and `off_474764` ("All") as the stage name; the header comes
  from the unchecked chain `DAT_00477838 -> +0x18`. The line's y position
  animates while `manager+0x2B4 < 10`:
  `y = (10.0 - *(float*)(mgr+0x2B8)) * (15*sel + 0x50 - 240.0) * 0.1 + 240.0`
  (`flt_470C1C`, `flt_470C18`, `flt_470BFC`); x = 56. Once
  `manager+0x2B4 >= 10` the comment editor renders: the 8-character name
  buffer (`+0x58DC`) in white at (112, 240), the caret `"_"` in
  `0xFFFFFF00` at x = `9 * cursor (+0x58E8) + 112` (minus 9 when cursor == 8),
  then the alphabet template `off_4746D8` as a 13-column grid starting at
  (212, 360), stepping x by 18 (`flt_470C0C`) and, every 13th cell
  (`index % 13 == 12`), resetting x and stepping y by 16 (`flt_470B48`).
  Cell color uses `manager+0x58F4` with the same branchless trick. The last
  three cells are forced to bytes 0x81 / 0x80 / 0x81 (length-3, length-2,
  length-1 respectively — the native's `0x7f + (index != length-2)` for the
  final two).
- Common tail: color `+0x8974` = `0xFFFFFFFF`, `text_mode +0x8988` = 0.
  Sub-states other than 2/3 return without touching the ASCII manager.

All text goes through `AsciiManager::AddFormatText` (0x00401630, already
object-matched); the native callers pass the Float3 position in EBX at the
register-ABI boundary, which the C++ method signature models directly.

## Boundaries

- `_time` / `_localtime` / `malloc` / `free` / `stricmp` — CRT platform
  boundaries (the C++ build maps `stricmp` to POSIX `strcasecmp`).
- `g_AsciiManagerHost` (DAT_004776E0), `g_ReplayCharacterNames`
  (off_4746DC), `g_ReplayRankNames` (off_4746F4), `g_ReplayStageNames`
  (off_474744), `g_ScoreStageAllName` (off_474764) — extern globals shared
  with the other reconstructed modules.
