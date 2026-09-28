// TH10 DAT_004776fc main-chain object lifecycle, timeline-gate state
// helpers, and the preset scene-object spawn family over the 0x474430
// configuration table.

#include "Th10Types.hpp"
#include "CallbackScheduler.hpp"
#include "TimelineGateState.hpp"
#include "EntityHelpers.hpp"
#include "TimelineRenderObjects.hpp"
#include "ManagerWork.hpp"
#include "TimelineRecordInterpreter.hpp"

namespace th10 {

extern CallbackScheduler *g_CallbackScheduler;  // TH10 ds:0x491be4
extern void *g_MainChainObject6fc;              // TH10 DAT_004776fc
extern void *g_MainChainRenderOwner;            // TH10 ds:0x491c10
extern u32 g_TimelineStepSelector;              // TH10 ds:0x491ff4
extern u32 g_TimelineStepLength;                // TH10 ds:0x491fb8
extern u32 g_TimelineSkipInputBank;             // TH10 ds:0x474e30

extern void AppendMainChainErrorText(const char *text); // TH10 0x44b810
// Critical-section bracket (native indirect calls through 0x4660b0 /
// 0x4660b4 with the activity byte at 0x49231c).
extern void EnterSchedulerLockBoundary();
extern void LeaveSchedulerLockBoundary();

// Native thunks 0x40b050 / 0x40b060: both simply return 1.
i32 TH10_FASTCALL AlwaysReadyThunk(void *);

// TH10 0x0040ae50. Native EAX = the 0x18-byte object: zero all six
// dwords, set flag bit 1 on the first, publish DAT_004776fc.
void *InitMainChainObject6fcEaxAbi(void *object) {
    u32 *fields = static_cast<u32 *>(object);
    for (int i = 0; i < 6; ++i) {
        fields[i] = 0;
    }
    fields[0] |= 2u;
    g_MainChainObject6fc = object;
    return object;
}

// TH10 0x0040ae70. Native EBX = the 0x18-byte object. Requests the
// manager-work resource (kind 7) for the object and stores it at +0x10
// (failure: 0x46cf74 diagnostic, return -1). Registers two disabled
// scheduler records with the object as argument: calc 0x40b050 (always
// ready) at priority 0x17 and draw 0x40b060 (always ready) at priority
// 0x20; elements land at +8 / +0xc. Returns 0.
i32 RegisterMainChainObject6fcRecordsEbxAbi(void *object) {
    u8 *bytes = static_cast<u8 *>(object);
    ManagerWorkPartial *work = RequestManagerWork(
        reinterpret_cast<ManagerWorkOwnerPartial *>(g_MainChainRenderOwner),
        7, reinterpret_cast<const char *>(0x46cd88));
    *reinterpret_cast<void **>(bytes + 0x10) = work;
    if (work == 0) {
        AppendMainChainErrorText(reinterpret_cast<const char *>(0x46cf74));
        return -1;
    }

    // Native thunks 0x40b050 / 0x40b060 simply return 1.
    ChainElem *calc = CallbackSchedulerApi::Create(AlwaysReadyThunk);
    calc->flags &= ~ChainElemFlag_Enabled;
    calc->arg = object;
    CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler, calc,
                                                0x17);
    *reinterpret_cast<ChainElem **>(bytes + 0x8) = calc;

    ChainElem *draw = CallbackSchedulerApi::Create(AlwaysReadyThunk);
    draw->flags &= ~ChainElemFlag_Enabled;
    draw->arg = object;
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw, 0x20);
    *reinterpret_cast<ChainElem **>(bytes + 0xc) = draw;
    return 0;
}

i32 TH10_FASTCALL AlwaysReadyThunk(void *) { return 1; } // 0x40b050/60

// TH10 0x0040af00. Native EAX = object, one stack argument (ret 4, the
// object again through +8/+0xc). Under the scheduler lock, removes the
// +8 and +0xc chain elements (0x449f60), then clears DAT_004776fc.
void DestroyMainChainObject6fcInPlaceEaxAbi(void *object) {
    u8 *bytes = static_cast<u8 *>(object);
    ChainElem *calc = *reinterpret_cast<ChainElem **>(bytes + 0x8);
    if (calc != 0) {
        EnterSchedulerLockBoundary();
        CallbackSchedulerApi::RemoveSynchronized(g_CallbackScheduler, calc);
        LeaveSchedulerLockBoundary();
    }
    ChainElem *draw = *reinterpret_cast<ChainElem **>(bytes + 0xc);
    if (draw != 0) {
        EnterSchedulerLockBoundary();
        CallbackSchedulerApi::RemoveSynchronized(g_CallbackScheduler, draw);
        LeaveSchedulerLockBoundary();
    }
    g_MainChainObject6fc = 0;
}

