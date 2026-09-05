#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0040cfb0. Native register ABI: EAX = source descriptor (three
// leading dwords copied to record+0x1094, dwords 3/4/5 to record+0x23f8 /
// +0x2408 / +0x23fc, bit 0 of dword 6 into +0x2480 bit 0x800, bit 0 of
// dword 7 into +0x2480 bit 0x40000, and 0x20 bytes at descriptor+0x20
// copied to record+0x1138), two stack arguments (ret 8): the list owner
// receiving the record node at +0x58/+0x5c/+0x60/+0x64 and the ctor
// argument forwarded to the 0x40d830 constructor. Allocates the
// 0x2518-byte ECL script object (vtable 0x46d0c0 installed by the
// constructor) and returns it in EAX; a failed allocation keeps writing
// through the null pointer (native quirk, preserved).
void *CreateEclScriptObjectEaxStackAbi(const u32 *descriptor,
                                       void *list_owner, i32 ctor_arg);

// TH10 0x0040d830 (boundary). Native userpurge: ESI = record, stack arg =
// ctor argument; installs the 0x46d0c0 vtable and initializes the record.
void *ConstructEclScriptObjectEsiStackAbi(void *record, i32 ctor_arg);

// TH10 0x0040dc80 (boundary). Native stack-arg ret 4: early-outs when
// [arg+0x1444] bit 0x400 is already set, otherwise sets it and runs the
// ECL script setup over the +0x1044 sub-record.
i32 RunEclScriptSetupStackAbi(void *sub_record);

} // namespace th10
