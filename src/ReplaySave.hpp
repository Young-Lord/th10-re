#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00429b60 (retn 4). Native ABI: ECX = replay save context (the
// game-mode object published at DAT_00477838), EDX = replay file name
// relative to the replay directory ("th10_NN.rpy"), stack = player name
// string. The body pads the name into the 100-byte replay header, serializes
// the stage records plus their frame/replay-data lists, compresses and
// scrambles the image, writes it to "replay/<file_name>" and appends the two
// "USER" text chunks. Always returns 0.
i32 CommitReplaySave(void *save_context, const char *file_name,
                     const char *player_name);

} // namespace th10
