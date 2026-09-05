#pragma once

#include <stddef.h>

#include "Th10Types.hpp"

namespace th10 {

struct ThreadControl {
    void *marker;
    void *thread_handle;
    u32 thread_id;
    u32 stop_requested;
    u32 field_0010;
    u8 unknown_0014[4];
    void *thread_entry;
    u8 unknown_001c[4];
};

typedef char AssertThreadControlSize[sizeof(ThreadControl) == 0x20 ? 1 : -1];
typedef char AssertThreadControlHandleOffset[
    offsetof(ThreadControl, thread_handle) == 0x4 ? 1 : -1];
typedef char AssertThreadControlStopOffset[
    offsetof(ThreadControl, stop_requested) == 0xc ? 1 : -1];
typedef char AssertThreadControlEntryOffset[
    offsetof(ThreadControl, thread_entry) == 0x18 ? 1 : -1];

// TH10 0x0044c150. Requests cooperative termination then waits in 200 ms
// intervals, sleeping 1 ms between timeouts; it never forcibly terminates.
void StopThreadControl(ThreadControl *control);

} // namespace th10
