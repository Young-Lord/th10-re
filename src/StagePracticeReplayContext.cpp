// Replay game-context snapshot/restore (TH10 0x00428f60 family). Builds the
// 0x2d4-byte record that mirrors a running/practiced game into the
// replay-save header layout ("t10r" secondary header at +0x14, 100-byte
// header at +0x18, per-stage 0x1c4 records at +0x1c + 4*stage, eight 0x24
// stage slots at +0xa0), and restores one back into the live globals.
#include <string.h>

#include "StagePracticeReplayContext.hpp"

#include "CallbackScheduler.hpp"
#include "ReplayViewLoad.hpp"
#include "TitleCalcCluster.hpp"

namespace th10 {

namespace {

// Main-chain object 840 (TH10 DAT_00477840); its +0x24 block is the 0x34-byte
// per-run stats image mirrored into replay headers.
extern u8 *g_MainChainObject840; // TH10 DAT_00477840
// Current stage index (TH10 DAT_00474cac).
extern i32 g_CurrentStageIndex; // TH10 DAT_00474cac
// Run snapshot globals mirrored into the replay header.
extern u32 g_ResultScore;        // TH10 DAT_00474c98
extern u32 g_ResultPower;        // TH10 DAT_00474c9c
extern u32 g_ResultPiv;          // TH10 DAT_00474ca4
extern u32 g_PlayerDifficulty;   // TH10 DAT_00474c74
extern u16 g_PlayerDifficultyWord; // TH10 DAT_00474c78
extern u32 g_PlayerShotMode;     // TH10 DAT_00474c7c
extern u32 g_PlayerContinueMode; // TH10 DAT_00474c88
extern u32 g_GameModeFlags;      // TH10 DAT_00474ca0
extern u32 g_PracticeStageValue; // TH10 DAT_00474cc8
extern u32 g_PracticeStageValue2; // TH10 DAT_00474cc0
// Replay determinism seeds: the native snapshots the second LCG state word
// (TH10 0x4918b0) into the per-stage record at +2 on the setup path and
// seeds it back from the record (clearing the draw counter 0x4918b4) on the
// restore path. The state word and counter are views of
// g_TimelinePrngStateB[0] and g_TimelinePrngStateB[2..3]; the replay-flag
// pair lives at TH10 0x491fc4.
extern u16 g_TimelinePrngStateB[4]; // TH10 DAT_004918b0 (LCG-B state word + draw counter)

inline u16 &ReplayPrngSeedWord()
{
    return g_TimelinePrngStateB[0];
}

inline u32 &ReplayPrngDrawCounter()
{
    return *reinterpret_cast<u32 *>(&g_TimelinePrngStateB[2]);
}

inline u32 &ReplayFlagLatch()
{
    return *reinterpret_cast<u32 *>(0x491fc4U);
}
// Score block (TH10 DAT_00474c40).
extern u8 g_ScoreBlock; // TH10 DAT_00474c40
// Game-manager sub-object used by the restore path (TH10 DAT_00477810).
extern u8 *g_ManagerObject810; // TH10 DAT_00477810
// Global scheduler (TH10 DAT_00491c14).
extern CallbackScheduler *g_GameScheduler; // TH10 DAT_00491c14
// Input edge byte used by the retry callback (TH10 0x474e61).
extern u8 g_ReplayRetryEdge; // TH10 0x474e61
// TH10 0x004297d0: shared replay-context timer tick (implemented in
// ReplayContextTimerTick.cpp; native body takes the record in EDI).
i32 TickReplayContextTimers4297d0(void *record);
// TH10 0x00429a70: context retry handler (boundary).
extern i32 HandleReplayContextRetry429a70(void *record, void *record2);

i32 TH10_FASTCALL ReplayContextTimerCallback(void *arg)
{
    // TH10 0x42a3d0 moves the scheduler's ECX argument into EDI and tail
    // calls the tick; its eax (always 1) is returned unchanged.
    return TickReplayContextTimers4297d0(arg);
}

i32 TH10_FASTCALL ReplayContextRetryGateCallback(void *arg)
{
    // TH10 0x0042a3e0. arg = the 0x24-byte chain element whose +0x10 holds
    // the armed flag and +0x1c8 the blink phase counter.
    if (g_MainChainObject840 == 0)
        return 1;
    u8 *record = static_cast<u8 *>(arg);
    if ((g_MainChainObject840[88] & 4) != 0)
        return 1;
    if (*reinterpret_cast<const u32 *>(record + 0x10U) != 1)
        return 1;
    if ((g_ReplayRetryEdge & 1) == 0)
        return 1;
    // Signed mod-4 phase (native and 0x80000003 / dec / or 0xFFFFFFFC / inc).
    i32 phase = *reinterpret_cast<const i32 *>(record + 0x1C8U);
    if (phase < 0)
        phase = (phase - 3) % 4 + 1; // native sign-preserving idiom
    else
        phase %= 4;
    if (phase != 0)
        return 6;
    return 1;
}

i32 TH10_FASTCALL ReplayContextRetryDrawCallback(void *arg)
{
    // TH10 0x0042a430: while the keep-alive flag runs, idle; otherwise the
    // shared retry draw handler.
    if (g_MainChainObject840 != 0 && (g_MainChainObject840[88] & 4) != 0)
        return 1;
    return HandleReplayContextRetry429a70(arg, arg);
}

} // namespace

// TH10 0x00418b80.
void SnapshotScoreBlockMaxScoreThisAbi(void *score_block, i32 value)
{
    u8 *block = static_cast<u8 *>(score_block);
    *reinterpret_cast<i32 *>(block + 0x0CU) = value / 10;
    u32 *flags = reinterpret_cast<u32 *>(block + 0x24U);
    if ((*flags & 1U) == 0U) {
        *flags |= 1U;
        *reinterpret_cast<i32 *>(block + 0x18U) = 0;
        *reinterpret_cast<i32 *>(block + 0x14U) = -999999;
        *reinterpret_cast<i32 *>(block + 0x1CU) = 0;
        *reinterpret_cast<u32 **>(block + 0x20U)
            = reinterpret_cast<u32 *>(0x476F78U); // &flt_476f78 rate
    }
    *reinterpret_cast<i32 *>(block + 0x18U) = 0;
    *reinterpret_cast<i32 *>(block + 0x1CU) = 0;
    *reinterpret_cast<i32 *>(block + 0x14U) = -1;
}

// TH10 0x0042a930.
void ResetScoreBlockPowerTimerEaxStackAbi(void *score_block, i32 value)
{
    u8 *block = static_cast<u8 *>(score_block);
    u32 *flags = reinterpret_cast<u32 *>(block + 0x24U);
    if ((*flags & 1U) == 0U) {
        *reinterpret_cast<float *>(block + 0x18U) = 0.0f;
        *reinterpret_cast<u32 *>(block + 0x14U) = 0xFFF0BDC1U; // NaN sentinel
        *reinterpret_cast<float *>(block + 0x1CU) = 0.0f;
        *reinterpret_cast<u32 **>(block + 0x20U)
            = reinterpret_cast<u32 *>(0x476F78U);
        *flags |= 1U;
    }
    *reinterpret_cast<i32 *>(block + 0x18U) = value;
    *reinterpret_cast<i32 *>(block + 0x14U) = value - 1;
    *reinterpret_cast<float *>(block + 0x1CU) = static_cast<float>(value);
}

// TH10 0x00418c40.
void InitReplayHeaderStatsBlockEdxAbi(u8 *block)
{
    memset(block, 0, 0x34U);
    *reinterpret_cast<u16 *>(block + 22U) = 600; // +0x16
    *reinterpret_cast<u16 *>(block + 24U) = 600; // +0x18
    block[27] = 1; // +0x1b
    block[28] = 1; // +0x1c
    *reinterpret_cast<u32 *>(block + 48U) |= 0x100U; // +0x30
    block[26] = 0; // +0x1a
    *reinterpret_cast<u32 *>(block) = 1048579U; // 0x100003
    block[29] = 0; // +0x1d
    block[30] = 0; // +0x1e
    *reinterpret_cast<u32 *>(block + 4U)
        = *reinterpret_cast<const u32 *>(0x474E88U);
    *reinterpret_cast<u32 *>(block + 8U)
        = *reinterpret_cast<const u32 *>(0x474E8CU);
    *reinterpret_cast<u32 *>(block + 12U)
        = *reinterpret_cast<const u32 *>(0x474E90U);
    *reinterpret_cast<u32 *>(block + 16U)
        = *reinterpret_cast<const u32 *>(0x474E94U);
    *reinterpret_cast<u16 *>(block + 20U)
        = *reinterpret_cast<const u16 *>(0x474E98U);
    block[31] = 2;  // +0x1f
    block[34] = 0;  // +0x22
    block[32] = 100; // +0x20
    block[33] = 80;  // +0x21
}

namespace {

// Build one 0x24-byte chain element and register it. callback_kind 0/1 go to
// the calculation chain (0x00449ae0), kind 2 to the draw chain (0x00449b70).
ChainElem *RegisterContextCallback(ChainCallback callback, void *context,
                                   i32 kind)
{
    ChainElem *elem = new ChainElem();
    if (elem != 0) {
        elem->priority = 0;
        elem->flags = (elem->flags & ~1U) | 1U;
        elem->callback = callback;
        elem->registration_hook = 0;
        elem->calculation_followup = 0;
        elem->link.owner = reinterpret_cast<ChainElem *>(elem);
        elem->link.next = 0;
        elem->link.previous = 0;
        elem->arg = context;
        if (kind == 2)
            (void)CallbackSchedulerApi::AddToDrawChain(g_GameScheduler, elem,
                                                       0);
        else
            (void)CallbackSchedulerApi::AddToCalculationChain(g_GameScheduler,
                                                              elem, 0);
    }
    return elem;
}

} // namespace

// TH10 0x00428f60.
i32 SetupReplayGameContextEaxEcxAbi(i32 mode, const char *file_name,
                                    void *context)
{
    u8 *const ctx = static_cast<u8 *>(context);
    *reinterpret_cast<i32 *>(ctx + 0x10U) = mode;

    if (mode == 0) {
        *reinterpret_cast<u8 **>(ctx + 0x00U + 0U)
            = reinterpret_cast<u8 *>(ctx); // native republishes the record
        (void)0; // dword_477868 = ctx is the twin global below
        extern u8 *g_ReplayContextOwner; // TH10 DAT_004777.. owner dword
        g_ReplayContextOwner = ctx;
        (void)FreeGameModeChainEntriesEaxEcxAbi(g_CurrentStageIndex, ctx);
        *reinterpret_cast<void **>(ctx + 0x9CU)
            = AllocateGameModeChainEntryEsiStackAbi(g_CurrentStageIndex, ctx);

        // Secondary 0x24 header: "t10r" tag, version 5, +0x10 = 0x100.
        u8 *secondary = new u8[0x24U];
        if (secondary != 0) {
            memset(secondary, 0, 0x24U);
            *reinterpret_cast<u32 *>(secondary) = 0x72303174U;
            *reinterpret_cast<u16 *>(secondary + 4U) = 5;
            *reinterpret_cast<u32 *>(secondary + 16U) = 0x100U;
        }
        *reinterpret_cast<u8 **>(ctx + 0x14U) = secondary;

        // 100-byte replay header. Native quirk: the stats block is written
        // first and then the whole header is zeroed over it.
        u8 *header = new u8[0x64U];
        if (header != 0) {
            InitReplayHeaderStatsBlockEdxAbi(header + 0x14U);
            memset(header, 0, 0x64U);
        }
        *reinterpret_cast<u8 **>(ctx + 0x18U) = header;
        if (header != 0) {
            *reinterpret_cast<u32 *>(header + 0x50U) = g_ResultScore;
            *reinterpret_cast<u32 *>(header + 0x54U) = g_ResultPower;
            *reinterpret_cast<u32 *>(header + 0x58U) = g_ResultPiv;
            if (g_MainChainObject840 != 0)
                memcpy(header + 0x14U, g_MainChainObject840 + 0x24U, 0x34U);
            *reinterpret_cast<u32 *>(header + 0x60U) = g_PracticeStageValue2;
        }

        // Fresh 0x1c4 stage record for the current stage.
        u8 *record = new u8[0x1C4U];
        if (record != 0)
            memset(record, 0, 0x1C4U);
        *reinterpret_cast<u8 **>(ctx + 0x1CU + 4U * g_CurrentStageIndex)
            = record;
        if (record != 0) {
            *reinterpret_cast<u16 *>(record) =
                static_cast<u16>(g_CurrentStageIndex);
            *reinterpret_cast<u16 *>(record + 2U) = ReplayPrngSeedWord();
            ReplayPrngDrawCounter() = 0;
            u32 flag = *reinterpret_cast<const u32 *>(record + 0x1C0U);
            flag ^= (ReplayFlagLatch() ^ flag) & 1U;
            *reinterpret_cast<u32 *>(record + 0x1C0U) = flag;
            if (ReplayFlagLatch() != 0) {
                *reinterpret_cast<u32 *>(record + 0x24U) = 0;
                *reinterpret_cast<u32 *>(record + 0x28U) = 0;
            }
            *reinterpret_cast<u32 *>(record + 0x0CU) = g_PlayerDifficulty;
            *reinterpret_cast<u16 *>(record + 0x10U) =
                g_PlayerDifficultyWord;
            *reinterpret_cast<u32 *>(record + 0x14U) = g_PlayerShotMode;
            *reinterpret_cast<u32 *>(record + 0x18U) = g_PlayerContinueMode;
            *reinterpret_cast<u32 *>(record + 0x1CU) = g_GameModeFlags;
            *reinterpret_cast<u32 *>(record + 0x20U) = g_PracticeStageValue;
            *reinterpret_cast<u32 *>(record + 0x1B4U) = g_PracticeStageValue2;
        }

        *reinterpret_cast<void **>(ctx + 8U) = RegisterContextCallback(
            &ReplayContextTimerCallback, ctx, 0);
        *reinterpret_cast<void **>(ctx + 0x1CCU) = RegisterContextCallback(
            &ReplayContextRetryGateCallback, ctx, 1);
        *reinterpret_cast<void **>(ctx + 0x0CU) = RegisterContextCallback(
            &ReplayContextRetryDrawCallback, ctx, 2);
        *reinterpret_cast<i32 *>(ctx + 0x1D0U) = g_CurrentStageIndex;
        return 0;
    }

    if (mode != 1 && mode != 2)
        return 0;

    void *view = ctx;
    if (mode == 2) {
        return (LoadReplayViewRecord(view, file_name) == 0) ? 0 : -1;
    }

    // Mode 1: load, then restore the loaded snapshot into the live globals.
    *reinterpret_cast<u8 **>(ctx) = reinterpret_cast<u8 *>(ctx);
    extern u8 *g_ReplayContextOwner;
    g_ReplayContextOwner = ctx;
    if (LoadReplayViewRecord(view, file_name) != 0)
        return -1;

    u8 *const header = *reinterpret_cast<u8 **>(ctx + 0x18U);
    if (g_MainChainObject840 != 0)
        memcpy(g_MainChainObject840 + 0x24U, header + 0x14U, 0x34U);

    const i32 stage = g_CurrentStageIndex;
    u8 *const entry = *reinterpret_cast<u8 **>(
        ctx + 0xB0U + 36U * stage);
    // Per-stage slot refresh in the native (index × 9 dword addressing).
    *reinterpret_cast<void **>(ctx + 0xA4U + 36U * stage)
        = *reinterpret_cast<void **>(ctx + 0xA0U + 36U * stage);
    *reinterpret_cast<void **>(ctx + 0xB4U + 36U * stage) = 0;
    *reinterpret_cast<void **>(ctx + 0xACU + 36U * stage)
        = *reinterpret_cast<void **>(ctx + 0xA8U + 36U * stage);

    if (header != 0) {
        *reinterpret_cast<u32 *>(0x474C98U)
            = *reinterpret_cast<const u32 *>(header + 0x50U);
        *reinterpret_cast<u32 *>(0x474C9CU)
            = *reinterpret_cast<const u32 *>(header + 0x54U);
        *reinterpret_cast<u32 *>(0x474CA4U)
            = *reinterpret_cast<const u32 *>(header + 0x58U);
    }
    if (entry != 0) {
        *reinterpret_cast<u32 *>(0x474C68U)
            = *reinterpret_cast<const u32 *>(header + 0x50U);
        *reinterpret_cast<u32 *>(0x474C6CU)
            = *reinterpret_cast<const u32 *>(header + 0x54U);
        *reinterpret_cast<u32 *>(0x474C74U)
            = *reinterpret_cast<const u32 *>(header + 0x58U);
        ReplayPrngSeedWord() = *reinterpret_cast<const u16 *>(entry + 2U);
        ReplayPrngDrawCounter() = 0;
        *reinterpret_cast<u32 *>(0x474C44U)
            = *reinterpret_cast<const u32 *>(entry + 0x0CU);
        *reinterpret_cast<u16 *>(0x474C48U)
            = *reinterpret_cast<const u16 *>(entry + 0x10U);
        SnapshotScoreBlockMaxScoreThisAbi(
            &g_ScoreBlock,
            *reinterpret_cast<const i32 *>(entry + 0x14U) * 10);
        ResetScoreBlockPowerTimerEaxStackAbi(
            &g_ScoreBlock, *reinterpret_cast<const i32 *>(entry + 0x18U));
        *reinterpret_cast<u32 *>(0x474CA0U)
            = *reinterpret_cast<const u32 *>(entry + 0x1CU);
        *reinterpret_cast<u32 *>(0x474CC8U)
            = *reinterpret_cast<const u32 *>(entry + 0x20U);
        *reinterpret_cast<u32 *>(0x474CC0U)
            = *reinterpret_cast<const u32 *>(entry + 0x1B4U);
        *reinterpret_cast<u32 *>(0x474CCCU)
            = *reinterpret_cast<const u32 *>(entry + 0x1B8U);
    }
    (void)g_ManagerObject810; // read by the native gate callbacks only

    *reinterpret_cast<void **>(ctx + 8U) = RegisterContextCallback(
        &ReplayContextTimerCallback, ctx, 0);
    *reinterpret_cast<void **>(ctx + 0x1CCU) = RegisterContextCallback(
        &ReplayContextRetryGateCallback, ctx, 1);
    *reinterpret_cast<void **>(ctx + 0x0CU) = RegisterContextCallback(
        &ReplayContextRetryDrawCallback, ctx, 2);
    *reinterpret_cast<i32 *>(ctx + 0x1D0U) = -1;
    return 0;
}

// TH10 0x00429610.
void *CreateReplayGameContextStdcallAbi(i32 mode, const char *file_name)
{
    u8 *record = new u8[0x2D4U];
    if (record != 0) {
        // Eight 0x24-byte stage slots at +0xa0 (eh vector constructor
        // iterator in the native, value-initialized here).
        memset(record, 0, 0x2D4U);
    }
    if (SetupReplayGameContextEaxEcxAbi(mode, file_name, record) == 0)
        return record;
    if (record != 0) {
        extern void DestroyUnknownMainChainObjectInPlace(void *object);
        DestroyUnknownMainChainObjectInPlace(record); // TH10 0x004294a0
        delete[] record;
    }
    return 0;
}

} // namespace th10
