// TH10 scene-trigger popup ("thank you" phase object) update and submit,
// plus the record frame-timer reset and the aim-and-spread instruction.
//
// The popup object (0x48 bytes, DAT_004776ec) is driven by the scheduler
// records registered in GameContextLifecycle.cpp. The trigger descriptor
// helpers operate on the 0x7f0-byte scene-trigger records.

#include "Th10Types.hpp"
#include "EntityHelpers.hpp"
#include "PlayerTimerHelpers.hpp"
#include "PlayerStageHelpers.hpp"
#include "BgmRuntime.hpp"
#include "SceneTriggerUpdate.hpp"

namespace th10 {

extern void *g_MainChainRenderOwner;   // TH10 ds:0x491c10
extern void *g_StageState;             // TH10 DAT_004776f4
extern void *g_EffectManagerRoot;      // TH10 DAT_004776f0
extern void *g_TransitionRoot;         // TH10 ds:0x492590
extern void *g_EntranceTweenManager;   // TH10 ds:0x47781c

namespace {
const float kXOffset224 = 224.0f;     // TH10 0x470b4c
const float kYOffset16 = 16.0f;       // TH10 0x470b48
const float kDeltaSlow = 0.5f;        // TH10 0x470b0c
const float kDeltaFast = 1.0f;        // TH10 0x470afc
const float kDeltaFastAlt = 1.3f;     // TH10 0x470d34
const float kPiHalf = 1.5707964f;     // TH10 0x3fc90fdb
} // namespace

// TH10 0x408710 boundary: native fpatan (angle = atan2 with the FPU
// operand order dy, dx).
extern float Atan2Boundary(float dy, float dx);

// TH10 0x00405750. Native EDI = popup object. State machine:
//  - state +0x28 == 0: done, return 1.
//  - state != 1: fall through to the +0x14 timer tick and return 1.
//  - state == 1: resolve the +0x2c handle (0x4491c0); when gone, clear
//    +0x2c/+0x28 and return 1. Otherwise pick the position delta from the
//    stage-state phase ([0x4776f4]+0x378c bit 0 and the +0x3788 phase
//    value in [0x5d..0x60] or == 0x6d give 0.5; otherwise DAT_00474c68
//    being zero gives 1.0, else 1.3), subtract it from +0x34, publish the
//    offset position (+0x30 + 224, +0x34 + 16, +0x38) through 0x4492f0,
//    copy the resolved object's +0x40 into +0x3c, run the entrance
//    submitter 0x405ac0, and finally tick the +0x14 timer (0x404ed0).
i32 UpdateSceneTriggerPopupEdiAbi(void *popup) {
    u8 *bytes = static_cast<u8 *>(popup);
    u32 state = *reinterpret_cast<u32 *>(bytes + 0x28);
    if (state == 0) {
        return 1;
    }
    if (state == 1) {
        const i32 handle =
            *reinterpret_cast<i32 *>(bytes + 0x2c);
        void *resolved = ResolveTimelineHandle(g_MainChainRenderOwner,
                                               handle);
        if (resolved == 0) {
            *reinterpret_cast<u32 *>(bytes + 0x2c) = 0;
            *reinterpret_cast<u32 *>(bytes + 0x28) = 0;
            return 1;
        }

        float delta = kDeltaFast;
        if (g_StageState != 0) {
            const u32 phase_flags = *reinterpret_cast<u32 *>(
                static_cast<u8 *>(g_StageState) + 0x378c);
            const i32 phase = *reinterpret_cast<i32 *>(
                static_cast<u8 *>(g_StageState) + 0x3788);
            if ((phase_flags & 1) != 0 &&
                ((phase >= 0x5d && phase <= 0x60) || phase == 0x6d)) {
                delta = kDeltaSlow;
            } else {
                // DAT_00474c68: nonzero selects the faster 1.3 step.
                extern u32 g_PopupStepSelector; // TH10 DAT_00474c68
                delta = (g_PopupStepSelector != 0) ? kDeltaFastAlt
                                                   : kDeltaFast;
            }
        }
        *reinterpret_cast<float *>(bytes + 0x34) -= delta;

        float position[3];
        position[0] =
            *reinterpret_cast<float *>(bytes + 0x30) + kXOffset224;
        position[1] =
            *reinterpret_cast<float *>(bytes + 0x34) + kYOffset16;
        position[2] = *reinterpret_cast<float *>(bytes + 0x38);
        SetEntityPositionDirectEsiAbi(
            g_MainChainRenderOwner, handle, position);

        *reinterpret_cast<u32 *>(bytes + 0x3c) =
            *reinterpret_cast<u32 *>(static_cast<u8 *>(resolved) + 0x40);
        SubmitSceneTriggerPopupEntranceEsiAbi(popup);
    }

    TickTimerForwardEsiAbi(bytes + 0x14);
    return 1;
}

// TH10 0x00405ac0. Native ESI = popup object. When the +0x28 state is
// live, runs the intro-activation scan against the effect manager root
// with the popup's +0x30 position (require-unused selects 0 inside the
// story phase window [0x5d..0x60]/0x6d, else 1), then broadcasts the
// entrance tween for the same position with the radius at +0x3c.
i32 SubmitSceneTriggerPopupEntranceEsiAbi(void *popup) {
    u8 *bytes = static_cast<u8 *>(popup);
    if (*reinterpret_cast<u32 *>(bytes + 0x28) == 0) {
        return 0;
    }

    u32 phase_flags = 0;
    i32 phase = 0;
    if (g_StageState != 0) {
        phase_flags = *reinterpret_cast<u32 *>(
            static_cast<u8 *>(g_StageState) + 0x378c);
        phase = *reinterpret_cast<i32 *>(
            static_cast<u8 *>(g_StageState) + 0x3788);
    }
    const bool in_story_phase =
        (phase_flags & 1) != 0 &&
        ((phase >= 0x5d && phase <= 0x60) || phase == 0x6d);

    const float position[3] = {
        *reinterpret_cast<float *>(bytes + 0x30),
        *reinterpret_cast<float *>(bytes + 0x34),
        *reinterpret_cast<float *>(bytes + 0x38)};
    const float radius = *reinterpret_cast<float *>(bytes + 0x3c);
    const i32 spawn_fx = (~(phase_flags)) & 1;

    ScanIntroActivations(g_EffectManagerRoot, position, radius, spawn_fx,
                         in_story_phase ? 0 : 1);
    BroadcastEntranceTweenEaxEbxStackAbi(g_EntranceTweenManager, position,
                                         radius, spawn_fx);
    return 0;
}

// TH10 0x00405be0. Native EAX = trigger record. Zeroes the +0x446 state
// word, lazily initializes the two timer records at +0x3f8 and +0x40c
// (sentinel int 0xfff0bdc1 = -999999, zero counters, default rate
// pointer 0x476f78, flag bit 0), then hard-resets both timer ints to -1
// and their counters to zero.
void ResetSceneTriggerFrameTimersEaxAbi(void *record) {
    u8 *bytes = static_cast<u8 *>(record);
    *reinterpret_cast<u16 *>(bytes + 0x446) = 0;

    if ((*reinterpret_cast<u32 *>(bytes + 0x408) & 1) == 0) {
        *reinterpret_cast<u32 *>(bytes + 0x3fc) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x3f8) = 0xfff0bdc1U;
        *reinterpret_cast<u32 *>(bytes + 0x400) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x404) = 0x476f78U;
        *reinterpret_cast<u32 *>(bytes + 0x408) |= 1;
    }
    *reinterpret_cast<u32 *>(bytes + 0x3fc) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x400) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x3f8) = 0xffffffffU;

    if ((*reinterpret_cast<u32 *>(bytes + 0x41c) & 1) == 0) {
        *reinterpret_cast<u32 *>(bytes + 0x410) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x40c) = 0xfff0bdc1U;
        *reinterpret_cast<u32 *>(bytes + 0x414) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x418) = 0x476f78U;
        *reinterpret_cast<u32 *>(bytes + 0x41c) |= 1;
    }
    *reinterpret_cast<u32 *>(bytes + 0x410) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x414) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x40c) = 0xffffffffU;
}

