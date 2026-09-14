// Title -> game-scene startup orchestration (TH10 0x00417870). The title
// callback worker (0x00417c70) forwards the published title-screen state
// here; the body blocks on the render-owner handover, seeds the shared
// frame-state block, registers the scene calc/draw scheduler records, runs
// the full manager creation cascade (or the replay-reuse path), then parks
// until the mode worker drains before returning to the caller.
#ifndef TH10_TITLESCENESETUP_HPP
#define TH10_TITLESCENESETUP_HPP

#include "Th10Types.hpp"

// TH10 0x00417870. Native stdcall, one stack argument (the published
// title-screen state object); returns 0 on success and -1 on any setup
// failure (with the failure tail: scene kill flag, gate release and both
// scheduler records re-enabled).
int SetupGameSceneFromTitle(void *title_state);

#endif // TH10_TITLESCENESETUP_HPP
