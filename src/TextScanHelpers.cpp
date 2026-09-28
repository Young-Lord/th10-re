// Text scan helpers (TH10 0x0044c000 / 0x0044c070 / 0x0044c080 /
// 0x0044c0b0). ZUN's Shift-JIS line copier plus the three byte-sum
// checksum helpers; none of them carry direct cross-references in the
// retail binary (the hint-text reader inlines an equivalent), so they are
// reconstructed as standalone utilities.
#include "TextScanHelpers.hpp"

#include <stddef.h>

namespace th10 {

namespace {

bool IsShiftJisLeadByte(u8 value)
{
    return (value >= 0x81U && value <= 0x9fU)
        || (value >= 0xe0U && value <= 0xfcU);
}

} // namespace

const u8 *CopyShiftJisTextLineEsiEdxStackAbi(u8 *dest, const u8 *src,
                                             u32 max_bytes)
{
    // Native ESI = dest, EDX = src, stack = max_bytes.
    u8 *out = dest;
    const u8 *cursor = src;
    u32 budget = max_bytes;

    // rep stos: the whole window is zero-filled up front, so the line is
    // NUL-terminated whenever it stops short of the budget.
    for (u32 index = 0; index < max_bytes; ++index)
        dest[index] = 0;

    u8 current = *cursor;
    if (current != '\n') {
        while (true) {
            if (current == '\r' || budget == 0)
                break;

            *out++ = current;

            // The lead-byte test re-reads the byte just consumed; the
            // trail copy decrements the budget a second time, which lets
            // it wrap to 0xffffffff when the line ends on a double-byte
            // lead with exactly one byte left.
            if (IsShiftJisLeadByte(current)) {
                const u8 trail = cursor[1];
                ++cursor;
                --budget;
                *out++ = trail;
            }

            current = cursor[1];
            ++cursor;
            --budget;
            if (current == '\n') {
                ++cursor; // consume the newline
                break;
            }
        }
    }

    while (*cursor == '\n' || *cursor == '\r')
        ++cursor;
    return cursor;
}

u8 SumByteChecksumStride1EcxEaxEdxAbi(const u8 *data, u32 count)
{
    // Native ECX = data, EDX = count.
    u8 sum = 0;
    while (count != 0U) {
        sum = static_cast<u8>(sum + *data++);
        --count;
    }
    return sum;
}

u8 SumByteChecksumStride2EcxEaxEdxAbi(const u8 *data, u32 count)
{
    // Native ECX = data, EDX = count; the odd tail byte is excluded.
    u8 sum = 0;
    const u32 even_count = count - (count & 1U);
    for (u32 index = 0; index < even_count; index += 2U)
        sum = static_cast<u8>(sum + data[index]);
    return sum;
}

u8 SumByteChecksumStride4EcxEaxEdxAbi(const u8 *data, u32 count)
{
    // Native ECX = data, EDX = count; the tail bytes are excluded.
    u8 sum = 0;
    const u32 aligned_count = count - (count & 3U);
    for (u32 index = 0; index < aligned_count; index += 4U)
        sum = static_cast<u8>(sum + data[index]);
    return sum;
}

} // namespace th10
