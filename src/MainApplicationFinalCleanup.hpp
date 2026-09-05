#pragma once

namespace th10 {

// Semantic subset of TH10 0x00438ad0 final-exit path at 0x00438efa.
// It deliberately excludes the local-status retry path.
void FinalizeMainChainApplication();

// Semantic subset of the local-status==2 retry branch in TH10 0x00438ad0.
// The enclosing application loop owns the subsequent jump back to startup.
void PrepareMainChainStartupRetry();

} // namespace th10
