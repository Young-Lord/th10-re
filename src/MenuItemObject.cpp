// Menu-item string helper (0x004386B0): the MSVC std::string
// assign-with-growth routine the menu item ctor/copy paths call.
#include "MenuItemObject.hpp"

#include "Th10Types.hpp"

#include <stdlib.h>
#include <string.h>

namespace th10 {

namespace {

// TH10 0x452493 operator new (CRT malloc wrapper).
void *OperatorNew(u32 bytes);

} // namespace

// FUNCTION: TH10 0x004386B0
void *MenuItemStringAssign(void *string_object, u32 new_capacity,
                           u32 copy_size)
{
    u8 *const object = static_cast<u8 *>(string_object);
    u32 capacity = new_capacity | 0xFU;

    // Growth heuristic: when the rounded request is small relative to the
    // existing capacity, grow by half the old capacity instead (with the
    // native overflow guard).
    if (capacity != 0xFFFFFFFFU) {
        const u32 old_capacity =
            *reinterpret_cast<u32 *>(object + 0x18);
        const u32 half = old_capacity >> 1;
        if (capacity / 3 < half && old_capacity <= 0xFFFFFFFEU - half)
            capacity = half + old_capacity;
    }

    u8 *allocated = static_cast<u8 *>(OperatorNew(capacity + 1));
    u8 *const allocated_copy = allocated;

    if (copy_size != 0) {
        // Copy from the current buffer: SSO bytes at +4 when the old
        // capacity is small, heap pointer at +4 otherwise. The native
        // copies exactly copy_size bytes without a length check.
        const void *source;
        if (*reinterpret_cast<u32 *>(object + 0x18) < 0x10U)
            source = object + 4;
        else
            source = *reinterpret_cast<void *const *>(object + 4);
        memcpy(allocated, source, copy_size);
    }

    if (*reinterpret_cast<u32 *>(object + 0x18) >= 0x10U) {
        free(*reinterpret_cast<void **>(object + 4));
        allocated = allocated_copy;
    }

    *reinterpret_cast<void **>(object + 4) = allocated;
    *reinterpret_cast<u32 *>(object + 0x14) = copy_size;
    *reinterpret_cast<u32 *>(object + 0x18) = capacity;
    allocated[copy_size] = 0;
    return allocated;
}

} // namespace th10
