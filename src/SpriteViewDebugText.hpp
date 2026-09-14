#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0040a940. Native EDI = sprite-view debug state (the 0x474788-mode
// record published through DAT_004776f8); queued into the ASCII manager's
// primary text queue through 0x00401690. Returns 1 unconditionally.
i32 DrawSpriteViewOverlayTextEdiAbi(void *state_memory);

} // namespace th10
