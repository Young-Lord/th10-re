#include "AsciiOverlayFactory.hpp"

#include <string.h>

#include "AsciiOverlayCallbacks.hpp"
#include "CallbackScheduler.hpp"

namespace th10 {

namespace {

typedef i32 (*OverlayCallback)(void *);

struct AsciiOverlayContext {
    u32 flags_0000;
    u32 unknown_0004;
    ChainElem *calculation_node;
    ChainElem *draw_node;
    u32 kind;
    u8 unknown_0014[4];
    i32 alpha_source;
    u32 value_001c;
    u32 value_0020;
    u32 value_0024;
    u32 value_0028;
    u32 fade_out_flag;
    i32 previous_tick;
    i32 tick;
    float accumulated_time;
    const float *rate;
    u32 flags_0040;
};

typedef char AssertAsciiOverlayContextSize[
    sizeof(AsciiOverlayContext) == 0x44 ? 1 : -1];

extern void *AllocateAsciiOverlayMemory(u32 bytes); // TH11 0x00452493
extern void FreeAsciiOverlayMemory(void *memory); // TH11 0x004524a1
extern CallbackScheduler *g_CallbackScheduler; // TH11 DAT_00491be4
extern const float g_AsciiOverlayInitialRate; // TH11 DAT_00476f78

ChainElem *CreateAsciiOverlayCalculationNode(void *context,
                                              OverlayCallback callback,
                                              u32 priority)
{
    ChainElem *const node = CallbackSchedulerApi::Create(
        reinterpret_cast<ChainCallback>(callback));
    node->arg = context;
    node->flags |= ChainElemFlag_Enabled;
    (void)CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler,
        node, static_cast<i32>(priority));
    return node;
}

ChainElem *CreateAsciiOverlayDrawNode(void *context, OverlayCallback callback,
                                       u32 priority)
{
    ChainElem *const node = CallbackSchedulerApi::Create(
        reinterpret_cast<ChainCallback>(callback));
    node->arg = context;
    node->flags |= ChainElemFlag_Enabled;
    (void)CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, node,
        static_cast<i32>(priority));
    return node;
}

i32 TH10_FASTCALL DestroyAsciiOverlayContextAndNodes(void *context_memory)
{
    AsciiOverlayContext *const context =
        static_cast<AsciiOverlayContext *>(context_memory);
    if (context != 0) {
        if (context->calculation_node != 0)
            CallbackSchedulerApi::RemoveSynchronized(g_CallbackScheduler,
                context->calculation_node);
        if (context->draw_node != 0)
            CallbackSchedulerApi::RemoveSynchronized(g_CallbackScheduler,
                context->draw_node);
        FreeAsciiOverlayMemory(context);
    }
    return 0;
}

void ConfigureAsciiOverlayContext(AsciiOverlayContext *context, u32 kind,
                                  u32 value_1, u32 value_2, u32 value_3,
                                  u32 draw_priority)
{
    OverlayCallback calculation = 0;
    OverlayCallback draw = 0;
    switch (kind) {
    case 0:
        calculation = UpdateAsciiOverlayFullFade;
        draw = DrawAsciiFullScreenOverlayWithViewport;
        break;
    case 1:
        calculation = UpdateAsciiOverlayKindOne;
        break;
    case 2:
        calculation = UpdateAsciiOverlayFadeIn;
        draw = DrawAsciiInsetOverlayA;
        break;
    case 3:
        context->alpha_source = 0xff;
        calculation = UpdateAsciiOverlayFullFade;
        draw = DrawAsciiInsetOverlayA;
        break;
    case 4:
        calculation = UpdateAsciiOverlayFadeIn;
        draw = DrawAsciiFullScreenOverlayWithViewport;
        break;
    case 5:
        calculation = UpdateAsciiOverlayKindFive;
        draw = DrawAsciiInsetOverlayMaskedRgb;
        break;
    case 6:
        calculation = UpdateAsciiOverlayHalfAlpha;
        draw = DrawAsciiOverlayKindSix;
        break;
    case 7:
        calculation = UpdateAsciiOverlayHalfAlpha;
        draw = DrawAsciiInsetOverlayB;
        break;
    case 8:
        calculation = UpdateAsciiOverlayKindEight;
        break;
    }

    if (calculation != 0)
        context->calculation_node = CreateAsciiOverlayCalculationNode(
            context, calculation, 14);
    if (draw != 0)
        context->draw_node = CreateAsciiOverlayDrawNode(context, draw,
                                                         draw_priority);

    // Native performs this dereference for every kind, including out-of-range
    // kinds whose freshly allocated context has no calculation node.
    context->calculation_node->calculation_followup =
        DestroyAsciiOverlayContextAndNodes;
    if ((context->flags_0040 & 1U) == 0) {
        context->previous_tick = static_cast<i32>(0xfff0bdc1U);
        context->tick = 0;
        context->rate = &g_AsciiOverlayInitialRate;
        context->flags_0040 |= 1;
    }
    context->previous_tick = -1;
    context->tick = 0;
    context->value_001c = value_1;
    context->kind = kind;
    context->value_0020 = value_2;
    context->value_0024 = value_3;
    context->value_0028 = value_3;
}

} // namespace

void *CreateAsciiOverlayContext(u32 kind, u32 value_1, u32 value_2,
                                u32 ignored_value, u32 value_3,
                                u32 draw_priority)
{
    (void)ignored_value;
    AsciiOverlayContext *const context = static_cast<AsciiOverlayContext *>(
        AllocateAsciiOverlayMemory(sizeof(AsciiOverlayContext)));
    if (context == 0)
        return 0;
    memset(context, 0, sizeof(*context));
    context->flags_0000 |= 2;
    ConfigureAsciiOverlayContext(context, kind, value_1, value_2, value_3,
                                 draw_priority);
    return context;
}

} // namespace th10
