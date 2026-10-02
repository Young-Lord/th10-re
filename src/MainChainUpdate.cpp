#include "MainChainContext.hpp"
#include "MainChainRuntime.hpp"

namespace th10 {

namespace {

extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590
extern ManagerWorkOwnerPartial *g_ManagerWorkOwner; // TH10 DAT_00491c10
extern i32 g_MainChainSharedStatus; // TH10 DAT_00491fb8

} // namespace

// TH10 0x0041ff80. The scheduler supplies context in ECX. Its final state
// advance uses EAX in the original; the semantic C++ method represents that
// recovered behavior while a future ABI bridge owns the register convention.
i32 TH10_FASTCALL MainChainContext::Update(MainChainContext *context)
{
    if (static_cast<signed char>(context->callback_state_byte) < 0 &&
        context->thread_control.field_0010 == 0) {
        g_MainChainSharedStatus = 3;
    }

    AdvanceTransitionEaxAbi(&g_TransitionRoot);
    UpdateInputSlotEcxAbi(0);

    if (ServiceManagerWork(g_ManagerWorkOwner) != 0)
        return MainChainAdvance_Failed;

    if (context->thread_control.update_status_001c != 0)
        return context->thread_control.update_status_001c == 2
            ? MainChainAdvance_Failed
            : MainChainAdvance_Continue;

    return context->AdvanceState();
}

} // namespace th10
