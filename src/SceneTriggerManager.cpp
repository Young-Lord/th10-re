// TH10 effect-manager-root trigger-record lifecycle and the six trigger
// group bind/tick callbacks.
//
// The 0x3e0b54-byte effect manager root (DAT_004776f0) owns 2001
// 0x7f0-byte trigger records at +0x60 (the SceneTriggerObject pool).
// This module reconstructs the record scalar ctor (0x405d00) and dtor
// (0x405de0), the root-side loader/teardown pair (0x405e20 / 0x405ed0),
// the group bind pass (0x4065c0), the per-group tick (0x4066e0) and the
// two scheduler callbacks (0x406770 calc, 0x4067a0 draw).

#include "Th10Types.hpp"
#include "TitleScreenObject.hpp"
#include "CallbackScheduler.hpp"
#include "EffectManagerRoot.hpp"
#include "ManagerWork.hpp"
#include "EntityHelpers.hpp"
#include "AsciiRenderModeDispatcher.hpp"
#include "SceneTriggerUpdate.hpp"
#include "SceneTriggerFeatures.hpp"

namespace th10 {

extern CallbackScheduler *g_CallbackScheduler; // TH10 ds:0x491be4
extern void *g_EffectManagerRoot;              // TH10 DAT_004776f0
extern void *g_TitleScreen;                    // TH10 DAT_00477810
extern void *g_MainChainRenderOwner;           // TH10 ds:0x491c10

// Boundaries implemented outside the semantic layer.
extern void AppendMainChainErrorText(const char *text); // TH10 0x44b810
extern i32 FloatToIntBoundary(float value);             // TH10 0x463b2c
extern void CrtFreeBoundary(void *memory);              // TH10 0x452422

// Scheduler callback adapters (defined at the bottom of this file).
i32 TH10_FASTCALL EffectTriggerGroupCalcCallbackThunk(void *root);
i32 TH10_FASTCALL EffectTriggerGroupDrawCallbackThunk(void *root);

namespace {
const u32 kGroupCount = 6;
const u32 kBindSlotCount = 0x7d0;      // bind pass slot budget
const float kXOffset224 = 224.0f;      // TH10 0x470b4c
const float kYOffset16 = 16.0f;        // TH10 0x470b48
const float kAngleStepPiHalf = 1.5707964f; // pi/2 (0x3fc90fdb), pushed as the
                                           //   angle step at TH10 0x406732
const float kFrameWindowLow = 0.99f;   // TH10 0x470b68 (0x3f7d70a4)
const float kFrameWindowHigh = 1.01f;  // TH10 0x470b64
} // namespace

// TH10 0x00405d00. Native __thiscall ECX = one 0x7f0-byte trigger record
// (the eh vector scalar ctor). Clears bit 0 (&= ~1, 0x405d11) of the
// record's +0x74 timer flag, then bit 0 of the eight embedded-animation
// flag dwords at +0x8+{0xb0..0x378} (0x405d15..0x405d49), wipes the
// 0x3ac-byte embedded region at +0x8 (0x405d56), stores the 0xffff sprite
// sentinel at +0x38c, clears bit 0 of the +0x408/+0x41c timer-init flags
// (0x405d61/0x405d67), and clears the ten 0x34-stride flag dwords starting
// at +0x624. Returns the record.
void *InitSceneTriggerRecordEcxAbi(void *record) {
    u8 *bytes = static_cast<u8 *>(record);
    EffectTriggerRecord &rec = *static_cast<EffectTriggerRecord *>(record);

    // Native clears bit 0 with &= ~1 at every site (TH10 0x405d00 region).
    rec.vm.timer_flags &= ~1u;         // +0x74 (vm+0x6c)
    rec.vm.position_anim.flags &= ~1u; // +0xb0 (vm+0xb0)
    rec.vm.rgb_anim_1.flags &= ~1u;    // +0xfc (vm+0xfc)
    rec.vm.alpha_anim_1.flags &= ~1u;  // +0x128 (vm+0x128)
    rec.vm.rotation_anim.flags &= ~1u; // +0x174 (vm+0x174)
    rec.vm.scale_anim.flags &= ~1u;    // +0x1b0 (vm+0x1b0)
    rec.vm.rgb_anim_2.flags &= ~1u;    // +0x1fc (vm+0x1fc)
    rec.vm.alpha_anim_2.flags &= ~1u;  // +0x228 (vm+0x228)
    rec.vm.saved_timer_flags &= ~1u;   // +0x380 (vm+0x378)
    u32 *wipe = reinterpret_cast<u32 *>(&rec.vm);
    for (int i = 0; i < 0xeb; ++i) {
        wipe[i] = 0;
    }
    rec.vm.sprite_entry_id = 0xffffU;
    rec.flags_0408 &= ~1u;
    rec.flags_041c &= ~1u;

    // Ten 0x34-stride flag dwords at +0x624 (the native walks them with
    // pointer bumps of 0x34 while storing the ANDed flag; region kept raw).
    u32 *cursor = reinterpret_cast<u32 *>(bytes + 0x624);
    for (int i = 0; i < 10; ++i) {
        *cursor &= ~1u;
        cursor += 0x34 / 4;
    }
    return record;
}

// TH10 0x00405de0. Native __thiscall ECX = trigger record (the eh vector
// scalar dtor): releases the record's +0x360 heap buffer (vm.vertex_buffer)
// and clears the pointer. Other fields are left alone.
void DestroySceneTriggerRecordInPlaceEcxAbi(void *record) {
    EffectTriggerRecord &rec = *static_cast<EffectTriggerRecord *>(record);
    if (rec.vm.vertex_buffer != 0) {
        CrtFreeBoundary(rec.vm.vertex_buffer);
    }
    rec.vm.vertex_buffer = 0;
}

// TH10 0x00405e20. Native EBX = the effect manager root. Requests the
// manager-work resource (kind 7) for the trigger section, records the
// work at root+0x3e0b50, seeds the section state word at +0x3e07a6 to 5,
// points root+0x10 at the record pool (+0x60), and registers the two
// scheduler callbacks with the root as their argument: calc 0x406770 at
// priority 0x14 (element disabled: flags &= ~2) and draw 0x4067a0 at
// priority 0x1d (also disabled). Elements land at root+8 / root+0xc.
// Returns 0, or -1 after the 0x474f70 diagnostic when the resource
// request fails.
i32 LoadEffectManagerTriggerSectionEbxAbi(void *root) {
    EffectManagerRoot &mgr = *static_cast<EffectManagerRoot *>(root);
    ManagerWorkPartial *work = RequestManagerWork(
        reinterpret_cast<ManagerWorkOwnerPartial *>(g_MainChainRenderOwner),
        7, reinterpret_cast<const char *>(0x46cd88));
    mgr.bullet_resource_3e0b50 = work;
    if (work == 0) {
        AppendMainChainErrorText(reinterpret_cast<const char *>(0x46cd54));
        return -1;
    }

    // u16-5 seed into records[2000].kind_0446 (root+0x3e07a6) — kept raw
    // (aliasing quirk).
    *reinterpret_cast<u16 *>(
        reinterpret_cast<u8 *>(root) + 0x3e07a6) = 5;
    mgr.pool_self_0010 = mgr.records;

    ChainElem *calc = CallbackSchedulerApi::Create(
        EffectTriggerGroupCalcCallbackThunk);
    calc->flags &= ~ChainElemFlag_Enabled;
    calc->arg = root;
    CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler, calc,
                                                0x14);
    mgr.calc_element = calc;

