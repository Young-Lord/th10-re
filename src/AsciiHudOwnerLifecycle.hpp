#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00413810. EH-scoped constructor of the 0x9ED0-byte ASCII HUD
// owner record: runs the six 0x3AC-byte VM-record array constructors
// (counts 10/10/9/4/2/7 at +0x10/+0x24c8/+0x4980/+0x6a8c/+0x793c/
// +0x8094), clears the nine ready flags of the +0x9a48 block, wipes that
// block (0x3ac bytes) with the -1 word at +0x9dcc, clears +0x9e70 bit 0,
// wipes the whole 0x9ED0 record, sets word 2 at +0 and publishes the
// record into TH10 DAT_004770C. Retained as a boundary for the native
// entry ABI.
void *ConstructAsciiHudOwnerRecords(void *record);

// TH10 0x00414730 caller body 0x00414370. Frees the ASCII HUD owner's
// render-owner slot, glyph scratch, text mirror and tracked entities.
// Native stdcall with the record in the first stack slot.
void ReleaseAsciiHudOwnerResources(void *record);

} // namespace th10
