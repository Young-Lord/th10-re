// TH10 GDI text/surface support around the outlined text renderer
// (DrawGdiOutlinedTextEaxEdiStackAbi 0x00437380, registered separately).
//
//   0x00436890: builds the 0x2c-byte font-spec record the callers place on
//     their stack: {HFONT result slot, width, height=16, string object
//     (base +0x0c: SSO buffer at +0x10, size +0x20, capacity +0x24),
//     weight=600}. The face name is assigned from the 13-byte default.
//   0x004369e0 / 0x00436a30: reset/release of the GDI surface record
//     (memory DC at +0x114, selected object +0x118, bitmap +0x11c and the
//     dimension words +0x100..0x110).
//   0x004372a0: creates the font from the record and draws with the
//     outlined renderer, then destroys the font.
//   0x00437d10: releases the sixteen cached font handles.
#include <stdlib.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"

namespace th10 {

void StringAssignRangeEcxAbi(void *string_object, const char *text,
                             u32 byte_count); // StdStringCrt.cpp
i32 DrawGdiOutlinedTextEaxEdiStackAbi(void *owner_raw, char *text, i32 a1,
                                      i32 a2, void *gdi_object,
                                      u32 fill_color,
                                      i32 outline_color); // GdiOutlinedText

namespace {

extern "C" i32 TH10_STDCALL DeleteObject(void *object);
extern "C" void *TH10_STDCALL SelectObject(void *dc, void *object);
extern "C" i32 TH10_STDCALL DeleteDC(void *dc);
extern "C" void *TH10_STDCALL CreateFontA(i32 height, i32 width,
    i32 escapement, i32 orientation, i32 weight, u32 italic, u32 underline,
    u32 strike_out, u32 char_set, u32 output_precision,
    u32 clip_precision, u32 quality, u32 pitch_and_family,
    const char *face_name);

// Default face name (13 bytes) at TH10 0x0046f36c.
extern const char g_DefaultLogfontFace[13];

// Cached font handles table (TH10 0x004918a0 descending to 0x00491868).
extern void *g_GdiFontHandles[15]; // [0] = 0x004918a0 ("h")

} // namespace

// TH10 0x00436890. Native EAX = 0x2c-byte record.
void InitializeGdiTextRecordEaxAbi(void *record_raw)
{
    u32 *record = static_cast<u32 *>(record_raw);
    record[9] = 15;                          // +0x24 capacity
    record[8] = 0;                           // +0x20 size
    record[4] = 0;                           // +0x10 (SSO body dword)
    record[0] = 0;                           // +0x00 HFONT result slot
    record[10] = 600;                        // +0x28 weight
    record[2] = 16;                          // +0x08 default height
    record[1] = 0;                           // +0x04 width
    StringAssignRangeEcxAbi(reinterpret_cast<u8 *>(record_raw) + 0x0cU,
                            g_DefaultLogfontFace, 13U);
}

// TH10 0x004369e0. Native EAX = surface record.
void ResetGdiSurfaceRecordEaxAbi(void *record_raw)
{
    u32 *record = static_cast<u32 *>(record_raw);
    record[64] = 0xffffffffU;                // +0x100
    record[65] = 0;                          // +0x104
    record[66] = 0;                          // +0x108
    record[69] = 0;                          // +0x114 memory DC
    record[71] = 0;                          // +0x11c bitmap
    record[70] = 0;                          // +0x118 selected object
    record[72] = 0;                          // +0x120
}

// TH10 0x00436a30. Native ESI = surface record.
i32 ReleaseGdiSurfaceDcEsiAbi(void *record_raw)
{
    u32 *record = static_cast<u32 *>(record_raw);
    void *dc = reinterpret_cast<void *>(record[69]);
    if (dc == 0)
        return 0;
    (void)SelectObject(dc, reinterpret_cast<void *>(record[70]));
    (void)DeleteDC(dc);
    (void)DeleteObject(reinterpret_cast<void *>(record[71]));
    record[65] = 0;                          // +0x104
    record[66] = 0;                          // +0x108
    record[69] = 0;                          // +0x114
    record[71] = 0;                          // +0x11c
    record[70] = 0;                          // +0x118
    record[72] = 0;                          // +0x120
    record[64] = 0xffffffffU;                // +0x100
    return 1;
}

// TH10 0x004372a0. Native: ESI = owner, ECX = text, stack (ret 0x14) =
// anchor left, anchor top, font height, fill color, outline color.
i32 DrawGdiTextWithFontStackAbi(void *owner_esi, char *text_ecx,
                                i32 anchor_left, i32 anchor_top,
                                i32 font_height, i32 fill_color,
                                i32 outline_color)
{
#pragma pack(push, 1)
    // 0x2c-byte font-spec record (see module header).
    struct FontSpecRecord {
        void *font_result;                   // +0x00
        u32 width;                           // +0x04
        u32 height;                          // +0x08
        u32 string_body;                     // +0x0c (allocator root)
        char face_name[16];                  // +0x10 SSO buffer
        u32 face_heap;                       // +0x20 heap pointer
        u32 face_size;                       // +0x24
        u32 face_capacity;                   // +0x28
        u32 weight;                          // +0x2c
    };
#pragma pack(pop)

    FontSpecRecord spec;
    InitializeGdiTextRecordEaxAbi(&spec);
    spec.width = 0;
    spec.height = static_cast<u32>(font_height);

    const char *face = (spec.face_capacity < 0x10U)
        ? spec.face_name
        : reinterpret_cast<const char *>(spec.face_heap);
    spec.font_result = CreateFontA(
        font_height, 0, 0, 0, static_cast<i32>(spec.weight), 0, 0, 0, 1U,
        0, 0, 0, 0x30U, face);

    const i32 result = DrawGdiOutlinedTextEaxEdiStackAbi(
        owner_esi, text_ecx, anchor_left, anchor_top, spec.font_result,
        static_cast<u32>(fill_color), outline_color);

    if (spec.font_result != 0) {
        (void)DeleteObject(spec.font_result);
        spec.font_result = 0;
    }
    if (spec.face_capacity >= 0x10U)
        free(reinterpret_cast<void *>(spec.face_heap));
    return result;
}

// TH10 0x00437d10. Releases the shared GDI surface record and the fifteen
// cached font handles 0x004918a0..0x00491868; returns the last result.
i32 ReleaseGdiFontCache(void)
{
    extern u8 g_GdiSurfaceRecordBase; // TH10 byte_474cb0
    ReleaseGdiSurfaceDcEsiAbi(&g_GdiSurfaceRecordBase);
    for (i32 index = 0; index < 15; ++index)
        (void)DeleteObject(g_GdiFontHandles[index]);
    return DeleteObject(g_GdiFontHandles[14]);
}

} // namespace th10
