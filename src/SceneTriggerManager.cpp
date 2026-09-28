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
#include "CallbackScheduler.hpp"
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
const u32 kRecordStride = 0x7f0;
const float kXOffset224 = 224.0f;      // TH10 0x470b4c
const float kYOffset16 = 16.0f;        // TH10 0x470b48
const float kAngleStep1 = 1.0f;        // TH10 0x470afc
const float kFrameWindowLow = 0.98f;   // TH10 0x470b68
const float kFrameWindowHigh = 1.01f;  // TH10 0x470b64
} // namespace

// TH10 0x00405d00. Native __thiscall ECX = one 0x7f0-byte trigger record
// (the eh vector scalar ctor). Clears bit 1 of the record's +0x74 flag,
// then of the nine embedded-record flag dwords at +0x8+{0x6c..0x378},
// wipes the 0x3ac-byte embedded region at +0x8, stores the 0xffff sprite
// sentinel at +0x38c, clears the +0x408/+0x41c timer-init flags, and
// clears the ten 0x34-stride flag dwords starting at +0x624. Returns the
// record.
void *InitSceneTriggerRecordEcxAbi(void *record) {
    u8 *bytes = static_cast<u8 *>(record);
    *reinterpret_cast<u32 *>(bytes + 0x74) &= ~2u;

    static const u32 kEmbeddedFlagOffsets[9] = {
        0x6c, 0xb0, 0xfc, 0x128, 0x174, 0x1b0, 0x1fc, 0x228, 0x378};
    for (int i = 0; i < 9; ++i) {
        u32 *flag = reinterpret_cast<u32 *>(
            bytes + 0x8 + kEmbeddedFlagOffsets[i]);
        *flag &= ~2u;
    }
    u32 *wipe = reinterpret_cast<u32 *>(bytes + 0x8);
    for (int i = 0; i < 0xeb; ++i) {
        wipe[i] = 0;
    }
    *reinterpret_cast<u16 *>(bytes + 0x38c) = 0xffff;
    *reinterpret_cast<u32 *>(bytes + 0x408) &= ~2u;
    *reinterpret_cast<u32 *>(bytes + 0x41c) &= ~2u;

    // Ten 0x34-stride flag dwords at +0x624 (the native walks them with
    // pointer bumps of 0x34 while storing the ANDed flag 0x34 back).
    u32 *cursor = reinterpret_cast<u32 *>(bytes + 0x624);
    for (int i = 0; i < 10; ++i) {
        *cursor &= ~2u;
        cursor += 0x34 / 4;
    }
    return record;
}

// TH10 0x00405de0. Native __thiscall ECX = trigger record (the eh vector
// scalar dtor): releases the record's +0x360 heap buffer and clears the
// pointer. Other fields are left alone.
void DestroySceneTriggerRecordInPlaceEcxAbi(void *record) {
    u8 *bytes = static_cast<u8 *>(record);
    void *buffer = *reinterpret_cast<void **>(bytes + 0x360);
    if (buffer != 0) {
        CrtFreeBoundary(buffer);
    }
    *reinterpret_cast<void **>(bytes + 0x360) = 0;
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
    u8 *bytes = static_cast<u8 *>(root);
    ManagerWorkPartial *work = RequestManagerWork(
        reinterpret_cast<ManagerWorkOwnerPartial *>(g_MainChainRenderOwner),
        7, reinterpret_cast<const char *>(0x46cd88));
    *reinterpret_cast<void **>(bytes + 0x3e0b50) = work;
    if (work == 0) {
        AppendMainChainErrorText(reinterpret_cast<const char *>(0x46cd54));
        return -1;
    }

    *reinterpret_cast<u16 *>(bytes + 0x3e07a6) = 5;
    *reinterpret_cast<void **>(bytes + 0x10) = bytes + 0x60;

    ChainElem *calc = CallbackSchedulerApi::Create(
        EffectTriggerGroupCalcCallbackThunk);
    calc->flags &= ~ChainElemFlag_Enabled;
    calc->arg = root;
    CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler, calc,
                                                0x14);
    *reinterpret_cast<ChainElem **>(bytes + 0x8) = calc;

    ChainElem *draw = CallbackSchedulerApi::Create(
        EffectTriggerGroupDrawCallbackThunk);
    draw->flags &= ~ChainElemFlag_Enabled;
    draw->arg = root;
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw, 0x1d);
    *reinterpret_cast<ChainElem **>(bytes + 0xc) = draw;
    return 0;
}

// TH10 0x00405ed0. Native ESI = the effect manager root. Releases the
// trigger-section manager work through the resource-matched entity
// release (0x4493e0), wipes the 0xf82bc-dword record pool region at
// +0x60, re-points root+0x10 at the pool and re-seeds the +0x3e07a6 word
// to 5. No return value.
void TeardownEffectManagerTriggerSectionEsiAbi(void *root) {
    u8 *bytes = static_cast<u8 *>(root);
    void *work = *reinterpret_cast<void **>(bytes + 0x3e0b50);
    ReleaseEntitiesUsingResourceEaxEdxAbi(
        g_MainChainRenderOwner, reinterpret_cast<u32>(work));

    u32 *wipe = reinterpret_cast<u32 *>(bytes + 0x60);
    for (int i = 0; i < 0xf82bc; ++i) {
        wipe[i] = 0;
    }
    *reinterpret_cast<void **>(bytes + 0x10) = bytes + 0x60;
    *reinterpret_cast<u16 *>(bytes + 0x3e07a6) = 5;
}

