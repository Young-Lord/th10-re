#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0042a200. Native ESI = replay view record, stack = replay file
// name ("th10_*.rpy" relative to the replay directory); returns 0 on
// success, -1 when the file cannot be opened or fails the "t10r" version-5
// header check. On success the decompressed replay body is allocated and
// indexed (record +0x1c0 buffer, +0x18 cursor, per-stage pointers at
// +0xa0/+0xa8/+0xb0, stride 0x24) and the name copied to +0x1d4.
i32 LoadReplayViewRecord(void *view, const char *file_name);

} // namespace th10
