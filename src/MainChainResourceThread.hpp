#pragma once

#include "MainChainRuntime.hpp"
#include "Th10Types.hpp"

namespace th10 {

enum { kMainChainWaveCount = 37 };

// Semantic producer behind the nonstandard native entry at TH10 0x0043d080.
void PreloadMainChainWaveResources(TransitionRootPartial *root);

// Conventional Win32 boundary used when registering the semantic producer.
u32 TH10_STDCALL MainChainResourceThreadAdapter(void *unused);

// Narrow raw-image subset used where only producer-owned images are relevant.
void DestroyUnconsumedMainChainWaveImages(TransitionRootPartial *root);
// TH10 0x0043d120. Releases all root-held DirectSound/BGM/raw-image resources.
void DestroyTransitionRootSoundResources(TransitionRootPartial *root);

} // namespace th10
