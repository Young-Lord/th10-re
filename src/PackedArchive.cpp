#include <string.h>

#include "PackedArchive.hpp"

namespace th10 {

namespace {

struct DecodedArchiveIndex {
    u8 *records;
    u32 count;
    u32 trailing_boundary;
};

extern Win32CriticalSection g_ResourceLoaderLock; // TH10 DAT_004922a4
extern volatile u8 g_ResourceLoaderNesting; // TH10 DAT_0049231e
extern PackedArchive g_PackedArchive; // TH10 DAT_00497990

extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);

extern void *AllocateArchiveVector(u32 bytes); // TH10 0x00452493
extern void FreeArchiveVector(void *pointer); // TH10 0x004524a1
extern void *AllocateResourceBuffer(u32 bytes); // TH10 0x00452706
extern void ReleaseResourceBuffer(void *pointer); // TH10 0x00452422

// TH10 0x00435220. Native EDI = source, plain return; strlen + malloc
// (CRT 0x452706) + strcpy, leaving the copy null only when allocation
// failed.
char *GameDuplicateString(const char *text)
{
    const u32 length = static_cast<u32>(strlen(text));
    char *const copy =
        static_cast<char *>(AllocateResourceBuffer(length + 1));
    if (copy != 0)
        strcpy(copy, text);
    return copy;
}
extern "C" void *TH10_STDCALL CreateFileA(const char *path, u32 access,
    u32 share, void *security_attributes, u32 creation, u32 flags,
    void *template_file);
extern "C" i32 TH10_STDCALL CloseHandle(void *handle);
extern "C" i32 TH10_STDCALL ReadFile(void *handle, void *buffer, u32 byte_count,
                                     u32 *actual_bytes, void *overlapped);
extern "C" u32 TH10_STDCALL SetFilePointer(void *handle, i32 distance_to_move,
                                           i32 *distance_high,
                                           u32 move_method);
extern "C" u32 TH10_STDCALL GetFileSize(void *handle, u32 *high_size);
extern "C" i32 TH10_STDCALL WriteFile(void *handle, const void *buffer,
                                      u32 byte_count, u32 *written,
                                      void *overlapped);

// The reader object mirrors the 0xc-byte object the archive open at
// 0x00434c30 builds inline (vtable PTR_FUN_0046f230): +0 vtable, +4 OS file
// handle, +8 access flags.
struct ArchiveReaderState {
    const void *vtable;
    void *handle;
    u32 access_flags;
};

ArchiveReaderState *ReaderState(ArchiveReader *reader)
{
    return reinterpret_cast<ArchiveReaderState *>(reader);
}

ArchiveReader *CreateArchiveReader()
{
    ArchiveReaderState *state = static_cast<ArchiveReaderState *>(
        AllocateArchiveVector(sizeof(ArchiveReaderState)));
    if (state != 0) {
        state->vtable = reinterpret_cast<const void *>(0x0046f230);
        state->handle = reinterpret_cast<void *>(static_cast<i32>(-1));
        state->access_flags = 0;
    }
    return reinterpret_cast<ArchiveReader *>(state);
}

void DestroyArchiveReader(ArchiveReader *reader, bool destructive)
{
    ArchiveReaderState *state = ReaderState(reader);
    if (state == 0)
        return;
    if (state->handle != reinterpret_cast<void *>(static_cast<i32>(-1)))
        (void)CloseHandle(state->handle);
    if (destructive)
        FreeArchiveVector(state);
}

void CloseArchiveReaderFile(ArchiveReader *reader);

void OpenArchiveReaderIgnoredResult(ArchiveReader *reader, const char *path)
{
    ArchiveReaderState *state = ReaderState(reader);
    if (state == 0)
        return;
    // Read-open branch of 0x00435370 (mode 'r'); native open first closes any
    // previously open handle through the vtable +0x04 method.
    CloseArchiveReaderFile(reader);
    state->access_flags = 0x80000000U;
    state->handle = CreateFileA(path, 0x80000000U, 1, 0, 3, 0x8000080U, 0);
}

bool SeekArchiveReader(ArchiveReader *reader, u32 offset, u32 origin)
{
    ArchiveReaderState *state = ReaderState(reader);
    if (state == 0 ||
        state->handle == reinterpret_cast<void *>(static_cast<i32>(-1)))
        return false;
    // 0x00435550 ignores the SetFilePointer result and always reports true.
    (void)SetFilePointer(state->handle, static_cast<i32>(offset), 0, origin);
    return true;
}

bool ReadArchiveReader(ArchiveReader *reader, void *output, u32 bytes)
{
    ArchiveReaderState *state = ReaderState(reader);
    if (state == 0 || state->access_flags != 0x80000000U ||
        state->handle == reinterpret_cast<void *>(static_cast<i32>(-1)))
        return false;
    u32 actual_bytes = 0;
    (void)ReadFile(state->handle, output, bytes, &actual_bytes, 0);
    return actual_bytes != 0;
}

// TH10 0x00435460 (vtable +0x04). Closes the OS handle and resets the reader
// to the closed state without releasing the reader allocation.
void CloseArchiveReaderFile(ArchiveReader *reader)
{
    ArchiveReaderState *state = ReaderState(reader);
    if (state == 0 ||
        state->handle == reinterpret_cast<void *>(static_cast<i32>(-1)))
        return;
    (void)CloseHandle(state->handle);
    state->handle = reinterpret_cast<void *>(static_cast<i32>(-1));
    state->access_flags = 0;
}

// TH10 0x004354d0 (vtable +0x0c). Full-write boolean for a reader opened for
// writing.
bool WriteArchiveReader(ArchiveReader *reader, const void *input, u32 bytes)
{
    ArchiveReaderState *state = ReaderState(reader);
    if (state == 0 || state->access_flags != 0x40000000U ||
        state->handle == reinterpret_cast<void *>(static_cast<i32>(-1)))
        return false;
    u32 written = 0;
    (void)WriteFile(state->handle, input, bytes, &written, 0);
    return written == bytes;
}

// TH10 0x00435510 (vtable +0x10). Current file position, or zero on a closed
// handle. Implemented as SetFilePointer(handle, 0, 0, FILE_CURRENT).
u32 TellArchiveReaderPosition(ArchiveReader *reader)
{
    ArchiveReaderState *state = ReaderState(reader);
    if (state == 0 ||
        state->handle == reinterpret_cast<void *>(static_cast<i32>(-1)))
        return 0;
    return SetFilePointer(state->handle, 0, 0, 1U);
}

// TH10 0x00435530 (vtable +0x14). File size, or zero on a closed handle.
u32 GetArchiveReaderFileSize(ArchiveReader *reader)
{
    ArchiveReaderState *state = ReaderState(reader);
    if (state == 0 ||
        state->handle == reinterpret_cast<void *>(static_cast<i32>(-1)))
        return 0;
    return GetFileSize(state->handle, 0);
}

// TH10 0x00435580 (vtable +0x20). Reads the whole file into a fresh resource
// allocation when the reader is open for reading and the file size does not
// exceed the caller-supplied maximum. The read starts from the current
// position and the native restores that position before returning the
// buffer; on failure the destination is released and null returned.
u8 *ReadWholeArchiveReaderFile(ArchiveReader *reader, u32 max_bytes)
{
    ArchiveReaderState *state = ReaderState(reader);
    if (state == 0 || state->access_flags != 0x80000000U ||
        state->handle == reinterpret_cast<void *>(static_cast<i32>(-1)))
        return 0;

    const u32 file_size = GetFileSize(state->handle, 0);
    if (file_size > max_bytes)
        return 0;

    u8 *const buffer =
        static_cast<u8 *>(AllocateResourceBuffer(file_size));
    if (buffer == 0)
        return 0;

    const u32 position = SetFilePointer(state->handle, 0, 0, 1U);
    (void)SetFilePointer(state->handle, static_cast<i32>(position), 0, 0U);

    u32 actual_bytes = 0;
    (void)ReadFile(state->handle, buffer, file_size, &actual_bytes, 0);
    if (actual_bytes == 0) {
        ReleaseResourceBuffer(buffer);
        return 0;
    }

    (void)SetFilePointer(state->handle, static_cast<i32>(position), 0, 0U);
    return buffer;
}

// Index decode remains a native leaf handled by 0x00434f70 (header and
// trailing index blob) plus 0x004350d0 (record table build); see
// docs/evidence/packed-archive-resource-loader.md.
extern bool DecodeArchiveIndex(ArchiveReader *reader, const char *path,
                               DecodedArchiveIndex *decoded);

// TH10 0x0046054e. Case-insensitive leaf-name comparison equivalent to the
// __stricmp the native lookup helpers use for every 0x10-byte index record.
i32 CompareArchiveLeafCaseInsensitive(const char *left, const char *right)
{
    for (;;) {
        u8 left_char = static_cast<u8>(*left);
        u8 right_char = static_cast<u8>(*right);
        if (left_char >= 'A' && left_char <= 'Z')
            left_char = static_cast<u8>(left_char + 0x20);
        if (right_char >= 'A' && right_char <= 'Z')
            right_char = static_cast<u8>(right_char + 0x20);
        if (left_char != right_char)
            return static_cast<i32>(left_char) -
                static_cast<i32>(right_char);
        if (left_char == 0)
            return 0;
        ++left;
        ++right;
    }
}

// Core of the XOR stream transform used for archive leaves and the trailing
// index blob. The key advances by key_step per transformed byte while each
// block is processed from its end backward in alternating byte lanes.
void TransformBytesInPlace(u8 *bytes, u32 byte_count, u8 key_step,
                           u32 block_size, u32 total_span)
{
    const u32 copy_bytes = byte_count < total_span ? byte_count : total_span;
    u8 *const original_prefix = static_cast<u8 *>(
        AllocateResourceBuffer(copy_bytes));
    if (original_prefix == 0)
        return;
    memcpy(original_prefix, bytes, copy_bytes);

    const i32 block_signed = static_cast<i32>(block_size);
    const i32 remainder = static_cast<i32>(byte_count % block_size);
    const i32 compare = ((block_signed + (block_signed >> 31 & 3)) >> 2)
        <= remainder;
    i32 remaining = static_cast<i32>(byte_count) -
        ((static_cast<i32>(byte_count) & 1) + ((compare - 1) & remainder));

    i32 block = block_signed;
    u32 span = total_span;
    u8 *cursor = bytes;
    u8 *prefix_cursor = original_prefix;
    u8 running_key = key_step;

    while (remaining > 0 && static_cast<i32>(span) > 0) {
        if (remaining < block)
            block = remaining;
        cursor += block;

        i32 half = (block + 1) / 2;
        u8 *back = cursor - 1;
        for (i32 index = 0; index < half; ++index) {
            *back = static_cast<u8>(*prefix_cursor ^ running_key);
            running_key = static_cast<u8>(running_key + key_step);
            back -= 2;
            ++prefix_cursor;
        }

        half = block / 2;
        back = cursor;
        for (i32 index = 0; index < half; ++index) {
            back[-2] = static_cast<u8>(*prefix_cursor ^ running_key);
            running_key = static_cast<u8>(running_key + key_step);
            back -= 2;
            ++prefix_cursor;
        }

        remaining -= block;
        span -= static_cast<u32>(block);
    }

    ReleaseResourceBuffer(original_prefix);
}

// TH10 0x0044b0d0. The XOR stream transform the archive applies to each leaf.
// A key record is selected by the low three bits of the byte sum of the entry
// name from the eight twelve-byte records at DAT_00474bd8 .. +0x60. The
// record's first byte is the per-byte key step and the following two words
// are the per-block size and the transform span (spans are exact multiples of
// their block sizes).
void TransformPackedBytesInPlace(const PackedArchiveEntry *entry, u8 *bytes,
                                 u32 byte_count)
{
    static const u8 kKeyStep[8] = {
        0x37, 0xe9, 0x51, 0x19, 0xcd, 0x34, 0x97, 0x37
    };
    static const u32 kBlockSize[8] = {
        0x00000040U, 0x00000040U, 0x00000080U, 0x00000400U,
        0x00000200U, 0x00000080U, 0x00000080U, 0x00000400U
    };
    static const u32 kTotalSpan[8] = {
        0x00002800U, 0x00003000U, 0x00003200U, 0x00007800U,
        0x00002800U, 0x00003200U, 0x00002800U, 0x00002000U
    };

    u8 name_sum = 0;
    for (const char *name = entry->owned_name; *name != 0; ++name)
        name_sum = static_cast<u8>(name_sum + static_cast<u8>(*name));
    const u32 key_index = name_sum & 7U;

    TransformBytesInPlace(bytes, byte_count, kKeyStep[key_index],
                          kBlockSize[key_index], kTotalSpan[key_index]);
}

// TH10 0x00435dc0. LZSS-style decoder over the packed bytes. Encoded tokens
// are MSB-first flag/literal/match groups read from a byte stream that feeds
// zero bits past its end. A set flag precedes an 8-bit literal; a clear flag
// precedes a 13-bit absolute history offset and a 4-bit match length whose
// stored code means count + 3. The 0x2000-byte history (DAT_0048f868) is a
// ring written with a cursor that starts at one. Output is written to the
// supplied destination or to a fresh resource allocation of output_size.
u8 *DecompressPackedBytes(const u8 *encoded, u32 encoded_size, u8 *output,
                          u32 output_size)
{
    if (output == 0) {
        output = static_cast<u8 *>(AllocateResourceBuffer(output_size));
        if (output == 0)
            return 0;
    }

    static u8 history[0x2000];

    const u8 *source_cursor = encoded;
    const u8 *const source_end = encoded + encoded_size;
    u8 current_byte = 0;
    u8 bit_mask = 0x80;
    u32 history_cursor = 1;
    u8 *output_cursor = output;

    for (;;) {
        // Consecutive literal tokens share one flag probe per byte.
        for (;;) {
            if (bit_mask == 0x80) {
                if (source_cursor < source_end)
                    current_byte = *source_cursor++;
                else
                    current_byte = 0;
            }
            const bool is_literal = (current_byte & bit_mask) != 0;
            bit_mask >>= 1;
            if (bit_mask == 0)
                bit_mask = 0x80;
            if (!is_literal)
                break;

            u32 literal = 0;
            for (u32 weight = 0x80; weight != 0; weight >>= 1) {
                if (bit_mask == 0x80) {
                    if (source_cursor < source_end)
                        current_byte = *source_cursor++;
                    else
                        current_byte = 0;
                }
                if ((current_byte & bit_mask) != 0)
                    literal |= weight;
                bit_mask >>= 1;
                if (bit_mask == 0)
                    bit_mask = 0x80;
            }
            *output_cursor++ = static_cast<u8>(literal);
            history[history_cursor] = static_cast<u8>(literal);
            history_cursor = (history_cursor + 1) & 0x1fffU;
        }

        u32 offset = 0;
        for (u32 weight = 0x1000; weight != 0; weight >>= 1) {
            if (bit_mask == 0x80) {
                if (source_cursor < source_end)
                    current_byte = *source_cursor++;
                else
                    current_byte = 0;
            }
            if ((current_byte & bit_mask) != 0)
                offset |= weight;
            bit_mask >>= 1;
            if (bit_mask == 0)
                bit_mask = 0x80;
        }
        if (offset == 0)
            break;

        u32 match_length = 0;
        for (u32 weight = 8; weight != 0; weight >>= 1) {
            if (bit_mask == 0x80) {
                if (source_cursor < source_end)
                    current_byte = *source_cursor++;
                else
                    current_byte = 0;
            }
            if ((current_byte & bit_mask) != 0)
                match_length |= weight;
            bit_mask >>= 1;
            if (bit_mask == 0)
                bit_mask = 0x80;
        }

        u32 index = 0;
        do {
            const u8 match_byte = history[(offset + index) & 0x1fffU];
            *output_cursor++ = match_byte;
            history[history_cursor] = match_byte;
            history_cursor = (history_cursor + 1) & 0x1fffU;
            ++index;
        } while (index <= match_length + 2);
    }
    return output;
}
// TH10 0x00434f70. Archive index decode: opens the reader for reading, reads
// and transforms the 16-byte header (key 0x37 / block 0x10 / span 0x10),
// validates the "THA1" signature, derives the entry count, index blob size
// and decoded index size from the shifted header words, then seeks to the
// trailing blob (file_size - index_size), reads and transforms it with
// key 0x9b / block 0x80 and LZSS-decompresses it into the decoded records.
bool DecodeArchiveIndex(ArchiveReader *reader, const char *path,
                        DecodedArchiveIndex *decoded)
{
    decoded->records = 0;
    decoded->count = 0;
    decoded->trailing_boundary = 0;

    OpenArchiveReaderIgnoredResult(reader, path);

    u8 header[16];
    if (!ReadArchiveReader(reader, header, sizeof(header)))
        return false;
    TransformBytesInPlace(header, sizeof(header), 0x37, 0x10, 0x10);

    const u32 *const header_words = reinterpret_cast<const u32 *>(header);
    if (header_words[0] != 0x31414854U) // "THA1"
        return false;

    const u32 entry_count = header_words[3] - 135792468U;
    const u32 index_size = header_words[2] - 987654321U;
    const u32 decoded_size = header_words[1] - 123456789U;

    const u32 file_size = GetArchiveReaderFileSize(reader);
    const u32 trailing_boundary = file_size - index_size;

    if (!SeekArchiveReader(reader, trailing_boundary, 0))
        return false;

    u8 *const stored = static_cast<u8 *>(
        AllocateResourceBuffer(index_size));
    if (stored == 0)
        return false;
    if (!ReadArchiveReader(reader, stored, index_size)) {
        ReleaseResourceBuffer(stored);
        return false;
    }

    TransformBytesInPlace(stored, index_size, 0x9b, 0x80, index_size);
    u8 *const records = DecompressPackedBytes(stored, index_size, 0,
                                              decoded_size);
    ReleaseResourceBuffer(stored);
    if (records == 0)
        return false;

    decoded->records = records;
    decoded->count = entry_count;
    decoded->trailing_boundary = trailing_boundary;
    return true;
}

// Direct-file resource helpers mirroring the nonzero-mode branch of the
// loader at 0x0044b360.
void *OpenDirectResourceFile(const char *path)
{
    void *const handle = CreateFileA(path, 0x80000000U, 1, 0, 3,
        0x8000080U, 0);
    if (handle == reinterpret_cast<void *>(static_cast<i32>(-1)))
        return 0;
    return handle;
}

u32 GetDirectResourceFileSize(void *handle)
{
    return GetFileSize(handle, 0);
}

void ReadDirectResourceFileIgnoredResult(void *handle, void *buffer,
                                         u32 bytes, u32 *actual_bytes)
{
    (void)ReadFile(handle, buffer, bytes, actual_bytes, 0);
}

void CloseDirectResourceFile(void *handle)
{
    (void)CloseHandle(handle);
}

u32 ReadU32(const u8 *bytes)
{
    return static_cast<u32>(bytes[0]) |
        (static_cast<u32>(bytes[1]) << 8) |
        (static_cast<u32>(bytes[2]) << 16) |
        (static_cast<u32>(bytes[3]) << 24);
}

PackedArchiveEntry *BuildArchiveEntries(const u8 *decoded_records, u32 count,
                                        u32 trailing_boundary)
{
    const u32 allocation_count = count + 1;
    void *const allocation = AllocateArchiveVector(
        sizeof(u32) + allocation_count * sizeof(PackedArchiveEntry));
    if (allocation == 0)
        return 0;

    *static_cast<u32 *>(allocation) = allocation_count;
    PackedArchiveEntry *const entries =
        reinterpret_cast<PackedArchiveEntry *>(static_cast<u8 *>(allocation) + sizeof(u32));
    memset(entries, 0, allocation_count * sizeof(PackedArchiveEntry));

    const u8 *record = decoded_records;
    for (u32 index = 0; index != count; ++index) {
        entries[index].owned_name = GameDuplicateString(
            reinterpret_cast<const char *>(record));
        const u32 name_bytes = static_cast<u32>(strlen(
            reinterpret_cast<const char *>(record))) + 1;
        record += (name_bytes + 3) & ~3U;
        entries[index].stored_offset = ReadU32(record);
        entries[index].declared_size = ReadU32(record + 4);
        entries[index].field_0c = ReadU32(record + 8);
        record += 12;
    }

    entries[count].stored_offset = trailing_boundary;
    entries[count].declared_size = 0;
    return entries;
}

const char *LeafName(const char *requested_name)
{
    const char *leaf = requested_name;
    const char *cursor = requested_name;
    while (*cursor != 0) {
        if (*cursor == '\\')
            leaf = cursor + 1;
        ++cursor;
    }

    cursor = leaf;
    while (*cursor != 0) {
        if (*cursor == '/')
            leaf = cursor + 1;
        ++cursor;
    }
    return leaf;
}

} // namespace

