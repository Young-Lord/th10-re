#pragma once

#include "Th10Types.hpp"

namespace th10 {

struct GeneratedSurface;

// TH10 0x437750. Native EAX = destination row, ECX = destination column,
// EDX = source GeneratedSurface, stack = {texture, columns, rows}; ret 0xc,
// boolean result in AL. Locks the destination texture, verifies its pixel
// format against the surface's, and blends the surface's GDI bitmap into
// the locked bits with the kernel selected by the surface format id
// (+0x100): 0x15 (A8R8G8B8) per-byte lerp, 0x19 (A1R5G5B5) alpha-bit copy,
// 0x1a (A4R4G4B4) nibble accumulative blend; other formats skip the blend.
bool BlendGeneratedSurfaceIntoTexture(void *texture,
                                      const GeneratedSurface *surface,
                                      u32 destination_row,
                                      u32 destination_column, i32 columns,
                                      i32 rows);

} // namespace th10
