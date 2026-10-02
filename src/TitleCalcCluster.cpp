// Semantic cluster of the remaining title-screen calculation boundaries:
// 0x00424d90, 0x0042a450, 0x00417040, 0x00404450, 0x00417770, 0x00418a00,
// 0x00409f90 plus the shared 0x004188a0 extend helper used by 0x00417040.
// Every fixed global address and struct offset below is taken from the
// native disassembly of the corresponding function (see
// docs/evidence/title-calc-cluster.md).
#include <string.h>

#include "AsciiAnimationVm.hpp"
#include "AsciiHudOwner.hpp"
#include "BgmRuntime.hpp"
#include "EntityHelpers.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "PlayerOptionRecords.hpp"
#include "PlayerRecord.hpp"
#include "PlayerShotData.hpp"
#include "PlayerTimerHelpers.hpp"
#include "Th10Platform.hpp"
#include "Th10Types.hpp"
#include "TitleScreenState.hpp"
#include "TimelineRenderObjectSetup.hpp"
#include "TimelineRenderObjects.hpp"
#include "TitleCalcCluster.hpp"

namespace th10 {

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern void *g_AsciiHudOwner;        // TH10 DAT_0047770c
extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590
extern i32 g_PlayerLivesRemaining;   // TH10 DAT_00474c70

// --- Still-unreconstructed narrow callees -------------------------------

u32 LoadU32From(const void *address)
{
    return *static_cast<const u32 *>(address);
}

u32 LoadU32At(const void *base, u32 offset)
{
    return LoadU32From(static_cast<const u8 *const>(base) + offset);
}

void StoreU32To(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

i32 LoadI32FromAddress(u32 address)
{
    return static_cast<i32>(*reinterpret_cast<const u32 *>(address));
}

i32 LoadI32At(const void *base, u32 offset)
{
    return static_cast<i32>(LoadU32At(base, offset));
}

void StoreI32To(void *base, u32 offset, i32 value)
{
    StoreU32To(base, offset, static_cast<u32>(value));
}

// Shared 1.0f constant the native sub-records reference (TH10 0x00476f78).
const float kUnitRate = 1.0f;

// --- TH10 0x004188a0. Native EAX = the 0x474c40 frame-state block
// ([eax+0x30] == DAT_00474c70, the signed lives dword), ECX = increment;
// plain ret. Adds the increment to the lives, capped at 9 (with the extend
// sound and a fresh extend-entity spawn), then refreshes the life icons.
void AwardExtendedLifeEaxEcxAbi(void *frame_state, i32 increment)
{
    i32 lives = LoadI32At(frame_state, 0x30U) + increment;
    StoreI32To(frame_state, 0x30U, lives);
    if (lives > 9) {
        StoreI32To(frame_state, 0x30U, 9);
    } else {
        // Native: (ECX = 0x492590, EDI = 0x2c, stack = 0) -> 0x43dc90.
        EnqueueBgmSoundValue(&g_TransitionRoot, 44U, 0);
        AsciiHudOwner &hud =
            *reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner);
        ReleaseEntityById(g_MainChainRenderOwner,
                          hud.second_banner_handle);
        hud.second_banner_handle = 0U;
        i32 *const spawned = SpawnSetupEffectVmListABack(
            static_cast<i32>(
                reinterpret_cast<u32>(hud.front_anm_work)), 0x4BU);
        hud.second_banner_handle = static_cast<u32>(*spawned);
    }
    RefreshLifeIconsEaxStackAbi(
        LoadI32At(frame_state, 0x30U)); // == DAT_00474c70
}

} // namespace