bool PackedArchive::OpenAndIndex(const char *archive_path)
{
    Clear();
    reader = CreateArchiveReader();
    if (reader == 0)
        return false;

    if (!ParseAndBuildIndex(archive_path)) {
        Clear();
        return false;
    }

    archive_name = GameDuplicateString(archive_path);
    if (archive_name == 0) {
        Clear();
        return false;
    }

    OpenArchiveReaderIgnoredResult(reader, archive_name);
    return true;
}

void PackedArchive::Clear()
{
    if (archive_name != 0) {
        ReleaseResourceBuffer(archive_name);
        archive_name = 0;
    }

    if (entries != 0) {
        const u32 allocation_count = reinterpret_cast<u32 *>(entries)[-1];
        for (u32 index = 0; index != allocation_count; ++index) {
            if (entries[index].owned_name != 0) {
                ReleaseResourceBuffer(entries[index].owned_name);
                entries[index].owned_name = 0;
            }
        }
        FreeArchiveVector(reinterpret_cast<u8 *>(entries) - sizeof(u32));
        entries = 0;
    }

    if (reader != 0) {
        DestroyArchiveReader(reader, true);
        reader = 0;
    }
    entry_count = 0;
}

bool PackedArchive::ParseAndBuildIndex(const char *archive_path)
{
    if (reader == 0)
        return false;

    DecodedArchiveIndex decoded;
    decoded.records = 0;
    decoded.count = 0;
    decoded.trailing_boundary = 0;
    if (!DecodeArchiveIndex(reader, archive_path, &decoded)) {
        DestroyArchiveReader(reader, true);
        reader = 0;
        return false;
    }

    entry_count = decoded.count;
    entries = BuildArchiveEntries(decoded.records, decoded.count,
                                  decoded.trailing_boundary);
    ReleaseResourceBuffer(decoded.records);
    if (entries != 0)
        return true;

    DestroyArchiveReader(reader, true);
    reader = 0;
    return false;
}

