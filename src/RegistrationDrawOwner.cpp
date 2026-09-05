#include <string.h>

#include "AsciiRenderAdapter.hpp"
#include "RegistrationDrawOwner.hpp"

namespace th10 {

namespace {

extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern RegistrationDrawOwner *g_RegistrationDrawOwner; // DAT_00447708
extern RegistrationDrawOwner *AllocateRegistrationDrawOwner(u32 bytes);
extern void FreeRegistrationDrawOwner(void *owner); // TH10 0x004524a1
extern i32 g_MainChainSharedStatus; // TH10 DAT_00491fb8
extern AsciiManagerAdapterSlice *g_AsciiManager; // TH10 DAT_004776e0
extern u8 g_RegistrationFrameIncrement; // TH10 DAT_00491d66

extern double GetRegistrationDrawTick(); // TH10 0x00439540
extern bool IsTitleTimingAdvanceAllowed();
extern void ClearRegistrationTimingGlobalsAtPhaseFour();
extern void StoreRegistrationTimingSample(double tick);
extern Win32CriticalSection g_CallbackSchedulerLock; // TH10 DAT_00492274

extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);

} // namespace

// TH10 0x00413350. Both allocation-null paths in the original immediately
// dereference their result; this source boundary preserves the normal path
// without fabricating a partial-object error contract.
RegistrationDrawOwner *CreateRegistrationDrawOwner()
{
    RegistrationDrawOwner *owner = AllocateRegistrationDrawOwner(
        sizeof(RegistrationDrawOwner));
    memset(owner, 0, sizeof(*owner));
    owner->flags |= ChainElemFlag_Enabled;
    g_RegistrationDrawOwner = owner;

    ChainElem *record = CallbackSchedulerApi::Create(
        reinterpret_cast<ChainCallback>(RegistrationDrawCallback));
    record->flags |= ChainElemFlag_Enabled;
    record->arg = owner;
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, record, 47);
    owner->draw_record = record;
    return owner;
}

void DestroyRegistrationDrawOwner(RegistrationDrawOwner *owner)
{
    if (owner == 0)
        return;

    if (owner->draw_record != 0) {
        EnterCriticalSection(&g_CallbackSchedulerLock);
        CallbackSchedulerApi::Remove(g_CallbackScheduler, owner->draw_record);
        LeaveCriticalSection(&g_CallbackSchedulerLock);
    }

    g_RegistrationDrawOwner = 0;
    FreeRegistrationDrawOwner(owner);
}

void UpdateRegistrationDrawTiming(RegistrationDrawOwner *owner)
{
    const double current_tick = GetRegistrationDrawTick();
    if (current_tick < owner->last_tick)
        owner->last_tick = current_tick;

    const double elapsed = current_tick - owner->last_tick;
    if (elapsed < 0.5)
        return;

    owner->sampled_fps = static_cast<float>(
        static_cast<double>(owner->frame_accumulator) / elapsed);
    if (owner->sampled_fps <= 0.0f) {
        owner->phase_count = 0;
    } else {
        owner->phase_count++;
        if (owner->phase_count == 2) {
            StoreRegistrationTimingSample(GetRegistrationDrawTick());
        } else if (owner->phase_count == 4) {
            ClearRegistrationTimingGlobalsAtPhaseFour();
            StoreRegistrationTimingSample(GetRegistrationDrawTick());
        }
    }

    if (IsTitleTimingAdvanceAllowed()) {
        owner->elapsed_window += 60.0;
        owner->displayed_fps += owner->sampled_fps > 57.0f
            ? 60.0
            : static_cast<double>(owner->sampled_fps);
    }

    owner->frame_accumulator = 0;
}

// TH10 0x00413690 is the scheduler-facing ECX callback. This source version
// keeps its EDI-only implementation detail out of the semantic method.
i32 TH10_FASTCALL RegistrationDrawCallback(RegistrationDrawOwner *owner)
{
    UpdateRegistrationDrawTiming(owner);

    if (g_MainChainSharedStatus != 14 && g_AsciiManager != 0) {
        const u32 color = owner->sampled_fps >= 30.0f
            ? 0xff5050ff
            : (owner->sampled_fps >= 40.0f ? 0xffa0a0ff : 0xffffffff);
        QueuePrimaryFpsTextSelected(g_AsciiManager, color, owner->sampled_fps);
    }

    owner->frame_accumulator += 1 + g_RegistrationFrameIncrement;
    return 1;
}

} // namespace th10
