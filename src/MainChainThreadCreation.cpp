#include "MainChainThreadCreation.hpp"

#include "MainChainBackgroundThread.hpp"
#include "MainChainResourceThread.hpp"
#include "MainChainSoundWorker.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

typedef u32 (TH10_STDCALL *Win32ThreadStartFn)(void *);
typedef u32 (TH10_STDCALL *CrtThreadStartFn)(void *);

extern "C" void *TH10_STDCALL CreateThread(void *security_attributes,
                                             u32 stack_size,
                                             Win32ThreadStartFn start_routine,
                                             void *argument,
                                             u32 creation_flags,
                                             u32 *thread_id);
extern "C" u32 TH10_CDECL _beginthreadex(void *security_attributes,
                                          u32 stack_size,
                                          CrtThreadStartFn start_routine,
                                          void *argument,
                                          u32 creation_flags,
                                          u32 *thread_id);

extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590
extern volatile u32 g_MainChainResourceGate; // TH10 DAT_004977b4
extern u32 g_MainChainSoundWorkerFinished; // TH10 DAT_004977bc
extern u32 g_MainChainSoundWorkerId; // TH10 DAT_004977b0

extern void SleepMilliseconds(u32 milliseconds);

u32 TH10_STDCALL MainChainSoundWorkerAdapter(void *)
{
    extern void *g_MainChainSoundWorkerWindow; // TH10 DAT_004977b8
    (void)InitializeMainChainSoundRoot(&g_TransitionRoot,
                                       g_MainChainSoundWorkerWindow);
    while (g_MainChainResourceGate == 0)
        SleepMilliseconds(1);
    g_MainChainSoundWorkerFinished = 1;
    return g_MainChainResourceGate;
}

} // namespace

void *StartMainChainResourceThread(void *argument, u32 *thread_id)
{
    // TH10 0x4201e7. The root argument is preserved only as the observed
    // CreateThread argument; the native resource entry never reads it.
    return CreateThread(0, 0, MainChainResourceThreadAdapter, argument, 0,
                        thread_id);
}

void *StartMainChainBackgroundThread(MainChainContext *context, u32 *thread_id)
{
    // TH10 0x420216. This is intentionally CRT-mediated: the original start
    // entry uses cdecl and is reached through a stdcall CRT adapter.
    return reinterpret_cast<void *>(_beginthreadex(
        0, 0, MainChainBackgroundThreadAdapter, context, 0, thread_id));
}

void *CreateMainChainSoundWorkerThread()
{
    // TH10 0x438c21. Failure is intentionally left to the caller's later
    // startup/teardown flow; the native code does not test this HANDLE.
    return CreateThread(0, 0, MainChainSoundWorkerAdapter,
                        &g_TransitionRoot, 0, &g_MainChainSoundWorkerId);
}

} // namespace th10
