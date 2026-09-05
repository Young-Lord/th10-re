#pragma once

#include "GeneratedFontTable.hpp"
#include "Th10Types.hpp"

namespace th10 {

struct TimelineD3DSurface {
    i32 left;
    i32 unknown_0004;
    i32 right;
};

struct TimelineTextWrapper {
    void *unknown_0000;
    TimelineD3DSurface *surface;
    float rect[4];
    u8 unknown_0018[0x1c];
    float center_anchor;
};

// TH10 0x00436ca0. Pre-tints generated-surface pixels for selected text.
void TintGeneratedSurfacePixels(GeneratedSurface *surface, u32 color);

// TH10 0x00437160. Format-specific scanline post-process over pitch * row_height.
void PostProcessGeneratedSurfaceRows(GeneratedSurface *surface, i32 row_height);

// TH10 0x00436da0. Neighbour-aware pixel smoothing for format 0x15 / 0x1a.
void SmoothGeneratedSurfacePixels(GeneratedSurface *surface, u32 row_count);

GdiHandle SelectGeneratedFontForSize(i32 font_size);

// TH10 0x00437db0 / 0x00437fe0.
void RenderTimelineTextNormal(const i32 src_rect[4], i32 column, i32 font_size,
                              u32 color, const char *text,
                              TimelineD3DSurface *surface);
void RenderTimelineTextSelected(const i32 src_rect[4], i32 column, i32 font_size,
                                u32 color, const char *text,
                                TimelineD3DSurface *surface);

// TH10 0x004479d0 dispatch body.
void SubmitTimelineTextDispatch(i32 font_size, TimelineTextWrapper *wrapper,
                                const char *text, TimelineD3DSurface *surface,
                                i32 column, u32 color, i32 selected);

} // namespace th10
