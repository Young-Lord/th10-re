#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004359b0 (native stdcall, `retn 0xc`). LZSS-style encoder shared by
// the replay writer 0x00429b60 and the score writer 0x0042b1e0. Parameters:
// N = 0x2000 ring (DAT_0048f868, data starts at cursor one), F = 18 lookahead,
// THRESHOLD = 2, 12-bit match positions, 4-bit length codes storing
// length - 3. A set flag bit precedes an 8-bit literal, a clear flag bit
// precedes the 12-bit position and 4-bit length. Returns a malloc'd buffer of
// `2 * size` bytes (never freed by the encoder) and the packed size through
// `out_size`; returns null on malloc failure without touching `out_size`.
void *CompressReplayImageStdcallAbi(const void *image, i32 size,
                                    u32 *out_size);

// TH10 0x0044b220 (native __userpurge: initial key in AL; stack = buffer,
// size, key step, block size, size again). Scrambles `buffer` in place from a
// scratch copy: per block of `block_size` bytes the output sequence is the
// block's odd offsets in descending order followed by its even offsets in
// descending order, each output byte being the source byte XOR the running
// key (key advances by `key_step` per emitted byte). Native EAX returns the
// buffer; the C++ ABI drops it (all callers ignore it).
void ScrambleReplayImageUserpurgeAbi(u8 initial_key, void *buffer, i32 size,
                                     u8 key_step, i32 block_size,
                                     i32 size_again);

} // namespace th10
