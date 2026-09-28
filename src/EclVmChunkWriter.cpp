// ECL VM value-chunk writer and context-list manager (TH10 0x00450470,
// 0x00450500, 0x00450700, 0x00450210, 0x00450190, 0x004501b0, 0x004505b0,
// 0x00450690, 0x004506d0, 0x0044df20). The producer side of the ECL run
// context: the sorted script-name table lookup, the typed value stack
// with mark/link bookkeeping, and the sub-context list maintenance used
// by ExecuteEclInstruction, BeginEclSubFrame and the script test menu.
#include "EclVmChunkWriter.hpp"

#include <math.h>
#include <string.h>

namespace th10 {

namespace {

const u32 kChunkLimit = 0x1000U;   // cursor field at chunk + 0x1000
const u32 kChunkMarkOffset = 0x1004U;

// TH10 0x452493 operator new / 0x4524a1 operator delete.
void *NativeAlloc(u32 bytes);

extern void *GetEclContextListVtable(); // TH10 off_46d0d8

// Entry of the sorted script-name table at owner +0x8c.
struct ScriptNameEntry {
    const char *name;
    i32 value;
};

} // namespace

i32 ResolveScriptTableIndexEaxAbi(i32 table_owner, const char *name)
{
    // Native EAX = table owner, stack = name.
    const u32 owner = static_cast<u32>(table_owner);
    // Native keeps count-1 in ebx as the upper search bound (js on a
    // negative count bails out immediately).
    i32 high = static_cast<i32>(*reinterpret_cast<const u32 *>(owner + 8U));
    --high;
    if (high < 0)
        return 0;

    const ScriptNameEntry *const table = *reinterpret_cast<
        const ScriptNameEntry *const *>(owner + 0x8cU);
    i32 low = 0;
    while (low <= high) {
        // (low + high) / 2 without overflow, as the native does with
        // cdq/sar rounding toward negative infinity.
        const i32 middle = low + ((high - low) >> 1);
        const int comparison = strcmp(name, table[middle].name);
        if (comparison == 0)
            // The resolved value points 0x10 bytes into the entry's
            // target record (past its header).
            return static_cast<i32>(
                static_cast<u32>(table[middle].value) + 0x10U);
        if (comparison < 0)
            high = middle - 1;
        else
            low = middle + 1;
    }
    return 0;
}

void *CreateEclContextListStackAbi(void *table_owner, const char *name)
{
    // Native EDI = table owner, stack = name; result in EAX.
    u32 *manager = static_cast<u32 *>(NativeAlloc(0x103cU));
    if (manager != 0) {
        manager[0] = reinterpret_cast<u32>(GetEclContextListVtable());
        manager[0x1010U / 4U] = 0;
        manager[0x1014U / 4U] = 0;
    }
    // The native continues on the null path and dereferences the manager
    // unconditionally below; preserved as-is.
    manager[0x102cU / 4U] = reinterpret_cast<u32>(table_owner);

    // Native quirk: the record pointer at +4 is read out of the fresh
    // allocation (whatever the allocator handed back) and receives both
    // the resolved table index and the zeroed head dword.
    u32 *record = reinterpret_cast<u32 *>(manager[1]);
    record[1] = static_cast<u32>(ResolveScriptTableIndexEaxAbi(
        reinterpret_cast<i32>(table_owner), name));
    record[0] = 0;
    return manager;
}

i32 BindEclContextListEntryEcxEsiAbi(void *manager, const char *name)
{
    // Native ESI = manager, ECX = name.
    u32 *const holder = static_cast<u32 *>(manager);
    const i32 owner = static_cast<i32>(holder[0x102cU / 4U]);
    u32 *record = reinterpret_cast<u32 *>(holder[1]);
    record[1] = static_cast<u32>(ResolveScriptTableIndexEaxAbi(owner, name));
    record[0] = 0;
    return 0;
}

void SetEclContextListRecordEaxStackAbi(void *owner, void *record)
{
    // Native EAX = owner, stack = record.
    *reinterpret_cast<u32 *>(static_cast<u8 *>(owner) + 4U) =
        reinterpret_cast<u32>(record);
}

void EclVmOpcode21EaxAbi(void *manager)
{
    // Native EAX = manager; list head at +0x1034, nodes are
    // {u32 target; u32 next} pairs whose target's +4 dword is cleared.
    // The native leaves the head and the nodes themselves untouched.
    u32 *node = *reinterpret_cast<u32 **>(
        static_cast<u32 *>(manager) + 0x1034U / 4U);
    while (node != 0) {
        u32 *const next = reinterpret_cast<u32 *>(node[1]);
        *reinterpret_cast<u32 *>(node[0] + 4U) = 0;
        node = next;
    }
}

void EclVmOpcode81HelperEcxEfxAbi(float out_pair[2], float angle,
                                  float value)
{
    // Native ECX = out pair; fsincos, then both products against the
    // stack value.
    out_pair[0] = cos(angle) * value;
    out_pair[1] = sin(angle) * value;
}

i32 EclVmPushTypedEaxDlStackAbi(void *chunk, u8 type_tag, u32 byte_count,
                                const void *value)
{
    // Native EAX = chunk, DL = type tag, stack = {byte_count, value}.
    u32 cursor = *reinterpret_cast<u32 *>(
        static_cast<u8 *>(chunk) + kChunkLimit);
    const u32 end = cursor + byte_count;
    // Signed comparison in the native (jl against 0x1000).
    if (static_cast<i32>(end) >= static_cast<i32>(kChunkLimit))
        return -1;

    if (type_tag != 0U) {
        *(static_cast<u8 *>(chunk) + cursor) = type_tag;
        // Native quirk: the tag byte occupies a full dword slot, but the
        // overflow check above never accounted for those 4 bytes.
        *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk) + kChunkLimit) =
            cursor + 4U;
    }

    cursor = *reinterpret_cast<u32 *>(
        static_cast<u8 *>(chunk) + kChunkLimit);
    memcpy(static_cast<u8 *>(chunk) + cursor, value, byte_count);
    *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk) + kChunkLimit) =
        cursor + byte_count;
    return 0;
}