namespace {

// Advance the record's frame accumulator block: +0x3f8 mirrors the int
// at +0x3fc; when the rate float at +0x404 sits inside the (0.98, 1.01)
// window the int increments and the accumulator float at +0x400 gains
// 1.0; otherwise the float absorbs the rate and the int is re-truncated
// from it (native ftol).
void AdvanceRecordFrameAccumulator(u8 *record) {
    *reinterpret_cast<u32 *>(record + 0x3f8) =
        *reinterpret_cast<u32 *>(record + 0x3fc);
    i32 &frame_int = *reinterpret_cast<i32 *>(record + 0x3fc);
    float &accumulator = *reinterpret_cast<float *>(record + 0x400);
    const float rate = *reinterpret_cast<float *>(record + 0x404);
    if (rate > kFrameWindowLow && rate < kFrameWindowHigh) {
        ++frame_int;
        accumulator += 1.0f;
    } else {
        accumulator += rate;
        frame_int = FloatToIntBoundary(accumulator);
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
// the record's +0x4c is cleared and the bound count incremented; then
// the record's frame accumulator advances. Returns 1.
i32 BindEffectTriggerGroupsEcxEcxAbi(void *root) {
    u8 *bytes = static_cast<u8 *>(root);
    *reinterpret_cast<u32 *>(bytes + 0x5c) = 0;
    for (u32 i = 0; i < kGroupCount; ++i) {
        *reinterpret_cast<void **>(bytes + 0x14 + i * 4) = 0;
        *reinterpret_cast<void **>(bytes + 0x2c + i * 4) = 0;
    }

    u8 *record = bytes + 0x60;
    for (u32 slot = 0; slot < kBindSlotCount;
         ++slot, record += kRecordStride) {
        if (*reinterpret_cast<u16 *>(record + 0x446) == 0) {
            continue;
        }
        bool skip_bind = false;
        const bool running = (g_TitleScreen != 0) &&
            (((*reinterpret_cast<u32 *>(
                   static_cast<u8 *>(g_TitleScreen) + 0x58) & 2) != 0) &&
             ((*reinterpret_cast<u32 *>(
                   static_cast<u8 *>(g_TitleScreen) + 0x58) & 0x400) != 0));
        if (!running) {
            if (UpdateSceneTriggerObjectStackAbi(record) != 0) {
                skip_bind = true;
            }
        }
        if (!skip_bind) {
            const u32 group = *reinterpret_cast<u32 *>(record + 0x460);
            void **head =
                reinterpret_cast<void **>(bytes + 0x14 + group * 4);
            void **tail =
                reinterpret_cast<void **>(bytes + 0x2c + group * 4);
            if (*head == 0) {
                *head = record;
            } else {
                *reinterpret_cast<void **>(
                    static_cast<u8 *>(*tail) + 0x44c) = record;
            }
            *tail = record;
            *reinterpret_cast<u32 *>(record + 0x4c) = 0;
            ++*reinterpret_cast<u32 *>(bytes + 0x5c);
        }

        AdvanceRecordFrameAccumulator(record);
    }
    return 1;
}

// TH10 0x004066e0. Native EAX = root, ECX = group index (0..5). Walks
// the group's linked records (next at +0x44c): for each, adds the +0x3b4
// position deltas into +0x33c/+0x340/+0x344 and, when the +0x364 flag
// word has bit 0x8000000 set, wraps the +0x34 angle by 1.0 and raises
// flag bit 0x4. Then dispatches the record's animation VM render mode
// (0x4451c0) against the main render owner. Returns 1.
i32 TickEffectTriggerGroupEcxEaxAbi(void *root, u32 group) {
    u8 *record = *reinterpret_cast<u8 **>(
        static_cast<u8 *>(root) + 0x14 + group * 4);
    while (record != 0) {
        *reinterpret_cast<float *>(record + 0x33c) =
            *reinterpret_cast<float *>(record + 0x3b4) + kXOffset224;
        *reinterpret_cast<float *>(record + 0x340) =
            *reinterpret_cast<float *>(record + 0x3b8) + kYOffset16;
        *reinterpret_cast<u32 *>(record + 0x344) =
            *reinterpret_cast<u32 *>(record + 0x3bc);

        if ((*reinterpret_cast<u32 *>(record + 0x364) & 0x8000000U) != 0) {
            const float raw = *reinterpret_cast<float *>(record + 0x3e4);
            *reinterpret_cast<float *>(record + 0x34) =
                WrapAngleSumStackAbi(raw, kAngleStep1);
            *reinterpret_cast<u32 *>(record + 0x364) |= 0x4u;
        }

        DispatchAsciiAnimationVmRenderMode(record + 0x8,
                                           g_MainChainRenderOwner);

        record = *reinterpret_cast<u8 **>(record + 0x44c);
    }
    return 1;
}

// TH10 0x00406770. Native ECX = root (calc callback). When DAT_00477810
// is live and its +0x58 mode flags satisfy ((flags >> 2) | flags) & 1,
// returns 1 without touching the groups; otherwise runs the bind pass.
i32 EffectTriggerGroupCalcCallbackEcxEcxAbi(void *root) {
    if (g_TitleScreen != 0) {
        const u32 mode_flags = *reinterpret_cast<u32 *>(
            static_cast<u8 *>(g_TitleScreen) + 0x58);
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
        if ((*reinterpret_cast<u32 *>(
                 static_cast<u8 *>(g_TitleScreen) + 0x58) & 0x4) != 0) {
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
