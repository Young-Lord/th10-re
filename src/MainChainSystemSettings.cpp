#include "Th10Platform.hpp"

namespace th10 {

namespace {

struct Win32LargeInteger {
    long long quad_part;
};

extern u32 g_MainChainScreenSaverWasActive; // DAT_0049251c
extern u32 g_MainChainLowPowerWasActive; // DAT_00492520
extern u32 g_MainChainPowerOffWasActive; // DAT_00492524
extern Win32LargeInteger g_MainChainPerformanceFrequency; // DAT_00492508
extern Win32LargeInteger g_MainChainPerformanceBase; // DAT_00492510

extern "C" i32 TH10_STDCALL SystemParametersInfoA(u32 action, u32 parameter,
                                                    void *data, u32 flags);
extern "C" i32 TH10_STDCALL QueryPerformanceFrequency(
    Win32LargeInteger *value);
extern "C" i32 TH10_STDCALL QueryPerformanceCounter(Win32LargeInteger *value);

} // namespace

void InitializeMainChainSystemSettings()
{
    (void)SystemParametersInfoA(0x10, 0, &g_MainChainScreenSaverWasActive, 0);
    (void)SystemParametersInfoA(0x53, 0, &g_MainChainLowPowerWasActive, 0);
    (void)SystemParametersInfoA(0x54, 0, &g_MainChainPowerOffWasActive, 0);
    (void)SystemParametersInfoA(0x11, 0, 0, 2);
    (void)SystemParametersInfoA(0x55, 0, 0, 2);
    (void)SystemParametersInfoA(0x56, 0, 0, 2);
    (void)QueryPerformanceFrequency(&g_MainChainPerformanceFrequency);
    (void)QueryPerformanceCounter(&g_MainChainPerformanceBase);
}

} // namespace th10