i32 EclVmPushIntEaxEcxAbi(void *chunk, i32 advance)
{
    // Native EAX = chunk, ECX = advance.
    const u32 previous = *reinterpret_cast<u32 *>(
        static_cast<u8 *>(chunk) + kChunkLimit);
    const u32 cursor = previous + static_cast<u32>(advance);
    if (static_cast<i32>(cursor) >= static_cast<i32>(kChunkLimit))
        return -1;

    *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk) + kChunkLimit) =
        cursor;
    if (static_cast<i32>(cursor + 4U) < static_cast<i32>(kChunkLimit)) {
        *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk) + cursor) =
            *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk)
                                     + kChunkMarkOffset);
        *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk) + kChunkLimit) =
            cursor + 4U;
    }
    *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk)
        + kChunkMarkOffset) = previous;
    return 0;
}

i32 EclVmPopTopEaxAbi(void *chunk)
{
    // Native EAX = chunk. The signed cursor-4 test decides whether the
    // mark is reloaded from the popped entry; the cursor is then always
    // replaced by the saved mark.
    const u32 cursor = *reinterpret_cast<u32 *>(
        static_cast<u8 *>(chunk) + kChunkLimit);
    const i32 popped = static_cast<i32>(cursor) - 4;
    const u32 saved_mark = *reinterpret_cast<u32 *>(
        static_cast<u8 *>(chunk) + kChunkMarkOffset);
    if (popped >= 0) {
        *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk) + kChunkLimit) =
            static_cast<u32>(popped);
        *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk) + kChunkMarkOffset) =
            *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk)
                                     + static_cast<u32>(popped));
    }
    *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk) + kChunkLimit) =
        saved_mark;
    return 0;
}

void *ResolveEclChunkMarkedSlotEcxEaxAbi(void *chunk, i32 offset)
{
    // Native ECX = chunk, EDX = offset.
    return static_cast<u8 *>(chunk)
        + *reinterpret_cast<u32 *>(static_cast<u8 *>(chunk)
              + kChunkMarkOffset)
        + static_cast<u32>(offset);
}

} // namespace th10
