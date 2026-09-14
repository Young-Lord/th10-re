#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0041b8e0. Native input is the effect pool base in EAX (plain
// ret, EAX = 1). Walks the 0x896 slots (stride 0x3f0, first record at
// pool+0x14): each live slot republishes the script position with the
// +224/+16 playfield offset into its animation VM record, fades the
// alpha byte at vm+0x2ff when the scripted y drops below 8.0 (clamping
// the VM y to 24.0), re-spawns the VM (0x0043e5a0) when the script word
// at vm+0x384 no longer matches effect script id + 0x157/+0x161, and
// finishes with the render-mode dispatch 0x004451c0 on the render owner.
i32 TickEffectPoolSlots(void *pool);

// TH10 0x0041c330. Native input is the effect-node container in EDI
// (plain ret, EAX = 1). Walks the doubly linked node list at
// container+0x18 (nodes: +0 vtable, +4/+8 links, +0xc kind, +0x14 timer,
// +0x18 accumulator, +0x1c rate pointer, +0x50 finish latch): a non-zero
// latch (or kind 1, or an update returning non-zero) runs the node's
// finish vtable slot (+0x10) and unlinks/frees the node; a surviving node
// advances its {timer, accumulator, rate} record with the shared
// 0.99..1.01 window semantics. Tail pointer (container+0x434) and count
// (container+0x438) are maintained on removal.
i32 TickEffectNodeList(void *container);

} // namespace th10
