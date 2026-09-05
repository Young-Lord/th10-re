#include "PackedArchive.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

extern Win32CriticalSection g_ResourceLoaderLock; // DAT_004922a4
extern u8 g_ResourceLoaderActivityDepth; // DAT_0049231e
extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);
extern "C" void *TH10_STDCALL CreateFileA(const char *path, u32 access,
    u32 share, void *security_attributes, u32 creation, u32 flags,
    void *template_file);
extern "C" i32 TH10_STDCALL CloseHandle(void *handle);
extern "C" i32 TH10_STDCALL CreateDirectoryA(const char *path,
                                              void *security_attributes);
extern "C" i32 TH10_STDCALL WriteFile(void *handle, const void *data,
    u32 byte_count, u32 *written, void *overlapped);
extern "C" u32 TH10_STDCALL GetLastError();
extern "C" u32 TH10_STDCALL FormatMessageA(u32 flags, const void *source,
    u32 message_id, u32 language_id, char *buffer, u32 buffer_size,
    void *arguments);
extern "C" void *TH10_STDCALL LocalFree(void *memory);
extern void MapMainChainDosError(u32 error);

} // namespace

i32 DoesMainChainFileExist(const char *path)
{
    EnterCriticalSection(&g_ResourceLoaderLock);
    ++g_ResourceLoaderActivityDepth;
    void *const handle = CreateFileA(path, 0x80000000U, 1, 0, 3,
        0x8000080U, 0);
    if (handle != reinterpret_cast<void *>(static_cast<i32>(-1))) {
        (void)CloseHandle(handle);
        LeaveCriticalSection(&g_ResourceLoaderLock);
        --g_ResourceLoaderActivityDepth;
        return 1;
    }
    LeaveCriticalSection(&g_ResourceLoaderLock);
    --g_ResourceLoaderActivityDepth;
    return 0;
}

// TH10 0x0044b360. Same native as the packed/direct resource loader
// (LoadPackedResource). Main-chain file reads always pass filesystem_mode 1
// and therefore take the direct-file branch, but the archive branch is kept
// reachable because the binary shares the mode dispatch.
void *LoadMainChainFile(const char *path, u32 *file_size, i32 filesystem_mode)
{
    return LoadPackedResource(path, file_size, filesystem_mode);
}

// TH10 0x0044b540. Write-only probe under the shared resource-loader lock.
// Returns 0 on a full write, -1 when the file cannot be created, and -2 when
// WriteFile reports a short write. The target formats the system error text
// through FormatMessageA and immediately LocalFree-s it without surfacing it.
i32 WriteMainChainFile(const char *path, const void *data, u32 size)
{
    EnterCriticalSection(&g_ResourceLoaderLock);
    ++g_ResourceLoaderActivityDepth;

    void *const handle = CreateFileA(path, 0x40000000U, 1, 0, 2, 0x80U, 0);
    if (handle == reinterpret_cast<void *>(static_cast<i32>(-1))) {
        char *formatted = 0;
        const u32 error = GetLastError();
        (void)FormatMessageA(0x1300U, 0, error, 0x400U,
            reinterpret_cast<char *>(&formatted), 0, 0);
        if (formatted != 0)
            (void)LocalFree(formatted);
        LeaveCriticalSection(&g_ResourceLoaderLock);
        --g_ResourceLoaderActivityDepth;
        return -1;
    }

    u32 written = 0;
    (void)WriteFile(handle, data, size, &written, 0);
    (void)CloseHandle(handle);

    LeaveCriticalSection(&g_ResourceLoaderLock);
    --g_ResourceLoaderActivityDepth;
    if (written != size)
        return -2;
    return 0;
}

i32 CreateMainChainDirectory(const char *path)
{
    if (CreateDirectoryA(path, 0) != 0)
        return 0;
    const u32 error = GetLastError();
    if (error != 0) {
        MapMainChainDosError(error);
        return -1;
    }
    return 0;
}

} // namespace th10
