#include "MainChainFrameTime.hpp"

#include "Th10Platform.hpp"

namespace th10 {

namespace {

struct Win32LargeInteger {
    long long quad_part;
};

extern Win32CriticalSection g_MainChainFrameClockLock; // DAT_004922ec
extern u8 g_MainChainFrameClockDepth; // DAT_00492321
extern Win32LargeInteger g_MainChainPerformanceFrequency; // DAT_00492508
extern Win32LargeInteger g_MainChainPerformanceBase; // DAT_00492510
extern double g_MainChainFrameClockEpoch; // DAT_00492540

extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);
extern "C" i32 TH10_STDCALL QueryPerformanceCounter(Win32LargeInteger *value);
extern "C" i32 TH10_STDCALL timeBeginPeriod(u32 milliseconds);
extern "C" u32 TH10_STDCALL timeGetTime();
extern "C" i32 TH10_STDCALL timeEndPeriod(u32 milliseconds);

} // namespace

double GetMainChainFrameTime()
{
    EnterCriticalSection(&g_MainChainFrameClockLock);
    ++g_MainChainFrameClockDepth;

    if (g_MainChainPerformanceFrequency.quad_part != 0) {
        Win32LargeInteger now;
        (void)QueryPerformanceCounter(&now);
        const double sample = static_cast<double>(now.quad_part -
            g_MainChainPerformanceBase.quad_part) /
            static_cast<double>(g_MainChainPerformanceFrequency.quad_part);
        if (sample < g_MainChainFrameClockEpoch)
            g_MainChainFrameClockEpoch = sample;

        LeaveCriticalSection(&g_MainChainFrameClockLock);
        --g_MainChainFrameClockDepth;
        return sample - g_MainChainFrameClockEpoch;
    }

    (void)timeBeginPeriod(1);
    const double ticks = static_cast<double>(timeGetTime());
    (void)timeEndPeriod(1);
    if (ticks < g_MainChainFrameClockEpoch)
        g_MainChainFrameClockEpoch = ticks;
    const double result = (ticks - g_MainChainFrameClockEpoch * 1000.0) *
        0.001;

    LeaveCriticalSection(&g_MainChainFrameClockLock);
    --g_MainChainFrameClockDepth;
    return result;
}

} // namespace th10
