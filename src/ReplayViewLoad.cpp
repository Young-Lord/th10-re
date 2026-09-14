// Replay view record loader (TH10 0x0042a200). Reads "replay/<name>",
// validates the "t10r" version-5 container, decompresses the body with the
// shared packed-stream transforms and indexes the per-stage entries.
#include "ReplayViewLoad.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PackedArchive.hpp"

namespace th10 {

namespace {

// ---- shared globals ----------------------------------------------------

extern u32 g_ReplayPathFlags; // TH10 DAT_00474cd0 (bit 0x20 = direct view)

// Boundary leaves (native ABIs noted at each site).

// TH10 0x0044b360 (body in MainChainFileProbe.cpp). The EAX-register ABI
// variant of the packed/direct resource loader; the direct-view branch
// passes no size out and source mode 0.
extern void *LoadMainChainFile(const char *path, u32 *file_size,
                               i32 filesystem_mode);

extern i32 DoesMainChainFileExist(const char *path); // TH10 0x0044b4d0

// TH10 0x0044b0d0 / 0x00435dc0 (bodies in PackedArchive.cpp).
extern void TransformBytesInPlace(u8 *bytes, u32 byte_count, u8 key_step,
                                  u32 block_size, u32 total_span);
extern u8 *DecompressPackedBytes(const u8 *encoded, u32 encoded_size,
                                 u8 *output, u32 output_size);

// TH10 0x0044b6b0. Opens the replay file for the sequential reader; 0 on
// success.
extern i32 OpenReplayReader(const char *path);

// TH10 0x0044b790. Returns the next block read from the replay file (the
// reader hands out its internal buffer).
extern void *GetReplayReadBlock();

// TH10 0x0044b7e0. Consumes/closes the block handed out above.
extern void FinishReplayReadBlock();

// Replay container constants.
const u32 kReplayMagic = 0x72303174U; // "t10r" little-endian
const u32 kReplayVersion = 5U;

// Header fields of the first block: +0x1c packed size, +0x20 unpacked
// size, +0x4c stage count. Each stage entry is a 0x1c4-byte fixed part
// followed by extra data; the entry's first u16 is the stage slot.
const u32 kOffPackedSize = 28U;
const u32 kOffUnpackedSize = 32U;
const u32 kOffStageCount = 76U;
const u32 kFirstEntryOffset = 100U;
const u32 kEntryFixedPart = 452U;

// View record offsets.
const u32 kViewHandle = 0x14U;      // file handle / direct buffer
const u32 kViewBody = 0x1c0U;       // malloc'ed decompressed body
const u32 kViewCursor = 0x18U;      // read cursor into the body
const u32 kViewEntryPtr = 0xa0U;    // per-slot: fixed part (offset +0xa0)
const u32 kViewExtraPtr = 0xa8U;    // per-slot: extra data (+0xa8)
const u32 kViewEntryRaw = 0xb0U;    // per-slot: entry start (+0xb0)
const u32 kViewName = 0x1d4U;       // copied file name
const u32 kSlotStride = 36U;        // 0x24 per stage slot
const u32 kMaxSlot = 8U;            // slots clamp to 6 when count >= 8
const u32 kClampedSlotCount = 6U;

inline u32 LoadU32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline u16 LoadU16(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u16 *>(bytes + offset);
}

inline void StorePtr(u8 *base, u32 offset, void *value)
{
    *reinterpret_cast<void **>(base + offset) = value;
}

} // namespace

// TH10 0x0042a200. Native ESI/stdcall mix; reconstructed as a plain call.
i32 LoadReplayViewRecord(void *view, const char *file_name)
{
    u8 *const record = static_cast<u8 *>(view);
    u8 *header = 0;

    // The name is copied byte-by-byte including the terminator.
    {
        char *dst = reinterpret_cast<char *>(record + kViewName);
        const char *src = file_name;
        char c;
        do {
            c = *src++;
            *dst++ = c;
        } while (c != '\0');
    }

    void *block = 0;
    if ((g_ReplayPathFlags & 0x20U) != 0U) {
        // Direct-view mode: the loader returns the whole mounted buffer and
        // the header sits 36 bytes into it.
        u8 *direct = static_cast<u8 *>(
            const_cast<void *>(static_cast<const void *>(
                LoadMainChainFile(file_name, 0, 0))));
        *reinterpret_cast<void **>(record + kViewHandle) = direct;
        if (direct == 0)
            return -1;
        header = direct;
        block = direct + 36U;
    } else {
        char path[256];
        sprintf(path, "replay/%s", file_name);
        if (DoesMainChainFileExist(path) == 0 || OpenReplayReader(path) != 0)
            return -1;
        header = static_cast<u8 *>(GetReplayReadBlock());
        *reinterpret_cast<void **>(record + kViewHandle) = header;
        if (LoadU32(header, 0U) != kReplayMagic
            || LoadU16(header, 4U) != kReplayVersion) {
            FinishReplayReadBlock();
            return -1;
        }
        block = GetReplayReadBlock();
        FinishReplayReadBlock();
    }

    const u32 packed_size = LoadU32(header, kOffPackedSize);
    const u32 unpacked_size = LoadU32(header, kOffUnpackedSize);

    // Body: decompress with the shared LZSS decoder after the two fixed
    // XOR stream passes over the whole packed range (native reuses
    // 0x0044b0d0 with (key 0xe1, block 0x400) then (key 0x7a, block 0x80),
    // both spans equal to the packed size).
    u8 *body = static_cast<u8 *>(malloc(unpacked_size));
    *reinterpret_cast<void **>(record + kViewBody) = body;
    TransformBytesInPlace(static_cast<u8 *>(block), packed_size, 0xe1U,
                          0x400U, packed_size);
    TransformBytesInPlace(static_cast<u8 *>(block), packed_size, 0x7aU,
                          0x80U, packed_size);
    DecompressPackedBytes(static_cast<const u8 *>(block), packed_size, body,
                          unpacked_size);
    *reinterpret_cast<void **>(record + kViewCursor) = body;

    // Index the stage entries. The count clamps to 6 whenever it reaches 8
    // (7 stays 7 — the native only tests >= 8).
    u32 limit = LoadU32(body, kOffStageCount);
    if (limit >= kMaxSlot)
        limit = kClampedSlotCount;

    const u8 *entry = body + kFirstEntryOffset;
    for (u32 i = 0; i < limit; ++i) {
        const u32 slot = LoadU16(entry, 0U);
        const u32 extra_bytes = 6U * LoadU32(entry, 4U);
        StorePtr(record, kViewEntryRaw + kSlotStride * slot,
                 const_cast<u8 *>(entry));
        StorePtr(record, kViewEntryPtr + kSlotStride * slot,
                 const_cast<u8 *>(entry) + kEntryFixedPart);
        StorePtr(record, kViewExtraPtr + kSlotStride * slot,
                 const_cast<u8 *>(entry) + kEntryFixedPart + extra_bytes);
        entry += kEntryFixedPart + LoadU32(entry, 8U);
    }

    if ((g_ReplayPathFlags & 0x20U) == 0U && block != 0)
        free(block);
    return 0;
}

} // namespace th10
