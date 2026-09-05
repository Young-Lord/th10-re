// Score save writer (TH10 0x0042b1e0). Serializes the score-save state's
// seven 0x437c-byte stage-record slots (magic 0x5243) and the 0x448-byte
// clear-data region into a 0x200000 scratch image, LZSS-compresses it through
// 0x004359b0, applies one 0xac/0x35-over-0x10 scramble pass through
// 0x0044b220, and writes the 0x18-byte header plus the packed body to
// "scoreth10.dat" through the shared replay-file opener 0x0044b620 (handle
// DAT_00474C38, critical section stru_4922A4, depth byte byte_49231E).
#include <stdlib.h>
#include <string.h>

#include "ScoreSave.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

// Score-save state fields (EBX / the value of DAT_0047783c):
//   +0x0000  pointer to the 0x18-byte score header record written first
//            (+0x10 = uncompressed image size, +0x14 = total size published
//            as payload end + 0x430, +0x04 republished as compressed + 0x18)
//   +0x0008  seven 0x437c-byte stage-record slots (12-byte sub-header:
//            +0 word magic 0x5243, +4 checksum, +8 stage index, payload
//            from +0xc)
//   +0x1d86c 0x448-byte clear-data region (+4 = its checksum dword)

// Shared state with the replay-file opener 0x0044b620 (still a boundary).
extern void *g_ReplayFileHandle; // TH10 DAT_00474C38 (HANDLE slot)
extern u8 g_ReplayFileLockDepth; // TH10 byte_49231E
extern u8 g_ReplayFileLock[0x18]; // TH10 stru_4922A4

// TH10 0x004359b0 (stdcall): LZSS-style compressor over a 0x2000-byte window;
// returns a malloc'd buffer and the compressed size.
extern void *CompressReplayImageStdcallAbi(const void *image, i32 size,
                                           u32 *out_size); // 0x004359b0

// TH10 0x0044b220 (__userpurge: initial key in AL; stack = buffer, size,
// key step, block size, size): reversed XOR scramble inside blocks, key
// advancing per emitted byte.
extern void ScrambleReplayImageUserpurgeAbi(u8 initial_key, void *buffer,
                                            i32 size, u8 key_step,
                                            i32 block_size,
                                            i32 size_again); // 0x0044b220

// TH10 0x0044b620 (stdcall): enter the replay-file critical section, create
// or truncate the file (GENERIC_WRITE, SHARE_READ, OPEN_ALWAYS) and publish
// the handle; on failure reports through FormatMessageA and leaves the lock.
extern i32 OpenReplayFileForWriteStdcallAbi(const char *path); // 0x0044b620

extern "C" i32 TH10_STDCALL WriteFile(void *handle, const void *buffer,
                                      u32 bytes, u32 *written,
                                      void *overlapped);
extern "C" i32 TH10_STDCALL CloseHandle(void *handle);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);

// Native short-write failure epilogue, inlined at each WriteFile site: close
// the (possibly stale) handle, leave the critical section and drop the depth
// byte. The handle global is intentionally NOT invalidated, so the following
// guarded writes re-test a closed handle value — quirk preserved.
void ScoreSaveShortWriteFailure()
{
    CloseHandle(g_ReplayFileHandle);
    LeaveCriticalSection(g_ReplayFileLock);
    --g_ReplayFileLockDepth;
}

// Stage-record checksum (native loop at 0x42b253): 2878 iterations of six
// bytes stepping from record+0xc, i.e. the byte sum of record+8 ..
// record+0x437b (the checksum field itself at +4 is excluded).
u32 ScoreStageRecordChecksum(const u8 *record)
{
    u32 sum = 0;
    const u8 *cursor = record + 0xc;
    for (i32 k = 0; k < 2878; ++k, cursor += 6) {
        sum += static_cast<u32>(cursor[-4]) + cursor[-3] + cursor[-2] +
               cursor[-1] + cursor[0] + cursor[1];
    }
    return sum;
}

// Clear-data checksum (native loop at 0x42b2c0): 272 iterations of four bytes
// stepping from state+0x1d876, i.e. the byte sum of region+8 ..
// region+0x447 (the checksum field itself at +4 is excluded).
u32 ScoreClearDataChecksum(const u8 *region)
{
    u32 sum = 0;
    const u8 *cursor = region + 0xa;
    for (i32 k = 0; k < 272; ++k, cursor += 4) {
        sum += static_cast<u32>(cursor[-2]) + cursor[-1] + cursor[0] +
               cursor[1];
    }
    return sum;
}

} // namespace