// TH10 0x00424d90.
void ResetOptionPositionRecordsEsiAbi(void *manager)
{
    u8 *const base = static_cast<u8 *>(manager);
    PlayerRecord &player = *reinterpret_cast<PlayerRecord *>(base);
    player.mode = 1;

    // First-use initialization of the three option sub-records, then the
    // per-call reset values (the first record also gets the -1.0f float and
    // the -2 dword the other two do not).
    if ((player.autocollect_timer.flags & 1U) == 0U) {
        player.autocollect_timer.count = 0;
        player.autocollect_timer.prev = static_cast<i32>(-999999);
        player.autocollect_timer.accum = 0;
        player.autocollect_timer.rate = &kUnitRate;
        player.autocollect_timer.flags |= 1U;
    }
    player.autocollect_timer.count = static_cast<i32>(-1);
    *reinterpret_cast<u32 *>(&player.autocollect_timer.accum) =
        0xBF800000U; // -1.0f
    player.autocollect_timer.prev = static_cast<i32>(-2);

    if ((player.frame_timer.flags & 1U) == 0U) {
        player.frame_timer.count = 0;
        player.frame_timer.prev = static_cast<i32>(-999999);
        player.frame_timer.accum = 0;
        player.frame_timer.rate = &kUnitRate;
        player.frame_timer.flags |= 1U;
    }
    player.frame_timer.count = 0;
    player.frame_timer.accum = 0;
    player.frame_timer.prev = static_cast<i32>(-1);

    if ((player.move_gate_timer.flags & 1U) == 0U) {
        player.move_gate_timer.count = 0;
        player.move_gate_timer.prev = static_cast<i32>(-999999);
        player.move_gate_timer.accum = 0;
        player.move_gate_timer.rate = &kUnitRate;
        player.move_gate_timer.flags |= 1U;
    }
    player.move_gate_timer.count = 0;
    player.move_gate_timer.accum = 0;
    player.move_gate_timer.prev = static_cast<i32>(-1);

    // Release the tracked option entity (the native stores the cleared slot
    // twice; the second store is redundant) and refresh the life icons
    // through the 0x413790 tail call.
    ReleaseEntityById(g_MainChainRenderOwner,
                      player.focus_glide_entity_id);
    player.focus_glide_entity_id = 0;
    player.focus_glide_entity_id = 0;
    RefreshLifeIconsEaxStackAbi(g_PlayerLivesRemaining);
}

namespace {

// The scattered 4-iteration copy used by 0x0042a450. Eight dwords are moved
// between the manager and the selected record with source stride 0x98 and
// destination stride 8; the dword pairs sit at these offsets relative to the
// loop bases.
const u32 kScatterSrcOffsets[8] = {
    0U, 4U, 8U, 0xCU, 0x10U, 0x14U, 0x18U, 0x1CU};
const u32 kScatterDstOffsets[8] = {
    0U, 4U, 0x20U, 0x24U, 0x40U, 0x44U, 0x60U, 0x64U};

} // namespace

