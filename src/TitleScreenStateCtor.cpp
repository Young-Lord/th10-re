// TH10 0x00402230 — title-screen state constructor. Registers the three
// scheduler records of the 0x2b64-byte title-screen state:
//
//   record slot   adapter    target
//   state+0x08    0x00403050 0x00402720 (calculation body, `mov eax,ecx; jmp`)
//   state+0x0c    0x00403060 0x00402850 (draw pass 0, `push ecx; call`)
//   state+0x2a40  0x00403070 0x00402ca0 (draw pass 1, `push ecx; call`)
//
// every record created through 0x00449ed0 with its enabled flag (bit 1)
// cleared, the state stored at record+0x20, and registered on the global
// scheduler DAT_00491be4 with the priority taken from the stack argument
// (base+12 / base+7 / base+10 — the native passes `base+N` in EDI to
// 0x00449ae0/0x00449b70). All fixed globals and offsets are from the raw
// disassembly; see docs/evidence/title-screen-state-constructor.md.
#include <string.h>

#include "CallbackScheduler.hpp"
#include "MainChainRender.hpp"
#include "Th10Platform.hpp"
#include "Th10Types.hpp"
#include "TitleBackgroundScript.hpp"
#include "TitleScreenDrawPasses.hpp"
#include "TitleScreenState.hpp"
#include "TitleScreenStateCtor.hpp"

namespace th10 {

namespace {

// ------------------------------------------------------------- globals

extern void *g_TitleScreenStatePrimary;   // TH10 DAT_004776e4
extern void *g_TitleScreenStateSecondary; // TH10 DAT_004776e8
extern u32 g_SceneModeSelector;           // TH10 DAT_00474c7c (stage selector)
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern MainChainCameraWork g_AsciiCameraWork;   // TH10 DAT_00491d7c
extern float g_FrameTimeScale;                  // TH10 DAT_00476f78

// Shift-JIS diagnostic "ステージデータが読み込めません。データが壊れています\r\n"
// at TH10 0x0046cc70.
const char *const kStageDataCorruptText =
    reinterpret_cast<const char *>(0x46CC70U);

// ------------------------------------------------------------ accessors

inline void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

// ------------------------------------------------------- boundaries

// TH10 0x0044b8e0 (native stack = message, EDI = 0x474f70 text context):
// appends the diagnostic to the main-chain message log and raises it.
extern void AppendStageDataLoadError(const char *message);

// TH10 0x00402720 — the title-screen calculation body (native EAX = state;
// not reconstructed yet, kept as a boundary). The 0x00403050 scheduler
// adapter is `mov eax,ecx; jmp 0x00402720`.
extern i32 TH10_FASTCALL TitleScreenCalcBodyEaxAbi(void *state);

// -------------------------------------------------- scheduler adapters

// TH10 0x00403050 (calculation adapter).
i32 TH10_FASTCALL TitleScreenCalcAdapter(void *state)
{
    return TitleScreenCalcBodyEaxAbi(state);
}

// TH10 0x00403060 (draw adapter for pass 0).
i32 TH10_FASTCALL TitleScreenDrawAdapter0(void *state)
{
    return RunTitleScreenDrawPass0StackAbi(state);
}

// TH10 0x00403070 (draw adapter for pass 1).
i32 TH10_FASTCALL TitleScreenDrawAdapter1(void *state)
{
    return RunTitleScreenDrawPass1StackAbi(state);
}

// Creates a disabled scheduler record for `callback`, stores `state` at
// record+0x20 and registers it on the given chain with `priority`.
ChainElem *RegisterStateRecord(ChainCallback callback, void *state,
                               i32 priority, bool calculation_chain)
{
    ChainElem *element = CallbackSchedulerApi::Create(callback);
    element->flags &= ~ChainElemFlag_Enabled;
    element->arg = state;
    if (calculation_chain) {
        (void)CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler,
                                                          element, priority);
    } else {
        (void)CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler,
                                                   element, priority);
    }
    return element;
}

} // namespace

