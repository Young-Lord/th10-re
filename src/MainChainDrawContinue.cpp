#include "MainChainContext.hpp"

namespace th10 {

// TH10 0x004200c0. The scheduler supplies the context in ECX, but this
// callback deliberately has no observable dependency on it.
i32 TH10_FASTCALL MainChainContext::DrawContinue(MainChainContext *)
{
    return MainChainAdvance_Continue;
}

} // namespace th10
