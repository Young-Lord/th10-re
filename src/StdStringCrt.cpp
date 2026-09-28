// TH10 MSVCP60 std::basic_string<char> internals (0x00436770-0x004385b0)
// as the game links them. Object layout (dword slots):
//   +0x00 allocator root (unused here), +0x04 SSO buffer / heap pointer,
//   +0x14 size, +0x18 capacity (15 marks the small buffer).
// The native allocation path goes through MenuItemStringAssign (0x004386b0)
// which is already registered; error paths call _String_base::_Xran/_Xlen.
#include <stdlib.h>
#include <string.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"
#include "MenuItemObject.hpp"

namespace th10 {

void *StringEraseRangeEcxAbi(void *string_object, u32 position, u32 count);
void *StringAssignRangeEcxAbi(void *string_object, const char *text,
                              u32 byte_count);

namespace {

const u32 k_small_capacity = 15U;

char *BufferOf(u32 *string_object)
{
    if (string_object[6] < 0x10U)
        return reinterpret_cast<char *>(string_object) + 4U;
    return reinterpret_cast<char *>(string_object[1]);
}

void FreeArchiveVectorShim(void *pointer)
{
    free(pointer);
}

// TH10 0x462428 / 0x4624e5 (MSVCP60 imports).
extern "C" void TH10_STDCALL StdStringBaseXran(void *string_base);
extern "C" void TH10_STDCALL StdStringBaseXlen(void *string_base);

} // namespace

// TH10 0x00436770. __cdecl: copies one byte, returns the source pointer.
const u8 *CopySingleByteCdeclAbi(u8 *destination, const u8 *source)
{
    *destination = *source;
    return source;
}

// TH10 0x004367a0. __cdecl: block copy, returns the destination.
void *CopyByteBlockCdeclAbi(void *destination, const void *source,
                            u32 byte_count)
{
    memcpy(destination, source, byte_count);
    return destination;
}

// TH10 0x004381e0. Native EAX = object. Empty-string constructor state.
void *StringDefaultInitEaxAbi(void *string_object)
{
    u32 *object = static_cast<u32 *>(string_object);
    object[6] = k_small_capacity;
    object[5] = 0;
    *(reinterpret_cast<u8 *>(object) + 4U) = 0;
    return string_object;
}

// TH10 0x00438260. Native thiscall: stack = source object, pos, count.
// Assigns source.substr(pos, count); the self-assign path erases the tail
// then the head (native order preserved).
void *StringAssignSubstringEcxAbi(void *string_object,
                                  const void *source_raw, u32 position,
                                  u32 count)
{
    const u32 *source_object = static_cast<const u32 *>(source_raw);
    u32 *object = static_cast<u32 *>(string_object);
    if (source_object[5] < position)
        StdStringBaseXran(string_object);
    u32 take = source_object[5] - position;
    if (count < take)
        take = count;

    if (string_object == source_object) {
        StringEraseRangeEcxAbi(string_object, position + take, -1);
        StringEraseRangeEcxAbi(string_object, 0, position);
        return string_object;
    }
    if (take == 0xffffffffU)
        StdStringBaseXlen(string_object);

    const u32 capacity = object[6];
    if (capacity < take) {
        MenuItemStringAssign(string_object, take, object[5]);
    } else if (take == 0U) {
        object[5] = 0;
        if (capacity < 0x10U)
            *(reinterpret_cast<u8 *>(object) + 4U) = 0;
        else
            *reinterpret_cast<char *>(object[1]) = 0;
        return string_object;
    }

    const char *source = (source_object[6] < 0x10U)
        ? reinterpret_cast<const char *>(source_object) + 4U
        : reinterpret_cast<const char *>(source_object[1]);
    char *target = BufferOf(object);
    memcpy(target, source + position, take);
    object[5] = take;
    if (object[6] >= 0x10U)
        target = reinterpret_cast<char *>(object[1]);
    target[take] = 0;
    return string_object;
}

// TH10 0x00438350. Native thiscall: stack = NUL-terminated text.
void *StringAssignCStringEcxAbi(void *string_object, const char *text)
{
    return StringAssignRangeEcxAbi(string_object, text,
                                   static_cast<u32>(strlen(text)));
}

// TH10 0x00438380. Native thiscall: stack = keep flag, new size. Moves the
// heap buffer back into the SSO buffer when it fits and marks the object
// small-buffered again.
void StringTidyToSmallBufferEcxAbi(void *string_object, u8 keep_data,
                                   u32 new_size)
{
    u32 *object = static_cast<u32 *>(string_object);
    if (keep_data != 0 && object[6] >= 0x10U) {
        void *heap = reinterpret_cast<void *>(object[1]);
        if (new_size != 0U)
            memcpy(reinterpret_cast<u8 *>(object) + 4U, heap, new_size);
        FreeArchiveVectorShim(heap);
    }
    object[5] = new_size;
    object[6] = k_small_capacity;
    *(reinterpret_cast<u8 *>(string_object) + new_size + 4U) = 0;
}

// TH10 0x00438420. Native ECX = object, stack = text, byte count. Range
// assignment with the self-overlap check (native delegates the overlapping
// tail case to 0x00438260).
void *StringAssignRangeEcxAbi(void *string_object, const char *text,
                              u32 byte_count)
{
    u32 *object = static_cast<u32 *>(string_object);
    char *buffer = BufferOf(object);
    if (text >= buffer) {
        char *self = buffer;
        if (object[6] >= 0x10U)
            self = reinterpret_cast<char *>(object[1]);
        if (self + object[5] > text) {
            if (object[6] >= 0x10U)
                self = reinterpret_cast<char *>(object[1]);
            return StringAssignSubstringEcxAbi(
                string_object, object,
                static_cast<u32>(text - self), byte_count);
        }
    }
    if (byte_count == 0xffffffffU)
        StdStringBaseXlen(string_object);

    const u32 capacity = object[6];
    if (capacity < byte_count) {
        MenuItemStringAssign(string_object, byte_count, object[5]);
    } else if (byte_count == 0U) {
        object[5] = 0;
        if (capacity < 0x10U)
            *(reinterpret_cast<u8 *>(object) + 4U) = 0;
        else
            *reinterpret_cast<char *>(object[1]) = 0;
        return string_object;
    }

    char *target = (object[6] < 0x10U)
        ? reinterpret_cast<char *>(object) + 4U
        : reinterpret_cast<char *>(object[1]);
    memcpy(target, text, byte_count);
    object[5] = byte_count;
    if (object[6] < 0x10U)
        *(reinterpret_cast<u8 *>(object) + byte_count + 4U) = 0;
    else
        *(reinterpret_cast<char *>(object[1]) + byte_count) = 0;
    return string_object;
}

// TH10 0x00438510. Native thiscall: stack = position, count. Erases count
// bytes at position and shifts the tail left.
void *StringEraseRangeEcxAbi(void *string_object, u32 position, u32 count)
{
    u32 *object = static_cast<u32 *>(string_object);
    if (object[5] < position)
        StdStringBaseXran(string_object);
    u32 erase = count;
    const u32 tail = object[5] - position;
    if (tail < erase)
        erase = tail;
    if (erase != 0U) {
        char *buffer = BufferOf(object);
        memmove(buffer + position, buffer + position + erase, tail - erase);
        const u32 new_size = object[5] - erase;
        object[5] = new_size;
        if (object[6] >= 0x10U)
            buffer = reinterpret_cast<char *>(object[1]);
        buffer[new_size] = 0;
    }
    return string_object;
}

// TH10 0x00438590. Native thiscall: stack = new size. Sets the size and
// writes the terminator through whichever buffer is active.
void StringTruncateEcxAbi(void *string_object, u32 new_size)
{
    u32 *object = static_cast<u32 *>(string_object);
    object[5] = new_size;
    if (object[6] < 0x10U)
        *(reinterpret_cast<u8 *>(object) + new_size + 4U) = 0;
    else
        *(reinterpret_cast<u8 *>(object[1]) + new_size) = 0;
}

// TH10 0x004385b0. Native thiscall: stack = requested size, shrink flag.
// Grows through MenuItemStringAssign or shrinks into the SSO buffer.
u32 StringReserveCapacityEcxAbi(void *string_object, u32 requested_size,
                                u8 shrink_allowed)
{
    u32 *object = static_cast<u32 *>(string_object);
    if (requested_size == 0xffffffffU)
        StdStringBaseXlen(string_object);
    const u32 capacity = object[6];
    if (capacity < requested_size) {
        MenuItemStringAssign(string_object, requested_size, object[5]);
        return requested_size != 0U;
    }
    if (shrink_allowed != 0U && requested_size < 0x10U) {
        u32 keep = object[5];
        if (requested_size < keep)
            keep = requested_size;
        if (capacity >= 0x10U) {
            void *heap = reinterpret_cast<void *>(object[1]);
            if (keep != 0U)
                memcpy(reinterpret_cast<u8 *>(object) + 4U, heap, keep);
            FreeArchiveVectorShim(heap);
        }
        object[5] = keep;
        object[6] = k_small_capacity;
        *(reinterpret_cast<u8 *>(object) + keep + 4U) = 0;
        return requested_size != 0U;
    }
    if (requested_size == 0U) {
        object[5] = 0;
        if (capacity >= 0x10U)
            *reinterpret_cast<u8 *>(object[1]) = 0;
        else
            *(reinterpret_cast<u8 *>(object) + 4U) = 0;
    }
    return requested_size != 0U;
}

} // namespace th10