// TH10 0x0042b1e0. Native EBX body (plain retn, no stack cleanup).
i32 SaveScoreRecordFileEbx(void *save_state)
{
    u8 *const state = static_cast<u8 *>(save_state);
    u32 *const header = *reinterpret_cast<u32 **>(state + 0x00);
    if (header == 0)
        return -1;

    u8 *const block = static_cast<u8 *>(malloc(0x200000U));

    // The native copies the current 0x18-byte header to the front of the
    // scratch image, but the compressor input starts at block+0x18 and the
    // file's first block is the (later updated) record itself, so this copy
    // is dead data — quirk preserved.
    memcpy(block, header, 0x18);

    // Serialize the stage-record slots. The running offset advances only for
    // slots carrying the 0x5243 magic, while the stored stage index is the
    // raw loop counter (skipped slots leave gaps in the numbering — both
    // quirks preserved).
    u32 offset = 0x18;
    u32 stage_index = 0;
    u8 *slot = state + 0x08;
    for (u32 i = 0; i < 7U; ++i, slot += 0x437cU) {
        if (*reinterpret_cast<u16 *>(slot) == 0x5243U) {
            *reinterpret_cast<u32 *>(slot + 0x08) = stage_index;
            *reinterpret_cast<u32 *>(slot + 0x04) =
                ScoreStageRecordChecksum(slot);
            memcpy(block + offset, slot, 0x437cU);
            offset += 0x437cU;
        }
        // The stage index is the raw loop counter: it advances for every
        // slot, matched or not.
        ++stage_index;
    }

    // Clear-data region: refresh its checksum, append the 0x448 bytes, then
    // publish the total size as payload end + 0x430 (the compressor runs over
    // 0x430 bytes of uninitialized scratch beyond the copied data — native
    // quirk preserved).
    const u8 *const clear_region = state + 0x1d86cU;
    *reinterpret_cast<u32 *>(state + 0x1d870U) =
        ScoreClearDataChecksum(clear_region);
    memcpy(block + offset, clear_region, 0x448U);
    offset += 0x448U;
    offset += 0x430U;
    header[0x14 / 4] = offset;

    // Compress the payload (block+0x18 .. total size), storing the
    // compressed size into header+0x10, then republish header+0x04 as
    // compressed size + 0x18 and apply one scramble pass
    // (key 0xac, step 0x35, 0x10-byte blocks).
    void *const packed = CompressReplayImageStdcallAbi(
        block + 0x18, static_cast<i32>(header[0x14 / 4]), &header[0x10 / 4]);
    header[0x04 / 4] = header[0x10 / 4] + 0x18U;
    const u32 packed_size = header[0x10 / 4];
    ScrambleReplayImageUserpurgeAbi(0xacU, packed,
                                    static_cast<i32>(packed_size), 0x35U,
                                    0x10U, static_cast<i32>(packed_size));

    // Open (or truncate) scoreth10.dat. On failure the native returns -1
    // without freeing the two buffers — leak quirk preserved.
    if (OpenReplayFileForWriteStdcallAbi("scoreth10.dat") != 0)
        return -1;

    u32 written = 0;
    if (g_ReplayFileHandle != reinterpret_cast<void *>(-1)) {
        WriteFile(g_ReplayFileHandle, header, 0x18, &written, 0);
        if (written != 0x18)
            ScoreSaveShortWriteFailure();
    }
    if (g_ReplayFileHandle != reinterpret_cast<void *>(-1)) {
        WriteFile(g_ReplayFileHandle, packed, packed_size, &written, 0);
        if (written != packed_size)
            ScoreSaveShortWriteFailure();
        if (g_ReplayFileHandle != reinterpret_cast<void *>(-1)) {
            CloseHandle(g_ReplayFileHandle);
            LeaveCriticalSection(g_ReplayFileLock);
            --g_ReplayFileLockDepth;
        }
    }

    if (packed != 0)
        free(packed);
    free(block);
    return 0;
}

} // namespace th10
