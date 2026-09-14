#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00413a20. Native stdcall (ret 4), EAX-slot = scene owner
// (DAT_0047770c): requests manager-work slot 28 for the current mode
// record's stage script resource, opens the script file through the
// resource loader (reusing the pre-opened demo handle when present), arms
// the scene owner's script timers and copies the scene sub-timer. Returns
// 0, or -1 after formatting the load-error text.
i32 OpenSceneScriptResource(void *scene_owner);

} // namespace th10
