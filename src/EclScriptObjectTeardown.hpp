#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0040dae0. Native one stack argument (ret 4) = the 0x2518-byte ECL
// script object created by CreateEclScriptObjectEaxStackAbi. Plants the
// destruction vtable 0x46d0d8, unlinks the embedded node at +0x116c from
// the DAT_00477704 state's list (+0x58 head, +0x5c tail, +0x60 count),
// clears the published-id slot, soft-releases the ten entity ids published
// at +0x10fc, clears the player-block back references, and frees the
// script-name list at +0x1034. The record itself is freed by the caller.
void DestroyEclScriptObjectInPlaceStackAbi(void *record);

// TH10 0x0040cc50. Native __thiscall ECX = the record, one stack argument
// (ret 4) whose bit 0 requests the outer free (the scalar deleting
// destructor vtable slot +0x10 of vtable 0x46d0c0). Returns the record.
void *ReleaseEclScriptObjectDeletingEcxStackAbi(void *record /* ECX */,
                                                u32 delete_flags);

} // namespace th10
