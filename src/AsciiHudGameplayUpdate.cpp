// Gameplay ASCII HUD owner update (TH10 0x414900). The owner layout is the
// 0x9ed0-byte HUD owner built by CreateAsciiHudOwner (0x414830) and torn
// down by DestroyAsciiHudOwnerInPlace (0x4145f0); VM records are 0x3ac
// bytes as everywhere else in the ASCII pipeline.
#include "AsciiHudGameplayUpdate.hpp"

#include "AsciiAnimationVm.hpp"
#include "EntityHelpers.hpp"
#include "GameManagerState.hpp"
#include "PlayerRecord.hpp"
#include "ResultScreenScript.hpp"
#include "TimelineRenderObjectSetup.hpp"

namespace th10 {

namespace {

// ---- globals -------------------------------------------------------------

extern void *g_MainChainRenderOwner;  // TH10 DAT_00491c10 (entity manager)
extern void *g_BossBattleState;       // TH10 dword_477704 (+0x10 battle rec)
extern void *g_ScreenTargetBlock;     // TH10 dword_477834
extern void *g_StageState;            // TH10 dword_4776f4
extern void *g_SoundGateContext;      // TH10 0x492590
extern u32 g_GlobalModeFlags;         // TH10 dword_491ff4
extern u32 g_AsciiHudRenderMode;      // TH10 dword_491fb8
extern u32 g_StageIndex;              // TH10 dword_474c7c (1..7)
extern u32 g_StageProgress;           // TH10 dword_474c84 (vs 0x18 gate)
extern u32 g_ScoreValue;              // TH10 dword_474c4c

// Constant pool -----------------------------------------------------------

const float kTimerLow = 0.99f;        // flt_470b68
const float kTimerHigh = 1.01f;       // flt_470b64
const float kTimerStep = 1.0f;        // flt_470afc
const float kEnterY = 416.0f;         // flt_470bdc
const double kEnterX = -128.0;        // dbl_470d10
const float kLeaveY = 400.0f;         // flt_470d08
const double kLeaveX = -112.0;        // dbl_470d00
const float kBenchYHigh = 80.0f;      // flt_470c28
const float kBenchYLow = 64.0f;       // flt_470bc8
const float kBenchXHigh = 0.0f;       // flt_470b04
const float kBenchXLow = -64.0f;      // flt_470b5c
const float kHpFillStep = 0.025f;     // flt_470cf8
const float kBossBaseX = 224.0f;      // flt_470b4c
const double kBossNearX = 64.0;       // dbl_470cf0
const float kBossAlphaScale = -2.984375f; // flt_470ce8 (-191/64 ramp)
const float kBossOffLow = -192.0f;    // flt_470b40
const float kBossOffHigh = 192.0f;    // flt_470b3c

// TH10 0x43dc90: scheduler-channel sound cue (ECX = 0x492590, EDI = sound
// id, one stack arg).
extern void QueueHudSoundCueAbi(u32 sound_id, u32 arg);

// CRT free (j__free) for the +0x9eb8 script state.
extern void HudScriptFreeAbi(void *ptr);

// Helpers ------------------------------------------------------------------

inline u32 LoadU32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline void StoreU32(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

inline i32 LoadI32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline u16 LoadU16(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u16 *>(bytes + offset);
}

inline void StoreU16(u8 *bytes, u32 offset, u16 value)
{
    *reinterpret_cast<u16 *>(bytes + offset) = value;
}

inline void StoreU8(u8 *bytes, u32 offset, u8 value)
{
    bytes[offset] = value;
}

inline float LoadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void StoreFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

// Shared scaled-timer epilogue (inlined at 0x415797 and around the result
// script at 0x41515a): rate via pointer, 0.99..1.01 window.
i32 TickPointerRateTimer(u8 *base, u32 offset)
{
    const u32 count_off = offset + 4U;
    const u32 acc_off = offset + 8U;
    const float rate =
        *reinterpret_cast<const float *>(LoadU32(base, offset + 0xcU));
    i32 count = LoadI32(base, count_off);
    StoreU32(base, offset, static_cast<u32>(count)); // prev latch
    const bool in_window = !(rate < kTimerLow || rate != rate)
        && !(rate > kTimerHigh || rate != rate);
    if (in_window) {
        StoreFloat(base, acc_off, LoadFloat(base, acc_off) + kTimerStep);
        ++count;
    } else {
        StoreFloat(base, acc_off, LoadFloat(base, acc_off) + rate);
        count = static_cast<i32>(LoadFloat(base, acc_off));
    }
    StoreU32(base, count_off, static_cast<u32>(count));
    return count;
}

// Resolve the id in *slot and set the +0x35c kill flag (0x4000000) with
// the +0x14 child-chain propagation used by the +0x9eb8 teardown.
void SetEntityKillFlagByHandleSlot(u32 *slot)
{
    u8 *const entity =
        FindEntityEdxStackAbi(g_MainChainRenderOwner, *slot);
    if (entity == 0)
        return;
    StoreU32(entity, 0x35cU, LoadU32(entity, 0x35cU) | 0x4000000U);
    if (LoadU32(entity, 0x18U) == 0U) {
        u32 *child = *reinterpret_cast<u32 **>(
            LoadU32(entity, 0x14U));
        for (; child != 0;
             child = *reinterpret_cast<u32 **>(child[1]))
            StoreU32(reinterpret_cast<u8 *>(child[0]), 0x35cU,
                     LoadU32(reinterpret_cast<u8 *>(child[0]), 0x35cU)
                         | 0x4000000U);
    }
}

// The entity id-slot teardown used by the 0x9eb8 result-script end: kill
// flag, slot clear. (The native clears each slot after the walk.)
void TearDownHandleSlot(u32 *slot)
{
    if (*slot != 0U)
        SetEntityKillFlagByHandleSlot(slot);
    *slot = 0;
}

// The inline pool-VM spawn used at +0x9e24 and in the +0x9e28 fill loop:
// pool alloc, 0x40000000 flag, +0x20 = 0xf, script bind, list-A back
// append with the id counter, id published to the VM and the slot.
u8 *SpawnHudPoolVm(u32 script, void *resource)
{
    u8 *const vm = static_cast<u8 *>(
        AllocatePoolVmEsiAbi(g_MainChainRenderOwner));
    StoreU32(vm, 0x35cU, LoadU32(vm, 0x35cU) | 0x40000000U);
    StoreU32(vm, 0x20U, 0xfU);
    AssignPoolVmScriptEcxEaxAbi(vm, static_cast<i32>(script));
    u32 id = 0;
    LinkEntityAndAssignIdEaxEsiAbi(&id, vm);
    (void)resource;
    return vm;
}

} // namespace

// TH10 0x414900.
i32 UpdateAsciiHudGameplayStackAbi(void *owner)
{
    u8 *const hud = static_cast<u8 *>(owner);

    // ---- render-mode latch (+0x9eb4 bit 0x10, 120-frame counter) ----
    if ((LoadU32(hud, 0x9eb4U) & 0x10U) != 0U) {
        i32 counter = LoadI32(hud, 0x9eccU) + 1;
        StoreU32(hud, 0x9eccU, static_cast<u32>(counter));
        if (counter >= 120) {
            // (flags & 0x1000) ? 0 : 14 via the neg/sbb/and/add chain.
            g_AsciiHudRenderMode =
                ((g_GlobalModeFlags & 0x1000U) != 0U ? 0U : 14U);
        }
    }

    // ---- VM pools ----
    for (u32 i = 0; i != 9U; ++i)
        (void)FinalizeTimelineRenderObjectSetup(hud + 0x4980U
                                                + i * 0x3acU);
    for (u32 i = 0; i != 4U; ++i)
        (void)FinalizeTimelineRenderObjectSetup(hud + 0x6a8cU
                                                + i * 0x3acU);
    for (u32 i = 0; i != 2U; ++i)
        (void)FinalizeTimelineRenderObjectSetup(hud + 0x793cU
                                                + i * 0x3acU);

    // ---- region state words (7 records at +0x8398) ----
    if (g_ScreenTargetBlock != 0) {
        const PlayerRecord &player =
            *reinterpret_cast<const PlayerRecord *>(g_ScreenTargetBlock);
        const float tx = player.position_x;
        const float ty = player.position_y;
        if ((LoadU32(hud, 0x9eb4U) & 1U) == 0U) {
            // Enter region: y > 432 and x < -128.
            if (!(ty <= kEnterY || ty != ty) && tx < kEnterX) {
                for (u32 i = 0; i != 7U; ++i)
                    StoreU16(hud, 0x8398U + i * 0x3acU, 3U);
                StoreU32(hud, 0x9eb4U, LoadU32(hud, 0x9eb4U) | 1U);
            }
        } else {
            // Leave region: y < 400 or x > -112.
            if (ty < kLeaveY || ty != ty) {
                for (u32 i = 0; i != 7U; ++i)
                    StoreU16(hud, 0x8398U + i * 0x3acU, 2U);
                StoreU32(hud, 0x9eb4U, LoadU32(hud, 0x9eb4U)
                                           & 0xfffffffeU);
            } else if (!(tx <= kLeaveX || tx != tx)) {
                for (u32 i = 0; i != 7U; ++i)
                    StoreU16(hud, 0x8398U + i * 0x3acU, 2U);
                StoreU32(hud, 0x9eb4U, LoadU32(hud, 0x9eb4U)
                                           & 0xfffffffeU);
            }
        }
    }

    // ---- score digits ----
    (void)FinalizeTimelineRenderObjectSetup(hud + 0x8094U);
    {
        u32 divisor = 10000U;
        i32 remaining = static_cast<i32>(g_ScoreValue);
        for (u32 i = 0; i != 5U; ++i) {
            u8 *vm = hud + 0x8440U + i * 0x3acU;
            const i32 digit = remaining / static_cast<i32>(divisor);
            remaining %= static_cast<i32>(divisor);
            (void)InitializeAsciiAnimationVmEntry(
                vm, static_cast<u32>(digit + 0x1e),
                *reinterpret_cast<void **>(hud + 0x9ec8U));
            divisor /= 10U;
            (void)FinalizeTimelineRenderObjectSetup(vm);
        }
    }
    (void)FinalizeTimelineRenderObjectSetup(hud + 0x969cU);

    // ---- boss battle block ----
    if (g_BossBattleState != 0
        && *reinterpret_cast<void **>(
               static_cast<u8 *>(g_BossBattleState) + 0x10U)
               != 0
        && LoadU32(hud, 0x9eb8U) == 0U) {
        u8 *const battle = *reinterpret_cast<u8 **>(
            static_cast<u8 *>(g_BossBattleState) + 0x10U);

        // HP fill: +0x9e84 rises by 0.025 toward hp/hp_max (+0x9e88) and
        // is clamped down to it.
        const i32 hp = LoadI32(battle, 0x23fcU);
        StoreU32(hud, 0x9e8cU, static_cast<u32>(hp));
        const float frac = static_cast<float>(hp)
            / static_cast<float>(LoadI32(battle, 0x2400U));
        StoreFloat(hud, 0x9e88U, frac);
        if (frac > LoadFloat(hud, 0x9e84U))
            StoreFloat(hud, 0x9e84U,
                       LoadFloat(hud, 0x9e84U) + kHpFillStep);
        if (LoadFloat(hud, 0x9e84U) > frac)
            StoreFloat(hud, 0x9e84U, frac);

        // Bench-entity state words: bit 8 of +0x9eb4 toggles with the
        // boss bench region; word 2 inside, word 3 outside.
        u8 *const boss = static_cast<u8 *>(g_ScreenTargetBlock);
        PlayerRecord &player =
            *reinterpret_cast<PlayerRecord *>(boss);
        u32 mode_flags = LoadU32(hud, 0x9eb4U);
        if ((mode_flags & 8U) != 0U) {
            const bool outside =
                player.position_y < kBenchYHigh
                && player.position_x > kBenchXHigh;
            if (outside) {
                const i32 count = LoadI32(hud, 0x9e90U);
                for (i32 i = 0; i < count; ++i)
                    SetEntityStateWordByHandleSlot(
                        reinterpret_cast<u32 *>(hud + 0x9e28U)
                            + static_cast<u32>(i),
                        2);
                SetEntityStateWordByHandleSlot(
                    reinterpret_cast<u32 *>(hud + 0x9e24U), 2);
                StoreU32(hud, 0x9eb4U,
                         (LoadU32(hud, 0x9eb4U) & 0xfffffff7U));
            }
        } else {
            const bool inside =
                !(player.position_y > kBenchYLow
                  || player.position_y != player.position_y)
                && player.position_x < kBenchXLow;
            if (inside) {
                const i32 count = LoadI32(hud, 0x9e90U);
                for (i32 i = 0; i < count; ++i)
                    SetEntityStateWordByHandleSlot(
                        reinterpret_cast<u32 *>(hud + 0x9e28U)
                            + static_cast<u32>(i),
                        3);
                SetEntityStateWordByHandleSlot(
                    reinterpret_cast<u32 *>(hud + 0x9e24U), 3);
                StoreU32(hud, 0x9eb4U, LoadU32(hud, 0x9eb4U) | 8U);
            }
        }
        (void)mode_flags;

        // Spawn: when the +0x9e24 handle is free, spawn the stage HUD
        // entity (script by stage index) plus up to ten child VMs with
        // scripts 0x5b..; extra filled slots are reset to state word 1.
        if (LoadU32(hud, 0x9e24U) == 0U) {
            u32 script = 0x85U;
            const u32 stage = g_StageIndex;
            switch (stage) {
            case 1U:
                if (g_StageProgress >= 0x18U)
                    script = 0x86U;
                break;
            case 2U:
                if (g_StageProgress < 0x18U)
                    goto skip_spawn;
                script = 0x87U;
                break;
            case 3U:
                if (g_StageProgress < 0x18U)
                    goto skip_spawn;
                script = 0x88U;
                break;
            case 4U:
                script = 0x89U + (g_StageProgress >= 0x18U ? 1U : 0U);
                break;
            case 5U:
                script = 0x8bU;
                break;
            case 6U:
                script = 0x8cU;
                break;
            case 7U:
                script = 0x8cU + (g_StageProgress >= 0x18U ? 1U : 0U);
                break;
            default:
                break;
            }
            {
                u8 *vm = SpawnHudPoolVm(
                    script, *reinterpret_cast<void **>(hud + 0x9ec8U));
                StoreU32(hud, 0x9e24U, LoadU32(vm, 0));
            }
        }
    skip_spawn:
        {
            // Fill/reset the ten +0x9e28 slots (bounded by +0x9e90).
            u32 *slots = reinterpret_cast<u32 *>(hud + 0x9e28U);
            const i32 wanted = LoadI32(hud, 0x9e90U);
            for (u32 i = 0; i != 10U; ++i) {
                if (static_cast<i32>(i) < wanted) {
                    if (slots[i] == 0U) {
                        u8 *vm = SpawnHudPoolVm(
                            i + 0x5bU,
                            *reinterpret_cast<void **>(hud + 0x9ec8U));
                        slots[i] = LoadU32(vm, 0);
                    }
                } else if (slots[i] != 0U) {
                    SetEntityStateWordByHandleSlot(&slots[i], 1);
                    slots[i] = 0U;
                }
            }
        }
    } else {
        // Boss gate failed: release the +0x9e24 handle with state word 1
        // and clear the HP/aux accumulators.
        if (LoadU32(hud, 0x9e24U) != 0U)
            SetEntityStateWordByHandleSlot(
                reinterpret_cast<u32 *>(hud + 0x9e24U), 1);
        StoreU32(hud, 0x9e24U, 0U);
        StoreU32(hud, 0x9e84U, 0U);
        StoreU32(hud, 0x9e94U, 0U);
        StoreU32(hud, 0x9e9cU, 0U);
        StoreU32(hud, 0x9ea4U, 0U);
        StoreU32(hud, 0x9eacU, 0U);
    }

    // ---- result-screen script state (+0x9eb8) ----
    {
        void *script_state =
            *reinterpret_cast<void **>(hud + 0x9eb8U);
        if (script_state != 0) {
            if (RunResultScreenScriptStreamStackAbi(
                    static_cast<ResultScreenScriptState *>(script_state))
                == 0) {
                // Frame timer on the script state (+0x8 block).
                TickPointerRateTimer(static_cast<u8 *>(script_state),
                                     4U);
            } else {
                u8 *const ss = static_cast<u8 *>(script_state);
                TearDownHandleSlot(reinterpret_cast<u32 *>(ss + 0x40U));
                TearDownHandleSlot(reinterpret_cast<u32 *>(ss + 0x44U));
                TearDownHandleSlot(reinterpret_cast<u32 *>(ss + 0x48U));
                TearDownHandleSlot(reinterpret_cast<u32 *>(ss + 0x4cU));
                TearDownHandleSlot(reinterpret_cast<u32 *>(ss + 0x50U));
                TearDownHandleSlot(reinterpret_cast<u32 *>(ss + 0x54U));
                HudScriptFreeAbi(script_state);
                StoreU32(hud, 0x9eb8U, 0U);
            }
        }
    }

    // ---- spell/timer block (gated on the battle record) ----
    u8 *battle = 0;
    if (g_BossBattleState != 0)
        battle = *reinterpret_cast<u8 **>(
            static_cast<u8 *>(g_BossBattleState) + 0x10U);
    if (battle != 0) {
        const i32 seconds = LoadI32(hud, 0x9ec0U);
        if (seconds >= 0 && LoadU32(hud, 0x9eb8U) == 0U) {
            const i32 shown = LoadI32(hud, 0x9ec4U);
            if (seconds < shown) {
                u16 word;
                u32 sound;
                if (seconds <= 5) {
                    word = 9U;
                    sound = 0x24U;
                } else if (seconds <= 10) {
                    word = 8U;
                    sound = 0x1bU;
                } else {
                    word = 0U;
                    sound = 0U;
                }
                if (word != 0U) {
                    StoreU16(hud, 0x7c40U, word);
                    StoreU16(hud, 0x7fecU, word);
                    QueueHudSoundCueAbi(sound, 0U);
                }
            } else if (seconds > shown) {
                StoreU16(hud, 0x7c40U, 7U);
                StoreU16(hud, 0x7fecU, 7U);
            }
            if (seconds != shown) {
                (void)InitializeAsciiAnimationVmEntry(
                    hud + 0x793cU,
                    static_cast<u32>(seconds / 10 + 8),
                    *reinterpret_cast<void **>(hud + 0x9ec8U));
                (void)InitializeAsciiAnimationVmEntry(
                    hud + 0x7ce8U,
                    static_cast<u32>(seconds % 10 + 8),
                    *reinterpret_cast<void **>(hud + 0x9ec8U));
                StoreU32(hud, 0x9ec4U, static_cast<u32>(seconds));
            }
        }

        // Spell-card flag block: needs battle+0x2480 bits {0 set, 4 set}.
        const u32 misc = LoadU32(battle, 0x2480U);
        const bool bit4 = ((misc >> 4) & 1U) != 0U;
        const bool bit0 = (misc & 1U) != 0U;
        if (bit4 && !bit0) {
            const u32 spell_timer = LoadU32(battle, 0x2404U);
            const bool practice =
                (LoadU32(static_cast<u8 *>(g_StageState), 0x378cU)
                 & 1U) != 0U;
            u32 mode = (LoadU32(hud, 0x9eb4U) >> 1) & 3U;
            u32 flags = LoadU32(hud, 0x9eb4U);
            bool apply = false;
            if (practice) {
                const u32 thresholds[4] = {0x7d0U, 0x3e8U, 0x190U,
                                           0x190U};
                if (mode == 0U) {
                    if (spell_timer < thresholds[0]) {
                        flags = (flags & 0xfffffffbU) | 2U;
                        StoreU16(hud, 0x9d4cU, 7U);
                        apply = true;
                    }
                } else if (mode == 1U) {
                    if (spell_timer < thresholds[1]) {
                        flags = (flags & 0xfffffffdU) | 4U;
                        StoreU16(hud, 0x9d4cU, 8U);
                        apply = true;
                    }
                } else if (mode == 2U) {
                    if (spell_timer < thresholds[2]) {
                        flags |= 6U;
                        StoreU16(hud, 0x9d4cU, 9U);
                        apply = true;
                    }
                } else if (mode == 3U) {
                    if (spell_timer > thresholds[3]) {
                        flags &= 0xfffffff9U;
                        StoreU16(hud, 0x9d4cU, 0xaU);
                        apply = true;
                    }
                }
            } else {
                const u32 thresholds[4] = {0x2bcU, 0x190U, 0xc8U, 0xc8U};
                if (mode == 0U) {
                    if (spell_timer < thresholds[0]) {
                        flags = (flags & 0xfffffffbU) | 2U;
                        StoreU16(hud, 0x9d4cU, 7U);
                        apply = true;
                    }
                } else if (mode == 1U) {
                    if (spell_timer < thresholds[1]) {
                        flags = (flags & 0xfffffffdU) | 4U;
                        StoreU16(hud, 0x9d4cU, 8U);
                        apply = true;
                    }
                } else if (mode == 2U) {
                    if (spell_timer < thresholds[2]) {
                        flags |= 6U;
                        StoreU16(hud, 0x9d4cU, 9U);
                        apply = true;
                    }
                } else if (mode == 3U) {
                    if (spell_timer > thresholds[3]) {
                        flags &= 0xfffffff9U;
                        StoreU16(hud, 0x9d4cU, 0xaU);
                        apply = true;
                    }
                }
            }
            if (apply)
                StoreU32(hud, 0x9eb4U, flags);

            // Boss overlay anchor/alpha, and the +0x9a48 VM run.
            (void)FinalizeTimelineRenderObjectSetup(hud + 0x9a48U);
            StoreFloat(hud, 0x9d8cU, 480.0f);
            StoreFloat(hud, 0x9d88U,
                       LoadFloat(battle, 0x1068U) + kBossBaseX);
            const float boss_x =
                (*reinterpret_cast<const PlayerRecord *>(
                    g_ScreenTargetBlock)).position_x;
            const float diff = LoadFloat(battle, 0x1068U) - boss_x;
            const float abs_diff = diff < 0.0f ? -diff : diff;
            if (abs_diff < kBossNearX) {
                // byte +0x9d47 = 0x40 - (i32)(|dx| * -2.984375) low byte
                // (native fmul ds:0x470ce8 at 0x415722; the product is
                // negative, so the ramp runs 64 -> 254 over |dx| < 64).
                const i32 scaled = static_cast<i32>(
                    abs_diff * kBossAlphaScale);
                StoreU8(hud, 0x9d47U,
                        static_cast<u8>(0x40
                                        - static_cast<u8>(scaled)));
            } else {
                StoreU8(hud, 0x9d47U, 0xffU);
            }
            if (LoadFloat(battle, 0x1068U) < kBossOffLow
                || LoadFloat(battle, 0x1068U) > kBossOffHigh)
                StoreU8(hud, 0x9d47U, 0U);
        }
    }

    // ---- tail: prev/count latch and the frame timer ----
    StoreU32(hud, 0x9e60U, LoadU32(hud, 0x9e64U));
    TickPointerRateTimer(hud, 0x9e64U);
    return 1;
}

} // namespace th10