// TH10 0x0042a450.
void ApplyOptionPositionStateEbxAbi(void *record)
{
    u8 *const rec = static_cast<u8 *>(record);
    const u32 slot_index = LoadU32From(reinterpret_cast<const void *>(
        0x474C7CU)); // TH10 DAT_00474c7c (selected slot)

    // Mark the three scheduler records (rec+8, rec+0x1cc, rec+0xc).
    if (LoadU32At(rec, 8U) != 0U)
        StoreU32To(reinterpret_cast<void *>(LoadU32At(rec, 8U)), 4U,
                   LoadU32At(reinterpret_cast<void *>(LoadU32At(rec, 8U)),
                             4U) | 2U);
    if (LoadU32At(rec, 0x1CCU) != 0U)
        StoreU32To(reinterpret_cast<void *>(LoadU32At(rec, 0x1CCU)), 4U,
                   LoadU32At(reinterpret_cast<void *>(LoadU32At(rec, 0x1CCU)),
                             4U) | 2U);
    if (LoadU32At(rec, 0xCU) != 0U)
        StoreU32To(reinterpret_cast<void *>(LoadU32At(rec, 0xCU)), 4U,
                   LoadU32At(reinterpret_cast<void *>(LoadU32At(rec, 0xCU)),
                             4U) | 2U);

    const u32 mode = LoadU32At(rec, 0x10U);
    if (mode == 0U) {
        // Build: fill the record selected by the slot index from the run
        // globals and the 0x477834 manager.
        u8 *const slot = reinterpret_cast<u8 *>(
            LoadU32At(rec, 0x1CU + 4U * slot_index));
        FreeGameModeChainEntriesEaxEcxAbi(static_cast<i32>(slot_index), rec);
        StoreU32To(rec, 0x9CU, reinterpret_cast<u32>(
            AllocateGameModeChainEntryEsiStackAbi(static_cast<i32>(slot_index),
                                                  rec)));

        // The stats block is skipped while a replay is loaded
        // (DAT_00491fc4).
        if (LoadU32From(reinterpret_cast<const void *>(0x491FC4U)) == 0U) {
            StoreU32To(slot, 0xCU,
                       LoadU32From(reinterpret_cast<const void *>(0x474C44U)));
            StoreU32To(slot, 0x10U,
                       static_cast<u32>(
                           *reinterpret_cast<const u16 *>(0x474C48U)));
            StoreU32To(slot, 0x14U,
                       LoadU32From(reinterpret_cast<const void *>(0x474C4CU)));
            StoreU32To(slot, 0x18U,
                       LoadU32From(reinterpret_cast<const void *>(0x474C58U)));
            StoreU32To(slot, 0x1CU,
                       LoadU32From(reinterpret_cast<const void *>(0x474C70U)));
            StoreU32To(slot, 0x20U,
                       LoadU32From(reinterpret_cast<const void *>(0x474C98U)));
            StoreU32To(slot, 0x1B4U,
                       LoadU32From(reinterpret_cast<const void *>(0x474C90U)));
        }

        u8 *const manager = reinterpret_cast<u8 *>(LoadU32From(
            reinterpret_cast<const void *>(0x477834U)));
        PlayerRecord &player = *reinterpret_cast<PlayerRecord *>(manager);
        StoreU32To(slot, 0x24U,
                   static_cast<u32>(player.position_x_fixed));
        StoreU32To(slot, 0x28U,
                   static_cast<u32>(player.position_y_fixed));
        memcpy(slot + 0x2C, player.trail_history, 0x108U);
        {
            u8 *src = reinterpret_cast<u8 *>(
                player.options[0].unfocused_position);
            u8 *dst = slot + 0x134U;
            for (u32 i = 0; i != 4U; ++i) {
                for (u32 j = 0; j != 8U; ++j)
                    StoreU32To(dst, kScatterDstOffsets[j],
                               LoadU32At(src, kScatterSrcOffsets[j]));
                src += 0x98U;
                dst += 8U;
            }
        }
        StoreU32To(slot, 0x1B8U,
                   LoadU32From(reinterpret_cast<const void *>(0x474C9CU)));
        StoreU32To(rec, 0x1D0U, slot_index);
        StoreU32To(slot, 0x1BCU, player.focus_flag);
    } else if (mode == 1U) {
        // Commit: write the record back into the 0x477834 manager.
        u8 *const manager = reinterpret_cast<u8 *>(LoadU32From(
            reinterpret_cast<const void *>(0x477834U)));
        PlayerRecord &player = *reinterpret_cast<PlayerRecord *>(manager);
        u8 *const slot = reinterpret_cast<u8 *>(
            LoadU32At(rec, 0xB0U + 0x24U * slot_index));
        StoreU32To(rec, 0x1D0U, slot_index);
        PublishSelectedRunStatsEaxEcxAbi(manager, reinterpret_cast<const u32 *>(slot + 0x24U));
        memcpy(player.trail_history, slot + 0x2C, 0x108U);
        player.focus_flag = LoadU32At(slot, 0x1BCU);
        RebuildPlayerOptionRecords(manager);
        {
            u8 *src = slot + 0x134U;
            u8 *dst = reinterpret_cast<u8 *>(
                player.options[0].unfocused_position);
            const u32 stage =
                LoadU32From(reinterpret_cast<const void *>(0x474C68U));
            const u32 floor =
                LoadU32From(reinterpret_cast<const void *>(0x474C6CU));
            for (u32 i = 0; i != 4U; ++i) {
                for (u32 j = 0; j != 8U; ++j)
                    StoreU32To(dst, kScatterDstOffsets[j],
                               LoadU32At(src, kScatterSrcOffsets[j]));
                if (stage + floor + 2U * stage == 5U) {
                    // The extra-stage variant overwrites the last pair with
                    // the first one.
                    StoreU32To(dst, kScatterDstOffsets[6],
                               LoadU32At(src, kScatterSrcOffsets[0]));
                    StoreU32To(dst, kScatterDstOffsets[7],
                               LoadU32At(src, kScatterSrcOffsets[1]));
                }
                src += 8U;
                dst += 0x98U;
            }
        }
        player.options[0].tier_latch = 0U;
        player.options[1].tier_latch = 0U;
        player.options[2].tier_latch = 0U;
        player.options[3].tier_latch = 0U;
        ResetOptionPositionRecordsEsiAbi(manager);
    }

    StoreU32To(rec, 0x1C8U, 0U);
}

