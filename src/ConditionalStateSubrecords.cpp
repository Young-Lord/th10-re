// Semantic reconstruction of TH10 0x0040d280 with its two scheduler
// callbacks 0x0040d810 (which forwards to the tick body 0x0040d750) and
// 0x0040d820.
//
// The conditional state object (0x68 bytes, published at DAT_00477704 by
// 0x0040d6b0) owns:
//   +0x08  ChainElem*  frame ticker on the calculation chain (priority 0x12)
//   +0x0c  ChainElem*  draw-chain no-op (priority 0x14)
//   +0x30  void*       effect-manager pool word (DAT_004776f0 + 0x3e0b50)
//   +0x40  i32         score copy of the frame counter (seeded -999999, then -1)
//   +0x44  i32         frame counter
//   +0x48  float       rate accumulator
//   +0x4c  const float*rate pointer (seeded &flt_476F78 == 1.0f)
//   +0x50  u32         block-init flag (bit 0)
//   +0x54  void*       the 0x1098-byte script viewer (vtable 0x46d0b4)
//   +0x58  node*       ECL script object list head (nodes live inside the
//   +0x5c  node*       records at +0x116c: {record, next, prev})
//   +0x60  u32         record count
//   +0x10  u32[]       per-record published-id slots (record+0x248c indexes)

#include "ConditionalStateSubrecords.hpp"

#include "CallbackScheduler.hpp"
#include "ConditionalStateObject.hpp"
#include "EclScriptLibrary.hpp"
#include "EclScriptObject.hpp"

