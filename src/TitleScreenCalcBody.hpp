#pragma once

#include "Th10Platform.hpp"
#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00418190. Title-screen calculation-record body (forwarded to by the
// one-instruction ECX callback at 0x004187c0). Native ABI: one stack argument
// (the 0x60-byte title screen), ret 4, result in EAX.
//
// Frame value at +0x14 drives the body:
//   - 0: title-screen idle/startup frame. While the primary title state
//     (DAT_004776e4) exists it only arms the 30/60-frame score anims and
//     expires the HUD overlay handle; otherwise it tears the 0x800 flag down
//     and performs the whole game-start reset (manager cleanups, the
//     0x21cea0-byte bullet-manager wipe, "main" ECL script creation, HUD
//     overlay re-arm, option-record rebuild, scheduler-record flag set, BGM
//     mode select and handle expiry).
//   - 30: restart frame gated on the 0x800 flag; repeats the same game-start
//     reset plus the "dummy" BGM opcode-3/4 command and the +0x10 timer tick.
//   - anything else: only the common epilogue.
//
// The common epilogue frees the primary title state when its +0x2a18 block
// flag bit 3 is set, handles the pending-shutdown (bit 4 -> bit 0x80) and
// end-of-frame paths (shared-status gate publication, the 2940/3000 overlay
// and game-over triggers, the 0x417040 late update, the play-time counter
// increment and the +0x10 timer forward tick), returning 1 or 3.
i32 TH10_STDCALL RunTitleScreenCalcBodyStackAbi(void *title_screen);

} // namespace th10