// TH10 0x00417040.
void UpdateInGameScoreDisplayEsiAbi(void *hud_owner)
{
    AsciiHudOwner &hud = *reinterpret_cast<AsciiHudOwner *>(hud_owner);
    const u32 kHundredMillion = 100000000U;

    i32 displayed = static_cast<i32>(hud.displayed_score);
    if (static_cast<i32>(LoadU32From(
            reinterpret_cast<const void *>(0x474C44U))) != displayed) {
        // Advance the displayed score toward DAT_00474c44. The rate is the
        // signed difference / 32, clamped above at 578910 and floored at 1
        // only when exactly zero (negative differences stay negative); the
        // stored rate only grows and is capped so the displayed score never
        // overshoots the target.
        const i32 target =
            static_cast<i32>(LoadU32From(
                reinterpret_cast<const void *>(0x474C44U)));
        const i32 previous = displayed;
        i32 rate = (target - displayed) / 32;
        if (rate >= 578910)
            rate = 578910;
        else if (rate == 0)
            rate = 1;
        if (static_cast<i32>(hud.score_display_rate) < rate)
            hud.score_display_rate = static_cast<u32>(rate);
        if (static_cast<i32>(hud.score_display_rate) >
            target - static_cast<i32>(hud.displayed_score))
            hud.score_display_rate = static_cast<u32>(
                target - static_cast<i32>(hud.displayed_score));
        displayed = static_cast<i32>(hud.displayed_score) +
                    static_cast<i32>(hud.score_display_rate);
        hud.displayed_score = static_cast<u32>(displayed);
        if (displayed >= target)
            hud.score_display_rate = 0U;
        if (displayed >= static_cast<i32>(kHundredMillion) &&
            previous < static_cast<i32>(kHundredMillion))
            hud.pool_b[9].state_word = 4;
    }

    // Publish a new best score.
    if (static_cast<i32>(LoadU32From(
            reinterpret_cast<const void *>(0x474C40U))) < displayed) {
        StoreU32To(reinterpret_cast<void *>(0x474C40U), 0U,
                   static_cast<u32>(displayed));
        StoreU32To(reinterpret_cast<void *>(0x474C94U), 0U,
                   LoadU32From(reinterpret_cast<const void *>(0x474C90U)));
        StoreU32To(reinterpret_cast<void *>(0x474CA0U), 0U,
                   LoadU32From(reinterpret_cast<const void *>(0x474CA0U)) |
                       4U);
        // The native tests bit 2 of the just-updated value here, which is
        // always set, making the 0x448d00 spawn at 0x41711f dead code.
    }

    if (hud.hud_timer.count >= 20) {
        if (static_cast<i32>(LoadU32From(
                reinterpret_cast<const void *>(0x474C40U))) >=
                static_cast<i32>(kHundredMillion) &&
                static_cast<i32>(hud.best_score_mirror) <
                static_cast<i32>(kHundredMillion))
            hud.pool_a[9].state_word = 4;
        hud.best_score_mirror =
            LoadU32From(reinterpret_cast<const void *>(0x474C40U));
    }

    // Redraw the 9+9 score digit VMs (upper row = best score, lower row =
    // displayed score; digits are consumed least-significant first with the
    // entry index digit + 8).
    i32 best = static_cast<i32>(LoadU32From(
        reinterpret_cast<const void *>(0x474C40U)));
    i32 lower = displayed;
    void *const resource = hud.front_anm_work;
    for (u32 i = 1; i != 10U; ++i) {
        VmRecord &upper_vm = hud.pool_a[i];
        VmRecord &lower_vm = hud.pool_b[i];
        if (best != 0) {
            InitializeAsciiAnimationVmEntry(
                &upper_vm,
                static_cast<u32>(best % 10) + 8U, resource);
            best /= 10;
        } else {
            InitializeAsciiAnimationVmEntry(&upper_vm, 8U, resource);
        }
        if (lower != 0) {
            InitializeAsciiAnimationVmEntry(
                &lower_vm,
                static_cast<u32>(lower % 10) + 8U, resource);
            lower /= 10;
        } else {
            InitializeAsciiAnimationVmEntry(&lower_vm, 8U, resource);
        }
        FinalizeTimelineRenderObjectSetup(&upper_vm);
        FinalizeTimelineRenderObjectSetup(&lower_vm);
    }
    // The two auxiliary digit rows show DAT_00474c94 and DAT_00474c90
    // (life/power-of-life counters) through the same digit+8 entry mapping.
    InitializeAsciiAnimationVmEntry(
        &hud.pool_a[0],
        static_cast<u32>(LoadI32FromAddress(0x474C94U)) + 8U, resource);
    InitializeAsciiAnimationVmEntry(
        &hud.pool_b[0],
        static_cast<u32>(LoadI32FromAddress(0x474C90U)) + 8U, resource);
    FinalizeTimelineRenderObjectSetup(&hud.pool_a[0]);
    FinalizeTimelineRenderObjectSetup(&hud.pool_b[0]);

    if ((hud.hud_mode_flags & 0x20U) == 0U) {
        // Score-rank check: the table depends on the difficulty dword
        // (DAT_00474c74 == 4 selects the second table).
        const u32 rank =
            LoadU32From(reinterpret_cast<const void *>(0x474C9CU));
        const u32 *const table =
            LoadU32From(reinterpret_cast<const void *>(0x474C74U)) == 4U
                ? reinterpret_cast<const u32 *>(0x474488U)
                : reinterpret_cast<const u32 *>(0x474474U);
        if (displayed >= static_cast<i32>(table[rank])) {
            AwardExtendedLifeEaxEcxAbi(
                const_cast<void *>(reinterpret_cast<const void *>(
                    0x474C40U)),
                1);
            StoreU32To(reinterpret_cast<void *>(0x474C9CU), 0U, rank + 1U);
        }
    }
}

