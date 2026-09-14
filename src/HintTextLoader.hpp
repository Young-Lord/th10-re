// Hint-text state release / reload trigger (TH10 0x00418ee0), the routine
// referencing "hint/hint_auto.txt". Runs when the hint-text manager state
// (published through DAT_00477814) is torn down: it frees the two tracked
// text buffers under the scheduler lock, expires the ten tracked entity
// handles, re-runs the automatic hint loader for the Extra-mode save flag,
// and clears the published state pointer.
#ifndef TH10_HINTTEXTLOADER_HPP
#define TH10_HINTTEXTLOADER_HPP

#include "Th10Types.hpp"

// TH10 0x00418ee0. Native stdcall, one stack argument (the hint-text
// state object); returns the result of the reload worker 0x00419040.
int ReleaseHintTextState(void *state);

#endif // TH10_HINTTEXTLOADER_HPP
