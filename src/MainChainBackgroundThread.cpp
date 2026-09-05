#include "MainChainContext.hpp"
#include "MainChainBackgroundThread.hpp"
#include "MainChainInput.hpp"

namespace th10 {

namespace {

extern MainChainContext g_MainChainContext; // TH10 DAT_00491c28
extern u32 g_MainChainRuntimeFlags; // TH10 DAT_00491ff4

} // namespace

// TH10 0x0043ba90. __beginthreadex invokes this as a C-convention function;
// its supplied argument is intentionally ignored in favor of the global.
u32 TH10_CDECL MainChainBackgroundThread(void *)
{
    g_MainChainRuntimeFlags &= ~0x600U;
    InitializeMainChainInputFromEax(&g_MainChainContext);

    g_MainChainRuntimeFlags = (g_MainChainRuntimeFlags & ~0x200U) |
        (g_MainChainContext.input_keyboard_0010 != 0 ? 0x200U : 0U);
    g_MainChainRuntimeFlags = (g_MainChainRuntimeFlags & ~0x400U) |
        (g_MainChainContext.input_controller_0014 != 0 ? 0x400U : 0U);
    return g_MainChainRuntimeFlags;
}

u32 TH10_STDCALL MainChainBackgroundThreadAdapter(void *unused)
{
    return MainChainBackgroundThread(unused);
}

} // namespace th10