// TH10 0x00404450.
void InitializeTitleSecondaryStateStackAbi(void *state)
{
    TitleScreenState &title = *reinterpret_cast<TitleScreenState *>(state);

    title.calc_element->flags |= 2U;
    title.draw_element->flags |= 2U;
    title.draw_pass1_element->flags |= 2U;

    const i32 count = static_cast<i32>(
        *reinterpret_cast<const short *>(title.stage_script_buffer));
    u32 running_index = 0U;
    if (count > 0) {
        u8 *const *const stage_table =
            reinterpret_cast<u8 *const *>(title.script_pointer_table);
        for (i32 stage = 0; stage < count; ++stage) {
            u8 *const entry = stage_table[stage];
            entry[3] = 1;
            u8 *record = entry + 0x1CU;
            if (*reinterpret_cast<const short *>(record) >= 0) {
                u32 vm_offset = running_index * 0x3ACU;
                do {
                    InitializePlayerMainVmEsiStackAbi(
                        title.anm_manager_work,
                        title.vm_heap_array + vm_offset,
                        static_cast<i32>(
                            *reinterpret_cast<const short *>(record + 4)));
                    *reinterpret_cast<u16 *>(record + 6) =
                        static_cast<u16>(running_index);
                    record += static_cast<i32>(
                        *reinterpret_cast<const short *>(record + 2));
                    ++running_index;
                    vm_offset += 0x3ACU;
                } while (*reinterpret_cast<const short *>(record) >= 0);
            }
        }
    }
    title.script_cursor =
        reinterpret_cast<u8 *>(title.script_base_2);
}

