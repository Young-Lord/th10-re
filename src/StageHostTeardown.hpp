#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0040a1a0. Native one stack argument (ret 4) = the 0x688-byte
// stage-host object (created by 0x0040a3c0 / 0x0040a040, published through
// DAT_004776f8). Composite destructor: releases the ECL select-menu name
// table, removes the host's two scheduler records (+0x8, +0xc) under the
// global scheduler lock, destroys and frees every published manager global
// (DAT_0047770c, DAT_00477834, DAT_004776f0, DAT_00477704, DAT_004776ec,
// DAT_00477818, DAT_00477840), frees the +0x620 buffer and clears the
// published DAT_004776f8 holder, and finally stops the embedded thread
// control block at +0x10.
void DestroyStageHostObjectStackAbi(void *host);

} // namespace th10
