#include "TimelineAudioActions.hpp"

#include <string.h>

#include "BgmRuntime.hpp"
#include "MainChainRuntime.hpp"

namespace th10 {

namespace {

extern TransitionRootPartial g_TransitionRoot;
extern const float g_AsciiOverlayInitialRate;
extern float g_MainChainStartupScale; // TH10 DAT_00476f78
extern u8 g_BgmUsePreloadedTrackImages;
extern i32 ConvertFloatToI32TowardZeroX87(float value);

u8 *TimelineAudioFlagByte(i32 mode_index)
{
    extern u8 g_TimelineAudioFlags[]; // TH10 DAT_0047783c region
    return &g_TimelineAudioFlags[0x1d892 + mode_index];
}

} // namespace

// Backing store for SelectTimelineAudioMode's observed byte write. Only the
// indexed byte participates in timeline opcode 10/11 behavior.
u8 g_TimelineAudioFlags[0x20000];

void SubmitTimelineAudioPath(i32 track_slot, const char *path)
{
    char local_path[256];
    i32 i = 0;
    char c;
    do {
        c = path[i];
        local_path[i] = c;
        ++i;
    } while (c != '\0');

    char *dot = strrchr(local_path, '.');
    if (dot != 0) {
        dot[1] = 'w';
        dot[2] = 'a';
        dot[3] = 'v';
    }
    QueueBgmCommand(&g_TransitionRoot, local_path, 1, track_slot);
}

void SelectTimelineAudioMode(i32 track_slot, i32 mode_index)
{
    if ((g_BgmUsePreloadedTrackImages & 0x10U) != 0)
        QueueBgmCommand(&g_TransitionRoot, "dummy", 4, 0);
    QueueBgmCommand(&g_TransitionRoot, "dummy", 2, track_slot);
    *TimelineAudioFlagByte(mode_index) = 1;
}

void SetTimelineAudioValue(float value)
{
    float selected = value;
    const float zero_rate = 0.0f;
    const float unity_rate = 1.0f;
    if (g_MainChainStartupScale == zero_rate) {
        selected = value;
    } else if (g_MainChainStartupScale == unity_rate &&
               value != g_MainChainStartupScale) {
        selected = value / g_MainChainStartupScale;
    }
    QueueBgmCommand(&g_TransitionRoot, "", 5,
                     ConvertFloatToI32TowardZeroX87(selected));
}

} // namespace th10
