# Save Run High-Score Entry — 0x00423570

Implemented as `SaveRunHighScoreEntry` in
`src/SaveRunHighScoreEntry.cpp/.hpp`. Native stdcall `ret 4`; the 0x3ac-byte
scheduler record arrives as the stack argument. Called by the post-run
replay-save menu (0x004236f0, mode 7 non-practice path); the same routine's
cursor/name setup is repeated inline by that menu's mode-10 epilogue.

## Flow

1. Extra-stage swap: when the stage selector DAT_00474c7c reads 7 and the
   record's extra-stage flag at +0x1e4 is set, the published mode record
   DAT_00477848 is switched to the alternate table at 0x474908 and the
   selector (DAT_00474c7c and its mirror DAT_00474c80) to 8, so the run is
   filed as stage 8 by the insert below.
2. Score-save table: `edi = DAT_0047783c + 8 + 0x437c * (DAT_00474c6c +
   3 * DAT_00474c68)` (native `charaSlot + chara*2 + chara` addressing,
   preserved) and the run is inserted through 0x00421fa0
   (`InsertScoreRecordEdi`, already semantic in `src/ScoreFileFormats.cpp`).
   Returns the insertion rank or -1.
3. Restore guard: the native re-checks `DAT_00474c7c == 7 && flag` and would
   restore the 0x4748d8 table and selector 7 — but step 1 has already set the
   selector to 8, so the branch never fires after the swap. Preserved
   verbatim as a dead branch.
4. Negative rank (the run did not place): `record+0x1e8 = 1` (replay-present
   gate — keeps the replay-save menu closed) and return.
5. Rank accepted — replay slot selector setup:
   - `record+0x2c = 0x19` (max 25) and `record+0xf4 = 1` (wrap flag);
   - cursor A (+0x24) = the rank clamped against +0x2c (`rank >= max` →
     `max - 1`; a zero max skips the clamp);
   - cursor B (+0xfc) is clamped against its pre-existing maximum at +0x104
     (negative max → `max - 1`, otherwise 0), and only afterwards is +0x104
     refreshed with `strlen(*DAT_004746d8)` — the charset pointer variable
     (points at "ABCDEFGHIJKLMNOPQRST..." at 0x46e2f8); `+0x1cc = 1`.
6. Name buffer setup: `strcpy(record+0x2b4, DAT_0047783c + 0x1d878)` (the
   saved replay name); when the copied name differs from the nine-space
   sentinel at 0x46e354 (`repz cmpsb` of 9 bytes, `je` skips the shift), the
   cursor B shift 0x0044bea0 runs with -1. Note the polarity is the opposite
   of the mode-10 epilogue in 0x004236f0, which shifts when the name equals
   the sentinel.
7. Trailing-space trim: the native counter starts at 8 and walks
   `name[eax-1] == ' '` down to `eax > 0`, so the reported length
   (`record+0x1e0`) is capped at 8 even for a longer stored name; then
   `record+0x1e8 = 0` (replay gate cleared — the name-entry modes run).

## Globals

`DAT_00474c7c`/`DAT_00474c80` (stage selector + mirror), `DAT_00474c68`
(chara), `DAT_00474c6c` (chara slot), `DAT_0047783c` (score save state),
`DAT_00477848` (published mode record), `DAT_004746d8` (char* charset
pointer — the native loads the pointer value, never the variable's bytes).

## Verification

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` — 0 errors.
