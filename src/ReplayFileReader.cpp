// Replay sequential-file reader (TH10 0x0044b6b0 / 0x0044b790 /
// 0x0044b7e0). The reader trio behind LoadReplayViewRecord: open under the
// resource-loader lock, hand out one raw block per call, close the handle.
// The paired write path lives in ReplaySave.cpp (0x0044b620).
#include "ReplayFileReader.hpp"

#include <stdlib.h>

#include "Th10Platform.hpp"

namespace th10 {

namespace {

// ---- shared globals (definitions pending the final link) ----------------

extern void *g_ReplayFileHandle;        // TH10 DAT_00474c38 (HANDLE slot)
extern u8 g_ReplayFileLockDepth;        // TH10 byte_49231e (nesting byte)
extern u8 g_ReplayFileLock[0x18];       // TH10 stru_4922a4 (CRITICAL_SECTION)

// Win32 imports used verbatim (x86 stdcall).
extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);
extern "C" void *TH10_STDCALL CreateFileA(const char *path, u32 access,
    u32 share, void *security_attributes, u32 creation, u32 flags,
    void *template_file);
extern "C" i32 TH10_STDCALL CloseHandle(void *handle);
extern "C" i32 TH10_STDCALL ReadFile(void *handle, void *buffer,
    u32 byte_count, u32 *bytes_read, void *overlapped);
extern "C" u32 TH10_STDCALL GetLastError();
extern "C" u32 TH10_STDCALL FormatMessageA(u32 flags, const void *source,
    u32 message_id, u32 language_id, char *buffer, u32 buffer_size,
    void *arguments);
extern "C" void *TH10_STDCALL LocalFree(void *memory);

} // namespace

i32 TH10_STDCALL OpenReplayFileForReadStdcallAbi(const char *path)
{
    EnterCriticalSection(&g_ReplayFileLock[0]);
    ++g_ReplayFileLockDepth;

    void *const handle = CreateFileA(path, 0x80000000U, 1U, 0, 3U,
        0x8000080U, 0);
    g_ReplayFileHandle = handle;
    if (handle != reinterpret_cast<void *>(static_cast<i32>(-1)))
        return 0;

    // Native failure path: format the Win32 error text with
    // FORMAT_MESSAGE_FROM_SYSTEM | FROM_HMODULE | ALLOCATE_BUFFER
    // (0x1300, language 0x400) and immediately LocalFree it — the message
    // is never surfaced anywhere.
    char *message = 0;
    FormatMessageA(0x1300U, 0, GetLastError(), 0x400U,
                   reinterpret_cast<char *>(&message), 0, 0);
    LocalFree(message);

    LeaveCriticalSection(&g_ReplayFileLock[0]);
    --g_ReplayFileLockDepth;
    return -1;
}

void *ReadReplayFileBlockEdiAbi(u32 byte_count)
{
    // `byte_count` travels in EDI in the native binary.
    if (g_ReplayFileHandle == reinterpret_cast<void *>(static_cast<i32>(-1)))
        return 0;

    u8 *const block = static_cast<u8 *>(malloc(byte_count));
    if (block == 0) {
        // Allocation failure: the native closes the file but does not
        // touch the critical section or the nesting byte.
        (void)CloseHandle(g_ReplayFileHandle);
        return 0;
    }

    u32 bytes_read = 0;
    (void)ReadFile(g_ReplayFileHandle, block, byte_count, &bytes_read, 0);
    // The ReadFile result and the transferred count are both ignored; the
    // caller validates the block contents itself ("t10r" header checks).
    return block;
}

i32 CloseReplayFileReader()
{
    if (g_ReplayFileHandle != reinterpret_cast<void *>(static_cast<i32>(-1))) {
        (void)CloseHandle(g_ReplayFileHandle);
        LeaveCriticalSection(&g_ReplayFileLock[0]);
        --g_ReplayFileLockDepth;
    }
    return 0;
}

} // namespace th10
