#include "TimelineStreamLoader.hpp"

#include <string.h>

#include "PackedArchive.hpp"
#include "TimelineGateState.hpp"

namespace th10 {

namespace {

char g_TimelinePathScratch[256]; // TH10 DAT_00497c38

void CopyPathToScratch(const char *path)
{
    g_TimelinePathScratch[0] = '\0';
    char *dst = g_TimelinePathScratch;
    const char *src = path;
    char c;
    do {
        c = *src++;
        *dst++ = c;
    } while (c != '\0');
}

extern void ReleaseResourceBuffer(void *pointer); // TH10 0x00452422

} // namespace

u8 *LoadTimelineStream(TimelineGateStatePartial *gate_state, const char *path)
{
    CopyPathToScratch(path);
    u8 *const loaded = LoadPackedResource(g_TimelinePathScratch, 0, 0);
    if (gate_state->stream_buffer != 0) {
        ReleaseResourceBuffer(gate_state->stream_buffer);
        gate_state->stream_buffer = 0;
    }
    gate_state->stream_buffer = loaded;
    return loaded;
}

} // namespace th10
