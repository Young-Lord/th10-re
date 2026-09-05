#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0044a5f0. Native register ABI: ECX = config slot index into the
// 0x6a-stride input record bank at 0x474e30 (also forwarded as the joystick
// config slot), EDX = second fastcall argument (unused), stack args cleaned
// by the caller. Reads the keyboard state (DirectInput keyboard device at
// 0x491c38 when 0x491ff4 bit 0x200 is set, GetKeyboardState otherwise),
// builds the hardcoded 15-bit key mask, ORs in the joystick poll of config
// slot ECX / joystick 0, then refreshes the 0x6a-stride input record:
//   +0x00 current mask, +0x02 previous mask, +0x04 held-repeat mask,
//   +0x06 pressed-this-frame, +0x08 released-this-frame,
//   +0x0a..+0x29 sixteen u16 repeat counters (threshold 0x1a, reload -8).
u32 UpdateKeyConfigInputRecordEcxAbi(u32 config_slot);

// TH10 0x0044a9d0. Byte-twin of 0x0044a5f0 (identical key tables and record
// epilogue) with one difference: the joystick poll always uses config slot 0
// instead of the ECX slot. The ECX slot still selects the record that is
// refreshed.
u32 UpdateKeyConfigInputRecordSlotZeroPollEcxAbi(u32 config_slot);

// TH10 0x0044ad30. Mask-only variant used by menu screens: same keyboard
// gate and key tables, poll of config slot 0 / joystick 0, no input record
// update. Both fastcall argument registers are dead in the native body.
u32 PollKeyConfigMenuInputMask();

} // namespace th10
