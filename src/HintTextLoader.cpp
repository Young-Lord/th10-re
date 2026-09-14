// Hint-text state release / reload trigger (TH10 0x00418ee0). The tracked
// buffers are freed through the shared scheduler-removal boundary
// (0x00449f60) bracketed by the scheduler critical section and the
// activity-depth byte; the ten handle slots at +0xdc are expired through
// the soft-release path 0x004492a0 (entity +0x35c |= 0x4000000 with
// child-chain propagation). Native stdcall, one stack argument.
#include "HintTextLoader.hpp"

#include "CallbackScheduler.hpp"
#include "EntityHelpers.hpp"
#include "HintTextFile.hpp"
#include "Th10Platform.hpp"
#include "Th10Types.hpp"

namespace th10 {

namespace {

// Published hint-text state pointer (TH10 DAT_00477814) and the Extra-mode
// save selector byte (TH10 DAT_00491d6a; value 2 selects the automatic
// hint file).
extern void *g_PublishedHintState; // TH10 DAT_00477814
extern u8 g_ExtraSaveSelector; // TH10 DAT_00491d6a

// Scheduler lock / activity depth shared with the other release paths.
extern Win32CriticalSection g_CallbackSchedulerLock; // TH10 DAT_00492274
extern u8 g_CallbackSchedulerActivityDepth; // TH10 DAT_0049231c

// TH10 0x00449f60. Shared scheduler-record removal + free boundary (the
// semantic body lives in CallbackScheduler.cpp).
extern "C" void TH10_STDCALL FreeSchedulerRecord(void *record);

// (0x0041a200 and 0x00419040 are now the semantic bodies
// WriteHintTextTemplateStackAbi / FreeHintTipListsEaxAbi in
// HintTextFile.cpp.)

extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);

// Free one tracked buffer through 0x00449f60 under the scheduler lock,
// matching the shared destructor idiom.
void FreeHintBufferSynchronized(void *buffer)
{
    if (buffer == 0)
        return;
    EnterCriticalSection(&g_CallbackSchedulerLock);
    ++g_CallbackSchedulerActivityDepth;
    FreeSchedulerRecord(buffer);
    LeaveCriticalSection(&g_CallbackSchedulerLock);
    --g_CallbackSchedulerActivityDepth;
}

} // namespace

// TH10 0x00418ee0. Native stdcall with one stack argument.
int ReleaseHintTextState(void *state)
{
    u8 *const bytes = static_cast<u8 *>(state);

    FreeHintBufferSynchronized(*reinterpret_cast<void **>(bytes + 8));
    FreeHintBufferSynchronized(*reinterpret_cast<void **>(bytes + 12));

    // Expire the ten tracked entity handles at +0xdc: soft-release each
    // resolved entity (flag word +0x35c |= 0x4000000) and clear the slot.
    u32 *const handles = reinterpret_cast<u32 *>(bytes + 0xdc);
    for (u32 i = 0; i < 10U; ++i) {
        if (handles[i] != 0U)
            ReleaseEntityById(reinterpret_cast<void *>(0x00491C10U),
                              handles[i]);
        handles[i] = 0U;
    }

    if (g_ExtraSaveSelector == 2U)
        WriteHintTextTemplateStackAbi(state, "hint/hint_auto.txt");
    FreeHintTipListsEaxAbi(state);
    const int result = 0;

    g_PublishedHintState = 0;
    return result;
}

} // namespace th10
