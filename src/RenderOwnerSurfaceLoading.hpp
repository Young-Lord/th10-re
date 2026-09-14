#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x447c80. Native ECX = render owner, EDX = file name, stack = cache
// slot index; ret 4, HRESULT-like in EAX. Releases the cached surface pair,
// loads the named file (or reuses the cached buffer), creates the 640x1024
// image surface plus the per-slot primary and shadow surfaces, and copies
// the decoded image into both via D3DX.
i32 LoadRenderOwnerCachedSurfaceFromFile(void *owner, const char *file_name,
                                         u32 cache_slot);

// TH10 0x446d70. Native EAX = format selector, ECX = destination top,
// EDI = glyph record / file image, stack = {dead, record holder,
// source byte count, embedded flag}; ret 0x10, always returns 0. Creates a
// destination surface through the record holder's +0x00 wrapper (vtable
// +0x48) and uploads either the record's embedded bitmap
// (D3DXLoadSurfaceFromMemory, selector tables 0x46f288/0x46f2a0) or the
// file-in-memory image (D3DXLoadSurfaceFromFileInMemory); the selector is
// remapped through the DAT_00491d78 gate and the record holder receives the
// source size (+0x08) and the format's byte count (+0x0c).
i32 LoadGeneratedGlyphIntoSurface(u32 selector, void *record_holder,
                                  void *glyph_record, u32 source_size,
                                  u32 embedded_flag, i32 destination_top);

} // namespace th10