// TH10 0x00417770.
void ReleaseOwnerRecordChainEaxAbi(void *owner)
{
    u32 node = LoadU32At(owner, 0x18U);
    if (node == 0U)
        return;
    u32 next = 0U;
    do {
        next = LoadU32At(reinterpret_cast<const void *>(node), 8U);
        void **const vtable =
            *reinterpret_cast<void ***>(node);
#if defined(_MSC_VER)
        typedef void (__stdcall *RecordMethodFn)(void *);
#else
        typedef void __attribute__((stdcall)) RecordMethodT(void *);
        typedef RecordMethodT *RecordMethodFn;
#endif
        reinterpret_cast<RecordMethodFn>(vtable[4])(
            reinterpret_cast<void *>(node));
        const u32 previous = LoadU32At(reinterpret_cast<const void *>(node),
                                       4U);
        StoreU32To(reinterpret_cast<void *>(previous), 8U, next);
        if (next != 0U)
            StoreU32To(reinterpret_cast<void *>(next), 4U, previous);
        extern void FreeMainChainObject(void *object); // TH10 0x004524a1
        FreeMainChainObject(reinterpret_cast<void *>(node));
        node = next;
    } while (next != 0U);
}

// TH10 0x00418a00.
void TickTitleFrameStateEaxAbi(void *frame_state)
{
    if (LoadI32At(frame_state, 0x18U) > 0) {
        ShiftTimerByEsiStackAbi(
            static_cast<u8 *>(frame_state) + 0x14U, -1.0f);
        return;
    }
    i32 value = LoadI32At(frame_state, 0xCU);
    if (value > 5000) {
        const i32 decrement = LoadI32At(frame_state, 0x10U);
        value -= decrement;
        StoreI32To(frame_state, 0xCU, value);
        if (decrement < 18)
            StoreI32To(frame_state, 0x10U, 18);
        if (value < 5000)
            StoreI32To(frame_state, 0xCU, 5000);
    }
}

// TH10 0x00409f90.
void ReleaseAsciiHudConditionalState(void *state)
{
    // Release the four entity resource pools hanging off the render owner.
    // Native 0x409f9e is `lea esi,[ebx+3AD090h]`: the four dword handles at
    // owner+0x3ad090..0x3ad09c, i.e. LargeRenderOwnerLayout::work_slots[9]
    // ..work_slots[12] (0x3ad06c + 9*4) — NOT +0x3accb0, which would land
    // inside pooled_node_active.
    u32 *const resource_slots = reinterpret_cast<u32 *>(
        static_cast<u8 *>(g_MainChainRenderOwner) + 0x3AD090U);
    for (u32 i = 0; i != 4U; ++i)
        ReleaseEntitiesUsingResourceEaxEdxAbi(g_MainChainRenderOwner,
                                              resource_slots[i]);

    // Run vtable+0x14 (index 5) with argument 1 on every record of the
    // state+0x58 chain.
    u32 node = LoadU32At(state, 0x58U);
    while (node != 0U) {
        const u32 next =
            LoadU32At(reinterpret_cast<const void *>(node), 4U);
        const u32 object = LoadU32At(reinterpret_cast<const void *>(node), 0U);
        if (object != 0U) {
            void **const vtable = *reinterpret_cast<void ***>(object);
#if defined(_MSC_VER)
            typedef void (__stdcall *ReleaseFn)(void *, i32);
#else
            typedef void __attribute__((stdcall)) ReleaseT(void *, i32);
            typedef ReleaseT *ReleaseFn;
#endif
            reinterpret_cast<ReleaseFn>(vtable[5])(
                reinterpret_cast<void *>(object), 1);
        }
        node = next;
    }

    StoreU32To(state, 0x64U, 0U);
    StoreU32To(state, 0x10U, 0U);
    const u32 first_record = LoadU32At(state, 8U);
    if (first_record != 0U)
        StoreU32To(reinterpret_cast<void *>(first_record), 4U,
                   LoadU32At(reinterpret_cast<const void *>(first_record),
                             4U) & ~2U);
    const u32 second_record = LoadU32At(state, 0xCU);
    if (second_record != 0U)
        StoreU32To(reinterpret_cast<void *>(second_record), 4U,
                   LoadU32At(reinterpret_cast<const void *>(second_record),
                             4U) & ~2U);
}


