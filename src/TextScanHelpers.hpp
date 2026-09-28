#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Native ESI = destination, EDX = source, stack = max bytes (ret 4).
// TH10 0x0044c000. Zero-fills max_bytes at dest, then copies one text
// line from src: stops at '\n'/'\r' or the byte budget, and when the
// current byte is a Shift-JIS lead (0x81-0x9f, 0xe0-0xfc) copies the
// trail byte too (the lead test re-reads the already-consumed byte, and
// each copied byte decrements the budget — twice for a double-byte
// character, which lets the budget wrap when it reaches exactly 1 on a
// double-byte lead). Afterwards the trailing CR/LF run is consumed.
// Returns the advanced source cursor.
const u8 *CopyShiftJisTextLineEsiEdxStackAbi(u8 *dest, const u8 *src,
                                             u32 max_bytes);

// Native ECX = data, EDX = count. TH10 0x0044c070 / 0x0044c080 /
// 0x0044c0b0. Byte-sum checksum helpers accumulating in AL: stride 1
// sums every byte, stride 2 the even-index bytes of the even-aligned
// prefix, stride 4 every fourth byte. The u8 result is zero-extended.
u8 SumByteChecksumStride1EcxEaxEdxAbi(const u8 *data, u32 count);
u8 SumByteChecksumStride2EcxEaxEdxAbi(const u8 *data, u32 count);
u8 SumByteChecksumStride4EcxEaxEdxAbi(const u8 *data, u32 count);

} // namespace th10