// TH10 0x0040af90. operator new(0x18) + init + register; on failure
// destroys and frees. Returns the object or null.
void *CreateMainChainObject6fc() {
    void *object = ::operator new(0x18U);
    if (object != 0) {
        InitMainChainObject6fcEaxAbi(object);
    }
    const i32 result = RegisterMainChainObject6fcRecordsEbxAbi(object);
    if (result != 0) {
        if (object != 0) {
            DestroyMainChainObject6fcInPlaceEaxAbi(object);
            ::operator delete(object);
        }
        return 0;
    }
    return object;
}

// ---- Preset scene-object spawns over the 0x474430 table ------------------
//
// Table entry (0x10 bytes): u16 preset id, void init(ECX = entity,
// EDX = param), u32 field_398, u32 field_39c.

namespace {
const u32 kPresetTableBase = 0x474430U;
const u32 kPresetFlag = 0x40000000U;
const u32 kPresetKind = 0xf;

// Shared body: allocate a pool VM, apply the preset clone, link it into
// the render-owner list and resolve the id; run the entry's init
// callback and copy its two parameter dwords into +0x398/+0x39c.
void *SpawnPresetObjectShared(u16 preset_id, u32 init_param,
                              u32 field_398, u32 field_39c,
                              u32 (*extra_init)(void *, u32)) {
    u8 *entry = reinterpret_cast<u8 *>(kPresetTableBase +
                                       preset_id * 0x10);
    const u32 id = *reinterpret_cast<u16 *>(entry);
    void *vm = AllocatePoolVmEsiAbi(g_MainChainRenderOwner);
    if (vm != 0) {
        *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x35c) |=
            kPresetFlag;
        *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x20) =
            kPresetKind;
    }
    ApplyTimelineRenderObjectPresetClone(
        reinterpret_cast<ManagerWorkPartial *>(g_MainChainRenderOwner),
        vm, static_cast<u16>(id));

    u32 assigned = 0;
    LinkEntityAndAssignIdEaxEsiAbi(&assigned, vm);
    u8 *entity = static_cast<u8 *>(
        ResolveTimelineHandle(g_MainChainRenderOwner,
                              static_cast<i32>(assigned)));
    if (entity == 0) {
        return 0;
    }
    if (extra_init != 0) {
        extra_init(entity, init_param);
    }
    *reinterpret_cast<u32 *>(entity + 0x398) = field_398;
    *reinterpret_cast<u32 *>(entity + 0x39c) = field_39c;
    return entity;
}

} // namespace

// TH10 0x0040b070. Native EAX = table index, stack (out_record, param,
// unused; ret 0xc). Reads the u16 preset id from the entry, spawns it
// (the native reuses the out_record slot as the assigned-id scratch and
// pre-loads out_record+0x10 — a dead store against the id write, kept
// in the evidence notes), runs the entry init with EDX = param and
// publishes the resolved entity through *out_record.
void *SpawnPresetSceneObjectFromTableEaxStackAbi(u32 table_index,
                                                 u32 *out_record,
                                                 u32 init_param,
                                                 u32 unused_param) {
    (void)unused_param;
    // The native reads out_record+0x10 into the id scratch slot before
    // linking; the link overwrites it (dead store, no observable effect).
    void *entity = SpawnPresetObjectShared(
        static_cast<u16>(table_index), init_param,
        *reinterpret_cast<u32 *>(kPresetTableBase + table_index * 0x10 +
                                 0x8),
        *reinterpret_cast<u32 *>(kPresetTableBase + table_index * 0x10 +
                                 0xc),
        0);
    if (out_record != 0) {
        *out_record = reinterpret_cast<u32>(entity);
    }
    return entity;
}

// TH10 0x0040b110. Same spawn without the out_record+0x10 pre-load; the
// init callback receives EDX = param and the entity handle lands in
// *out_record.
void *SpawnPresetSceneObjectFromTableAltEaxStackAbi(u32 table_index,
                                                    u32 *out_record,
                                                    u32 init_param,
                                                    u32 unused_param) {
    (void)unused_param;
    void *entity = SpawnPresetObjectShared(
        static_cast<u16>(table_index), init_param,
        *reinterpret_cast<u32 *>(kPresetTableBase + table_index * 0x10 +
                                 0x8),
        *reinterpret_cast<u32 *>(kPresetTableBase + table_index * 0x10 +
                                 0xc),
        0);
    if (out_record != 0) {
        *out_record = reinterpret_cast<u32>(entity);
    }
    return entity;
}

