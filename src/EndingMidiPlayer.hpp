#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x43af30. Native entry receives the ending player context in ECX and
// plain-returns; it is the update callback published in the ending-state
// function pointer table at 0x46f80c. Advances the 64-bit playback time,
// drives the volume fade, and runs every scheduled event block whose time
// window covers the current time.
void UpdateEndingMidiPlayback(void *context);

} // namespace th10