// TH10 0x00402230 (native usercall, `ret 4`; returns 0 or -1).
i32 CreateTitleScreenStateEaxEcxStackAbi(void *state_arg,
                                         const char *stage_data_name,
                                         u32 priority_base)
{
    TitleScreenState &state =
        *reinterpret_cast<TitleScreenState *>(state_arg);

    // Publication selector: base 0 -> secondary slot, nonzero -> primary.
    if (priority_base != 0U) {
        g_TitleScreenStatePrimary = &state;
    } else {
        g_TitleScreenStateSecondary = &state;
    }

    state.scene_mode_copy = g_SceneModeSelector;

    // Stage background script load (native: EBX = state, stack = name via the
    // pushed ECX). Nonzero return is the failure path.
    if (LoadTitleBackgroundScriptEbxStackAbi(&state, stage_data_name) != 0) {
        AppendStageDataLoadError(kStageDataCorruptText);
        return -1;
    }

    // Camera snapshot at +0x2a4c: 0x46 dwords copied from the startup camera
    // block DAT_00491d7c, then the title camera pose is written over it
    // (raw bit-pattern writes; the +0x18/+0x1c/+0x20 vec3 spans the `up`
    // pointer plus the unnamed unknown_001c head, so it stays raw).
    u8 *const snapshot = reinterpret_cast<u8 *>(&state.camera_snapshot);
    memcpy(snapshot, &g_AsciiCameraWork, 0x118U);
    // translation = (0, 0, -600.0)
    StoreU32At(snapshot, 0x00U, 0U);
    StoreU32At(snapshot, 0x04U, 0U);
    StoreU32At(snapshot, 0x08U, 0xC4160000U); // -600.0f
    // eye = (0, 300.0, 600.0)
    StoreU32At(snapshot, 0x0cU, 0U);
    StoreU32At(snapshot, 0x10U, 0x43960000U); // 300.0f
    StoreU32At(snapshot, 0x14U, 0x44160000U); // 600.0f
    // +0x18/+0x1c/+0x20 = (0, 1.0, 0) — the up-pointer slot plus the head of
    // unknown_001c, written as one vec3 in the native.
    StoreU32At(snapshot, 0x18U, 0U);
    StoreU32At(snapshot, 0x1cU, 0x3F800000U); // 1.0f
    StoreU32At(snapshot, 0x20U, 0U);
    // target = (0, 0, 0)
    StoreU32At(snapshot, 0x3cU, 0U);
    StoreU32At(snapshot, 0x40U, 0U);
    StoreU32At(snapshot, 0x44U, 0U);

    // Background script fade scalar (9610000.0f).
    *reinterpret_cast<u32 *>(&state.background_fade) = 0x4B12A310U;

    // The three scheduler records (all created disabled).
    state.calc_element = RegisterStateRecord(
        &TitleScreenCalcAdapter, &state,
        static_cast<i32>(priority_base + 12U), true);
    state.draw_element = RegisterStateRecord(
        &TitleScreenDrawAdapter0, &state,
        static_cast<i32>(priority_base + 7U), false);
    state.draw_pass1_element = RegisterStateRecord(
        &TitleScreenDrawAdapter1, &state,
        static_cast<i32>(priority_base + 10U), false);

    state.intro_counter = 0U;

    // The embedded frame-state timer (native base +0x24) whose rate/flags
    // family lands at state+0x38/0x3c/0x40/0x44/0x48 — the wait_timer
    // TimerNode. The native writes the rate pointer and flag bit only when
    // bit 0 was clear; the NaN-sentinel and zero stores of that branch are
    // immediately overwritten by the unconditional tail (preserved verbatim).
    if ((state.wait_timer.flags & 1U) == 0U) {
        state.wait_timer.count = 0;
        *reinterpret_cast<u32 *>(&state.wait_timer.prev) = 0xFFF0BDC1U; // NaN sentinel
        state.wait_timer.accum = 0;
        state.wait_timer.rate = &g_FrameTimeScale;
        state.wait_timer.flags |= 1U;
    }
    state.wait_timer.count = 0;
    state.wait_timer.accum = 0;
    *reinterpret_cast<u32 *>(&state.wait_timer.prev) = 0xFFFFFFFFU; // -1 / NaN float

    // Fade-in active latch and cleared scratch words.
    state.master_flags |= 1U;
    state.interp_a_gate = 0U;
    state.interp_b_gate_flag = 0U;
    return 0;
}

} // namespace th10