    ChainElem *draw = CallbackSchedulerApi::Create(
        EffectTriggerGroupDrawCallbackThunk);
    draw->flags &= ~ChainElemFlag_Enabled;
    draw->arg = root;
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw, 0x1d);
    mgr.draw_element = draw;
    return 0;
}

// TH10 0x00405ed0. Native ESI = the effect manager root. Releases the
// trigger-section manager work through the resource-matched entity
// release (0x4493e0), wipes the 0xf82bc-dword record pool region at
// +0x60, re-points root+0x10 at the pool and re-seeds the +0x3e07a6 word
// to 5. No return value.
void TeardownEffectManagerTriggerSectionEsiAbi(void *root) {
    EffectManagerRoot &mgr = *static_cast<EffectManagerRoot *>(root);
    void *work = mgr.bullet_resource_3e0b50;
    ReleaseEntitiesUsingResourceEaxEdxAbi(
        g_MainChainRenderOwner, reinterpret_cast<u32>(work));

    // Bulk wipe of the whole record-pool region +0x60..+0x3e0b50 — kept raw.
    u32 *wipe = reinterpret_cast<u32 *>(
        reinterpret_cast<u8 *>(root) + 0x60);
    for (int i = 0; i < 0xf82bc; ++i) {
        wipe[i] = 0;
    }
    mgr.pool_self_0010 = mgr.records;
    // u16-5 seed into records[2000].kind_0446 (root+0x3e07a6) — kept raw
    // (aliasing quirk).
    *reinterpret_cast<u16 *>(
        reinterpret_cast<u8 *>(root) + 0x3e07a6) = 5;
}

