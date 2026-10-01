// TH10 manager-work pipeline helpers (0x004472e0-0x00448360).
//
//   0x004472e0: loads one image leaf into the work-context slot table
//     (work+0x120, 16-byte entries {data, size, ...}), validating the
//     record kind (must be 4) and skipping 0x40-prefixed names.
//   0x004473c0 / 0x004470 0: queue walkers over the 33 work contexts at
//     owner+0x3ad06c, advancing the parse cursor (work+0x124).
//   0x00447940: materializes a 0x44-byte image descriptor into work+0x118
//     slot table and derives the ratio/extent floats.
//   0x00447ec0: loads a cached surface pair for the render owner.
//   0x00448360: copies the level-0 surface of one work slot onto another.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "MainChainRender.hpp"
#include "ManagerWork.hpp"
#include "MainChainRuntime.hpp"

namespace th10 {

namespace {

extern D3D9Device *g_MainChainD3DDevice; // TH10 DAT_00491c30
extern void *g_MainChainRenderOwner;     // TH10 DAT_00491c10

// TH10 0x0044b8e0: formatted error print (EDI = console FILE* 0x474f70).
extern void PrintErrorEdiAbi(const char *format, const char *argument);

// TH10 0x0044b360 (registered in MainChainFileProbe.cpp).
extern void *LoadMainChainFile(const char *path, u32 *file_size,
                               i32 filesystem_mode);
// TH10 0x0044470 (registered in ManagerWork.cpp).
extern i32 ProcessSelectedManagerWorkStage(i32 argument, void *context,
    u32 offset_a, u32 offset_b, void *record);

u32 ReadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

void WriteU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

extern "C" i32 TH10_STDCALL D3DXLoadSurfaceFromSurface(void *device,
    void *dest_surface, const void *dest_rect, void *src_surface,
    const void *src_rect, u32 filter, u32 color_key);

void *GetVtableSlot(u32 object, u32 offset)
{
    return *reinterpret_cast<void **>(
        *reinterpret_cast<u8 **>(object) + offset);
}

i32 ReleaseComObject(void *object)
{
    typedef i32 (TH10_STDCALL *ReleaseFn)(void *);
    return reinterpret_cast<ReleaseFn>(
        GetVtableSlot(reinterpret_cast<u32>(object), 0x08U))(object);
}

} // namespace

// TH10 0x004472e0. Native ECX = slot index, EDX = work context, stack =
// record. Returns 1 (or -1 after printing an error).
i32 LoadManagerWorkImageSlotEcxEdxStackAbi(u32 slot_index, void *work,
                                           const void *record)
{
    if (record == 0) {
        PrintErrorEdiAbi("no record", 0);
        return -1;
    }
    if (ReadU32At(record, 0x28U) != 4U) {
        PrintErrorEdiAbi("wrong kind", 0);
        return -1;
    }
    if (*reinterpret_cast<const u8 *>(
            static_cast<const u8 *>(record) + 0x34U) != 0U)
        return 1;

    const char *name = reinterpret_cast<const char *>(
        static_cast<const u8 *>(record) + ReadU32At(record, 0x1cU));
    if (*name == 0x40)
        return 1;

    char resolved[260];
    (void)sprintf(resolved, "%s", name);
    u32 size = 0;
    void *data = LoadMainChainFile(resolved, &size, 1);
    if (data == 0) {
        PrintErrorEdiAbi("load failed", name);
        return -1;
    }
    const u32 table = ReadU32At(work, 0x120U);
    WriteU32At(reinterpret_cast<void *>(table), 16U * slot_index + 8U,
               size);
    WriteU32At(reinterpret_cast<void *>(table), 16U * slot_index + 4U,
               reinterpret_cast<u32>(data));
    return 1;
}

// TH10 0x004473c0. Native stdcall: argument = selector, stack = work
// context. Walks the record chain of the context, running the stage
// processor on the boundary record and advancing the +0x124 cursor.
i32 AdvanceManagerWorkParseStackAbi(i32 selector, void *work_context)
{
    u8 *context = static_cast<u8 *>(work_context);
    void *record = *reinterpret_cast<void *const *>(context + 0x108U);
    u32 index = 0;
    u32 processed = 0;
    u32 offset_a = 0;
    u32 offset_b = 0;
    u32 resume = 0;

    for (;;) {
        if (index == ReadU32At(context, 0x124U) - 1U) {
            if (ProcessSelectedManagerWorkStage(selector, context,
                                                offset_a, offset_b,
                                                record) < 0) {
                WriteU32At(context, 0x124U, 0U);
                return 0;
            }
            index = resume;
            processed = 1U;
        }
        offset_a += ReadU32At(record, 0U);
        offset_b += ReadU32At(record, 4U);
        const u32 stride = ReadU32At(record, 14U * 4U);
        if (stride == 0U) {
            WriteU32At(context, 0x124U, 0U);
            return static_cast<i32>(reinterpret_cast<u32>(work_context));
        }
        record = reinterpret_cast<u8 *>(record) + stride;
        ++index;
        resume = index;
        if (index == ReadU32At(context, 0x124U) || processed != 0U) {
            WriteU32At(context, 0x124U,
                       ReadU32At(context, 0x124U) + 1U);
            return static_cast<i32>(reinterpret_cast<u32>(work_context));
        }
    }
}

// TH10 0x00447700. Native stdcall: argument = owner. Sweeps the 33 work
// slots at owner+0x3ad06c: releases finished works (+0x128 mark) or
// advances the parse of marked ones. Returns -1 when work advanced.
i32 PollManagerWorkQueuesStackAbi(void *owner)
{
    LargeRenderOwnerLayout &owner_state =
        *static_cast<LargeRenderOwnerLayout *>(owner);
    for (u32 index = 0; index < 0x21U; ++index) {
        u8 *work = static_cast<u8 *>(owner_state.work_slots[index]);
        if (work == 0)
            continue;
        if (ReadU32At(work, 0x128U) != 0U) {
            extern void ReleaseManagerWorkContents(ManagerWorkPartial *work);
            ReleaseManagerWorkContents(
                reinterpret_cast<ManagerWorkPartial *>(work));
            free(work);
            owner_state.work_slots[index] = 0;
            continue;
        }
        if (ReadU32At(work, 0x124U) == 0U)
            continue;
        return (AdvanceManagerWorkParseStackAbi(
                    0, owner_state.work_slots[index]) != 0) - 1;
    }
    return 0;
}

// TH10 0x00447940. Native EAX = slot index, EDX = work base, EBX = source
// 0x44-byte descriptor. Copies it into work+0x118 slot table and derives
// the normalized floats at +0x20/+0x24/+0x28/+0x2c/+0x30/+0x34.
void StoreWorkImageMaterialEaxEdxEbxAbi(u32 slot_index, void *work,
                                        const u8 *source)
{
    const u32 table = ReadU32At(work, 0x118U);
    u8 *slot = reinterpret_cast<u8 *>(table) + 0x44U * slot_index;
    memcpy(slot, source, 0x44U);

    const float *const src = reinterpret_cast<const float *>(slot);
    *reinterpret_cast<float *>(slot + 0x20U) = src[2] / src[7];
    *reinterpret_cast<float *>(slot + 0x28U) = src[4] / src[7];
    *reinterpret_cast<float *>(slot + 0x24U) = src[3] / src[6];
    *reinterpret_cast<float *>(slot + 0x2cU) = src[5] / src[6];
    *reinterpret_cast<float *>(slot + 0x34U) =
        (src[4] - src[2]) / reinterpret_cast<const float *>(source)[14];
    *reinterpret_cast<float *>(slot + 0x30U) =
        (src[5] - src[3]) / reinterpret_cast<const float *>(source)[15];
}

// TH10 0x00447ec0. Native ECX = slot index, EDX = owner, EBX = leaf name.
i32 LoadOwnerCachedSurfaceEdxCcxAbi(u32 slot_index, void *owner,
                                    const char *name)
{
    LargeRenderOwnerLayout &owner_state =
        *static_cast<LargeRenderOwnerLayout *>(owner);
    if (owner_state.render_targets[slot_index] != 0) {
        extern void ReleaseLargeRenderOwnerCachedSurfacePair(void *owner,
            u32 cache_slot);
        ReleaseLargeRenderOwnerCachedSurfacePair(owner, slot_index);
    }

    char resolved[260];
    (void)sprintf(resolved, "%s", name);
    u32 size = 0;
    void *data = LoadMainChainFile(resolved, &size, 0);
    if (data == 0) {
        PrintErrorEdiAbi("%s", name);
        return -1;
    }
    owner_state.cached_file_buffers[slot_index] = data;
    owner_state.cached_file_sizes[slot_index] = size;
    return 0;
}

// TH10 0x00448360. Native EAX = source slot, EDX = owner, ECX = destination
// slot, stack (ret 0x10) = source context, destination context, device
// word, spare. Copies the level-0 surface of the source slot's texture onto
// the destination slot's texture.
i32 CopyWorkSurfacePairEcxAbi(u32 src_slot, void *owner, u32 dst_slot,
                              u32 src_context, u32 dst_context)
{
    LargeRenderOwnerLayout &owner_state =
        *static_cast<LargeRenderOwnerLayout *>(owner);
    void *const src_work = owner_state.work_slots[src_context];
    const u32 src_table = ReadU32At(src_work, 0x120U);
    const u32 src_texture = ReadU32At(reinterpret_cast<const void *>(src_table),
                                      16U * src_slot);
    if (src_texture == 0U)
        return 0;
    void *const dst_work = owner_state.work_slots[dst_context];
    const u32 dst_table = ReadU32At(dst_work, 0x120U);
    const u32 dst_texture = ReadU32At(reinterpret_cast<const void *>(dst_table),
                                      16U * dst_slot);
    if (dst_texture == 0U)
        return 0;

    FlushRenderOwnerPendingVerticesEsiAbi(
        reinterpret_cast<RenderOwnerPartial *>(owner));

    typedef i32 (TH10_STDCALL *GetSurfaceLevelFn)(void *, u32, void **);
    void *src_surface = 0;
    if (reinterpret_cast<GetSurfaceLevelFn>(
            GetVtableSlot(src_texture, 0x48U))(reinterpret_cast<void *>(
                src_texture), 0U, &src_surface) != 0)
        return 0;
    void *dst_surface = 0;
    if (reinterpret_cast<GetSurfaceLevelFn>(
            GetVtableSlot(dst_texture, 0x48U))(reinterpret_cast<void *>(
                dst_texture), 0U, &dst_surface) != 0) {
        ReleaseComObject(src_surface);
        return 0;
    }

    (void)D3DXLoadSurfaceFromSurface(g_MainChainD3DDevice, dst_surface,
        0, src_surface, 0, 0xffffffffU, 0);
    ReleaseComObject(src_surface);
    ReleaseComObject(dst_surface);
    return 0;
}

} // namespace th10
