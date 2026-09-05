#pragma once

#include <stddef.h>

#include "CallbackScheduler.hpp"
#include "Th10Types.hpp"

namespace th10 {

// Heap object allocated by TH10 0x0041fd00. It is intentionally separate from
// g_MainChainContext, which stores its result at offset 0x768.
struct GlobalLifecycleManager {
    u32 flags;
    u8 unknown_0004[4];
    ChainElem *calc_callback;
    ChainElem *draw_callback;
    void *thread_control_0010;
    void *thread_handle;
    u32 thread_id;
    u32 close_requested;
    u32 thread_running;
    u8 unknown_0024[4];
    void *startup_thread_proc;
    u8 unknown_002c[0x35c];
    void *owned_buffer;
    u8 unknown_038c[0x50];
    void *startup_auxiliary;
    void *startup_result;
    i32 startup_stage;
    i32 draw_stage;
    i32 draw_frame_count;
};

typedef char AssertGlobalLifecycleManagerSize[
    sizeof(GlobalLifecycleManager) == 0x3f0 ? 1 : -1];
typedef char AssertGlobalLifecycleCalcCallbackOffset[
    offsetof(GlobalLifecycleManager, calc_callback) == 0x8 ? 1 : -1];
typedef char AssertGlobalLifecycleDrawCallbackOffset[
    offsetof(GlobalLifecycleManager, draw_callback) == 0xc ? 1 : -1];
typedef char AssertGlobalLifecycleThreadHandleOffset[
    offsetof(GlobalLifecycleManager, thread_handle) == 0x14 ? 1 : -1];
typedef char AssertGlobalLifecycleThreadControlOffset[
    offsetof(GlobalLifecycleManager, thread_control_0010) == 0x10 ? 1 : -1];
typedef char AssertGlobalLifecycleThreadProcOffset[
    offsetof(GlobalLifecycleManager, startup_thread_proc) == 0x28 ? 1 : -1];
typedef char AssertGlobalLifecycleOwnedBufferOffset[
    offsetof(GlobalLifecycleManager, owned_buffer) == 0x388 ? 1 : -1];
typedef char AssertGlobalLifecycleStartupResultOffset[
    offsetof(GlobalLifecycleManager, startup_result) == 0x3e0 ? 1 : -1];
typedef char AssertGlobalLifecycleStartupStageOffset[
    offsetof(GlobalLifecycleManager, startup_stage) == 0x3e4 ? 1 : -1];
typedef char AssertGlobalLifecycleDrawStageOffset[
    offsetof(GlobalLifecycleManager, draw_stage) == 0x3e8 ? 1 : -1];
typedef char AssertGlobalLifecycleDrawFrameCountOffset[
    offsetof(GlobalLifecycleManager, draw_frame_count) == 0x3ec ? 1 : -1];

GlobalLifecycleManager *CreateGlobalLifecycleManager();
void DestroyGlobalLifecycleManager(GlobalLifecycleManager *manager);

// TH10 0x0041f990. The CRT thread argument is deliberately ignored; this
// entry instead uses the already-published lifecycle-manager global.
u32 TH10_CDECL GlobalLifecycleStartupThread(void *unused);
u32 TH10_STDCALL GlobalLifecycleStartupThreadAdapter(void *unused);
i32 InitializeFrontAndBulletResources(); // TH10 0x0041f8d0

} // namespace th10
