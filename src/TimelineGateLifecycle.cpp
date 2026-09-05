#include "TimelineGateState.hpp"

#include <string.h>

#include "AsciiManager.hpp"
#include "CallbackScheduler.hpp"
#include "MainChainRender.hpp"
#include "MainChainRuntime.hpp"
#include "ManagerWork.hpp"
#include "PackedArchive.hpp"
#include "ThreadControl.hpp"
#include "TimelineContinuation.hpp"
#include "TimelineRecordInterpreter.hpp"
#include "TimelineRenderObjects.hpp"
#include "TimelineStreamLoader.hpp"

namespace th10 {

i32 g_TimelinePhase = 0;
i32 g_TimelinePhaseBase = 0;
i32 g_TimelinePhaseOffset = 0;
i32 g_TimelinePhaseGate = 0;
i32 g_AsciiOverlayUpdateSuspended = 0;

namespace {

extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern AsciiManager *g_AsciiManager; // TH10 DAT_004776e0
extern i32 g_MainChainSharedStatus; // TH10 DAT_00491fb8
extern u32 g_MainChainPresentDiagnosticOptions; // TH10 DAT_00474e30
extern u8 g_TimelineAudioFlags[]; // TH10 DAT_0047783c region
extern const float g_AsciiOverlayInitialRate;
extern void AppendAsciiResourceLoadError(); // TH10 0x44b810
extern void *AllocateAsciiManagerMemory(u32 bytes); // TH10 0x452493
extern void FreeAsciiManagerMemory(void *pointer); // TH10 0x4524a1
extern void ReleaseManagerWorkAllocation(void *pointer);
extern void ReleaseResourceBuffer(void *pointer); // TH10 0x452422

char g_TimelineGatePathScratch[256]; // TH10 DAT_00497c38

const char *const g_TimelineStageMessagePaths[] = {
    "e00.msg", "e01.msg", "e02.msg", "e03.msg", "e04.msg", "e05.msg",
    "e06.msg", "e07.msg", "e08.msg", "e09.msg", "e10.msg", "e11.msg",
    "staff.msg",
};

void CopyPathToScratch(const char *path)
{
    g_TimelineGatePathScratch[0] = '\0';
    char *dst = g_TimelineGatePathScratch;
    const char *src = path;
    char c;
    do {
        c = *src++;
        *dst++ = c;
    } while (c != '\0');
}

void ResetInnerTimerBlock(u8 *timer, i32 tick)
{
    u32 *const words = reinterpret_cast<u32 *>(timer);
    if ((words[4] & 1U) == 0) {
        words[4] |= 1U;
        words[1] = 0;
        words[0] = static_cast<u32>(0xfff0bdc1U);
        *reinterpret_cast<float *>(timer + 8) = 0.0f;
        *reinterpret_cast<const float **>(timer + 12) = &g_AsciiOverlayInitialRate;
    }
    words[1] = static_cast<u32>(tick);
    words[0] = static_cast<u32>(static_cast<u32>(tick) - 1U);
    *reinterpret_cast<float *>(timer + 8) = static_cast<float>(tick);
}

void *ResolveSlotNode(i32 handle)
{
    if (handle == 0)
        return 0;
    return ResolveTimelineHandle(g_MainChainRenderOwner, handle);
}

void ValidateTimelineTextSlot(i32 *slot)
{
    void *const node = ResolveSlotNode(*slot);
    if (node == 0) {
        *slot = 0;
        return;
    }
    *reinterpret_cast<u8 *>(static_cast<u8 *>(node) + 0x3a0) = 0x10;
    if (*slot == 0)
        return;
    void *const node2 = ResolveSlotNode(*slot);
    if (node2 == 0) {
        *slot = 0;
        return;
    }
    *reinterpret_cast<u8 *>(static_cast<u8 *>(node2) + 0x3a1) = 0x10;
    if (*slot == 0)
        return;
    void *const node3 = ResolveSlotNode(*slot);
    if (node3 == 0) {
        *slot = 0;
        return;
    }
    *reinterpret_cast<u32 *>(static_cast<u8 *>(node3) + 0x360) |= 2U;
}

void DestroyInnerTimelineState(u8 *state)
{
    StopThreadControl(reinterpret_cast<ThreadControl *>(state + 0xd0));
    i32 *const slots = reinterpret_cast<i32 *>(state + 0x40);
    for (i32 i = 0; i != 5; ++i) {
        if (ResolveSlotNode(slots[i]) == 0)
            continue;
        ReleaseTimelineHandle(g_MainChainRenderOwner, slots[i]);
        slots[i] = 0;
    }
}

void ReleaseOwnerTimelineWorkSlot(void **slot)
{
    ManagerWorkPartial *const work =
        reinterpret_cast<ManagerWorkPartial *>(*slot);
    if (work == 0)
        return;
    ReleaseManagerWorkContents(work);
    ReleaseManagerWorkAllocation(work);
    *slot = 0;
}

void *AsciiManagerField(AsciiManager *manager, u32 offset)
{
    return *reinterpret_cast<void **>(
        reinterpret_cast<u8 *>(manager) + offset);
}

i32 TH10_FASTCALL TimelineGateCalculationCallback(void *arg)
{
    TimelineGateStatePartial *const gate =
        static_cast<TimelineGateStatePartial *>(arg);
    u8 *const inner = gate->inner_timeline_state;
    if (AdvanceTimelineRecordController(inner) != 0) {
        const u32 masked = g_MainChainPresentDiagnosticOptions & 0x1000U;
        const u32 neg = 0U - masked;
        const u32 folded = (neg >> 31) & 0xfffffff3U;
        g_MainChainSharedStatus = static_cast<i32>(folded + 0xfU);
        return 1;
    }

    ++gate->frame_counter;
    const u32 inner_flags = *reinterpret_cast<const u32 *>(inner + 0x74);
    if ((inner_flags & 4U) != 0)
        return gate->frame_counter;
    if ((gate->gate_word_0020 & 2U) != 0)
        return gate->frame_counter;
    if ((inner_flags & 2U) == 0)
        return gate->frame_counter;
    if ((g_MainChainPresentDiagnosticOptions & 0x100U) == 0)
        return gate->frame_counter;
    if (gate->frame_counter % 12 != 0)
        return 6;
    return gate->frame_counter;
}

i32 TH10_FASTCALL TimelineGateDrawCallback(void *)
{
    return 1;
}

u8 *InitializeInnerTimelineState(u8 *state, const u8 *record)
{
    memset(state, 0, 0xec);
    ManagerWorkPartial *const text_work = static_cast<ManagerWorkPartial *>(
        AsciiManagerField(g_AsciiManager, 0x899c));
    i32 *const slots = reinterpret_cast<i32 *>(state + 0x40);
    for (u32 index = 0; index != 5; ++index) {
        slots[index] = CreateTimelinePresetTextSlotNode(
            g_MainChainRenderOwner, text_work,
            static_cast<u16>(index + 0x42));
        ValidateTimelineTextSlot(slots + index);
    }

    *reinterpret_cast<const u8 **>(state + 0x54) = record;
    ResetInnerTimerBlock(state + 0x00, 0);
    ResetInnerTimerBlock(state + 0x14, 0);
    ResetInnerTimerBlock(state + 0x28, 0);
    *reinterpret_cast<u32 *>(state + 0x74) |= 1U;
    *reinterpret_cast<u32 *>(state + 0x7c) = 0xffffffU;
    return state;
}

} // namespace

TimelineGateStatePartial *CreateTimelineGateState()
{
    TimelineGateStatePartial *const gate =
        static_cast<TimelineGateStatePartial *>(
            AllocateAsciiManagerMemory(sizeof(TimelineGateStatePartial)));
    if (gate == 0)
        return 0;
    memset(gate, 0, sizeof(TimelineGateStatePartial));
    gate->flags |= 2U;
    g_TimelineGateState = gate;
    if (InitializeTimelineGateState(gate) != 0) {
        DestroyTimelineGateState(gate);
        FreeAsciiManagerMemory(gate);
        return 0;
    }
    return gate;
}

i32 InitializeTimelineGateState(TimelineGateStatePartial *gate_state)
{
    ChainElem *record = CallbackSchedulerApi::Create(
        TimelineGateCalculationCallback);
    record->flags |= ChainElemFlag_OwnedByScheduler | ChainElemFlag_Enabled;
    record->arg = gate_state;
    (void)CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler, record, 3);
    gate_state->calculation_callback = record;

