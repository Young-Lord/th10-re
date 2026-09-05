// Replay save writer (TH10 0x00429b60). Builds the th10 *.rpy image from the
// save context's 100-byte replay header, the eight per-stage 0x1c4-byte stage
// records and the eight per-stage frame-list chains, LZSS-compresses it,
// applies the two byte-scramble passes, and writes it plus the two "USER"
// text chunks to "replay/<file_name>" through the shared replay-file opener
// 0x0044b620 (handle DAT_00474C38, critical section stru_4922A4, depth byte
// byte_49231E).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ReplaySave.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

// Replay save context fields (ECX / g_GameModeObject):
//   +0x14 : pointer to the 0x24-byte secondary header written first
//           (+0x0c = compressed size + 0x24, +0x1c = compressed size,
//            +0x20 = uncompressed image size)
//   +0x18 : pointer to the 100-byte replay header (its first 8 bytes are the
//           player name; +0x0c time_t, +0x10 score, +0x48 slow-rate float,
//           +0x4c stage count, +0x50 chara, +0x54 shot type, +0x58 rank,
//           +0x5c final stage value)
//   +0x1c : eight dword stage-record pointers (records are 0x1c4 bytes)
//   +0x40 : eight frame-list heads, stride 0x0c; node = {record, next}

extern u32 g_CurrentRunScore; // TH10 DAT_00474C44
extern void *g_SlowRateStats; // TH10 DAT_00477708; doubles at +0x24/+0x2c

extern const char *g_ReplayCharacterNames[]; // TH10 off_4746DC
extern const char *g_ReplayRankNames[]; // TH10 off_4746F4

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
extern "C" i32 TH10_CDECL _mkdir(const char *path);

// SJIS chunk-1 header comment: "東方風神録　リプレイファイル情報\r\n"
const char kReplayHeaderComment[] =
    "\x93\x8c\x95\xfb\x95\x97\x90\x5f\x98\x5e\x20\x83\x8a\x83\x76\x83\x8c"
    "\x83\x43\x83\x74\x83\x40\x83\x43\x83\x8b\x8f\xee\x95\xf1\xd\xa";
// SJIS chunk-2 comment template: "コメントを書けます"
const char kReplayCommentTemplate[] =
    "\x83\x52\x83\x81\x83\x93\x83\x67\x82\xf0\x8f\x91\x82\xaf\x82\xdc\x82\xb7";

// Native short-write failure epilogue, inlined at each WriteFile site: close
// the (possibly stale) handle, leave the critical section and drop the depth
// byte. The handle global is intentionally NOT invalidated, so the following
// guarded writes re-test a closed handle value — quirk preserved.
void ReplayShortWriteFailure()
{
    CloseHandle(g_ReplayFileHandle);
    LeaveCriticalSection(g_ReplayFileLock);
    --g_ReplayFileLockDepth;
}

} // namespace

