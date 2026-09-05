#include "MainChainStartupGlobals.hpp"

namespace th10 {

MainChainStartupGlobalStorage g_MainChainStartupGlobals; // TH10 DAT_00491d7c

namespace {

void ResetLeadingWords(u32 *words)
{
    words[0] = 0;
    words[1] = 0;
    words[2] = 0x447a0000;
    words[3] = 0;
    words[4] = 0;
    words[5] = 0;
    words[6] = 0;
    words[7] = 0x3f800000;
    words[8] = 0;
    words[15] = 0;
    words[16] = 0;
    words[17] = 0;
    words[18] = 0x3f060a92;
}

} // namespace

// TH10 0x004216f0. Intentional sparse initialization: all unmentioned words
// retain their old values exactly as they do in the original straight-line
// store sequence.
void ResetMainChainStartupGlobals()
{
    ResetLeadingWords(g_MainChainStartupGlobals.block_a.before_viewport);
    g_MainChainStartupGlobals.block_a.viewport.x = 32;
    g_MainChainStartupGlobals.block_a.viewport.y = 16;
    g_MainChainStartupGlobals.block_a.viewport.width = 384;
    g_MainChainStartupGlobals.block_a.viewport.height = 448;
    g_MainChainStartupGlobals.block_a.viewport.min_z = 0.0f;
    g_MainChainStartupGlobals.block_a.viewport.max_z = 1.0f;
    g_MainChainStartupGlobals.block_a.after_viewport[0] = 0;

    ResetLeadingWords(g_MainChainStartupGlobals.block_b.before_viewport);
    g_MainChainStartupGlobals.block_b.viewport.x = 0;
    g_MainChainStartupGlobals.block_b.viewport.y = 0;
    g_MainChainStartupGlobals.block_b.viewport.width = 640;
    g_MainChainStartupGlobals.block_b.viewport.height = 480;
    g_MainChainStartupGlobals.block_b.viewport.min_z = 0.0f;
    g_MainChainStartupGlobals.block_b.viewport.max_z = 1.0f;
    g_MainChainStartupGlobals.block_b.trailing_word = 1;
}

} // namespace th10