// --- Game-mode record helpers (0x428e10 / 0x42ab20 / 0x42aa50) ----------

void *PublishSelectedRunStatsEaxEcxAbi(void *state, const u32 *values)
{
    PlayerRecord &player = *reinterpret_cast<PlayerRecord *>(state);
    player.position_x_fixed = static_cast<i32>(values[0]);
    player.position_y_fixed = static_cast<i32>(values[1]);
    const float f0 =
        static_cast<float>(player.position_x_fixed) * 0.01f;
    const float f1 =
        static_cast<float>(player.position_y_fixed) * 0.01f;
    player.position_x = f0;
    player.position_y = f1;
    for (u32 i = 0; i < 4U; ++i) {
        player.options[i].tier_latch = 1U;
    }
    return state;
}

void *FreeGameModeChainEntriesEaxEcxAbi(i32 index, void *owner)
{
    extern void FreeMainChainObject(void *object); // TH10 0x004524a1
    u32 node =
        LoadU32At(owner, 64U + 12U * static_cast<u32>(index));
    while (node != 0U) {
        const u32 next = LoadU32From(reinterpret_cast<const void *>(node + 4U));
        const u32 record = LoadU32From(reinterpret_cast<const void *>(node));
        if (record != 0U) {
            const u32 prev = LoadU32At(reinterpret_cast<const void *>(record), 0x627cU);
            const u32 following = LoadU32At(reinterpret_cast<const void *>(record), 0x6280U);
            if (prev != 0U) {
                StoreU32To(reinterpret_cast<void *>(prev), 8U, following);
            }
            if (following != 0U) {
                StoreU32To(reinterpret_cast<void *>(following), 4U, prev);
            }
            StoreU32To(reinterpret_cast<void *>(record), 0x627cU, 0U);
            StoreU32To(reinterpret_cast<void *>(record), 0x6280U, 0U);
            FreeMainChainObject(reinterpret_cast<void *>(record));
        }
        node = next;
    }
    return reinterpret_cast<void *>(node);
}

void *AllocateGameModeChainEntryEsiStackAbi(i32 index, void *owner)
{
    void *raw = ::operator new(0x6284U);
    u32 record = reinterpret_cast<u32>(raw);
    if (record != 0U) {
        u8 *const base = reinterpret_cast<u8 *>(record);
        for (u32 i = 0; i < 0x6284U; ++i) {
            base[i] = 0U;
        }
        StoreU32To(base, 0x5470U, record);
        StoreU32To(base, 0x6274U, record + 0x5474U);
        StoreU32To(base, 0x6278U, record);
        StoreU32To(base, 0x627cU, 0U);
        StoreU32To(base, 0x6280U, 0U);
    }
    const u32 node = record + 0x6278U;

    // Walk to the tail of the bucket chain: head slot at
    // owner + 4*(3*index+15), first pointer at +4.
    u32 cursor = reinterpret_cast<u32>(owner)
               + 4U * (3U * static_cast<u32>(index) + 15U);
    while (LoadU32At(reinterpret_cast<const void *>(cursor), 4U) != 0U) {
        cursor = LoadU32At(reinterpret_cast<const void *>(cursor), 4U);
    }
    const u32 following = LoadU32At(reinterpret_cast<const void *>(cursor), 4U);
    if (following != 0U) {
        StoreU32To(reinterpret_cast<void *>(node), 4U, following);
        StoreU32To(reinterpret_cast<void *>(following), 8U, node);
    }
    StoreU32To(reinterpret_cast<void *>(cursor), 4U, node);
    StoreU32To(reinterpret_cast<void *>(node), 8U, cursor);
    return reinterpret_cast<void *>(node);
}

} // namespace th10
