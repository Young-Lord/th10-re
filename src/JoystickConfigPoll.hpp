#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0044a190. Native register ABI: ECX = accumulated key bitmask (the
// full 32-bit register participates in the ORs), EDX = dead stack slot,
// two stack arguments = config slot index into the 0x6a-stride bank at
// 0x474e30 and joystick index (0/1); ret 8. Only the low word of the
// result is meaningful to callers, but the native success path returns the
// full combined word while the failure paths only overwrite the low word
// of the failing API result (preserved here).
u32 PollJoystickConfigBitmaskEcxStackAbi(u32 accumulated,
                                         u32 config_slot,
                                         u32 joystick_index);

// TH10 0x0044a4e0. Native usercall: ECX = DirectInput device index,
// ESI = value forwarded to the Poll call; refreshes the 224-byte button
// byte array at 0x497bb0 (DirectInput path: Poll +0x64 with the forwarded
// value, GetDeviceState 0x110 into a local, copy the first 0xe0 bytes;
// winmm path: joyGetPosEx(joystick 0), each set button bit writes 0x80 at
// its bit index). Returns the array base.
u8 *PollJoystickButtonBytesEcxEsiAbi(u32 device_index, u32 poll_value);

} // namespace th10
