#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00420a90. Copies the supplied path, rewrites its extension to `.wav`,
// and queues BGM opcode 1.
void SubmitTimelineAudioPath(i32 track_slot, const char *path);

// TH10 0x00420b10. Queues dummy BGM commands and marks a timeline audio flag.
void SelectTimelineAudioMode(i32 track_slot, i32 mode_index);

// TH10 0x00420c30. Queues BGM opcode 5 using native float/x87 selection rules.
void SetTimelineAudioValue(float value);

} // namespace th10
