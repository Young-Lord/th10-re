#include "MainChainContext.hpp"
#include "MainChainRender.hpp"

namespace th10 {

namespace {

extern MainChainRenderOwnerFrameState *g_MainChainRenderOwner; // 0x491c10
extern D3D9Device *g_D3D9ClearDevice; // TH10 DAT_00491c30
extern u32 g_MainChainClearColor; // TH10 DAT_004923a8

void ResetDrawOwnerFrameState(MainChainRenderOwnerFrameState *owner)
{
    owner->field_3ada70 = 0;
    owner->field_3ada64 = 0;
    owner->field_3ada69 = 0xff;
    owner->field_3ada68 = 3;
    owner->field_3ada6b = 0xff;
    owner->field_3ada6c = 0xff;
    owner->field_73245c = 0;
    owner->field_732458 = 0x80808080;
    owner->field_3ada6e = 0xff;
    owner->camera_value_0060 = 0;
    owner->camera_value_005c = 0;
    owner->field_3ada6a = 0xff;
}

} // namespace

// TH10 0x00420000. The native camera helper consumes EDI; the rest of this
// callback has ordinary source-level dataflow and ignores all D3D HRESULTs.
i32 TH10_FASTCALL MainChainContext::DrawInitialize(MainChainContext *context)
{
    ResetDrawOwnerFrameState(g_MainChainRenderOwner);

    MainChainCameraWork *work =
        &context->camera_work_bank[1];
    context->draw_work_pointer = work;
    UpdateMainChainCameraWorkEdiAbi(work);

    SetD3D9Viewport(context->draw_target, &work->viewport);
    context->draw_initialized = 1;
    ClearD3D9Target(g_D3D9ClearDevice, g_MainChainClearColor);
    return MainChainAdvance_Continue;
}

} // namespace th10
