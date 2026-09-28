#include "ReplayContextHelpers.hpp"

#include "AsciiManager.hpp"

#include <string.h>

namespace th10 {

namespace {

// TH10 0x452493 / 0x452422 / 0x4524a1: operator new / free / delete.
u8 *AllocateHeapBlock(u32 bytes);
void FreeHeapBlock(void *pointer);

// TH10 0x0045252d: eh vector constructor iterator over the pool.
void InitReplayFramePool(void *pool, u32 record_size, u32 count,
                         void *reset_fn, void *free_fn);

// TH10 0x0042a200: demo parser (record in ESI, source on the stack).
i32 ParseDemoRecordIntoEsiAbi(void *record, u32 source);

// TH10 0x004294a0: in-place demo record teardown.
void DestroyDemoRecordInPlace(void *record);

// TH10 0x0042ac60: scalar frame-record destructor used as the release
// half of the pool pair (bound to UnlinkGameModeChainRecordInPlace in
// GameModeTeardown.cpp).
void UnlinkGameModeChainRecordInPlace(void *record);

// TH10 0x00401630: AsciiManager::AddFormatTextSelected (the native
// receives the manager as the first cdecl argument).
void AddFormatTextSelectedNative(void *manager, const char *format,
                                 ...);

extern void *g_MainChainContext;    // TH10 DAT_00477810
extern void *g_LoadingScreenContext; // TH10 DAT_004776e0

const u32 kDifficultyColorRed = 0xff5050ffU;
const u32 kDifficultyColorLight = 0xffa0a0ffU;
const u32 kDifficultyColorWhite = 0xffffffffU;

} // namespace

void *ParseDemoRecordEsiStackAbi(u32 source)
{
    u8 *record = AllocateHeapBlock(0x2d4);
    if (record != 0) {
        InitReplayFramePool(record + 0xa0, 0x24, 8,
                            reinterpret_cast<void *>(
                                &ResetReplayFrameRecordEcxAbi),
                            reinterpret_cast<void *>(
                                &UnlinkGameModeChainRecordInPlace));
        memset(record, 0, 0x2d4);
    }
    if (record != 0)
        *reinterpret_cast<u32 *>(record + 0x10) = 2;
    if (ParseDemoRecordIntoEsiAbi(record, source) != 0) {
        if (record != 0) {
            DestroyDemoRecordInPlace(record);
            FreeHeapBlock(record);
        }
        return 0;
    }
    return record;
}

void DestroyDemoRecordEsiAbi(void *record)
{
    if (record == 0)
        return;
    DestroyDemoRecordInPlace(record);
    FreeHeapBlock(record);
}

i32 DrawDifficultyLabelEaxEdxAbi(i32 unused, void *context)
{
    (void)unused;
    u8 *main_chain = static_cast<u8 *>(g_MainChainContext);
    if (main_chain == 0)
        return 1;
    const u32 mode = *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(context) + 0x10);
    if (mode != 1U)
        return 1;

    const i32 difficulty = static_cast<i32>(
        *reinterpret_cast<const u8 *>(
            static_cast<const u8 *>(context) + 0x1c4));
    const float scaled = static_cast<float>(difficulty);
    u32 color;
    // The native compares against DAT_00470c30 / DAT_00470c2c with
    // the unordered-takes-white branch; the thresholds are the two
    // difficulty cut-offs (red below the first, light below the
    // second, white above both).
    if (scaled < 480.0f)
        color = kDifficultyColorRed;
    else if (scaled < 496.0f)
        color = kDifficultyColorLight;
    else
        color = kDifficultyColorWhite;
    *reinterpret_cast<u32 *>(static_cast<u8 *>(g_LoadingScreenContext) +
                             0x8974) = color;
    AddFormatTextSelectedNative(g_LoadingScreenContext, "%3d",
                                difficulty);
    *reinterpret_cast<u32 *>(static_cast<u8 *>(g_LoadingScreenContext) +
                             0x8974) = kDifficultyColorWhite;
    return 1;
}

i32 ComputeReplayFrameOffsetDeltaEcxAbi(const void *record)
{
    const u8 *bytes = static_cast<const u8 *>(record);
    return static_cast<i32>(
        *reinterpret_cast<const u32 *>(bytes + 0x6274) -
        reinterpret_cast<u32>(bytes) - 0x5464U);
}

void SnapshotReplayFrameFieldsEaxAbi(void *record)
{
    u32 *words = static_cast<u32 *>(record);
    words[1] = words[0];
    words[3] = words[2];
    words[5] = 0;
}

void InitializeReplayHeaderDefaultsEaxAbi(void *header)
{
    u8 *bytes = static_cast<u8 *>(header);
    memset(bytes, 0, 0x24);
    // "t10r" magic with the 5 format byte at +4 and the 0x100 game
    // version at +0x10.
    bytes[0] = 't';
    bytes[1] = '1';
    bytes[2] = '0';
    bytes[3] = 'r';
    *reinterpret_cast<u16 *>(bytes + 4) = 5;
    *reinterpret_cast<u32 *>(bytes + 0x10) = 0x100U;
}

void ResetReplayFrameRecordEcxAbi(void *record)
{
    u8 *bytes = static_cast<u8 *>(record);
    memset(bytes, 0, 0x24);
    u32 *words = reinterpret_cast<u32 *>(bytes);
    words[1] = words[0];
    words[3] = words[2];
    words[6] = reinterpret_cast<u32>(bytes);
    words[7] = 0;
    words[8] = 0;
}

} // namespace th10
