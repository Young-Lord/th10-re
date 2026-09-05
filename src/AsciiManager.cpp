#include "AsciiManager.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace th10 {

// FUNCTION: TH10 0x00401530
// The original passes this, position, and text in ECX, EBX, and EAX respectively.
void AsciiManager::AddString(const Float3 *position, const char *text)
{
    AsciiManagerString *next;

    if (num_strings >= 256) {
        return;
    }

    next = &strings[num_strings];
    num_strings++;
    strcpy(next->text, text);
    next->position = *position;
    next->color = color;
    next->scale_x = scale_x;
    next->scale_y = scale_y;
    next->is_gui = is_gui;
    next->is_selected = 0;
    next->text_mode = text_mode;
}

// FUNCTION: TH10 0x00401630
void AsciiManager::AddFormatText(const Float3 *position, const char *format, ...)
{
    char buffer[512];
    va_list arguments;

    va_start(arguments, format);
    vsprintf(buffer, format, arguments);
    va_end(arguments);
    AddString(position, buffer);
}

// FUNCTION: TH10 0x00401690
void AsciiManager::AddFormatTextSelected(const Float3 *position, const char *format, ...)
{
    char buffer[512];
    va_list arguments;

    va_start(arguments, format);
    vsprintf(buffer, format, arguments);
    va_end(arguments);
    AddString(position, buffer);
    strings[num_strings - 1].is_selected = 1;
}

} // namespace th10
