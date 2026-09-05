#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00413bc0 semantic body. Native EDI = the DAT_0047770c ASCII HUD
// overlay owner; the ECX input is stored but never read (caller garbage).
// Re-arms the overlay HUD: disables the two scheduler records, re-creates
// the two background VM entities, resets every glyph VM pool script,
// refreshes the life-count display, and spawns the mode/state dependent
// overlay entities.
void ResetAsciiHudOverlayEdiAbi(void *owner);

} // namespace th10
