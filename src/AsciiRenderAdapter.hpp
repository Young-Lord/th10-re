#pragma once

#include <stddef.h>

#include "Th10Types.hpp"

namespace th10 {

struct Float3 {
    float x;
    float y;
    float z;
};

struct AsciiTextEntry {
    char text[0x40];
    Float3 position;
    u32 color;
    float scale_x;
    float scale_y;
    u32 unknown_0058;
    u32 gui_mode;
    u32 selected;
    u32 text_mode;
};

struct AsciiManagerAdapterSlice {
    u8 unknown_0000[0x76c];
    AsciiTextEntry primary_entries[256];
    u8 unknown_6f6c[0x1a00];
    i32 primary_count;
    i32 secondary_count;
    u32 color;
    float scale_x;
    float scale_y;
    u32 gui_mode;
    u32 unknown_8984;
    u32 text_mode;
    u32 glyph_height_units;
};

typedef char AssertAsciiTextEntrySize[sizeof(AsciiTextEntry) == 0x68 ? 1 : -1];
typedef char AssertAsciiPrimaryEntriesOffset[
    offsetof(AsciiManagerAdapterSlice, primary_entries) == 0x76c ? 1 : -1];
typedef char AssertAsciiPrimaryCountOffset[
    offsetof(AsciiManagerAdapterSlice, primary_count) == 0x896c ? 1 : -1];
typedef char AssertAsciiColorOffset[
    offsetof(AsciiManagerAdapterSlice, color) == 0x8974 ? 1 : -1];

void QueuePrimaryFpsTextSelected(AsciiManagerAdapterSlice *manager,
                                 u32 color, float sampled_fps);

} // namespace th10