namespace {

// Advance the record's frame accumulator block: +0x3f8 mirrors the int
// at +0x3fc; when the rate float behind the +0x404 pointer sits inside
// the (0.99, 1.01) window the int increments and the accumulator float at
// +0x400 gains 1.0; otherwise the float absorbs the rate and the int is
// re-truncated from it (native ftol). The +0x404 field is a POINTER to the
// rate float and is dereferenced (bind pass TH10 0x406652/0x406674).
void AdvanceRecordFrameAccumulator(u8 *record) {
    EffectTriggerRecord &rec = *reinterpret_cast<EffectTriggerRecord *>(
        record);
    *reinterpret_cast<u32 *>(&rec.frame_mirror_03f8) = rec.frame_count_03fc;
    const float rate = *rec.rate_ptr_0404;
    if (rate > kFrameWindowLow && rate < kFrameWindowHigh) {
        ++rec.frame_count_03fc;
        rec.frame_accum_0400 += 1.0f;
    } else {
        rec.frame_accum_0400 += rate;
        rec.frame_count_03fc = static_cast<u32>(
            FloatToIntBoundary(rec.frame_accum_0400));
    }
}

} // namespace

// TH10 0x004065c0. Native ECX = the effect manager root (scheduler
// callback body). Clears the root's +0x5c bound count, the six group
// heads (+0x14..+0x28) and tails (+0x2c..+0x40), then walks the first
// 0x7d0 trigger records (base root+0x60, stride 0x7f0): records with a
// zero +0x446 state word are skipped; when DAT_00477810 is absent or its
// +0x58 mode flags lack both bit 1 (0x2) and bit 10 (0x400), the record
// update (0x406240) runs and a nonzero result skips binding. Live
// records are appended to group [record+0x460] (head once, then tail),
// the record's +0x44c group-next (group_next_044c) is cleared (TH10
// 0x406649) and the bound count incremented; then the record's frame
// accumulator advances. Returns 1.
i32 BindEffectTriggerGroupsEcxEcxAbi(void *root) {
    EffectManagerRoot &mgr = *static_cast<EffectManagerRoot *>(root);
    mgr.bound_record_count_005c = 0;
    for (u32 i = 0; i < kGroupCount; ++i) {
        mgr.trigger_group_heads[i] = 0;
        mgr.trigger_group_tails[i] = 0;
    }

    for (u32 slot = 0; slot < kBindSlotCount; ++slot) {
        EffectTriggerRecord &rec = mgr.records[slot];
        if (rec.kind_0446 == 0U) {
            continue;
        }
        bool skip_bind = false;
        const TitleScreen *const ts =
            static_cast<const TitleScreen *>(g_TitleScreen);
        const bool running = (ts != 0) &&
            (((ts->flags & 2U) != 0U) && ((ts->flags & 0x400U) != 0U));
        if (!running) {
            if (UpdateSceneTriggerObjectStackAbi(&rec) != 0) {
                skip_bind = true;
            }
        }
        if (!skip_bind) {
            const u32 group = rec.group_index_0460;
            void **head = &mgr.trigger_group_heads[group];
            void **tail = &mgr.trigger_group_tails[group];
            if (*head == 0) {
                *head = &rec;
            } else {
                reinterpret_cast<EffectTriggerRecord *>(*tail)
                    ->group_next_044c = reinterpret_cast<u32>(&rec);
            }
            *tail = &rec;
            rec.group_next_044c = 0;
            ++mgr.bound_record_count_005c;
        }

        AdvanceRecordFrameAccumulator(reinterpret_cast<u8 *>(&rec));
    }
    return 1;
}

