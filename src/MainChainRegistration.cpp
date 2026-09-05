#include "CallbackScheduler.hpp"
#include "MainChainContext.hpp"

namespace th10 {

namespace {

extern MainChainContext g_MainChainContext; // TH10 DAT_00491c28
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern i32 g_MainChainRegistrationState; // TH10 DAT_00491fb4
extern i32 g_MainChainSharedStatus; // TH10 DAT_00491fb8
extern i32 g_MainChainRegistrationField; // TH10 DAT_00491fc0

extern i32 TH10_FASTCALL InitializeMainChainCalculationCallback(void *context);

ChainElem *CreateMainChainElement(ChainCallback callback)
{
    ChainElem *element = CallbackSchedulerApi::Create(callback);
    element->flags |= ChainElemFlag_Enabled;
    element->arg = &g_MainChainContext;
    return element;
}

} // namespace

// TH10 0x00420470. Native scheduler helpers have register-oriented ABIs;
// CallbackSchedulerApi is the deliberately narrow bridge for those details.
i32 RegisterMainChainCallbacks()
{
    g_MainChainRegistrationState = -2;
    g_MainChainSharedStatus = 0;
    g_MainChainRegistrationField = 0;

    ChainElem *element = CreateMainChainElement(
        reinterpret_cast<ChainCallback>(MainChainContext::Update));
    element->registration_hook = InitializeMainChainCalculationCallback;

    i32 result = CallbackSchedulerApi::AddToCalculationChain(
        g_CallbackScheduler, element, 1);
    if (result != 0)
        return result;

    element = CreateMainChainElement(
        reinterpret_cast<ChainCallback>(MainChainContext::DrawInitialize));
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, element, 1);

    element = CreateMainChainElement(
        reinterpret_cast<ChainCallback>(MainChainContext::DrawContinue));
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, element, 40);

    element = CreateMainChainElement(
        reinterpret_cast<ChainCallback>(MainChainContext::DrawFinalize));
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, element, 50);

    return 0;
}

} // namespace th10
