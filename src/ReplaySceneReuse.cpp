// TH10 0x0042a6a0 — replay/continue scene-reuse preparation, called from the
// title -> game-scene setup (0x00417870) on the mode-flags bit 1 path with
// EBX = the replay game-context record (DAT_00477838 value). It is the
// reduced sibling of the mode-0/mode-1 bodies of 0x00428f60
// (SetupReplayGameContextEaxEcxAbi): the same stage-record seeding for fresh
// stages and the same first four snapshot restores, but no secondary header,
// no difficulty stores, no callback registration and a different tail set of
// restored globals. All fixed addresses are from the raw disassembly; see
// docs/evidence/replay-scene-reuse.md.
#include <string.h>

#include "StagePracticeReplayContext.hpp"
#include "Th10Types.hpp"
#include "ReplaySceneReuse.hpp"

namespace th10 {

namespace {

// ------------------------------------------------------------- globals

extern u32 g_RunStage; // TH10 DAT_00474c7c (stage selector)
extern u32 g_CurrentRunScore;  // TH10 DAT_00474c44
extern u32 g_SceneWord48;      // TH10 DAT_00474c48 (u16 view in the native)
extern u32 g_PracticeStartIndex; // TH10 DAT_00474c70
extern u32 g_PracticeScoreSeed;  // TH10 DAT_00474c90
extern u32 g_RunRankValue;       // TH10 DAT_00474c98 (0xFFFFFE00 sentinel)
extern u32 g_ScenePlayCountSeed; // TH10 DAT_00474c9c
extern u8 g_ScoreBlock;          // TH10 DAT_00474c40 (score/frame block)
extern u16 g_TimelinePrngStateB[4]; // TH10 DAT_004918b0 (LCG-B state word + counter)

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
extern float g_FrameTimeScale;        // TH10 DAT_00476f78

// ------------------------------------------------------------ accessors

inline u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

inline i32 LoadI32At(const void *base, u32 offset)
{
    return static_cast<i32>(LoadU32At(base, offset));
}

inline u16 LoadU16At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u16 *>(
        static_cast<const u8 *>(base) + offset);
}

inline void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

inline void StoreU16At(void *base, u32 offset, u16 value)
{
    *reinterpret_cast<u16 *>(static_cast<u8 *>(base) + offset) = value;
}

inline u32 FloatBits(float value)
{
    u32 bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

} // namespace

// TH10 0x0042a6a0 (native EBX = context record).
void PrepareReplaySceneReuse(void *context_arg)
{
    u8 *const ctx = static_cast<u8 *>(context_arg);
    const u32 mode = LoadU32At(ctx, 0x10);
    const u32 stage = g_RunStage;

    if (mode == 0U) {
        // Fresh stage record (continue path): allocate, zero (native rep
        // stosd of 0x71 dwords), store into the per-stage pointer slot, then
        // seed it. The native reloads the slot and dereferences it without a
        // null check (quirk preserved).
        u8 *record = new u8[0x1c4U];
        if (record != 0) {
            memset(record, 0, 0x1c4U);
        }
        StoreU32At(ctx, 0x1cU + 4U * stage, reinterpret_cast<u32>(record));
        u8 *const slot = reinterpret_cast<u8 *>(
            LoadU32At(ctx, 0x1cU + 4U * stage));
        StoreU16At(slot, 0x2, ReplayPrngSeedWord());
        ReplayPrngDrawCounter() = 0;
        StoreU16At(slot, 0x0, static_cast<u16>(stage));
        u32 flag = LoadU32At(slot, 0x1c0);
        flag ^= (ReplayFlagLatch() ^ flag) & 1U;
        StoreU32At(slot, 0x1c0, flag);
        return;
    }

    if (mode != 1U) {
        return;
    }

    // Replay-load path: refresh the per-stage cursor record.
    u8 *const entry = ctx + 0xa0U + 36U * stage;
    StoreU32At(entry, 0x4, LoadU32At(entry, 0x0));  // saved = value
    StoreU32At(entry, 0xc, LoadU32At(entry, 0x8));  // +0xc = copy of +8
    StoreU32At(entry, 0x14, 0U);

    // Restore the run state from the loaded stage snapshot (unchecked
    // pointer in the native).
    u8 *const snap = reinterpret_cast<u8 *>(LoadU32At(entry, 0x10));
    ReplayPrngSeedWord() = LoadU16At(snap, 0x2);
    ReplayPrngDrawCounter() = 0;
    g_CurrentRunScore = LoadU32At(snap, 0xc);
    StoreU16At(&g_SceneWord48, 0x0, LoadU16At(snap, 0x10));

    // Score-block maximum reset (0x00418b80 thiscall, this = DAT_00474c40).
    SnapshotScoreBlockMaxScoreThisAbi(&g_ScoreBlock,
                                      LoadI32At(snap, 0x14) * 10);

    // Reduced inline of 0x0042a930 (power-timer reset) with the snapshot's
    // +0x18 value: the flag/rate guard first — dead in practice because
    // 0x00418b80 above always latches the flag bit — then the unconditional
    // tail stores.
    if ((LoadU32At(&g_ScoreBlock, 0x24) & 1U) == 0U) {
        StoreU32At(&g_ScoreBlock, 0x24,
                   LoadU32At(&g_ScoreBlock, 0x24) | 1U);
        StoreU32At(&g_ScoreBlock, 0x20,
                   reinterpret_cast<u32>(&g_FrameTimeScale));
    }
    {
        const i32 power = LoadI32At(snap, 0x18);
        StoreU32At(&g_ScoreBlock, 0x18, static_cast<u32>(power));
        StoreU32At(&g_ScoreBlock, 0x14, static_cast<u32>(power - 1));
        StoreU32At(&g_ScoreBlock, 0x1c, FloatBits(static_cast<float>(power)));
    }

    g_PracticeStartIndex = LoadU32At(snap, 0x1c);
    g_RunRankValue = LoadU32At(snap, 0x20);
    g_PracticeScoreSeed = LoadU32At(snap, 0x1b4);
    g_ScenePlayCountSeed = LoadU32At(snap, 0x1b8);
}

} // namespace th10
