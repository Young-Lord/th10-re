// TH10 0x00422660 / 0x004224c0 — draw bodies of the replay-record screens.
#ifndef TH10_REPLAYRECORDSCREENS_HPP
#define TH10_REPLAYRECORDSCREENS_HPP

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00422660. Native `retn 4` stdcall draw body over the 0x3ac-byte
// scheduler/VM record family (the draw sibling of the calculation-record
// dispatcher 0x004223f0; the draw mode byte at record+4 is dispatched as
// mode-10 through the byte table at 0x422a7c: modes 10 and 17 draw the
// replay list, 11 and 18 the selected-replay detail, 12 and 19 the score
// ranking, 13..16 and out-of-range values only reset the ASCII state).
// Returns 1.
i32 DrawReplayRecordScreensStackAbi(void *record);

// TH10 0x004224c0. Native `retn 0x10` stdcall body: stack arguments are
// (record, x, y, z). Draws the typed name (record+0x2b4), the insertion
// caret and the fixed 13-column alphabet grid at (112, 320); the grid
// cursor is record+0xfc and the typed-name cursor record+0x1e0. Also used
// (inlined there) by the manager draw body 0x00433b30.
void DrawNameEntryGridStackAbi(void *record, float x, float y, float z);

} // namespace th10

#endif // TH10_REPLAYRECORDSCREENS_HPP
