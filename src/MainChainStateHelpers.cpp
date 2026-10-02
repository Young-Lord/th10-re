#include "MainChainStateHelpers.hpp"

#include "MainChainContext.hpp"

#include <stdio.h>
#include <string.h>

namespace th10 {

namespace {

// TH10 0x0043e460: QueueBgmCommand(global queue DAT_00492590,
// command, argument).
void QueueBgmCommandWithQueue(i32 command, i32 argument);

// TH10 0x004660b8 / 0x004660bc: EnterCriticalSection /
// LeaveCriticalSection imports on the 0x18-byte stage-load sections.
void EnterStageLoadSection(void *section);
void LeaveStageLoadSection(void *section);

// TH10 0x0044d4e0: SetTransitionBufferVolume(EAX = volume).
void QueueFadeVolume(i32 volume);

// TH10 0x00418c40: title sub-object initializer.
void InitializeTitleScreenSubObjectNative(void *sub_object, void *state);

extern u32 g_BgmModeFlags;          // TH10 DAT_00491d78
extern void *g_BgmQueueFlagsBase;   // TH10 DAT_00477783c
extern u8 g_StageRecordTable;       // TH10 DAT_00474788
extern void *g_StageRecordSlot;     // TH10 DAT_004777848
extern u32 g_StageFrameCount;       // TH10 DAT_00474c44
extern u32 g_StageFramePeak;        // TH10 DAT_00474c40
extern u32 g_StagePauseGate;        // TH10 DAT_00474ca0
extern u32 g_StagePauseTrigger;     // TH10 DAT_00474e36
extern u32 g_StagePausePending;     // TH10 DAT_00491ff4

} // namespace

i32 QueueBgmTrackWavPathEsiStackAbi(i32 track, const char *path)
{
    char buffer[0x100];
    strncpy(buffer, path, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    char *dot = strrchr(buffer, '.');
    // The native rewrites the three bytes after the last dot without
    // any length check; preserved verbatim.
    dot[1] = 'w';
    dot[2] = 'a';
    dot[3] = 'v';
    (*reinterpret_cast<u8 **>(&g_BgmQueueFlagsBase)
         )[static_cast<u32>(track) + 0x1d892U] = 1;
    QueueBgmCommandWithQueue(2, -1);
    return 0;
}

i32 QueueBgmResumeModeSelect(void)
{
    if ((g_BgmModeFlags & 0x10U) != 0U)
        QueueBgmCommandWithQueue(4, 0);
    else
        QueueBgmCommandWithQueue(3, 0);
    return 0;
}

i32 SearchReplayHeaderKeyEaxStackAbi(const void *state, const char *key,
                                     i32 value_a, i32 value_b)
{
    const MainChainContext &ctx =
        *reinterpret_cast<const MainChainContext *>(state);
    const char *cursor = ctx.replay_header_text;
    if (cursor == 0)
        return 0;
    i32 remaining = ctx.replay_header_text_remaining;
    if (strncmp(key, "debug", 5) == 0)
        return 0;
    // TH10 0x0046dc9c vs 0x0046dca4: the native re-compares the two
    // literals ("0100a" against "debug"); always unequal, so the early
    // return is dead. Preserved as the always-false branch.
    if (strncmp("0100a", "debug", 6) == 0)
        return 0;
    while (remaining != 0) {
        if (strncmp(key, cursor, 5) == 0) {
            const char *values = cursor + 6;
            i32 parsed_a = 0;
            i32 parsed_b = 0;
            sscanf(values, "%d %d", &parsed_a, &parsed_b);
            if (parsed_a == value_a && parsed_b == value_b)
                return 0;
        }
        const char *previous = cursor;
        const char *next = strchr(cursor, '\n');
        cursor = next + 1;
        remaining += static_cast<i32>(previous - cursor);
        if (remaining == 0)
            return -1;
    }
    return -1;
}

void EnterAllStageLoadSectionsEaxAbi(void *state)
{
    MainChainContext &ctx = *reinterpret_cast<MainChainContext *>(state);
    for (u32 index = 0; index != 7U; ++index)
        EnterStageLoadSection(&ctx.state_locks[index]);
}

void LeaveAllStageLoadSectionsEaxAbi(void *state)
{
    MainChainContext &ctx = *reinterpret_cast<MainChainContext *>(state);
    for (u32 index = 0; index != 7U; ++index)
        LeaveStageLoadSection(&ctx.state_locks[index]);
}

void LeaveStageLoadSectionEdiEsiAbi(void *state, i32 index)
{
    MainChainContext &ctx = *reinterpret_cast<MainChainContext *>(state);
    LeaveStageLoadSection(&ctx.state_locks[static_cast<u32>(index)]);
    --ctx.state_update_depths[static_cast<u32>(index)];
}

void *SelectStageRecordSlotThiscall(void *selector)
{
    u8 *bytes = static_cast<u8 *>(selector);
    const u32 slot = *reinterpret_cast<u32 *>(bytes + 0x40);
    *reinterpret_cast<u32 *>(bytes + 0x3c) = slot;
    void *record = &g_StageRecordTable + slot * 48U;
    g_StageRecordSlot = record;
    return record;
}

void CrossProductVec3EaxDxEcxAbi(float out[3], const float a[3],
                                 const float b[3])
{
    out[0] = b[2] * a[1] - a[2] * b[1];
    out[1] = a[2] * b[0] - b[2] * a[0];
    out[2] = a[0] * b[1] - b[0] * a[1];
}

i32 ReadGameFlagBit6EaxAbi(const void *state)
{
    const MainChainContext &ctx =
        *reinterpret_cast<const MainChainContext *>(state);
    const u32 flags = ctx.input_setup_flags_0150;
    return static_cast<i32>((flags >> 6) & 1U);
}

i32 ReadGameFlagBit5EaxAbi(const void *state)
{
    const MainChainContext &ctx =
        *reinterpret_cast<const MainChainContext *>(state);
    const u32 flags = ctx.input_setup_flags_0150;
    return static_cast<i32>((flags >> 5) & 1U);
}

i32 ReadGameFlagBit3EaxAbi(const void *state)
{
    const MainChainContext &ctx =
        *reinterpret_cast<const MainChainContext *>(state);
    const u32 flags = ctx.input_setup_flags_0150;
    return static_cast<i32>((flags >> 3) & 1U);
}

i32 ReadGameFlagBit1EaxAbi(const void *state)
{
    const MainChainContext &ctx =
        *reinterpret_cast<const MainChainContext *>(state);
    const u32 flags = ctx.input_setup_flags_0150;
    return static_cast<i32>((flags >> 1) & 1U);
}

i32 ReadGameFlagBit2EaxAbi(const void *state)
{
    const MainChainContext &ctx =
        *reinterpret_cast<const MainChainContext *>(state);
    const u32 flags = ctx.input_setup_flags_0150;
    return static_cast<i32>((flags >> 2) & 1U);
}

void *SeedScoreRecordRetryCountersEaxAbi(void *record)
{
    MainChainContext &ctx = *reinterpret_cast<MainChainContext *>(record);
    // MainChainState enum fields written as raw dwords (width preserved).
    *reinterpret_cast<i32 *>(&ctx.previous_state) = -2;
    *reinterpret_cast<u32 *>(&ctx.requested_state) = 0;
    return record;
}

void *ReleaseScoreRecordSlotEsiAbi(void *record)
{
    u8 *bytes = static_cast<u8 *>(record);
    void *slot = *reinterpret_cast<void **>(bytes + 0xc);
    if (slot == 0)
        return 0;
    void **vtable = *reinterpret_cast<void ***>(slot);
    typedef void (*ReleaseThunk)(void *);
    reinterpret_cast<ReleaseThunk>(vtable[2])(slot);
    *reinterpret_cast<u32 *>(bytes + 0xc) = 0;
    return slot;
}

i32 TickBgmFadeSequencerEaxAbi(void *state)
{
    u8 *bytes = static_cast<u8 *>(state);
    u32 *fade = *reinterpret_cast<u32 **>(bytes + 0x5208);
    if (fade == 0)
        return 0;
    i32 result = 0;
    // Mode 1: ramp in over the remaining frames; at completion stop
    // the BGM through vtable slot 18 of the bound track (+4).
    if (fade[7] == 1U) {
        const i32 remaining = static_cast<i32>(fade[5]) - 1;
        fade[5] = static_cast<u32>(remaining);
        if (remaining > 0) {
            result = 5000 * remaining / static_cast<i32>(fade[6]) - 5000;
            QueueFadeVolume(result);
        } else {
            fade[7] = 0;
            void *track = *reinterpret_cast<void **>(fade + 1);
            void **vtable = *reinterpret_cast<void ***>(track);
            typedef void (*StopThunk)(void *);
            reinterpret_cast<StopThunk>(vtable[18])(track);
        }
    }
    if (fade[7] == 2U) {
        const i32 remaining = static_cast<i32>(fade[5]) - 1;
        fade[5] = static_cast<u32>(remaining);
        if (remaining > 0) {
            result = -5000 * remaining / static_cast<i32>(fade[6]);
            QueueFadeVolume(result);
        } else {
            result = remaining;
            fade[7] = 0;
        }
    }
    if (fade[7] == 4U) {
        const i32 remaining = static_cast<i32>(fade[5]) - 1;
        fade[5] = static_cast<u32>(remaining);
        if (remaining > 0) {
            result = 1000 * remaining / static_cast<i32>(fade[6]) - 1000;
            QueueFadeVolume(result);
        } else {
            result = remaining;
            fade[7] = 0;
        }
    }
    if (fade[7] == 3U) {
        const i32 remaining = static_cast<i32>(fade[5]) - 1;
        fade[5] = static_cast<u32>(remaining);
        if (remaining > 0) {
            result = -1000 * remaining / static_cast<i32>(fade[6]);
            QueueFadeVolume(result);
        } else {
            result = remaining;
            fade[7] = 0;
        }
    }
    return result;
}

void *ResetGameStateObjectEcxEsiAbi(void *sub_object, void *state)
{
    u8 *bytes = static_cast<u8 *>(state);
    MainChainContext &ctx = *reinterpret_cast<MainChainContext *>(state);
    InitializeTitleScreenSubObjectNative(sub_object, &ctx.window_0048);
    ctx.thread_control.thread_handle = 0;
    ctx.thread_control.thread_id = 0;
    ctx.thread_control.stop_requested = 0;
    ctx.thread_control.field_0010 = 0;
    // TH10 0x004703e4: stage-entity vtable.
    *reinterpret_cast<u32 *>(&ctx.thread_control.marker) = 0x4703e4U;
    memset(bytes, 0, 0x784);
    *reinterpret_cast<u32 *>(&ctx.callback_state_byte) |= 0x140U;
    return bytes;
}

i32 RecordStageFrameCountPeakEaxAbi(void *state)
{
    u8 *bytes = static_cast<u8 *>(state);
    const u32 count = g_StageFrameCount;
    *reinterpret_cast<u32 *>(bytes + 0x9e98) = count;
    if (g_StageFramePeak < count)
        g_StageFramePeak = count;
    return static_cast<i32>(count);
}

void InitializeStageFlagByteTableEaxAbi(void *table)
{
    u8 *bytes = static_cast<u8 *>(table);
    const u32 ones = 0x01010101U;
    *reinterpret_cast<u32 *>(bytes + 0x1d882) = ones;
    *reinterpret_cast<u32 *>(bytes + 0x1d886) = ones;
    *reinterpret_cast<u32 *>(bytes + 0x1d88a) = ones;
    *reinterpret_cast<u32 *>(bytes + 0x1d88e) = ones;
    for (u32 group = 0; group != 6U; ++group) {
        u8 *base = bytes + 0x4e9 + group * 0x437cU;
        for (u32 row = 0; row != 4U; ++row)
            for (u32 index = 0; index != 6U; ++index)
                base[row * 0x30U + index * 8U] = 1;
    }
}

const char *SkipLineBreaksEaxEdxAbi(const char *cursor, i32 *remaining)
{
    const char current = cursor[0];
    if (current != '\n' && current != '\r') {
        while (*remaining != 0) {
            ++cursor;
            --(*remaining);
            const char next = cursor[0];
            if (next == '\n' || next == '\r')
                break;
        }
    }
    while (*remaining != 0) {
        const char next = cursor[0];
        if (next != '\n' && next != '\r')
            break;
        ++cursor;
        --(*remaining);
    }
    return cursor;
}

} // namespace th10
