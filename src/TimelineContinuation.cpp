#include "TimelineContinuation.hpp"

#include <string.h>

#include "ManagerWork.hpp"
#include "ThreadControl.hpp"
#include "TimelineGateState.hpp"
#include "TimelineRenderObjects.hpp"

namespace th10 {

namespace {

char g_TimelineContinuationPathScratch[256]; // TH10 DAT_00497c38

struct AsciiManagerContinuationSlice {
    u8 unknown_0000[0x8994];
    void *ascii_animation_work;
    u8 unknown_8998[4];
    void *text_animation_work;
    u8 unknown_89a0[4];
    i32 continuation_handle;
    u8 unknown_89a8[8];
};

typedef char AssertAsciiManagerContinuationHandleOffset[
    offsetof(AsciiManagerContinuationSlice, continuation_handle) == 0x89a4
        ? 1 : -1];

extern AsciiManagerContinuationSlice *g_AsciiManager; // TH10 DAT_004776e0
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

extern "C" u32 TH10_CDECL _beginthreadex(void *security_attributes,
                                          u32 stack_size,
                                          u32 (TH10_STDCALL *start_routine)(void *),
                                          void *argument,
                                          u32 creation_flags,
                                          u32 *thread_id);

void CopyCStringToScratch(const char *source)
{
    g_TimelineContinuationPathScratch[0] = '\0';
    char *dst = g_TimelineContinuationPathScratch;
    const char *src = source;
    char c;
    do {
        c = *src++;
        *dst++ = c;
    } while (c != '\0');
}

i32 RunTimelineContinuationBody(u8 *state)
{
    CopyCStringToScratch(reinterpret_cast<const char *>(*reinterpret_cast<u8 **>(state + 0x70)));
    const u8 *const record = *reinterpret_cast<const u8 *const *>(state + 0x54);
    const i32 slot = *reinterpret_cast<const i32 *>(record + 4) + 0x1d;
    void *const work = RequestManagerWork(
        reinterpret_cast<ManagerWorkOwnerPartial *>(g_MainChainRenderOwner),
        slot, g_TimelineContinuationPathScratch);
    const i32 source_index = *reinterpret_cast<const i32 *>(record + 4);
    *reinterpret_cast<void **>(state + 0x80 + source_index * 4) = work;
    *reinterpret_cast<u32 *>(state + 0x74) &= ~4U;
    ReleaseTimelineContinuationHandle(&g_AsciiManager->continuation_handle);
    g_AsciiManager->continuation_handle = 0;
    return 0;
}

} // namespace

void StartTimelineContinuation(float width, float height)
{
    if (g_AsciiManager->continuation_handle != 0)
        return;
    const float params[3] = {width, height, 0.0f};
    void *const node = CreateTimelineContinuationRenderObject(
        g_AsciiManager->ascii_animation_work, 6, params);
    g_AsciiManager->continuation_handle =
        *reinterpret_cast<i32 *>(node);
}

void RegisterTimelineContinuation(ThreadControl *control, void *argument)
{
    (void)argument;
    StopThreadControl(control);
    control->thread_entry = reinterpret_cast<void *>(TimelineContinuationThreadEntry);
    control->field_0010 = 1;
    control->stop_requested = 0;
    control->thread_handle = reinterpret_cast<void *>(_beginthreadex(
        0, 0, TimelineContinuationThreadEntry, 0, 0, &control->thread_id));
}

u32 TH10_STDCALL TimelineContinuationThreadEntry(void *)
{
    u8 *const state = g_TimelineGateState->inner_timeline_state;
    (void)RunTimelineContinuationBody(state);
    return 0;
}

} // namespace th10
