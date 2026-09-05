#pragma once

#include <stddef.h>

#include "Th10Platform.hpp"

namespace th10 {

struct WorkRecordPartial {
    void *virtual_object;
    void *owned_allocation;
    i32 encoded_size_or_state;
    i32 format_pitch_factor;
};

struct ManagerWorkChainNodePartial {
    i32 generated_record_count;
    i32 pointer_output_count;
    u8 unknown_0008[4];
    i32 horizontal_normalizer;
    i32 vertical_normalizer;
    i32 format_index_or_relative;
    i32 adapter_argument;
    i32 marker_relative;
    void *raw_source_data;
    u8 unknown_0024[4];
    i32 required_kind;
    i32 virtual_method_argument;
    i32 alternate_source_relative;
    u8 use_alternate_adapter;
    u8 unknown_0035[3];
    i32 next_relative;
};

struct ManagerWorkStageOutputRecord {
    u8 unknown_0000[8];
    float field_0008;
    float field_000c;
    float field_0010;
    float field_0014;
    union {
        u32 bits;
        float value;
    } field_0018;
    union {
        u32 bits;
        float value;
    } field_001c;
    union {
        u32 bits;
        float value;
    } field_0020;
    union {
        u32 bits;
        float value;
    } field_0024;
    float field_0028;
    float field_002c;
    float field_0030;
    float field_0034;
    float field_0038;
    float field_003c;
    u8 unknown_0040[4];
};

struct ManagerWorkPartial {
    i32 slot;
    char resource_name[0x104];
    void *chain_head;
    i32 node_count;
    i32 output_pointer_count;
    i32 output_record_count;
    void *output_records_0118;
    void *output_pointer_list_011c;
    WorkRecordPartial *records;
    i32 active_cursor;
    u32 invariant_zero_0128;
    void *owned_012c;
};

struct ManagerWorkOwnerPartial;

enum ManagerWorkServiceResult {
    ManagerWorkService_Continue = 0,
    ManagerWorkService_Stop = -1,
};

typedef char AssertManagerWorkChainOffset[
    offsetof(ManagerWorkPartial, chain_head) == 0x108 ? 1 : -1];
typedef char AssertManagerWorkNameOffset[
    offsetof(ManagerWorkPartial, resource_name) == 0x4 ? 1 : -1];
typedef char AssertManagerWorkActiveOffset[
    offsetof(ManagerWorkPartial, active_cursor) == 0x124 ? 1 : -1];
typedef char AssertManagerWorkInvariantOffset[
    offsetof(ManagerWorkPartial, invariant_zero_0128) == 0x128 ? 1 : -1];
typedef char AssertManagerWorkSize[sizeof(ManagerWorkPartial) == 0x130 ? 1 : -1];
typedef char AssertManagerWorkChainNodeNextOffset[
    offsetof(ManagerWorkChainNodePartial, next_relative) == 0x38 ? 1 : -1];
typedef char AssertManagerWorkChainNodeKindOffset[
    offsetof(ManagerWorkChainNodePartial, required_kind) == 0x28 ? 1 : -1];
typedef char AssertManagerWorkStageOutputSize[
    sizeof(ManagerWorkStageOutputRecord) == 0x44 ? 1 : -1];
typedef char AssertWorkRecordSize[sizeof(WorkRecordPartial) == 0x10 ? 1 : -1];
typedef char AssertWorkRecordPitchOffset[
    offsetof(WorkRecordPartial, format_pitch_factor) == 0xc ? 1 : -1];

ManagerWorkServiceResult TH10_STDCALL ServiceManagerWork(
    ManagerWorkOwnerPartial *owner); // TH10 0x00447700
ManagerWorkPartial *TH10_STDCALL AdvanceManagerWork(
    ManagerWorkOwnerPartial *owner, ManagerWorkPartial *work); // 0x4473c0
// TH10 0x447280. Creates an empty slot on demand, then waits only while the
// global manager-work control byte permits background progression.
ManagerWorkPartial *RequestManagerWork(ManagerWorkOwnerPartial *owner,
                                       i32 slot, const char *resource_name);
ManagerWorkPartial *BuildManagerWork(ManagerWorkOwnerPartial *owner,
                                     i32 slot, const char *resource_name);
void ReleaseManagerWorkContents(ManagerWorkPartial *work);
// TH10 0x004477d0. Native receives owner in EBX and slot index in ESI.
void ReleaseManagerWorkAtSlot(ManagerWorkOwnerPartial *owner, i32 slot);
void CopyAndDeriveManagerWorkStageOutput(ManagerWorkPartial *work,
                                         i32 output_index,
                                         const ManagerWorkStageOutputRecord *source);

} // namespace th10
