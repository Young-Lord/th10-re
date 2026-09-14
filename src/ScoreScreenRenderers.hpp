#ifndef TH10_SCREENSRENDERERS_HPP
#define TH10_SCREENSRENDERERS_HPP

#include "Th10Types.hpp"

namespace th10 {

// Semantic bodies for the four score-screen draw targets dispatched by the
// game-manager draw controller 0x0042d260 (jumptable at 0x0042d2b4 over
// manager state +0x1c - 9). The native entry ABI differs per target (see
// each comment); every body returns 1 in EAX.

// TH10 0x00431410 (native: manager in EDI, plain retn, no stack argument).
// State 9 draw body: the six per-stage "stage name + best score" separator
// rows under the score list, plus the row tinting shared with the detail
// screen. The manager is read through the EDI register in the native code.
i32 RunManagerDrawBody9(void *manager);

// TH10 0x004329f0 (native: manager in EDI, plain retn, no stack argument).
// State 0xB (extra-mode unlock menu) draw body, sub-state 2: the ten
// high-score records of the menu's (shot-type x difficulty) block, the
// play-count / play-time totals and the per-difficulty play count.
i32 RunManagerDrawBodyB(void *manager);

// TH10 0x00431ba0 (native stdcall, manager as stack argument, `retn 4`).
// State 0xC (score-file viewer) draw body: sub-state 2 renders the 25 replay
// list rows loaded from scoreth10.dat, sub-state 4 renders the selected
// entry's detail line plus its seven per-stage spell-card history rows.
i32 RunManagerDrawBodyC(void *manager);

// TH10 0x00433230 (native stdcall, manager as stack argument, `retn 4`).
// State 0xF draw body, sub-state 2: the ten high-score records of the
// current (shot-type x character) block for the selected difficulty, with
// the fade tint while the name-entry editor is inactive, and the name-entry
// editor (entered name, caret, alphabet template grid) when it is active.
i32 RunManagerDrawBodyF(void *manager);

} // namespace th10

#endif // TH10_SCREENSRENDERERS_HPP