// TH10 0x004073e0. Native EAX = the trigger descriptor record, ECX =
// spawn float argument, stack = the manager work (`ret 4`). Computes the
// aim angle toward the screen target block (DAT_00477834 +0x3c0/+0x3c4
// minus descriptor +0x4/+0x8; pi/2 when the offset is exactly zero),
// then scans the row/column grid ([+0x1f6] rows x [+0x1f4] columns)
// spawning through 0x4067d0 until a spawn reports success. When the
// +0x1fc flags carry bit 0x200, enqueues the descriptor's +0x200 sound
// index with the +0x4 x position through the BGM runtime. Returns 0.
i32 SceneTriggerAimAndSpreadInstructionEaxEcxEcxStackAbi(
    void *descriptor, float spawn_argument, void *manager_work) {
    u8 *bytes = static_cast<u8 *>(descriptor);
    const u32 player_block =
        *reinterpret_cast<u32 *>(0x477834U); // DAT_00477834
    float angle = kPiHalf;
    if (player_block != 0) {
        const float dx =
            *reinterpret_cast<float *>(player_block + 0x3c0) -
            *reinterpret_cast<float *>(bytes + 0x4);
        const float dy =
            *reinterpret_cast<float *>(player_block + 0x3c4) -
            *reinterpret_cast<float *>(bytes + 0x8);
        // Native guards both components against +0.0 exactly and falls
        // back to pi/2; otherwise atan2(dy, dx) (fpatan).
        if (!(dx == 0.0f && dy == 0.0f)) {
            angle = Atan2Boundary(dy, dx);
        }
    }

    const i32 rows = *reinterpret_cast<i32 *>(bytes + 0x1f6);
    const i32 columns = *reinterpret_cast<i32 *>(bytes + 0x1f4);
    for (i32 row = 0; row < rows; ++row) {
        for (i32 column = 0; column < columns; ++column) {
            if (SpawnSceneTriggerFromDescriptorEbxStackAbi(
                    manager_work, descriptor, column, row,
                    spawn_argument) != 0) {
                row = rows; // native breaks out of both loops
                break;
            }
        }
    }

    if ((*reinterpret_cast<u32 *>(bytes + 0x1fc) & 0x200) != 0) {
        const float value = *reinterpret_cast<float *>(bytes + 0x4);
        const u32 sound_index = *reinterpret_cast<u32 *>(bytes + 0x200);
        EnqueueBgmSoundValueFromFloat(
            static_cast<TransitionRootPartial *>(g_TransitionRoot),
            sound_index, value);
    }
    return 0;
}

} // namespace th10