// TH10 0x00429b60. Native ECX/EDX/stack (retn 4) body.
i32 CommitReplaySave(void *save_context, const char *file_name,
                     const char *player_name)
{
    u8 *const context = static_cast<u8 *>(save_context);
    u32 *const header = *reinterpret_cast<u32 **>(context + 0x18);

    // Copy the entered player name into the header and pad with spaces to 8.
    char *name = *reinterpret_cast<char **>(context + 0x18);
    strcpy(name, player_name);
    i32 name_length = static_cast<i32>(strlen(player_name));
    for (; name_length < 8; ++name_length)
        name[name_length] = ' ';

    _mkdir("replay");
    char path[256];
    sprintf(path, "replay/%s", file_name);

    // Pass 1: total size, first/last stage indices, per-stage accumulators.
    u32 total_size = 100;
    u32 first_stage = 0;
    u32 last_stage = 0;
    u32 stage_count = 0;
    u32 *const stage_slots = reinterpret_cast<u32 *>(context + 0x1c);
    u32 *const list_heads = reinterpret_cast<u32 *>(context + 0x40);
    for (u32 stage = 0; stage < 8; ++stage) {
        u32 *const stage_record =
            reinterpret_cast<u32 *>(stage_slots[stage]);
        if (stage_record == 0)
            continue;
        if (first_stage == 0)
            first_stage = stage; // native: "unset" sentinel is index 0
        last_stage = stage;
        stage_record[2] = 0;
        total_size += 452;
        for (const u32 *node =
                 reinterpret_cast<const u32 *>(list_heads[stage * 3]);
             node != 0; node = reinterpret_cast<const u32 *>(node[1])) {
            const u32 entry = node[0];
            const i32 frames =
                (*reinterpret_cast<const i32 *>(entry + 0x5460) -
                 static_cast<i32>(entry)) /
                6;
            const u32 body_size =
                *reinterpret_cast<const u32 *>(entry + 0x6304) - entry -
                0x5464;
            total_size += body_size +
                6 * static_cast<u32>(frames);
            stage_record[2] += body_size + 6 * static_cast<u32>(frames);
            stage_record[1] += static_cast<u32>(frames);
        }
        ++stage_count;
    }
    header[0x4c / 4] = stage_count;
    header[0x10 / 4] = g_CurrentRunScore;
    *reinterpret_cast<float *>(reinterpret_cast<u8 *>(header) + 0x48) =
        static_cast<float>(
            100.0 -
            *reinterpret_cast<double *>(
                reinterpret_cast<u8 *>(g_SlowRateStats) + 0x24) /
                *reinterpret_cast<double *>(
                    reinterpret_cast<u8 *>(g_SlowRateStats) + 0x2c) *
                100.0);

    // Pass 2: serialize the image (header, stages, frame lists, bodies).
    u8 *const image = static_cast<u8 *>(malloc(total_size));
    memcpy(image, header, 100);
    u32 offset = 100;
    for (u32 stage = 0; stage < 8; ++stage) {
        const u32 *const stage_record =
            reinterpret_cast<const u32 *>(stage_slots[stage]);
        if (stage_record == 0)
            continue;
        memcpy(image + offset, stage_record, 0x1c4);
        offset += 452;
        for (const u32 *node =
                 reinterpret_cast<const u32 *>(list_heads[stage * 3]);
             node != 0; node = reinterpret_cast<const u32 *>(node[1])) {
            const u32 entry = node[0];
            const i32 frames =
                (*reinterpret_cast<const i32 *>(entry + 0x5460) -
                 static_cast<i32>(entry)) /
                6;
            memcpy(image + offset, reinterpret_cast<const void *>(entry),
                   6 * static_cast<u32>(frames));
            offset += 6 * static_cast<u32>(frames);
        }
        for (const u32 *node =
                 reinterpret_cast<const u32 *>(list_heads[stage * 3]);
             node != 0; node = reinterpret_cast<const u32 *>(node[1])) {
            const u32 entry = node[0];
            // Unsigned wrap preserved: a negative length copies a huge block.
            const u32 tail_size =
                *reinterpret_cast<const u32 *>(entry + 0x6304) - entry -
                0x5464;
            memcpy(image + offset,
                   reinterpret_cast<const void *>(entry + 0x5464),
                   tail_size);
            offset += tail_size;
        }
    }

    // Compress and apply the two scramble passes (0x3d/0x7a over 0x80-byte
    // blocks, then 0xaa/0xe1 over 0x400-byte blocks).
    u32 compressed_size = 0;
    void *const packed =
        CompressReplayImageStdcallAbi(image, static_cast<i32>(offset),
                                      &compressed_size);
    free(image);
    ScrambleReplayImageUserpurgeAbi(0x3d, packed,
                                    static_cast<i32>(compressed_size), 0x7a,
                                    0x80, static_cast<i32>(compressed_size));
    ScrambleReplayImageUserpurgeAbi(0xaa, packed,
                                    static_cast<i32>(compressed_size), 0xe1,
                                    0x400, static_cast<i32>(compressed_size));

    // Publish the sizes into the 0x24-byte secondary header.
    u32 *const secondary = *reinterpret_cast<u32 **>(context + 0x14);
    secondary[0x20 / 4] = offset;
    secondary[0x1c / 4] = compressed_size;
    secondary[0x0c / 4] = compressed_size + 36;

    // The opener's return value is ignored; every later step re-tests the
    // published handle against -1.
    (void)OpenReplayFileForWriteStdcallAbi(path);
    if (g_ReplayFileHandle != reinterpret_cast<void *>(-1)) {
        u32 written = 0;
        WriteFile(g_ReplayFileHandle, secondary, 0x24, &written, 0);
        if (written != 0x24)
            ReplayShortWriteFailure();
        if (g_ReplayFileHandle != reinterpret_cast<void *>(-1)) {
            WriteFile(g_ReplayFileHandle, packed, compressed_size, &written,
                      0);
            if (written != compressed_size)
                ReplayShortWriteFailure();
        }
    }
    if (packed != 0)
        free(packed);

    // Chunk 1 ('USER', type 0): title comment plus the run summary lines.
    // The two-arg stage branch pushes the last stage too, but the format
    // "Stage %d " only consumes the first stage and omits the CRLF — quirk
    // preserved (the last stage value is dead in the native code).
    {
        u8 *const chunk = static_cast<u8 *>(malloc(0xFFFF));
        memset(chunk, 0, 0xFFFF);
        *reinterpret_cast<u32 *>(chunk) = 0x52455355U; // 'USER'
        chunk[8] = 0;
        u8 *cursor = chunk + 12;
        cursor += sprintf(reinterpret_cast<char *>(cursor), "%s",
                          kReplayHeaderComment);
        cursor += sprintf(reinterpret_cast<char *>(cursor), "Version %s\r\n",
                          "1.00a");
        cursor += sprintf(reinterpret_cast<char *>(cursor), "Name %s\r\n",
                          *reinterpret_cast<char **>(context + 0x18));
        const time_t stamp = static_cast<time_t>(
            *reinterpret_cast<const i32 *>(reinterpret_cast<u8 *>(header) +
                                           0x0c));
        const struct tm *const local = localtime(&stamp);
        cursor += sprintf(
            reinterpret_cast<char *>(cursor),
            "Date %.2d/%.2d/%.2d %.2d:%.2d\r\n", local->tm_year % 100,
            local->tm_mon + 1, local->tm_mday, local->tm_hour, local->tm_min);
        cursor += sprintf(
            reinterpret_cast<char *>(cursor), "Chara %s\r\n",
            g_ReplayCharacterNames[3 * header[0x50 / 4] + header[0x54 / 4]]);
        cursor += sprintf(reinterpret_cast<char *>(cursor), "Rank %s\r\n",
                          g_ReplayRankNames[header[0x58 / 4]]);
        if (static_cast<i32>(header[0x5c / 4]) <= 7) {
            if (first_stage == last_stage) {
                if (first_stage == 7)
                    cursor += sprintf(reinterpret_cast<char *>(cursor),
                                      "Extra Stage\r\n");
                else
                    cursor += sprintf(reinterpret_cast<char *>(cursor),
                                      "Stage %d\r\n",
                                      static_cast<i32>(first_stage));
            } else {
                cursor += sprintf(reinterpret_cast<char *>(cursor),
                                  "Stage %d ", static_cast<i32>(first_stage));
            }
        } else if (first_stage == 7) {
            cursor += sprintf(reinterpret_cast<char *>(cursor),
                              "Extra Stage Clear\r\n");
        } else {
            cursor += sprintf(reinterpret_cast<char *>(cursor),
                              "Stage All Clear\r\n");
        }
        cursor += sprintf(reinterpret_cast<char *>(cursor), "Score %d\r\n",
                          static_cast<i32>(header[0x10 / 4]));
        cursor +=
            1 + sprintf(reinterpret_cast<char *>(cursor), "Slow Rate %2.2f\r\n",
                        static_cast<double>(
                            *reinterpret_cast<float *>(
                                reinterpret_cast<u8 *>(header) + 0x48)));
        u32 extra = static_cast<u32>(cursor - chunk) % 4;
        if (extra != 0)
            cursor += 4 - extra;
        const u32 chunk_size = static_cast<u32>(cursor - chunk);
        *reinterpret_cast<u32 *>(chunk + 4) = chunk_size;
        if (g_ReplayFileHandle != reinterpret_cast<void *>(-1)) {
            u32 written = 0;
            WriteFile(g_ReplayFileHandle, chunk, chunk_size, &written, 0);
            if (written != chunk_size)
                ReplayShortWriteFailure();
        }

        // Chunk 2 ('USER', type 1): the editable comment template alone; the
        // appended +1 (NUL) is included before the 4-alignment.
        memset(chunk, 0, 0xFFFF);
        *reinterpret_cast<u32 *>(chunk) = 0x52455355U; // 'USER'
        chunk[8] = 1;
        cursor = chunk + 12;
        cursor += sprintf(reinterpret_cast<char *>(cursor), "%s",
                          kReplayCommentTemplate);
        cursor += 1;
        extra = static_cast<u32>(cursor - chunk) % 4;
        if (extra != 0)
            cursor += 4 - extra;
        const u32 comment_size = static_cast<u32>(cursor - chunk);
        *reinterpret_cast<u32 *>(chunk + 4) = comment_size;
        if (g_ReplayFileHandle != reinterpret_cast<void *>(-1)) {
            u32 written = 0;
            WriteFile(g_ReplayFileHandle, chunk, comment_size, &written, 0);
            if (written != comment_size)
                ReplayShortWriteFailure();
        }
        free(chunk);
    }

    if (g_ReplayFileHandle != reinterpret_cast<void *>(-1)) {
        CloseHandle(g_ReplayFileHandle);
        LeaveCriticalSection(g_ReplayFileLock);
        --g_ReplayFileLockDepth;
    }
    return 0;
}

} // namespace th10
