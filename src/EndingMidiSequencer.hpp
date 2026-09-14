// TH10 0x0043b110 — ending script event interpreter (standard MIDI file
// event reader of the ending music player, ending.cpp cluster).
#ifndef TH10_ENDINGMIDISEQUENCER_HPP
#define TH10_ENDINGMIDISEQUENCER_HPP

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0043b110. Native `retn 8` stdcall body: (context, event block).
// Decodes one event of the block's MIDI event stream, updates the ending
// player's per-channel state (key bitmap, per-channel controller bytes)
// and forwards voice messages to the MIDI out device; the block's delta
// time is advanced by the event's variable-length quantity. The 0xff 0x2f
// meta event deactivates the block (block+0x00 cleared) and ends the
// interpretation.
void InterpretEndingMidiEventBlockStackAbi(void *context, void *block);

} // namespace th10

#endif // TH10_ENDINGMIDISEQUENCER_HPP