const PackedArchiveEntry *PackedArchive::FindLeafCaseInsensitive(
    const char *leaf_name) const
{
    for (u32 index = 0; index != entry_count; ++index) {
        if (CompareArchiveLeafCaseInsensitive(entries[index].owned_name,
                                              leaf_name) == 0)
            return &entries[index];
    }
    return 0;
}

u32 PackedArchive::StoredEnd(const PackedArchiveEntry *entry) const
{
    return reinterpret_cast<const u32 *>(entry)[5];
}

u8 *PackedArchive::LoadLeafLocked(const char *leaf_name, u8 *supplied_output)
{
    if (reader == 0)
        return 0;

    const PackedArchiveEntry *const entry = FindLeafCaseInsensitive(leaf_name);
    if (entry == 0)
        return 0;

    const u32 stored_size = StoredEnd(entry) - entry->stored_offset;
    u8 *read_buffer = supplied_output;
    if (stored_size != entry->declared_size || read_buffer == 0) {
        read_buffer = static_cast<u8 *>(AllocateResourceBuffer(stored_size));
        if (read_buffer == 0)
            return 0;
    }

    if (!SeekArchiveReader(reader, entry->stored_offset, 0) ||
        !ReadArchiveReader(reader, read_buffer, stored_size)) {
        ReleaseResourceBuffer(read_buffer);
        return 0;
    }

    TransformPackedBytesInPlace(entry, read_buffer, stored_size);
    if (stored_size == entry->declared_size)
        return read_buffer;

    u8 *const decoded = DecompressPackedBytes(read_buffer, stored_size,
                                               supplied_output,
                                               entry->declared_size);
    ReleaseResourceBuffer(read_buffer);
    return decoded;
}

