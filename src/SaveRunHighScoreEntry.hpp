#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00423570. Post-run high-score publication (native stdcall `ret 4`;
// the 0x3ac-byte scheduler record arrives as the stack argument). With the
// stage selector at 7 (extra) and the record's extra-stage flag at +0x1e4
// set, the published mode record (DAT_00477848) is switched to the 0x474908
// table and the selector to 8 for the insert. Computes the score-save table
// address DAT_0047783c + 8 + 0x437c * (DAT_00474c6c + 3 * DAT_00474c68) and
// inserts the run through 0x00421fa0 (`InsertScoreRecordEdi`). A negative
// rank sets the replay-present gate +0x1e8; otherwise the record's cursor A
// becomes the rank-clamped replay slot selector (max 25), cursor B is
// re-clamped against its old maximum, refreshed with the charset length, and
// the saved replay name is copied over the name buffer (shifting cursor B by
// -1 when it differs from nine spaces) with trailing spaces trimmed into
// +0x1e0 before clearing the gate.
void SaveRunHighScoreEntry(void *record);

} // namespace th10
