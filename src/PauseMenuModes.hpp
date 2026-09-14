// TH10 0x00422c80 / 0x00422c30 — in-game pause menu state machine.
#ifndef TH10_PAUSEMENUMODES_HPP
#define TH10_PAUSEMENUMODES_HPP

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00422c80. Native `retn 4` stdcall body: the stack argument is the
// 0x3ac-byte scheduler/VM record driven by the calculation-record dispatcher
// 0x004223f0 (its mode byte at record+4 selects the body; modes 1..5 land
// here, mode 0 goes to 0x00422ab0 and modes 6..13 to 0x004236f0). The record
// embeds the cursor record (+0x24 value / +0x28 copy / +0x2c maximum /
// +0xf4 wrap flag), the five-field scaled timer (+0x10), three entity
// handle slots (+0x1d4 / +0x1d8 / +0x1dc) and the float saved to
// DAT_00476f78 at +0x2c0.
void RunPauseMenuModesStackAbi(void *record);

// TH10 0x00422c30. Native ESI = record. Resume path of the pause menu:
// clears the pause bit 0x10 of the 0x477810 state object's +0x58 word,
// queues the "UnPause" BGM command on the 0x492590 sound context, releases
// the entity bound to the record's +0x1dc handle slot (slot cleared) and
// copies the record float at +0x2c0 back into DAT_00476f78.
void ResumeGameFromPauseEsiAbi(void *record);

} // namespace th10

#endif // TH10_PAUSEMENUMODES_HPP
