// TH10 0x450220 - script-name table registration (the producer half of the
// 0x450470 ResolveScriptTableIndexEaxAbi boundary).
//
// The registry object accumulates 'SCPT' v1 blocks and their name tables:
//   +0x00 vtable
//   +0x04 count of registered file slots
//   +0x08 total name count
//   +0x0c + i*4  loaded file block pointers
//   +0x8c name table: one {char *string, char *name} pair per entry,
//         kept sorted ascending by the string bytes
//
// A 'SCPT' block layout (little-endian file, big-endian name data):
//   +0x00 'SCPT' magic (0x54504353)
//   +0x04 u16 version (must be 1)
//   +0x06 u16 name count
//   +0x10 u16 string count
//   +0x24 name-count dwords (big-endian offsets of the name strings)
//   then the string bytes, NUL terminated
#include <string.h>

#include "ConditionalStateObject.hpp"
#include "Th10Platform.hpp"
#include "Th10Types.hpp"

namespace th10 {

namespace {

// TH10 0x00452706 - malloc through the CRT heap handle at 0x477364.
void *MallocHeap452706(u32 bytes);

// TH10 0x00452422 - free.
void FreeHeap452422(void *block);

// Registry vtable: slot 1 receives the just-registered file's name-offset
// table (file + 0x24).
struct NameRegistryVtable {
    void *unknown0;
    void (TH10_STDCALL *on_file_registered)(void *registry,
                                            const void *offset_table);
};

u16 LoadU16(const u8 *base, u32 offset)
{
    return static_cast<u16>(static_cast<u16>(base[offset])
                            | (static_cast<u16>(base[offset + 1]) << 8));
}

u32 LoadU32(const u8 *base, u32 offset)
{
    return static_cast<u32>(base[offset]) | (static_cast<u32>(base[offset + 1]) << 8)
         | (static_cast<u32>(base[offset + 2]) << 16)
         | (static_cast<u32>(base[offset + 3]) << 24);
}

// Big-endian dword (the SCPT name offsets are stored big-endian).
u32 LoadBE32(const u8 *base)
{
    return (static_cast<u32>(base[0]) << 24) | (static_cast<u32>(base[1]) << 16)
         | (static_cast<u32>(base[2]) << 8) | static_cast<u32>(base[3]);
}

// Byte length of the NUL-terminated string at `text`, computed exactly like
// the native inlined scan (edx = text+1 scan, length = end - (text+1) + ...).
u32 StringBytes(const char *text)
{
    const char *cursor = text;
    while (*cursor != '\0')
        ++cursor;
    return static_cast<u32>(cursor - text);
}

// Inline strcmp: -1 / 0 / 1 exactly as the native sbb idiom produces.
i32 CompareStrings(const char *left, const char *right)
{
    while (true) {
        const u8 l = static_cast<u8>(*left++);
        const u8 r = static_cast<u8>(*right++);
        if (l != r)
            return l < r ? -1 : 1;
        if (l == 0)
            return 0;
    }
}

} // namespace

// TH10 0x450220. Native __thiscall ECX = registry, stack = file block
// (ret 4); returns the file slot index used, or -1 when the block fails
// the 'SCPT' v1 validation (the slot pointer is nulled in that case).
i32 RegisterScriptFileNamesThisStackAbi(void *registry_raw, void *file_raw)
{
    u8 *const registry = static_cast<u8 *>(registry_raw);
    ConditionalNameRegistry &reg =
        *reinterpret_cast<ConditionalNameRegistry *>(registry);
    const u32 slot_index = reg.loaded_script_count_0004;

    // The native indexes the file-slot table unboundedly with slot_index
    // (count comes from the file — native quirk), so these stay raw.
    *reinterpret_cast<u32 *>(registry + 0x0cU + slot_index * 4U)
        = reinterpret_cast<u32>(file_raw);
    const u8 *file = *reinterpret_cast<u8 *const *>(
        registry + 0x0cU + slot_index * 4U);

    if (LoadU32(file, 0U) != 0x54504353U /* 'SCPT' */
        || LoadU16(file, 4U) != 1U) {
        *reinterpret_cast<u32 *>(registry + 0x0cU + slot_index * 4U) = 0U;
        return -1;
    }

    const u16 name_count = LoadU16(file, 6U);
    const u16 string_count = LoadU16(file, 0x10U);
    const u32 new_total = reg.name_entry_count_0008 + string_count;
    reg.name_entry_count_0008 = new_total;

    // Name-offset table and the string data that follows it.
    const u8 *const offset_table = file + 0x24U;
    const char *const string_base
        = reinterpret_cast<const char *>(offset_table + name_count * 4U
                                         + string_count * 4U);

    u32 *const fresh = static_cast<u32 *>(
        MallocHeap452706(new_total * 8U));
    u32 *const previous = static_cast<u32 *>(reg.name_table_008c);
    reg.name_table_008c = fresh;

    if (previous == 0) {
        // First registration: entries are appended in file order.
        const char *string_cursor = string_base;
        for (u16 index = 0; index != string_count; ++index) {
            fresh[index * 2U + 1U]
                = reinterpret_cast<u32>(file) + LoadBE32(offset_table + index * 4U);
            fresh[index * 2U] = reinterpret_cast<u32>(string_cursor);
            string_cursor += StringBytes(string_cursor) + 1U;
        }
    } else {
        // Re-registration: keep the old entries (ascending by string) and
        // insert every new string at its sorted position.
        const u32 old_total = new_total - string_count;
        memcpy(fresh, previous, old_total * 8U);
        FreeHeap452422(previous);

        const char *string_cursor = string_base;
        for (u16 index = 0; index != string_count; ++index) {
            const char *const insert_string = string_cursor;
            const u32 insert_name
                = reinterpret_cast<u32>(file) + LoadBE32(offset_table + index * 4U);

            // Find the first entry whose string is >= the new string.
            u32 position = 0;
            while (position < old_total + index
                   && CompareStrings(
                          insert_string,
                          reinterpret_cast<const char *>(fresh[position * 2U]))
                          > 0) {
                ++position;
            }

            // Shift the tail up by one entry (8 bytes).
            for (u32 cursor = old_total + index; cursor > position; --cursor) {
                fresh[cursor * 2U] = fresh[(cursor - 1U) * 2U];
                fresh[cursor * 2U + 1U] = fresh[(cursor - 1U) * 2U + 1U];
            }

            fresh[position * 2U] = reinterpret_cast<u32>(string_cursor);
            fresh[position * 2U + 1U] = insert_name;
            string_cursor += StringBytes(string_cursor) + 1U;
        }
    }

    // Advance the file-slot count, then hand the name-offset table of the
    // just-validated block to the registry's virtual slot-1 callback.
    reg.loaded_script_count_0004 = slot_index + 1U;
    const u8 *const stored = *reinterpret_cast<u8 *const *>(
        registry + 0x0cU + slot_index * 4U);
    if (LoadU16(stored, 6U) != 0U) {
        const NameRegistryVtable *const vtable
            = static_cast<const NameRegistryVtable *>(reg.vtable_0000);
        vtable->on_file_registered(registry, stored + 0x24U);
    }

    return static_cast<i32>(slot_index);
}

} // namespace th10
