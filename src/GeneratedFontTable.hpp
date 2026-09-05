#pragma once

#include "Th10Types.hpp"

namespace th10 {

typedef void *GdiHandle;

struct GeneratedSurface {
    u8 generated[0x100];
    i32 pixel_format_id;
    i32 width;
    i32 height;
    i32 image_bytes;
    i32 pitch_bytes;
    GdiHandle memory_dc;
    GdiHandle previous_selection;
    GdiHandle bitmap;
    void *pixel_bits;
};

typedef char AssertGeneratedSurfaceSize[sizeof(GeneratedSurface) == 0x124 ? 1 : -1];

bool ConstructGeneratedSurfaceEaxAbi(GeneratedSurface *surface,
                                     i32 width, i32 height,
                                     i32 pixel_format_id); // TH10 0x00436af0
void DestroyGeneratedSurfaceEsiAbi(GeneratedSurface *surface); // 0x436a30
void InitializeGeneratedTableAndFonts(); // TH10 0x00437a00
void DestroyGeneratedTableAndFonts(); // TH10 0x00437d10

} // namespace th10
