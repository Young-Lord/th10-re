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

extern double GetMainChainFrameTime(); // TH10 0x00439540
extern double g_MainChainFrameCalculationTick; // TH10 DAT_00492528
extern double g_MainChainFrameDrawTick;        // TH10 DAT_00492530
extern double g_MainChainFramePreviousTick;    // TH10 DAT_00492538
extern double g_MainChainFrameClockEpoch;      // TH10 DAT_00492540
extern u32 g_MainChainPerformanceFrequencyLo;  // TH10 DAT_00492508
extern u32 g_MainChainPerformanceFrequencyHi;  // TH10 DAT_0049250c
extern void *g_TitleScreen;                    // TH10 DAT_00477810
extern Win32CriticalSection g_CallbackSchedulerLock; // TH10 DAT_00492274

extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);

// The frame-time constants used by 0x004134b0.
const double k_frame_sample_threshold = 0.5;      // 0x470bc0
const double k_two_pow_32 = 4294967296.0;         // 0x470b30
const double k_window_seconds = 60.0;             // 0x470bb0
const float k_zero_fps = 0.0f;                    // 0x470bb8
const float k_full_speed_fps = 57.0f;             // 0x470ba8

// fcomp is unordered exactly when a NaN is involved: neither a < b nor
// a >= b holds.
bool IsFloatUnordered(float left, float right)
{
    return !(left < right) && !(left >= right);
}

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

// TH10 0x004134b0 real body. Native input is the owner in ESI (plain ret,
// EAX = 1 on every path); the FPU stack is used without x87 state
// preservation. Preserved quirks:
//   - the baseline at +0x14 is only advanced by the elapsed delta after a
//     sample is taken (a sub-0.5s frame leaves the baseline untouched, so
//     the next delta accumulates);
//   - the frame accumulator is divided as an *unsigned* 32-bit value (the
//     fild + 2^32 idiom);
//   - phase 4 zeroes the QueryPerformanceFrequency pair *before* reading
//     the tick, which silently switches 0x00439540 onto its timeGetTime
//     fallback for that one call;
//   - there is no zero-guard before the division.
void UpdateRegistrationDrawTiming(RegistrationDrawOwner *owner)
{
    const double current_tick = GetMainChainFrameTime();

    // Replace the baseline only on an ordered "less" comparison; NaN or
    // greater/equal keeps the old baseline.
    if (current_tick < owner->last_tick)
        owner->last_tick = current_tick;

    const double elapsed = current_tick - owner->last_tick;
    if (!(elapsed >= k_frame_sample_threshold))
        return; // unordered counts as too small (native jne on C0|C3)

    owner->last_tick = owner->last_tick + elapsed;

    const u32 raw_accumulator = owner->frame_accumulator;
    const double unsigned_accumulator = raw_accumulator >= 0x80000000U
        ? static_cast<double>(static_cast<i32>(raw_accumulator))
          + k_two_pow_32
        : static_cast<double>(raw_accumulator);
    const float sampled_fps =
        static_cast<float>(unsigned_accumulator / elapsed);
    owner->sampled_fps = sampled_fps;

    if (!(sampled_fps > k_zero_fps) || IsFloatUnordered(sampled_fps,
                                                          k_zero_fps)) {
        // fps <= 0 or unordered: clear the phase counter and skip the
        // sampling actions (but still run the title-timing tail).
        owner->phase_count = 0;
    } else {
        owner->phase_count += 1;
        if (owner->phase_count == 2) {
            const double tick = GetMainChainFrameTime();
            g_MainChainFrameClockEpoch = tick;
            g_MainChainFramePreviousTick = tick;
            g_MainChainFrameCalculationTick = tick;
            g_MainChainFrameDrawTick = tick;
        } else if (owner->phase_count == 4) {
            // Native zeroes the performance frequency (both dwords)
            // before reading the tick.
            g_MainChainPerformanceFrequencyLo = 0;
            g_MainChainPerformanceFrequencyHi = 0;
            const double tick = GetMainChainFrameTime();
            g_MainChainFrameClockEpoch = tick;
            g_MainChainFramePreviousTick = tick;
            g_MainChainFrameCalculationTick = tick;
            g_MainChainFrameDrawTick = tick;
        }
    }

    if (g_TitleScreen != 0) {
        u32 *const state_flags =
            static_cast<u32 *>(g_TitleScreen) + 0x58U / sizeof(u32);
        if ((*state_flags & 0x14U) == 0) {
            owner->elapsed_window += k_window_seconds;
            if (sampled_fps > k_full_speed_fps)
                owner->displayed_fps += k_window_seconds;
            else
                owner->displayed_fps += static_cast<double>(sampled_fps);
        }
        *state_flags &= 0xffffff7fU; // clear the 0x80 frame-request bit
    }

    owner->frame_accumulator = 0;
}

// TH10 0x00413690 is the scheduler-facing ECX callback. This source version
// keeps its EDI-only implementation detail out of the semantic method.
i32 TH10_FASTCALL RegistrationDrawCallback(RegistrationDrawOwner *owner)
{
    UpdateRegistrationDrawTiming(owner);

    if (g_MainChainSharedStatus != 14 && g_AsciiManager != 0) {
        // Literal native control flow (0x004135f5): the first comparison
        // selects 0xff5050ff for every ordered value — the jp only routes
        // *unordered* results to the second comparison, so the
        // 0xffa0a0ff band is unreachable dead code and a NaN fps resolves
        // through the second unordered check to 0xffffffff.
        const u32 color = IsFloatUnordered(owner->sampled_fps, 30.0f)
            ? (IsFloatUnordered(owner->sampled_fps, 40.0f)
                   ? 0xffffffffU
                   : 0xffa0a0ffU)
            : 0xff5050ffU;
        QueuePrimaryFpsTextSelected(g_AsciiManager, color,
                                    owner->sampled_fps);
    }

    owner->frame_accumulator += 1 + g_RegistrationFrameIncrement;
    return 1;
}

} // namespace th10