// TH10 0x004066e0. Native EAX = root, ECX = group index (0..5). Walks
// the group's linked records (next at +0x44c): for each, adds the +0x3b4
// position deltas into +0x33c/+0x340/+0x344 and, when the +0x364 flag
// word has bit 0x8000000 set, wraps the +0x34 angle by pi/2 (0x3fc90fdb,
// pushed at TH10 0x406732) and raises flag bit 0x4. Then dispatches the
// record's animation VM render mode (0x4451c0) against the main render
// owner. Returns 1.
i32 TickEffectTriggerGroupEcxEaxAbi(void *root, u32 group) {
    EffectManagerRoot &mgr = *static_cast<EffectManagerRoot *>(root);
    EffectTriggerRecord *record = static_cast<EffectTriggerRecord *>(
        mgr.trigger_group_heads[group]);
    while (record != 0) {
        EffectTriggerRecord &rec = *record;
        rec.vm.base_pos_x = rec.position_x_03b4 + kXOffset224;
        rec.vm.base_pos_y = rec.position_y_03b8 + kYOffset16;
        // z is a dword copy in the native (TH10 0x406714).
        *reinterpret_cast<u32 *>(&rec.vm.base_pos_z) =
            *reinterpret_cast<u32 *>(&rec.position_z_03bc);

        if ((rec.vm.flags & 0x8000000U) != 0) {
            rec.vm.rotation_z = WrapAngleSumStackAbi(rec.raw_angle_03e4,
                                                     kAngleStepPiHalf);
            rec.vm.flags |= 0x4u;
        }

        DispatchAsciiAnimationVmRenderMode(&rec.vm,
                                           g_MainChainRenderOwner);

        record = reinterpret_cast<EffectTriggerRecord *>(
            rec.group_next_044c);
    }
    return 1;
}

// TH10 0x00406770. Native ECX = root (calc callback). When DAT_00477810
// is live and its +0x58 mode flags satisfy ((flags >> 2) | flags) & 1,
// returns 1 without touching the groups; otherwise runs the bind pass.
i32 EffectTriggerGroupCalcCallbackEcxEcxAbi(void *root) {
    if (g_TitleScreen != 0) {
        const TitleScreen &ts =
            *reinterpret_cast<const TitleScreen *>(g_TitleScreen);
        const u32 mode_flags = ts.flags;
        if ((((mode_flags >> 2) | mode_flags) & 1) != 0) {
            return 1;
        }
    }
    BindEffectTriggerGroupsEcxEcxAbi(root);
    return 1;
}

// TH10 0x004067a0. Native ECX = root (draw callback). Skips the group
// ticks only when DAT_00477810 is live and its +0x58 flags have bit 2
// (0x4) set; otherwise ticks all six groups. Always returns 1.
i32 EffectTriggerGroupDrawCallbackEcxEcxAbi(void *root) {
    if (g_TitleScreen != 0) {
        const TitleScreen &ts =
            *reinterpret_cast<const TitleScreen *>(g_TitleScreen);
        if ((ts.flags & 0x4U) != 0) {
            return 1;
        }
    }
    for (u32 group = 0; group < kGroupCount; ++group) {
        TickEffectTriggerGroupEcxEaxAbi(root, group);
    }
    return 1;
}

// Scheduler-callback thunks (native 0x406770/0x4067a0 receive the root
// in ECX directly; the ABI bridge calls the semantic bodies).
i32 TH10_FASTCALL EffectTriggerGroupCalcCallbackThunk(void *root) {
    return EffectTriggerGroupCalcCallbackEcxEcxAbi(root);
}
i32 TH10_FASTCALL EffectTriggerGroupDrawCallbackThunk(void *root) {
    return EffectTriggerGroupDrawCallbackEcxEcxAbi(root);
}

} // namespace th10