ResourceLoader::ResourceLoader(PackedArchive *archive_state)
    : archive(archive_state)
{
}

u8 *ResourceLoader::Load(const char *requested_name, u32 *optional_out_size,
                         i32 source_mode)
{
    EnterCriticalSection(&g_ResourceLoaderLock);
    ++g_ResourceLoaderNesting;

    u8 *result = 0;
    if (source_mode == 0) {
        const PackedArchiveEntry *const entry = archive->FindLeafCaseInsensitive(
            LeafName(requested_name));
        if (entry != 0) {
            if (optional_out_size != 0)
                *optional_out_size = entry->declared_size;
            if (entry->declared_size != 0) {
                u8 *const output = static_cast<u8 *>(AllocateResourceBuffer(
                    entry->declared_size));
                if (output != 0)
                    result = archive->LoadLeafLocked(LeafName(requested_name), output);
            }
        } else if (optional_out_size != 0) {
            *optional_out_size = 0;
        }
    } else {
        void *const handle = OpenDirectResourceFile(requested_name);
        if (handle != 0) {
            const u32 bytes = GetDirectResourceFileSize(handle);
            result = static_cast<u8 *>(AllocateResourceBuffer(bytes));
            if (result != 0) {
                u32 actual_bytes = 0;
                ReadDirectResourceFileIgnoredResult(handle, result, bytes,
                                                    &actual_bytes);
                if (optional_out_size != 0)
                    *optional_out_size = actual_bytes;
            }
            CloseDirectResourceFile(handle);
        }
    }

    LeaveCriticalSection(&g_ResourceLoaderLock);
    --g_ResourceLoaderNesting;
    return result;
}

u8 *LoadPackedResource(const char *requested_name, u32 *optional_out_size,
                       i32 source_mode)
{
    ResourceLoader loader(&g_PackedArchive);
    return loader.Load(requested_name, optional_out_size, source_mode);
}

} // namespace th10
