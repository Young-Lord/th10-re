// Result-screen state-machine controller and stat-field setter (TH10
// 0x0042f540 / 0x00430250), the two callers of the digits presenter
// 0x0042f8b0 in ResultScreenDigits.cpp.
#ifndef TH10_RESULTSCREENUPDATE_HPP
#define TH10_RESULTSCREENUPDATE_HPP

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0044a4e0. Poll the joystick / DirectInput device selected by the
// ECX index and return the 32-byte pressed-button bank at DAT_00497bb0
// (bit n of the button word -> byte n = 0x80). Link boundary; not yet
// reconstructed.
const u8 *PollJoystickButtonBytesEcxAbi(u32 device_index);

// TH10 0x0042f540. Result-screen state-machine update (native EBX = game
// manager/result-screen state; caller 0x0042cdf0; always returns 1 in EAX).
// Switches on the sub-state at +0x20:
//   0: arm the +0x24 cursor record with the +0x2c maximum (7), spawn the
//      script-2 result-screen entity, set sub-state 1, copy the five u16
//      stat fields from DAT_00474e88/8c/98 into +0x59cc..+0x59d4 and
//      refresh the digit glyphs, then fall through into the sub-state 1
//      body.
//   1: once the +0x2b4 timer exceeds 6, set sub-state 2, run the
//      +0x2cc entity with stop word 3 and queue the slot-2 stop word
//      cursor + 17.
//   2: menu cursor handling over the record at +0x24 (shift by the
//      0x474e36/0x474e34 masks), change sound, joystick-button stat-field
//      pick, and the page 5/6 commit paths that refresh the digits or
//      publish the stat fields back to the globals and snapshot them to
//      DAT_00491d4c.
//   4: once the +0x2b4 timer reaches 10, set state 4 and finalize the
//      cursor record.
i32 UpdateResultScreenStateMachineEbxAbi(void *game_manager);

// TH10 0x00430250. Set the u16 stat field `field_index` (0..4) at
// state + 0x59cc + 2*field_index to `value` (native EAX = state, EDX =
// value, ECX = field index; returns the state in EAX). Fields that
// already hold `value` (compared as signed 16-bit) are overwritten with
// the field's previous raw value first, then the digit glyphs are
// refreshed (0x0042f8b0) and boundary channel 0xa is reserved.
void *SetResultScreenStatFieldEaxEdxEcxAbi(void *state, i32 value,
                                           i32 field_index);

} // namespace th10

#endif
