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
    // +0x1c: when the control is embedded in MainChainContext (+0x62c) this
    // dword is the update status consumed by MainChainUpdate (0x0041ff80).
    i32 update_status_001c;
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

// TH10 0x0044c0e0. Native EAX = control. Publishes the ThreadControl
// vtable (off_4703e4) and zeroes the handle, id, stop flag and the +0x10
// field. The vtable's plain/deleting destructors (0x44c130/0x44c100)
// route through StopThreadControl.
void InitializeThreadControlInPlaceEaxAbi(ThreadControl *control);

} // namespace th10