// TH10 0x0040b1b0. Handle-based variant (ret 0x1c): stack (out_handle,
// param, u16 clone id, init edx param, init fn, field_398, field_39c).
// Same spawn flow with caller-supplied clone id, init fn and field
// copies; *out_handle receives the resolved entity.
void *SpawnPresetSceneObjectByHandleStackAbi(u32 *out_handle,
                                             u32 param, u16 clone_id,
                                             u32 init_edx_param,
                                             u32 (*init_fn)(void *, u32),
                                             u32 field_398,
                                             u32 field_39c) {
    (void)param;
    u8 *entry = reinterpret_cast<u8 *>(kPresetTableBase + clone_id * 0x10);
    const u32 id = *reinterpret_cast<u16 *>(entry);
    void *vm = AllocatePoolVmEsiAbi(g_MainChainRenderOwner);
    if (vm != 0) {
        *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x35c) |=
            kPresetFlag;
        *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x20) =
            kPresetKind;
    }
    ApplyTimelineRenderObjectPresetClone(
        reinterpret_cast<ManagerWorkPartial *>(g_MainChainRenderOwner),
        vm, static_cast<u16>(id));

    u32 assigned = 0;
    LinkEntityAndAssignIdEaxEsiAbi(&assigned, vm);
    u8 *entity = static_cast<u8 *>(
        ResolveTimelineHandle(g_MainChainRenderOwner,
                              static_cast<i32>(assigned)));
    if (entity == 0) {
        if (out_handle != 0) {
            *out_handle = 0;
        }
        return 0;
    }
    if (init_fn != 0) {
        init_fn(entity, init_edx_param);
    }
    *reinterpret_cast<u32 *>(entity + 0x398) = field_398;
    *reinterpret_cast<u32 *>(entity + 0x39c) = field_39c;
    if (out_handle != 0) {
        *out_handle = reinterpret_cast<u32>(entity);
    }
    return entity;
}

// ---- Timeline gate helpers ----------------------------------------------

// TH10 0x0040b530. Native EAX = the 0x28-byte gate state: zero it, set
// flag bit 1, publish DAT_00477700.
void *InitTimelineGateStateEaxAbi(TimelineGateStatePartial *gate) {
    u32 *fields = reinterpret_cast<u32 *>(gate);
    for (int i = 0; i < 10; ++i) {
        fields[i] = 0;
    }
    fields[0] |= 2u;
    g_TimelineGateState = gate;
    return gate;
}

// TH10 0x0040b460. Native EAX = inner timeline state: bit 1 (0x2) of
// the +0x74 flags.
i32 GetTimelineInnerFlag1EaxAbi(void *inner_state) {
    return (*reinterpret_cast<u32 *>(
                static_cast<u8 *>(inner_state) + 0x74) >> 1) & 1;
}

// TH10 0x0040b470. Native EAX = inner timeline state: bit 2 (0x4) of
// the +0x74 flags.
i32 GetTimelineInnerFlag2EaxAbi(void *inner_state) {
    return (*reinterpret_cast<u32 *>(
                static_cast<u8 *>(inner_state) + 0x74) >> 2) & 1;
}

// TH10 0x0040b510. Native EAX = gate state: bit 1 (0x2) of the +0x20
// gate word.
i32 GetTimelineGateFlag1EaxAbi(TimelineGateStatePartial *gate) {
    return (gate->gate_word_0020 >> 1) & 1;
}

// TH10 0x0040b9f0. Native EDI = gate state (calc-gate poll). When the
// inner timeline controller (gate+0x18) still reports active (0x40bd20),
// publishes the step length selector: DAT_00491fb8 = (DAT_00491ff4 &
// 0x1000) ? 2 : 15, and returns 1. Otherwise increments the gate frame
// counter and applies the skip ladder: return 1 when the inner +0x74
// flags have bit 2, the gate word has bit 1, the inner flags lack bit 1,
// or the 0x474e30 input bank lacks bit 8; when the counter is not a
// multiple of 12 return 6; else return 1.
i32 PollTimelineGateAutoAdvanceEdiAbi(TimelineGateStatePartial *gate) {
    if (AdvanceTimelineRecordController(gate->inner_timeline_state) != 0) {
        g_TimelineStepLength =
            ((g_TimelineStepSelector & 0x1000U) != 0) ? 2u : 15u;
        return 1;
    }

    const u32 counter = ++gate->frame_counter;
    const u8 inner_flags =
        *reinterpret_cast<u8 *>(
            reinterpret_cast<u8 *>(gate->inner_timeline_state) + 0x74);
    if ((inner_flags & 4) != 0) {
        return 1;
    }
    if ((gate->gate_word_0020 & 2) != 0) {
        return 1;
    }
    if ((inner_flags & 2) == 0) {
        return 1;
    }
    if ((g_TimelineSkipInputBank & 0x100U) == 0) {
        return 1;
    }
    if (counter % 12 != 0) {
        return 6;
    }
    return 1;
}

} // namespace th10
