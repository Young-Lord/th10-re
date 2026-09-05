#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0042b1e0. Native ABI: EBX = the score-save state (the value of
// DAT_0047783c, handed over by the title-screen teardown 0x00417c80). The
// state layout is:
//   +0x0000  pointer to the 0x18-byte score header record (the file's first
//            written block; +0x04 and +0x14 are republished on every save)
//   +0x0008  seven 0x437c-byte stage-record slots, each: +0 word magic
//            0x5243, +4 checksum dword, +8 stage-index dword, +0xc payload
//   +0x1d86c 0x448-byte clear-data region (+4 = its checksum dword)
// The body serializes the matching stage records and the clear-data region
// into a 0x200000 scratch image, LZSS-compresses it, applies one scramble
// pass and writes header plus packed body to "scoreth10.dat" through the
// shared replay-file opener 0x0044b620. Returns -1 when the header record is
// null or the file cannot be opened, else 0.
i32 SaveScoreRecordFileEbx(void *save_state);

} // namespace th10
