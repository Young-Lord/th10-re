#pragma once

#include "Th10Types.hpp"

namespace th10 {

struct MainChainContext;

// TH10 0x00405300. Selects one of the two 0x118-byte camera-work slots of the
// main-chain context (slot 0 at +0x154, slot 1 at +0x26c), publishes it as the
// context draw work (+0x384), refreshes the D3D frame state from it and
// installs its viewport into the context draw target. Native inputs are
// ESI = context and EBX = slot index.
void SelectMainChainDrawWork(MainChainContext *context, u32 index);

// TH10 0x00402850. First title-screen draw scheduler record (registered by the
// state constructor 0x00402230 through the 0x403060 adapter, draw chain):
// restores the state's camera snapshot, clears the z-buffer and the menu
// region {32,16,416,464}, arms the fade-in overlay and timer, then renders
// scene channels 0..7 and the eight background VM records at +0x180 while the
// fade-in timer runs. Native ABI is stdcall ret 4 with the 0x2b64-byte
// title-screen state as the only stack argument; returns 1.
i32 RunTitleScreenDrawPass0StackAbi(void *state);

// TH10 0x00402ca0. Second title-screen draw scheduler record (0x403070
// adapter): renders the large-render-owner kind chains 0x11/0x12, scene
// channels 8..11, drains the fade timer toward zero and applies the fade-end
// flag update. Same native ABI; returns 1.
i32 RunTitleScreenDrawPass1StackAbi(void *state);

} // namespace th10
