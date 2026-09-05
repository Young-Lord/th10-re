#pragma once

#include "MainChainRender.hpp"

namespace th10 {

struct MainChainStartupGlobalBlockA {
    u32 before_viewport[0x33];
    D3DViewport viewport;
    u32 after_viewport[0x0d];
};

struct MainChainStartupGlobalBlockB {
    u32 before_viewport[0x33];
    D3DViewport viewport;
    u32 trailing_word;
};

struct MainChainStartupGlobalStorage {
    MainChainStartupGlobalBlockA block_a;
    MainChainStartupGlobalBlockB block_b;
};

typedef char AssertMainChainStartupBlockASize[
    sizeof(MainChainStartupGlobalBlockA) == 0x118 ? 1 : -1];
typedef char AssertMainChainStartupBlockBSize[
    sizeof(MainChainStartupGlobalBlockB) == 0x0e8 ? 1 : -1];
typedef char AssertMainChainStartupStorageSize[
    sizeof(MainChainStartupGlobalStorage) == 0x200 ? 1 : -1];

void ResetMainChainStartupGlobals(); // TH10 0x004216f0

} // namespace th10
