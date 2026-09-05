#include "MainChainRuntime.hpp"
#include "ManagerWork.hpp"
#include "ManagerWorkStageAdapters.hpp"

#include <string.h>

#include "PackedArchive.hpp"

namespace th10 {

namespace {

extern volatile signed char g_MainChainResourceGate; // low byte of DAT_004977b4
extern void SleepMilliseconds(u32 milliseconds); // KERNEL32!Sleep

extern void FreeManagerWork(void *work);
extern void MarkManagerWorkLinkedEntries(void *owner,
                                         ManagerWorkPartial *work);
extern void ReleaseManagerWorkAllocation(void *pointer);
extern void TriggerManagerWorkInvariantFault();
extern void ReportManagerWorkStageFailure(i32 message_id);
extern void *AllocateManagerWork(u32 bytes);
extern void LogInvalidManagerWorkSlot();
extern void LogInvalidManagerWorkNode();
extern void LogManagerWorkResourceFailure(const char *name);

i32 WrapAdd(i32 left, i32 right)
{
    return static_cast<i32>(static_cast<u32>(left) + static_cast<u32>(right));
}

void ReleaseWorkRecordVirtualObject(void *object)
{
    typedef void (TH10_STDCALL *ReleaseFn)(void *);
    void **const vtable = *static_cast<void ***>(object);
    reinterpret_cast<ReleaseFn>(vtable[2])(object);
}

ManagerWorkChainNodePartial *GetNextNode(ManagerWorkChainNodePartial *node)
{
    if (node->next_relative == 0)
        return 0;
    return reinterpret_cast<ManagerWorkChainNodePartial *>(
        reinterpret_cast<u8 *>(node) + node->next_relative);
}

void *ResolveNodeRelative(ManagerWorkChainNodePartial *node, i32 offset)
{
    return reinterpret_cast<u8 *>(node) + offset;
}

i32 WrapMultiply(i32 left, i32 right)
{
    return static_cast<i32>(static_cast<u32>(left) * static_cast<u32>(right));
}

i32 LoadManagerWorkNodeExternalData(i32 index, ManagerWorkPartial *work,
                                    ManagerWorkChainNodePartial *node)
{
    if (node == 0 || node->required_kind != 4) {
        LogInvalidManagerWorkNode();
        return -1;
    }
    if (node->use_alternate_adapter != 0)
        return 1;

    const char *const name = static_cast<const char *>(
        ResolveNodeRelative(node, node->marker_relative));
    if (*name == '@')
        return 1;

    u32 size = 0;
    void *const buffer = LoadPackedResource(name, &size, 1);
    if (buffer == 0) {
        LogManagerWorkResourceFailure(name);
        return -1;
    }

    work->records[index].owned_allocation = buffer;
    work->records[index].encoded_size_or_state = static_cast<i32>(size);
    return 1;
}

struct ManagerWorkStageItem {
    u8 unknown_0000[4];
    float field_0004;
    float field_0008;
    float field_000c;
    float field_0010;
};

i32 ProcessSelectedManagerWorkStage(
    ManagerWorkOwnerPartial *, ManagerWorkPartial *work,
    i32 prefix_record_count, i32 prefix_pointer_count,
    ManagerWorkChainNodePartial *node, i32 stage_index)
{
    if (node == 0) {
        ReportManagerWorkStageFailure(0);
        return -1;
    }
    if (node->required_kind != 4) {
        ReportManagerWorkStageFailure(1);
        return -1;
    }

    WorkRecordPartial *record = work->records + stage_index;
    if (node->use_alternate_adapter != 0) {
        if (SetupRawManagerWorkStage(record, node) != 0) {
            ReportManagerWorkStageFailure(2);
            return -1;
        }
    } else {
        const u8 marker = *static_cast<u8 *>(
            ResolveNodeRelative(node, node->marker_relative));
        if (marker == '@') {
            SetupEmptyManagerWorkStage(record, node);
        } else if (SetupEncodedManagerWorkStage(record, node) != 0) {
            ReportManagerWorkStageFailure(3);
            return -1;
        }
    }

    CallManagerWorkVirtualSlot1c(record->virtual_object,
                                 node->virtual_method_argument);
    CallManagerWorkVirtualSlot24(record->virtual_object);

    ManagerWorkStageOutputRecord output;
    FillManagerWorkVirtualOutput(record->virtual_object, &output);
    output.field_0020.bits = *reinterpret_cast<u32 *>(work);
    output.field_0024.bits = reinterpret_cast<u32>(record->virtual_object);

    i32 record_count = node->generated_record_count;
    for (i32 index = 0; index < record_count; ++index) {
        const i32 relative = *reinterpret_cast<i32 *>(
            reinterpret_cast<u8 *>(node) + 0x40 + index * 4);
        const ManagerWorkStageItem *item =
            static_cast<const ManagerWorkStageItem *>(
                ResolveNodeRelative(node, relative));
        const float horizontal = static_cast<float>(output.field_0018.bits) /
            static_cast<float>(node->horizontal_normalizer);
        const float vertical = static_cast<float>(output.field_001c.bits) /
            static_cast<float>(node->vertical_normalizer);

        output.field_0028 = horizontal * item->field_0004;
        output.field_002c = vertical * item->field_0008;
        output.field_0030 = horizontal * (item->field_000c + item->field_0004);
        output.field_0034 = vertical * (item->field_0010 + item->field_0008);
        output.field_0038 = output.field_001c.value;
        output.field_003c = output.field_0018.value;
        CopyAndDeriveManagerWorkStageOutput(work,
            WrapAdd(prefix_record_count, index), &output);
    }

    const u8 *pointer_entries = reinterpret_cast<const u8 *>(node) + 0x44 +
        record_count * 4;
    for (i32 index = 0; index < node->pointer_output_count; ++index) {
        const i32 relative = *reinterpret_cast<const i32 *>(
            pointer_entries + index * 8);
        void **outputs = static_cast<void **>(work->output_pointer_list_011c);
        outputs[WrapAdd(prefix_pointer_count, index)] =
            ResolveNodeRelative(node, relative);
    }

    return 1;
}

} // namespace

// TH10 0x004470c0. An empty work slot is published before the chain blob and
// later per-node resources are validated; each failure intentionally leaves
// that published partial work for the normal owner teardown path.
ManagerWorkPartial *BuildManagerWork(ManagerWorkOwnerPartial *owner,
                                     i32 slot, const char *resource_name)
{
    if (slot >= 0x21) {
        LogInvalidManagerWorkSlot();
        return 0;
    }

    u8 *const chain_blob = LoadPackedResource(resource_name, 0, 0);
    ManagerWorkPartial *const work = static_cast<ManagerWorkPartial *>(
        AllocateManagerWork(sizeof(ManagerWorkPartial)));
    if (work != 0)
        memset(work, 0, sizeof(*work));
    owner->work_slots[slot] = work;

    if (chain_blob == 0)
        return 0;

    // The native code intentionally proceeds after a null outer allocation.
    work->slot = slot;
    strcpy(work->resource_name, resource_name);
    work->chain_head = chain_blob;

    ManagerWorkChainNodePartial *node =
        reinterpret_cast<ManagerWorkChainNodePartial *>(chain_blob);
    i32 node_count = 1;
    i32 pointer_count = node->pointer_output_count;
    i32 record_count = node->generated_record_count;
    while (node->next_relative != 0) {
        node = GetNextNode(node);
        pointer_count = WrapAdd(pointer_count, node->pointer_output_count);
        record_count = WrapAdd(record_count, node->generated_record_count);
        node_count = WrapAdd(node_count, 1);
    }

    work->node_count = node_count;
    work->records = static_cast<WorkRecordPartial *>(
        AllocateManagerWork(WrapMultiply(node_count,
                                         sizeof(WorkRecordPartial))));
    memset(work->records, 0, WrapMultiply(node_count,
                                           sizeof(WorkRecordPartial)));
    work->output_records_0118 = AllocateManagerWork(
        WrapMultiply(record_count, sizeof(ManagerWorkStageOutputRecord)));
    work->output_pointer_list_011c = AllocateManagerWork(
        WrapMultiply(pointer_count, sizeof(void *)));
    work->output_pointer_count = pointer_count;
    work->output_record_count = record_count;

    node = reinterpret_cast<ManagerWorkChainNodePartial *>(chain_blob);
    for (i32 index = 0;; index = WrapAdd(index, 1)) {
        if (LoadManagerWorkNodeExternalData(index, work, node) < 0)
            return 0;
        if (node->next_relative == 0)
            return work;
        node = GetNextNode(node);
    }
}

// TH10 0x00447280. The original reads the slot before validating it, so this
// semantic boundary intentionally has no range check. A newly built work is
// only polled here: service callbacks perform the actual advancement. A
// negative control byte exposes still-active work immediately.
ManagerWorkPartial *RequestManagerWork(ManagerWorkOwnerPartial *owner,
                                       i32 slot, const char *resource_name)
{
    ManagerWorkPartial *work = reinterpret_cast<ManagerWorkPartial *>(
        owner->work_slots[slot]);
    if (work != 0)
        return work;

    work = BuildManagerWork(owner, slot, resource_name);
    if (work == 0)
        return 0;

    work->active_cursor = 1;
    for (;;) {
        if (g_MainChainResourceGate < 0)
            break;
        SleepMilliseconds(1);
        if (work->active_cursor == 0)
            break;
    }
    return work;
}

// TH10 0x00447700. The nonzero +0x128 branch is statically reachable but
// violates every recovered construction invariant and faults after clearing
// the slot in the original. It stays a distinct error boundary, not deletion.
ManagerWorkServiceResult TH10_STDCALL ServiceManagerWork(
    ManagerWorkOwnerPartial *owner)
{
    for (i32 index = 0; index < 0x21; ++index) {
        ManagerWorkPartial *work = reinterpret_cast<ManagerWorkPartial *>(
            owner->work_slots[index]);
        if (work == 0)
            continue;

        if (work->invariant_zero_0128 != 0) {
            ReleaseManagerWorkContents(work);
            FreeManagerWork(work);
            owner->work_slots[index] = 0;
            TriggerManagerWorkInvariantFault();
            return ManagerWorkService_Stop;
        }

        if (work->active_cursor == 0)
            continue;

        if (AdvanceManagerWork(owner, work) == 0)
            return ManagerWorkService_Stop;
        return ManagerWorkService_Continue;
    }

    return ManagerWorkService_Continue;
}

// TH10 0x00447810 semantic body. Native callers supply work in EDI; this
// typed body preserves the observed early return and one-shot ownership order.
void ReleaseManagerWorkAtSlot(ManagerWorkOwnerPartial *owner, i32 slot)
{
    if (slot < 0 || slot >= 0x21)
        return;

    ManagerWorkPartial *const work =
        reinterpret_cast<ManagerWorkPartial *>(owner->work_slots[slot]);
    if (work == 0)
        return;

    ReleaseManagerWorkContents(work);
    ReleaseManagerWorkAllocation(work);
    owner->work_slots[slot] = 0;
}

void ReleaseManagerWorkContents(ManagerWorkPartial *work)
{
    if (work->chain_head == 0)
        return;

    extern void *g_MainChainRenderOwner;
    MarkManagerWorkLinkedEntries(g_MainChainRenderOwner, work);

    for (i32 index = 0; index < work->node_count; ++index) {
        WorkRecordPartial &record = work->records[index];
        if (record.virtual_object != 0) {
            ReleaseWorkRecordVirtualObject(record.virtual_object);
            record.virtual_object = 0;
        }
        if (record.owned_allocation != 0) {
            ReleaseManagerWorkAllocation(record.owned_allocation);
            record.owned_allocation = 0;
        }
    }

    if (work->records != 0) {
        ReleaseManagerWorkAllocation(work->records);
        work->records = 0;
    }
    if (work->output_records_0118 != 0) {
        ReleaseManagerWorkAllocation(work->output_records_0118);
        work->output_records_0118 = 0;
    }
    if (work->output_pointer_list_011c != 0) {
        ReleaseManagerWorkAllocation(work->output_pointer_list_011c);
        work->output_pointer_list_011c = 0;
    }
    if (work->owned_012c != 0) {
        ReleaseManagerWorkAllocation(work->owned_012c);
        work->owned_012c = 0;
    }
    ReleaseManagerWorkAllocation(work->chain_head);
    work->chain_head = 0;
}

// TH10 0x004473c0 semantic body. Every call restarts from chain_head: the
// active_cursor is a one-based stage selector, never a persisted node cursor.
ManagerWorkPartial *TH10_STDCALL AdvanceManagerWork(
    ManagerWorkOwnerPartial *owner, ManagerWorkPartial *work)
{
    i32 prefix_record_count = 0;
    i32 prefix_pointer_count = 0;
    i32 stage_index = 0;
    const i32 selected_stage = WrapAdd(work->active_cursor, -1);
    ManagerWorkChainNodePartial *node =
        static_cast<ManagerWorkChainNodePartial *>(work->chain_head);

    for (;;) {
        if (stage_index == selected_stage) {
            if (ProcessSelectedManagerWorkStage(
                    owner, work, prefix_record_count, prefix_pointer_count,
                    node, stage_index) < 0) {
                work->active_cursor = 0;
                return 0;
            }
        }

        prefix_record_count = WrapAdd(prefix_record_count,
                                      node->generated_record_count);
        prefix_pointer_count = WrapAdd(prefix_pointer_count,
                                       node->pointer_output_count);

        ManagerWorkChainNodePartial *next = GetNextNode(node);
        if (next == 0) {
            work->active_cursor = 0;
            return work;
        }

        if (stage_index == selected_stage) {
            work->active_cursor = WrapAdd(work->active_cursor, 1);
            return work;
        }

        node = next;
        stage_index = WrapAdd(stage_index, 1);
    }
}

// TH10 0x00447940. The source and destination are exactly 0x44 bytes; then
// six normalized values replace their copied counterparts with no zero checks.
void CopyAndDeriveManagerWorkStageOutput(
    ManagerWorkPartial *work, i32 output_index,
    const ManagerWorkStageOutputRecord *source)
{
    ManagerWorkStageOutputRecord *destination =
        reinterpret_cast<ManagerWorkStageOutputRecord *>(
            static_cast<u8 *>(work->output_records_0118) +
            static_cast<u32>(output_index) * sizeof(*destination));
    memcpy(destination, source, sizeof(*destination));

    destination->field_0020.value = destination->field_0008 /
        destination->field_001c.value;
    destination->field_0028 = destination->field_0010 /
        destination->field_001c.value;
    destination->field_0024.value = destination->field_000c /
        destination->field_0018.value;
    destination->field_002c = destination->field_0014 /
        destination->field_0018.value;
    destination->field_0034 = (destination->field_0010 -
        destination->field_0008) / source->field_0038;
    destination->field_0030 = (destination->field_0014 -
        destination->field_000c) / source->field_003c;
}

} // namespace th10
