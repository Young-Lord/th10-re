// TH10 0x004236f0 — post-run replay-save menu state machine (modes 6..13).
#ifndef TH10_POSTRUNREPLAYSAVEMENU_HPP
#define TH10_POSTRUNREPLAYSAVEMENU_HPP

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004236f0. Native `retn 4` stdcall body: the single stack argument is
// the 0x3ac-byte scheduler/VM record driven by the calculation-record
// dispatcher 0x004223f0 (its mode byte at record+4 selects the body; modes
// 6..13 land here, modes 0..5 go to 0x00422ab0 / 0x00422c80, which are
// separate reconstruction targets). The record embeds two menu cursor
// records (+0x24 and +0xfc), a five-field scaled timer (+0x10), two entity
// handle slots (+0x1d4 / +0x1d8), the typed name buffer (+0x2b4) and 25
// parsed replay slots (+0x1ec).
void RunPostRunReplaySaveMenuStackAbi(void *record);

} // namespace th10

#endif // TH10_POSTRUNREPLAYSAVEMENU_HPP
