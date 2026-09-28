// TH10 0x0043e9d0. Loads the label map ("<script>.map") that accompanies an
// ECL/ANM script, rebasing every stored dword offset against the buffer
// base. Record fields used: +0x110/+0x114 map-entry counts (68/69 as dword
// slots), +0x12c map pointer (slot 75).
#include <stdio.h>
#include <string.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

extern "C" i32 TH10_STDCALL sprintf(char *buffer, const char *format, ...);

// TH10 0x0044b4d0 / 0x0044b360 (registered in MainChainFileProbe.cpp).
extern bool DoesMainChainFileExist(const char *path);
extern void *LoadMainChainFile(const char *path, u32 *file_size,
                               i32 filesystem_mode);

} // namespace

// Native ESI = script record, stack = script path (ret 4).
void LoadScriptLabelMapEsiStackAbi(void *record, const char *script_path)
{
    u32 *words = static_cast<u32 *>(record);

    char map_path[260];
    (void)sprintf(map_path, "%s", script_path);
    char *end = map_path + strlen(map_path);
    *(end - 3) = 'm';
    *(end - 2) = 'a';
    *(end - 1) = 'p';

    if (DoesMainChainFileExist(map_path)) {
        u32 map_size = 0;
        const u32 map_base = reinterpret_cast<u32>(
            LoadMainChainFile(map_path, &map_size, 1));
        words[75] = map_base;                    // +0x12c
        const u32 entry_count = words[68] + words[69]; // +0x110 + +0x114
        for (u32 index = 0; index < entry_count; ++index)
            *reinterpret_cast<u32 *>(map_base + 4U * index) += map_base;
        return;
    }
    words[75] = 0;
}

} // namespace th10