    record = CallbackSchedulerApi::Create(TimelineGateDrawCallback);
    record->flags |= ChainElemFlag_OwnedByScheduler | ChainElemFlag_Enabled;
    record->arg = gate_state;
    (void)CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, record, 3);
    gate_state->draw_callback = record;

    i32 *const continuation_slot = reinterpret_cast<i32 *>(
        reinterpret_cast<u8 *>(g_AsciiManager) + 0x89a4);
    if (*continuation_slot == 0) {
        const float params[3] = {480.0f, 392.0f, 0.0f};
        void *const node = CreateTimelineContinuationRenderObject(
            AsciiManagerField(g_AsciiManager, 0x8994), 6, params);
        *continuation_slot = *reinterpret_cast<i32 *>(node);
    }

    i32 message_index = g_TimelinePhaseOffset + g_TimelinePhaseBase * 3;
    if (g_TimelinePhaseGate == 0 && g_TimelinePhase > 0)
        message_index += 6;
    gate_state->message_index = message_index;

    u32 gate_word = gate_state->gate_word_0020;
    if (g_TimelineAudioFlags[0x1d882 + message_index] == 0)
        gate_word |= 1U;
    if (g_TimelineAudioFlags[0x1d88e] == 0)
        gate_word |= 2U;
    gate_state->gate_word_0020 = gate_word;
    g_TimelineAudioFlags[0x1d882 + message_index] = 1;
    if (message_index > 5)
        g_TimelineAudioFlags[0x1d88e] = 1;

    CopyPathToScratch(g_TimelineStageMessagePaths[message_index]);
    u8 *const loaded = LoadPackedResource(g_TimelineGatePathScratch, 0, 0);
    gate_state->stream_buffer = loaded;
    if (loaded == 0) {
        AppendAsciiResourceLoadError();
        return -1;
    }

    u8 *const inner = static_cast<u8 *>(AllocateAsciiManagerMemory(0xec));
    if (inner == 0)
        return -1;
    gate_state->inner_timeline_state = InitializeInnerTimelineState(
        inner, loaded + *reinterpret_cast<const i32 *>(loaded + 4));
    return 0;
}

