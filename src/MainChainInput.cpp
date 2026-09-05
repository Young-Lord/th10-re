#include "MainChainInput.hpp"
#include "MainChainDirectInputAdapter.hpp"

namespace th10 {

namespace {

extern void *GetMainChainWindowInstance(void *window);
extern void ReportMainChainInputMessage(i32 message_id);

void ReleaseAndClear(void **object)
{
    ReleaseMainChainInputInterfaceAdapter(*object);
    *object = 0;
}

} // namespace

// TH10 0x0043b8d0 semantic body. Its binary entry takes context in EAX; this
// ordinary source signature is the reconstruction-side boundary.
i32 InitializeMainChainInputSemantic(MainChainContext *context)
{
    void *instance = GetMainChainWindowInstance(context->window_0048);
    if ((context->input_setup_flags_0150 & 8U) != 0)
        return -1;

    if (CreateMainChainDirectInputAdapter(instance, &context->input_root_000c) < 0) {
        context->input_root_000c = 0;
        ReportMainChainInputMessage(0);
        return -1;
    }

    if (CreateMainChainKeyboardAdapter(context->input_root_000c,
                                &context->input_keyboard_0010) < 0) {
        ReleaseAndClear(&context->input_root_000c);
        ReportMainChainInputMessage(0);
        return -1;
    }

    if (SetMainChainKeyboardDataFormatAdapter(context->input_keyboard_0010) < 0) {
        ReleaseAndClear(&context->input_keyboard_0010);
        ReleaseAndClear(&context->input_root_000c);
        ReportMainChainInputMessage(1);
        return -1;
    }

    if (SetMainChainKeyboardCooperativeLevelAdapter(context->input_keyboard_0010,
                                              context->window_0048) < 0) {
        ReleaseAndClear(&context->input_keyboard_0010);
        ReleaseAndClear(&context->input_root_000c);
        ReportMainChainInputMessage(2);
        return -1;
    }

    AcquireMainChainKeyboardAdapter(context->input_keyboard_0010);
    ReportMainChainInputMessage(3);
    EnumerateMainChainControllersAdapter(context->input_root_000c);

    if (context->input_controller_0014 != 0) {
        ConfigureMainChainControllerAdapter(context->input_controller_0014,
                                     context->window_0048);
        ReportMainChainInputMessage(4);
    }
    return 0;
}

i32 TH10_FASTCALL InitializeMainChainInputFromEax(MainChainContext *context)
{
    return InitializeMainChainInputSemantic(context);
}

} // namespace th10
