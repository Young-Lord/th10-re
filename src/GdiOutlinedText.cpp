// TH10 0x437380 - GDI text renderer with a repeated-draw outline effect.
//
// Draws the given text into the owner's memory DC (owner+0x114) with GDI
// DrawTextA. When an outline color is supplied (arg != -1) the text is
// first drawn six times in that color with the destination rectangle
// shifted by two pixels between passes; the fill color is then selected
// and one final pass draws over an expanded rectangle. The memory DC's
// previously selected GDI object is restored afterwards.
//
// Owner fields used:
//   +0x104 text extent width  (paired with the a1 anchor)
//   +0x108 text extent height (paired with the a2 anchor)
//   +0x114 memory DC handle (HDC)
//
// Stack arguments (ret 0x14, native order as pushed):
//   a1 anchor left, a2 anchor top, a3 GDI object to select (the font
//   created by the caller, e.g. 0x466034 CreateFontA at the 0x4372b0 call
//   site), a4 fill color, a5 outline color (-1 disables the outline).
#include <string.h>

#include "Th10Types.hpp"

namespace th10 {

namespace {

// GDI import thunks (IAT): SelectObject 0x46602c, SetBkMode 0x46601c,
// SetTextColor 0x466018, DrawTextA 0x466268. Declared as boundaries in the
// established project style; the Win32 ordinals are stable.
void *SelectGdiObject(void *dc, void *object);
i32 SetGdiBkMode(void *dc, i32 mode);
i32 SetGdiTextColor(void *dc, u32 color);
i32 DrawTextGdiA(void *dc, char *text, i32 count, void *rect, u32 format);

// TRANSPARENT background mode constant used by the native SetBkMode call.
const i32 k_bk_transparent = 1;

struct GdiRect {
    i32 left;
    i32 top;
    i32 right;
    i32 bottom;
};

} // namespace

// TH10 0x437380. Native usercall: EAX = owner object, EDI = text (NUL
// terminated); returns 1 (and 0 early when the text pointer is null).
i32 DrawGdiOutlinedTextEaxEdiStackAbi(void *owner_raw, char *text, i32 a1,
                                      i32 a2, void *gdi_object, u32 fill_color,
                                      i32 outline_color)
{
    if (text == 0)
        return 0;

    u8 *const owner = static_cast<u8 *>(owner_raw);
    void *const dc = *reinterpret_cast<void **>(owner + 0x114U);
    const i32 extent_width = *reinterpret_cast<const i32 *>(owner + 0x104U);
    const i32 extent_height = *reinterpret_cast<const i32 *>(owner + 0x108U);

    // The previously selected object is stashed; the native's slot is
    // reused for scratch values during the rect setup (benign, the restore
    // below reads the untouched first slot).
    void *const previous_object = SelectGdiObject(dc, gdi_object);

    SetGdiBkMode(dc, k_bk_transparent);

    if (outline_color != -1) {
        SetGdiTextColor(dc, static_cast<u32>(outline_color));

        GdiRect rect;
        rect.left = a1;
        rect.top = a2;
        rect.right = a1 + extent_width - 2;
        rect.bottom = a2 + extent_height - 2;
        DrawTextGdiA(dc, text, -1, &rect, 0U);

        // Four two-pixel offset passes around the anchor (the native
        // builds each rect in an extended scratch block with the same +/-
        // 2 arithmetic; right/bottom clip edges never move the glyphs
        // under the DT_LEFT|DT_TOP format of 0).
        static const i32 offsets[4][2]
            = { { 2, 0 }, { 0, 2 }, { -2, 0 }, { 0, -2 } };
        for (u32 pass = 0; pass != 4U; ++pass) {
            GdiRect offset_rect;
            offset_rect.left = a1 + offsets[pass][0];
            offset_rect.top = a2 + offsets[pass][1];
            offset_rect.right = a1 + extent_width + offsets[pass][0] - 2;
            offset_rect.bottom = a2 + extent_height + offsets[pass][1] - 2;
            DrawTextGdiA(dc, text, -1, &offset_rect, 0U);
        }
    }

    SetGdiTextColor(dc, fill_color);
    GdiRect final_rect;
    final_rect.left = a1 + 1;
    final_rect.top = a2 + 1;
    final_rect.right = a1 + extent_width + 1;
    final_rect.bottom = a2 + extent_height + 1;
    DrawTextGdiA(dc, text, -1, &final_rect, 0U);

    SelectGdiObject(dc, previous_object);
    return 1;
}

} // namespace th10
