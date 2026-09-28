#pragma once

#include "Th10Platform.hpp"
#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0044b6b0. Native stdcall (ret 4). Opens a replay file for the
// sequential reader under the shared resource-loader critical section
// (DAT_004922a4, nesting byte DAT_0049231e); GENERIC_READ / share-read /
// OPEN_EXISTING / FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN. The
// handle is published to the global slot DAT_00474c38. Returns 0 on
// success, -1 after formatting the Win32 error text (immediately
// LocalFree'd, never surfaced) and releasing the lock.
i32 TH10_STDCALL OpenReplayFileForReadStdcallAbi(const char *path);

// TH10 0x0044b790. Native passes the byte count in EDI. Returns 0 when no
// replay file is open; otherwise mallocs `byte_count` bytes (closing the
// file and returning 0 when the allocation fails), ReadFile's once into it
// (the result is ignored) and hands the buffer out. The caller owns the
// buffer until CloseReplayFileReader/FinishReplayReadBlock.
void *ReadReplayFileBlockEdiAbi(u32 byte_count);

// TH10 0x0044b7e0. Closes the open replay handle (if any) and releases the
// resource-loader critical section; always returns 0. Note this native
// closes the FILE, not the handed-out block — the block stays with the
// caller and is freed separately.
i32 CloseReplayFileReader();

} // namespace th10
