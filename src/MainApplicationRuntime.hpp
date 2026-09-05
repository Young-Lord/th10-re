#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Semantic frame/message loop from TH10 0x00438cf1 through 0x00438d9a.
// Returns the local application status consumed by the outer cleanup branch.
i32 RunMainChainFrameLoop();

// Semantic body of TH10 0x00438ad0. The native entry has four stack
// arguments and ret 0x10; unknown second/third arguments are retained only
// by the future ABI thunk.
i32 RunMainApplication(void *application_instance,
                        void *unused_second,
                        void *unused_third,
                        void *main_context_initial_word);

} // namespace th10
