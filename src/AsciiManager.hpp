#pragma once

#include <stddef.h>

#include "Th10Types.hpp"

namespace th10 {

struct Float3 {
    float x;
    float y;
    float z;
};

struct AsciiManagerString {
    char text[0x40];
    Float3 position;
    u32 color;
    float scale_x;
    float scale_y;
    u32 unknown_58;
    u32 is_gui;
    u32 is_selected;
    u32 text_mode;
};

struct AsciiManager {
    void AddString(const Float3 *position, const char *text);
    void AddFormatText(const Float3 *position, const char *format, ...);
    void AddFormatTextSelected(const Float3 *position, const char *format, ...);

    u8 unknown_0000[0x76c];
    AsciiManagerString strings[256];
    AsciiManagerString secondary_strings[64];
    i32 num_strings;
    u8 unknown_8970[4];
    u32 color;
    float scale_x;
    float scale_y;
    u32 is_gui;
    u32 unknown_8984;
    u32 text_mode;
    u32 unknown_898c;
    u8 unknown_8990[0x1c];
};

typedef char AssertAsciiManagerStringSize[
    sizeof(AsciiManagerString) == 0x68 ? 1 : -1];
typedef char AssertAsciiManagerSize[sizeof(AsciiManager) == 0x89ac ? 1 : -1];
typedef char AssertAsciiManagerSecondaryStringsOffset[
    offsetof(AsciiManager, secondary_strings) == 0x6f6c ? 1 : -1];

// Semantic C++ bodies behind the native ESI/EAX/stack ABI entries.
AsciiManager *ConstructAsciiManager(AsciiManager *manager);
i32 InitializeAsciiManager(AsciiManager *manager);
void DestroyAsciiManagerInPlace(AsciiManager *manager);
AsciiManager *CreateAsciiManager();
i32 DrawAsciiManagerSecondaryStrings(AsciiManager *manager);
i32 DrawAsciiManagerPrimaryStrings(AsciiManager *manager);

} // namespace th10
