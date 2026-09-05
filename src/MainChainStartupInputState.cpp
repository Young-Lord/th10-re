#include "Th10Platform.hpp"

namespace th10 {

namespace {

struct MainChainJoyInfoEx {
    u32 size;
    u32 flags;
    u32 unknown_0008[11];
};

typedef char AssertMainChainJoyInfoExSize[
    sizeof(MainChainJoyInfoEx) == 0x34 ? 1 : -1];

extern u8 g_MainChainJoystickCaps[0x194]; // DAT_004918b8
extern i32 TH10_STDCALL joyGetPosEx(u32 device, MainChainJoyInfoEx *info);
extern i32 TH10_STDCALL joyGetDevCapsA(u32 device, void *caps, u32 bytes);
extern i32 TH10_STDCALL GetKeyboardState(u8 *state);
extern i32 TH10_STDCALL SetKeyboardState(const u8 *state);
extern void AppendMainChainNoJoystickDiagnostic();

} // namespace

bool ProbeMainChainJoystickAvailability()
{
    MainChainJoyInfoEx info;
    info.size = sizeof(info);
    info.flags = 0xff;
    if (joyGetPosEx(0, &info) == 0 && joyGetPosEx(1, &info) == 0) {
        AppendMainChainNoJoystickDiagnostic();
        return true;
    }

    (void)joyGetDevCapsA(0, g_MainChainJoystickCaps,
        sizeof(g_MainChainJoystickCaps));
    return false;
}

void ReleaseMainChainKeyboardPressedState()
{
    u8 state[256];
    (void)GetKeyboardState(state);
    for (u32 index = 0; index != sizeof(state); ++index)
        state[index] &= 0x7f;
    (void)SetKeyboardState(state);
}

} // namespace th10
