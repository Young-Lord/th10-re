#include "TimelineTextSubmission.hpp"

#include <string.h>

#include "GeneratedSurfaceText.hpp"

namespace th10 {

namespace {

void CopyTimelineText(char *destination, const char *source)
{
    char c;
    char *dst = destination;
    const char *src = source;
    do {
        c = *src++;
        *dst++ = c;
    } while (c != '\0');
}

} // namespace

void SubmitTimelineText(void *object, void *owner, u32 color, const char *text)
{
    (void)owner;
    if (object == 0)
        return;

    u8 *const node = static_cast<u8 *>(object);
    char local_text[128];
    CopyTimelineText(local_text, text);

    i32 font_size = node[0x3a0];
    if (font_size == 0)
        font_size = 0x11;

    TimelineTextWrapper *const wrapper =
        reinterpret_cast<TimelineTextWrapper *>(*reinterpret_cast<void **>(node + 0x394));
    TimelineD3DSurface *const surface = wrapper->surface;
    const i32 selected = (*reinterpret_cast<const u32 *>(node + 0x360) >> 1) & 1;

    SubmitTimelineTextDispatch(font_size, wrapper, local_text, surface, 0, color,
                               selected);
    *reinterpret_cast<u32 *>(node + 0x35c) |= 1U;
}

} // namespace th10
