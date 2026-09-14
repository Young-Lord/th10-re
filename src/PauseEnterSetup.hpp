#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00422ab0. Pause-entry setup body — mode 0 of the 0x004223f0
// calculation-record dispatcher family (see docs/evidence/pause-menu-modes.md;
// the record arrives as the stack argument, native `ret 4`). Sets the record
// mode to 1, arms the embedded timer family, latches the pause bit 0x10 into
// the DAT_00477810 state object's +0x58 word, spawns the two pause-effect VMs
// (menu script 0 into handle B at +0x1d8, HUD script 0x79 into handle A at
// +0x1d4), fires handle A, queues the pause sound and the "Pause" BGM
// command, releases the kind-0x75 child of handle A when [DAT_00477810]+0x5c
// is set, and parks the frame-time scale (DAT_00476f78) into record+0x2c0
// before resetting it to 1.0.
void RunPauseEnterSetupStackAbi(void *record);

} // namespace th10