namespace th10 {

namespace {

extern void *AllocateChainMemory(u32 bytes);   // TH10 0x00452493 operator new
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern void *g_EffectManagerRoot;              // TH10 DAT_004776f0
extern i32 ConvertFloatToI32TowardZeroX87(float value);
extern const float g_AsciiOverlayInitialRate;  // TH10 flt_476F78 (1.0f)

void StoreU32At(void *address, u32 value)
{
    *reinterpret_cast<u32 *>(address) = value;
}

// TH10 0x0040cd20 (vtable 0x46d0b4 slot +0x08 of the script viewer). Native
// ECX = the viewer, stack = the script path: copies the path into the
// shared DAT_00497c38 scratch and performs the stage-enemy-script load.
// Not reconstructed here; modeled as a boundary.
void InitScriptViewerPathEcxStackAbi(void *viewer /* ECX */,
                                     const char *path);

// TH10 0x0040c5e0 (slot 0 of the destruction vtable 0x46d0d8 planted by
// 0x0040dae0). Native ECX = the record, stack = the argument. The 0x0040d750
// frame ticker calls it directly after a record reports an update — it does
// not go through the live 0x46d0c0 vtable, whose slot +0x14 is the scalar
// deleting destructor 0x0040cc50. Modeled as a boundary.
i32 NotifyEclScriptObjectEcxStackAbi(void *record /* ECX */, i32 argument);

// TH10 0x0040d750, entered through 0x0040d810 with ECX = the state.
i32 TickConditionalStateRecords(void *state)
{
    // Typed view of the 0x68-byte conditional state (the scheduler passes it
    // in ECX). The walked list nodes are the EclScriptWork node triple
    // embedded in each ECL script object at record+0x116c (work+0x130:
    // list_self_0130 / list_next_0134 / list_prev_0138), so the walk reads
    // the typed record back-pointer for the object itself.
    ConditionalState &cond = *static_cast<ConditionalState *>(state);

    // One forced update per ECL script object in the state's list. Records
    // with the work.flags_1444 (record+0x2480) bit 0x20000 set skip the ECL
    // run and are notified directly; otherwise the ECL per-frame update
    // runs and its nonzero result also notifies. On a zero result the
    // run-gate bit 0x400 is cleared so the record can run again next frame.
    // The native latches the next node (list_next_0134) before running the
    // record, since the run may tear the record down and unlink it.
    for (u32 *node = static_cast<u32 *>(cond.script_list_head_0058);
         node != 0;) {
        u32 *const next = reinterpret_cast<u32 *>(node[1]);
        EclScriptObject &record =
            *reinterpret_cast<EclScriptObject *>(node[0]);
        // (The native dereferences the record without a null check.)
        if ((record.work.flags_1444 & 0x20000U) != 0U) {
            (void)NotifyEclScriptObjectEcxStackAbi(&record, 1);
        } else if (RunEclScriptSetupStackAbi(&record.work) != 0) {
            (void)NotifyEclScriptObjectEcxStackAbi(&record, 1);
        } else {
            record.work.flags_1444 &= ~0x400U;
        }
        node = next;
    }

    // Score/countdown advance (the shared overlay time pattern already
    // mirrored in AsciiOverlayCallbacks.cpp):
    // publish the frame counter into the score slot, then either count
    // whole frames (rate inside the 0.99..1.01 band, exclusive) or add the
    // fractional rate and reconvert. The accumulator (+0x48) is the i32
    // TimerNode::accum field reinterpreted as the float accumulator (same
    // storage, native bit pattern).
    const i32 tick = cond.frame_timer.count; // +0x44
    cond.frame_timer.prev = tick;            // +0x40
    const float rate = *cond.frame_timer.rate; // +0x4c -> flt_476F78 family
    float *const accumulator =
        reinterpret_cast<float *>(&cond.frame_timer.accum); // +0x48
    if (rate > 0.99f && rate < 1.01f) {
        cond.frame_timer.count = tick + 1;
        *accumulator += 1.0f;
    } else {
        *accumulator += rate;
        cond.frame_timer.count =
            ConvertFloatToI32TowardZeroX87(*accumulator);
    }
    return 1;
}

// TH10 0x0040d810. Calculation-chain callback; the scheduler passes the
// record argument in ECX.
i32 TH10_FASTCALL ConditionalStateCalcCallbackFastAbi(void *state)
{
    return TickConditionalStateRecords(state);
}

// TH10 0x0040d820. Draw-chain callback: a pure keep-alive that reports a
// handled frame without touching any state.
i32 TH10_FASTCALL ConditionalStateDrawCallbackFastAbi(void * /*state*/)
{
    return 1;
}

// Builds the 0x24-byte scheduler record exactly as the native inlines it:
// operator new (whose flags word starts as uninitialized heap memory, of
// which only bit 1 is cleared and bit 0 set here), then the callback, the
// back-linked node, and the state argument are filled before registration.
ChainElem *CreateStateChainRecord(ChainCallback callback, void *argument)
{
    u8 *const raw = static_cast<u8 *>(AllocateChainMemory(0x24U));
    if (raw == 0)
        return 0;

    ChainElem *const element = reinterpret_cast<ChainElem *>(raw);
    // Native quirk: the flags dword is (heap-garbage & ~2) | 1, so the
    // enabled bit is always clear and the upper bits are undefined. The
    // reconstruction pins them to the observed OwnedByScheduler value.
    element->priority = 0;
    element->flags = ChainElemFlag_OwnedByScheduler;
    element->callback = callback;
    element->registration_hook = 0;
    element->calculation_followup = 0;
    element->link.owner = element;
    element->link.next = 0;
    element->link.previous = 0;
    element->arg = argument;
    return element;
}

} // namespace

// TH10 0x0040d280. EBX = state, stack = script path, ret 4. Returns 0.
i32 InitializeConditionalStateSubrecordsEbxStackAbi(void *state /* EBX */,
                                                    const char *script_path)
{
    // Typed view of the 0x68-byte conditional state; the script viewer body
    // itself (the +0x54 target) is an opaque 0x1098-byte allocation and
    // keeps raw byte stores below.
    ConditionalState &cond = *static_cast<ConditionalState *>(state);

    // Publish the effect manager's pool word into resource_table entry 0
    // (the native dereferences the DAT_004776f0 holder without a null
    // check).
    cond.resource_table[0] = *reinterpret_cast<void *const *>(
        static_cast<const u8 *>(g_EffectManagerRoot) + 0x3e0b50U);

    // The 0x1098-byte script viewer: zero-filled (the trailing +0x1090 /
    // +0x1094 words are cleared again individually before the rep stos in
    // the native, which zeroes them anyway), vtable 0x46d0b4, then the
    // virtual initializer is invoked with the script path. The native
    // dereferences the viewer vtable even when allocation failed.
    u8 *viewer = static_cast<u8 *>(AllocateChainMemory(0x1098U));
    if (viewer != 0) {
        StoreU32At(viewer + 0x1090U, 0U);
        StoreU32At(viewer + 0x1094U, 0U);
        for (u32 i = 0; i < 0x426U; ++i)
            StoreU32At(viewer + i * 4U, 0U);
        StoreU32At(viewer, 0x46d0b4U);
    }
    cond.name_registry_0054 = viewer; // +0x54
    InitScriptViewerPathEcxStackAbi(viewer, script_path);

    // Frame ticker on the calculation chain (priority 0x12) and the draw
    // no-op (priority 0x14); both carry the state as their argument.
    ChainElem *calc = CreateStateChainRecord(
        ConditionalStateCalcCallbackFastAbi, state);
    cond.calc_element = calc; // +0x08
    (void)CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler,
        calc, 0x12);

    ChainElem *draw = CreateStateChainRecord(
        ConditionalStateDrawCallbackFastAbi, state);
    cond.draw_element = draw; // +0x0c
    (void)CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw,
        0x14);

    // Score/countdown block (the frame_timer TimerNode at +0x40..+0x50):
    // the first-time seed (score -999999, zero counters, 1.0f rate) runs
    // only when the timer flag bit 0 is clear, and the final unconditional
    // pass forces score -1 and zeroed counters.
    if ((cond.frame_timer.flags & 1U) == 0U) {
        cond.frame_timer.flags |= 1U;
        cond.frame_timer.count = 0;
        cond.frame_timer.prev = static_cast<i32>(0xfff0bdc1U); // -999999
        cond.frame_timer.accum = 0; // float accumulator storage zeroed
        cond.frame_timer.rate = &g_AsciiOverlayInitialRate;
    }
    cond.frame_timer.count = 0;
    cond.frame_timer.accum = 0;
    cond.frame_timer.prev = -1;

    return 0;
}

// TH10 0x0040d750 (via 0x0040d810). ECX = state.
i32 TickConditionalStateRecordsFastAbi(void *state)
{
    return TickConditionalStateRecords(state);
}

} // namespace th10
