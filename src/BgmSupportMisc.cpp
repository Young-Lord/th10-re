// TH10 BGM/misc support (0x0043cc30, 0x0043d250):
//   0x0043cc30: raises the BGM worker stop flag (+0x5224 = 2) on the
//     context in EAX — the sibling of 0x0043cc40 StopBgmWorkerControls.
//   0x0043d250: BGM data chunk search: walks {char tag[4]; u32 size;
//     u8 data[size]} records until the 4-byte tag matches, returning the
//     data pointer and storing the size. Native ECX = chunk walk cursor,
//     EAX = remaining bytes, EDI = size out, stack = tag.
#include <string.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"

namespace th10 {

// TH10 0x0043cc30. Native EAX = BGM context.
u32 RequestBgmWorkerStopEaxAbi(void *bgm_context)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(bgm_context) + 0x5224U) = 2U;
    return 0;
}

// TH10 0x0043d250. Native ECX = first chunk, EAX = remaining size,
// EDI = size out, stack = 4-byte tag. Returns the chunk payload pointer or
// 0. The walk cursor is advanced by 8 + size per record and the size out
// receives every record's size along the way (native quirk: it is written
// before the tag comparison, so the last miss leaves its size behind).
const char *FindBgmChunkByTagEcxAbi(const char *chunk, u32 remaining,
                                    u32 *size_out, const char *tag)
{
    while (remaining != 0U) {
        *size_out = *reinterpret_cast<const u32 *>(chunk + 4U);
        if (strncmp(chunk, tag, 4U) == 0)
            return chunk + 8U;
        remaining -= 8U + *size_out;
        chunk += 8U + *size_out;
    }
    return 0;
}

} // namespace th10
