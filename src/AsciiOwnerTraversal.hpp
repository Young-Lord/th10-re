#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00403090 semantic body. The native entry uses EAX=projection state,
// ECX=child record, EDX=outer translation and one stack distance limit.
i32 CullAsciiOwnerChild(void *projection_state, void *child_record,
                        const void *outer_translation,
                        float squared_distance_limit);

// TH10 0x00403a30 semantic body. Native ABI is two stack arguments and ret 8.
i32 RenderAsciiSceneChannel(void *scene, i32 channel);

} // namespace th10
