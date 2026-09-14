#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00421fa0 (native __usercall, EDI = one 0x437c-byte score-save slot
// payload holding the ten 24-byte high-score records of one character at
// 24-byte strides, record data at +0x10). Inserts the finished run into the
// table: ranks the current score (DAT_00474C44) against the ten stored
// scores, shifts the tail down by one, and writes score, DAT_00474C7C byte
// at +0x14, DAT_00474C90 byte at +0x15, a blank 8-space name at +0x16, the
// current time at +0x20 and the slow-rate float at +0x24. Returns the
// insertion rank 0..9, or -1 when the run did not place. Callers 0x00423570
// and 0x00432cb0 compute EDI as (value of DAT_0047783c) + 8 +
// 0x437c * (DAT_00474C6C + 3 * DAT_00474C68).
i32 InsertScoreRecordEdi(void *score_table);

// TH10 0x00434dd0 (native: ECX = section name, stack0 = section-list object,
// stack1 = output buffer, `retn 8`). Looks the section up case-insensitively
// in the list, reads its packed bytes through the list's reader object
// (+0x0c, vtable+0x18 seek-by-offset / vtable+0x08 read), unscrambles them
// with the DAT_00474bd8 key record selected by the name byte sum and, when
// the packed and unpacked sizes differ, decompresses into the caller's
// buffer. Returns the decoded buffer address or 0.
void *DecodePackedSectionEcxStackAbi(const char *section_name,
                                     const void *section_list,
                                     void *out_buffer);

// TH10 0x00433b30 (native stdcall stack argument, `retn 4`). Manager-draw
// body 10: renders the replay-view rows (sub-state 2) or the selected
// high-score detail line plus comment editor (sub-state 3) through the ASCII
// manager. Always returns 1.
i32 RunManagerDrawBody10(void *manager);

} // namespace th10
