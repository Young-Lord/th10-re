#include "Th10Platform.hpp"

namespace th10 {

namespace {

extern "C" u32 TH10_STDCALL GetModuleFileNameA(void *module, char *path,
                                                 u32 capacity);
extern void *LoadMainChainFile(const char *path, u32 *file_size,
                               i32 filesystem_mode); // TH10 0x0044b360
extern void FreeMainChainFileBuffer(void *buffer); // TH10 0x00452422
extern u32 g_MainChainExecutableSize; // DAT_00492384
extern u32 g_MainChainExecutableChecksum; // DAT_00492380

} // namespace

i32 CalculateMainChainExecutableChecksum()
{
    char path[0x105];
    if (GetModuleFileNameA(0, path, sizeof(path)) == 0)
        return -1;

    u32 file_size;
    const u8 *const file_data = static_cast<const u8 *>(
        LoadMainChainFile(path, &file_size, 1));
    if (file_data == 0)
        return -1;

    i32 checksum = 0;
    const i32 dword_count = static_cast<i32>((file_size +
        (static_cast<i32>(file_size) >> 31 & 3U)) >> 2) - 1;
    for (i32 index = 0; index < dword_count; ++index)
        checksum += reinterpret_cast<const i32 *>(file_data)[index];

    FreeMainChainFileBuffer(const_cast<u8 *>(file_data));
    g_MainChainExecutableSize = file_size;
    g_MainChainExecutableChecksum = static_cast<u32>(checksum);
    return checksum;
}

} // namespace th10