void DestroyTimelineGateState(TimelineGateStatePartial *gate_state)
{
    if (gate_state->calculation_callback != 0) {
        CallbackSchedulerApi::RemoveSynchronized(
            g_CallbackScheduler, gate_state->calculation_callback);
        gate_state->calculation_callback = 0;
    }
    if (gate_state->draw_callback != 0) {
        CallbackSchedulerApi::RemoveSynchronized(
            g_CallbackScheduler, gate_state->draw_callback);
        gate_state->draw_callback = 0;
    }

    ReleaseLargeRenderOwnerCachedSurfacePair(g_MainChainRenderOwner, 0);

    if (gate_state->inner_timeline_state != 0) {
        DestroyInnerTimelineState(gate_state->inner_timeline_state);
        FreeAsciiManagerMemory(gate_state->inner_timeline_state);
        gate_state->inner_timeline_state = 0;
    }

    u8 *const owner = static_cast<u8 *>(g_MainChainRenderOwner);
    ReleaseOwnerTimelineWorkSlot(
        reinterpret_cast<void **>(owner + 0x3ad0e0));
    ReleaseOwnerTimelineWorkSlot(
        reinterpret_cast<void **>(owner + 0x3ad0e4));
    ReleaseOwnerTimelineWorkSlot(
        reinterpret_cast<void **>(owner + 0x3ad0e8));
    ReleaseOwnerTimelineWorkSlot(
        reinterpret_cast<void **>(owner + 0x3ad0ec));

    if (gate_state->stream_buffer != 0) {
        ReleaseResourceBuffer(gate_state->stream_buffer);
        gate_state->stream_buffer = 0;
    }

    g_TimelineGateState = 0;
    g_AsciiOverlayUpdateSuspended = 0;
}

// TH10 0x0040b9d0. Native ESI = gate object: in-place destroy then free
// the 0x28-byte gate allocation. The global is cleared by the in-place
// destroy, so a caller using this after a failed create must not re-read
// g_TimelineGateState.
void DestroyTimelineGateStateObject(TimelineGateStatePartial *gate_state)
{
    if (gate_state == 0)
        return;
    DestroyTimelineGateState(gate_state);
    FreeAsciiManagerMemory(gate_state);
}

} // namespace th10
