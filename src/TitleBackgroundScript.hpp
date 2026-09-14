#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Title-screen background scene script: a time-stamped record stream that
// drives the demo background (positions, interpolators, color fade, VM
// binds) over the title-screen state block. The VM records live at
// state+0x180 (8 records, stride 0x3ac) plus three aux VMs at +0x1f08 /
// +0x22b4 / +0x2660.

// TH10 0x403c80. Native stdcall ret 4 (state block; EAX also carries it on
// entry). Processes every script record whose time stamp is <= the frame
// count (records: {i32 time, i16 opcode, i16 next delta, i32 arg size,
// args}), ticks the frame-count timer (rate pointer at +0x44) and applies
// the active interpolators. Returns 0.
i32 RunTitleBackgroundScriptStackAbi(void *state);

// TH10 0x402720. Native EAX = title-screen state block (register ABI at
// the boundary). Per-frame calc body for the background scene: gate check,
// camera vector normalize, HUD palette latch, the script stream above, the
// eight +0x180 VM records, the three aux VMs under a forced 1.0 rate, and
// the 0x46-dword pause-state copy. Returns 1.
i32 UpdateTitleBackgroundCalcBodyEaxAbi(void *state);

// TH10 0x403850. Native EBX = title-screen state block, stack = filename.
// Load step for the same state block: appends the filename to the global
// 0x497c38 path buffer, loads the script file, copies it into a private
// buffer, resolves the manager-work resource (slot = (state+0x2a30 & 1)+4),
// rebases the script pointer tables (+0x14/+0x18/+0x1c) and allocates the
// (i16)base[2] * 0x3ac VM-record array into +0x17c. Returns 0/-1.
i32 LoadTitleBackgroundScriptEbxStackAbi(void *state, const char *filename);

} // namespace th10
