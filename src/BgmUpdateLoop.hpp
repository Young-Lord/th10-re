#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 DAT_0049795c. DirectSound attenuation (hundredths of a dB) recomputed
// by the audio-options presentation refresh; negative values attenuate.
extern i32 g_BgmDirectSoundAttenuation;

// TH10 0x0042e5a0 semantic body. The native entry receives the audio-options
// game-manager state in EDI (its +0x2c8 field caches the options screen text
// container handle); callers use this typed interface and leave that register
// ABI to a thunk.
//
// Publishes the current BGM/SE volume bytes (DAT_00491d68/491d69) into the
// volume-scale globals, queues the "SetVol" BGM command (opcode 8, track 0),
// recomputes the DirectSound attenuation, and rebinds the twelve options
// screen volume digit glyphs plus their leading-zero visibility flags.
void RefreshAudioOptionPresentationEdiAbi(void *options_state);

} // namespace th10
