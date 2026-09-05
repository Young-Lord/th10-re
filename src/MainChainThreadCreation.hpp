#pragma once

#include "MainChainContext.hpp"
#include "Th10Types.hpp"

namespace th10 {

// Source-level boundaries used by the registration hook. The resource thread
// uses a Win32 stdcall adapter; the background thread retains its CRT cdecl
// callback convention.
void *StartMainChainResourceThread(void *argument, u32 *thread_id);
void *StartMainChainBackgroundThread(MainChainContext *context, u32 *thread_id);

// TH10 0x438c21 creates this worker directly with CreateThread. The worker
// entry is separate from the root's DirectSound initialization body.
void *CreateMainChainSoundWorkerThread();

} // namespace th10
