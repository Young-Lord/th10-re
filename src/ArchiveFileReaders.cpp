// TH10 0x00434c10-0x004366c0 cluster: the three polymorphic leaf readers
// behind the ArchiveReader interface plus their path/lookup helpers.
//
//   vtable PTR_FUN_0046f230 (file handle): slot0 open 0x00435370,
//     +0x04 close 0x00435460, +0x08 read 0x00435490, +0x0c write 0x004354d0,
//     +0x10 tell 0x00435510, +0x14 size 0x00435530, +0x18 seek 0x00435550
//   vtable PTR_FUN_0046f308 (memory) and 0046f328 (resource) share the same
//     body shape: {vtable, size, cursor, base allocation}.
//
// The leaf loaders 0x00435800/0x004358e0 resolve "dir/file" leaves through
// the "<prefix>.dat" archive table first and fall back to raw files.
#include <stdlib.h>
#include <string.h>

#include "ArchiveFileReaders.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

extern "C" void *TH10_STDCALL FindResourceA(void *module, const char *name,
                                            const char *type);
extern "C" void *TH10_STDCALL LoadResource(void *module, void *res_info);
extern "C" void *TH10_STDCALL LockResource(void *res_data);
extern "C" i32 TH10_STDCALL FreeResource(void *res_data);
extern "C" u32 TH10_STDCALL SizeofResource(void *module, void *res_info);
extern "C" void *TH10_STDCALL CreateFileA(const char *path, u32 access,
    u32 share, void *security_attributes, u32 creation, u32 flags,
    void *template_file);
extern "C" i32 TH10_STDCALL CloseHandle(void *handle);
extern "C" i32 TH10_STDCALL ReadFile(void *handle, void *buffer,
                                     u32 byte_count, u32 *actual_bytes,
                                     void *overlapped);
extern "C" u32 TH10_STDCALL SetFilePointer(void *handle, i32 distance_to_move,
                                           i32 *distance_high,
                                           u32 move_method);
extern "C" u32 TH10_STDCALL GetFileSize(void *handle, u32 *high_size);
extern "C" i32 TH10_STDCALL DeleteFileA(const char *path);
extern "C" u32 TH10_STDCALL GetModuleFileNameA(void *module, char *path,
                                               u32 size);
extern "C" i32 TH10_STDCALL CreateProcessA(const char *application_name,
    char *command_line, void *process_attributes, void *thread_attributes,
    i32 inherit_handles, u32 creation_flags, void *environment,
    const char *current_directory, void *startup_info,
    void *process_information);
extern "C" i32 TH10_STDCALL GetExitCodeProcess(void *process,
                                               u32 *exit_code);

extern void *AllocateArchiveVector(u32 bytes); // TH10 0x00452493
extern void FreeArchiveVector(void *pointer);  // TH10 0x004524a1
extern void ReleaseResourceBuffer(void *pointer); // TH10 0x00452422

// TH10 0x004525ff: `eh vector destructor iterator' (MSVCP60).
extern "C" void TH10_STDCALL EhVectorDestructorIterator(
    void *array, u32 element_size, u32 count, void *element_destructor);

// Mode-string global the leaf loaders pass to the file reader (points at
// the default "r" mode text).
extern char *g_FileOpenModeDefault; // TH10 DAT_00474908

const u32 k_os_reader_vtable = 0x0046f230U;
const u32 k_reader_base_vtable = 0x0046f210U;
const u32 k_memory_reader_vtable = 0x0046f308U;
const u32 k_resource_reader_vtable = 0x0046f328U;

// File-open flags the native reader uses.
const u32 k_generic_read = 0x80000000U;
const u32 k_generic_write = 0x40000000U;
const u32 k_file_attribute_normal = 0x00000080U;
const u32 k_file_flag_no_buffering = 0x80000000U;
const u32 k_create_new = 1U;
const u32 k_create_always = 2U;
const u32 k_open_existing = 3U;
const u32 k_open_always = 4U;
const u32 k_file_begin = 0U;
const u32 k_file_end = 2U;
const u32 k_share_read = 1U;
const u32 k_create_no_window = 0x20U;
const u32 k_still_active = 259U;

const char *const k_default_open_mode = "r"; // TH10 0x00474908 target

// Raw 12-byte reader mirror used by the loaders.
struct RawReader {
    const void *vtable;
    void *handle;
    u32 access;
};

void *ReadPointerAt(const void *base, u32 offset)
{
    void *const *slot = reinterpret_cast<void *const *>(
        static_cast<const u8 *>(base) + offset);
    return *slot;
}

void WritePointerAt(void *base, u32 offset, void *value)
{
    void **slot = reinterpret_cast<void **>(
        static_cast<u8 *>(base) + offset);
    *slot = value;
}

} // namespace

// TH10 0x00434c10. Native thiscall.
void ZeroFillReaderSlotEcxAbi(void *slot)
{
    u32 *words = static_cast<u32 *>(slot);
    words[0] = 0;
    words[1] = 0;
    words[2] = 0;
    words[3] = 0;
}

// TH10 0x00434c30. EAX = path, ECX = context (native usercall).
bool OpenArchiveReaderOnPathEaxCcxAbi(const char *path, void *context)
{
    ReleaseVersionDataSlotEsiAbi(context);

    RawReader *reader = static_cast<RawReader *>(
        AllocateArchiveVector(0xcU));
    if (reader != 0) {
        reader->vtable = reinterpret_cast<const void *>(k_os_reader_vtable);
        reader->handle = reinterpret_cast<void *>(-1);
        reader->access = 0;
    }
    WritePointerAt(context, 0x0cU, reader);
    if (reader == 0)
        return false;

    // Native 0x00434f70: EAX = reader, stack = path (boolean result).
    extern bool DecodeArchiveHeaderEaxStackAbi(void *reader,
                                               const char *path);
    if (!DecodeArchiveHeaderEaxStackAbi(reader, path)) {
        ReleaseVersionDataSlotEsiAbi(context);
        return false;
    }

    // TH10 0x00435220 GameDuplicateString (native EDI = source).
    extern char *GameDuplicateString(const char *text);
    char *copy = GameDuplicateString(path);
    WritePointerAt(context, 0x08U, copy);
    if (copy == 0) {
        ReleaseVersionDataSlotEsiAbi(context);
        return false;
    }

    // vtable slot 0 (open): thiscall(reader, path copy, mode string).
    const char *mode = g_FileOpenModeDefault;
    OpenOsFileReaderEcxStackAbi(reader, copy, mode);
    return true;
}

// TH10 0x00434d10. Native ESI = context.
void ReleaseVersionDataSlotEsiAbi(void *context)
{
    u8 *slot = static_cast<u8 *>(context);
    void *path_copy = ReadPointerAt(slot, 0x08U);
    if (path_copy != 0) {
        free(path_copy);
        WritePointerAt(slot, 0x08U, 0);
    }
    void *entries = ReadPointerAt(slot, 0x00U);
    WritePointerAt(slot, 0x08U, 0);
    if (entries != 0) {
        const u32 count = *reinterpret_cast<const u32 *>(
            static_cast<u8 *>(entries) - 4U);
        EhVectorDestructorIterator(entries, 0x10U, count,
            reinterpret_cast<void *>(&ReleaseArchiveEntryStringEcxAbi));
        free(static_cast<u8 *>(entries) - 4U);
    }
    void *reader = ReadPointerAt(slot, 0x0cU);
    WritePointerAt(slot, 0x00U, 0);
    if (reader != 0) {
        // vtable +0x1c (slot 7): release with the destructive flag set.
        typedef void (TH10_STDCALL *ReleaseFn)(void *, i32);
        void **vtable = *static_cast<void ***>(reader);
        reinterpret_cast<ReleaseFn>(vtable[7])(reader, 1);
    }
    WritePointerAt(slot, 0x0cU, 0);
    WritePointerAt(slot, 0x04U, 0);
}

// TH10 0x00434d70. Native ESI = allocation, stack = mode flags.
void *ReleaseArchiveEntryArrayEsiAbi(void *allocation, u8 mode_flags)
{
    if ((mode_flags & 2U) != 0U) {
        const u32 count = *reinterpret_cast<const u32 *>(
            static_cast<u8 *>(allocation) - 4U);
        EhVectorDestructorIterator(allocation, 0x10U, count,
            reinterpret_cast<void *>(&ReleaseArchiveEntryStringEcxAbi));
        if ((mode_flags & 1U) != 0U)
            FreeArchiveVector(static_cast<u8 *>(allocation) - 4U);
        return static_cast<u8 *>(allocation) - 4U;
    }

    char **first = static_cast<char **>(allocation);
    if (*first != 0) {
        free(*first);
        *first = 0;
    }
    if ((mode_flags & 1U) != 0U)
        FreeArchiveVector(allocation);
    return allocation;
}

// TH10 0x00434ef0. Native EAX = list, EBX = name.
u32 FindArchiveLeafEntryInsensitiveEaxEbxAbi(const u32 *list, const char *name)
{
    const u8 *entry = reinterpret_cast<const u8 *>(list[0]);
    if (entry == 0)
        return 0;
    i32 remaining = static_cast<i32>(list[1]);
    if (remaining <= 0)
        return 0;
    while (strcmp(name, *reinterpret_cast<const char *const *>(entry)) != 0) {
        --remaining;
        entry += 0x10U;
        if (remaining <= 0)
            return 0;
    }
    return *reinterpret_cast<const u32 *>(entry + 8U);
}

// TH10 0x00435250. Native thiscall.
void ReleaseArchiveEntryStringEcxAbi(char **slot)
{
    if (*slot != 0) {
        free(*slot);
        *slot = 0;
    }
}

// TH10 0x00435270. Native EAX = cursor.
u32 AdvanceArchiveIndexCursorEaxAbi(u32 *cursor)
{
    const u32 position = *cursor + 4U;
    *cursor = position;
    return *reinterpret_cast<const u32 *>(position);
}

// TH10 0x00435300. Native EAX = storage.
void ConstructOsFileReaderEaxAbi(void *storage)
{
    RawReader *reader = static_cast<RawReader *>(storage);
    reader->vtable = reinterpret_cast<const void *>(k_os_reader_vtable);
    reader->handle = reinterpret_cast<void *>(-1);
    reader->access = 0;
}

// TH10 0x00435320. Native thiscall (mode on stack).
void *DestroyOsFileReaderEcxAbi(void *object, u8 mode_flags)
{
    CloseOsFileReaderEcxAbi(object);
    if ((mode_flags & 1U) != 0U)
        FreeArchiveVector(object);
    return object;
}

// TH10 0x00435340. Native thiscall.
void CloseOsFileReaderEcxAbi(void *object)
{
    RawReader *reader = static_cast<RawReader *>(object);
    void *handle = reader->handle;
    reader->vtable = reinterpret_cast<const void *>(k_os_reader_vtable);
    if (handle != reinterpret_cast<void *>(-1)) {
        (void)CloseHandle(handle);
        reader->handle = reinterpret_cast<void *>(-1);
        reader->access = 0;
    }
    reader->vtable = reinterpret_cast<const void *>(k_reader_base_vtable);
}

// TH10 0x00435370. Native thiscall: stack = path, mode string.
bool OpenOsFileReaderEcxStackAbi(void *reader_storage, const char *path,
                                 const char *mode)
{
    RawReader *reader = static_cast<RawReader *>(reader_storage);
    u32 creation = 0;
    u32 append = 0;

    CloseOsFileReaderEcxAbi(reader);

    if (*mode == 0)
        return false;
    for (const char *scan = mode; *scan != 0; ++scan) {
        if (*scan == 'r') {
            reader->access = k_generic_read;
            creation = k_open_existing;
            break;
        }
        if (*scan == 'w') {
            (void)DeleteFileA(path);
            creation = k_create_always;
            reader->access = k_generic_write;
            break;
        }
        if (*scan == 'a') {
            append = 1;
            creation = k_open_always;
            reader->access = k_generic_write;
            break;
        }
    }
    if (*mode == 0)
        return false;

    char resolved[260];
    ResolveLeafPathEcxStackAbi(resolved, path);
    reader->handle = CreateFileA(resolved, reader->access, k_share_read, 0,
        creation, k_file_flag_no_buffering | k_file_attribute_normal, 0);
    if (reader->handle == reinterpret_cast<void *>(-1))
        return false;
    if (append != 0)
        (void)SetFilePointer(reader->handle, 0, 0, k_file_end);
    return true;
}

// TH10 0x00435490. Native thiscall: stack = buffer, byte count.
u32 ReadOsFileReaderEcxAbi(void *reader_storage, void *buffer,
                           u32 byte_count)
{
    RawReader *reader = static_cast<RawReader *>(reader_storage);
    if (reader->access != k_generic_read)
        return 0;
    u32 actual_bytes = 0;
    (void)ReadFile(reader->handle, buffer, byte_count, &actual_bytes, 0);
    return actual_bytes;
}

// TH10 0x00435550. Native thiscall: stack = distance, origin.
bool SeekOsFileReaderEcxAbi(void *reader_storage, i32 distance, u32 origin)
{
    RawReader *reader = static_cast<RawReader *>(reader_storage);
    if (reader->handle == reinterpret_cast<void *>(-1))
        return false;
    (void)SetFilePointer(reader->handle, distance, 0, origin);
    return true;
}

// TH10 0x00435610. Native ECX = destination, stack = leaf.
void ResolveLeafPathEcxStackAbi(char *destination, const char *leaf)
{
    if (strchr(leaf, ':') != 0) {
        // Shift the leaf text back over the destination prefix.
        char *out = destination;
        const char *in = leaf;
        char moved;
        do {
            moved = *in;
            *out = moved;
            ++out;
            ++in;
        } while (moved != 0);
        return;
    }

    (void)GetModuleFileNameA(0, destination, 0x104U);
    char *separator = strrchr(destination, '\\');
    if (separator == 0)
        *destination = 0;
    separator[1] = 0;
    const u32 leaf_bytes = static_cast<u32>(strlen(leaf)) + 1U;
    memcpy(destination + strlen(destination), leaf, leaf_bytes);
}

// TH10 0x004356a0. Native thiscall (mode on stack).
void *ReleaseReaderBaseObjectEcxAbi(void *object, u8 mode_flags)
{
    *static_cast<const void **>(object) =
        reinterpret_cast<const void *>(k_reader_base_vtable);
    if ((mode_flags & 1U) != 0U)
        FreeArchiveVector(object);
    return object;
}

// TH10 0x004356f0. Native ECX = leaf path.
const u32 *FindArchiveRecordByPathEcxAbi(const char *path)
{
    const char *prefix_end = strchr(path, '/');
    const char *name_start = (prefix_end != 0) ? prefix_end + 1 : path;
    const i32 dat_index = static_cast<i32>(name_start - path) - 1;

    char dat_name[260];
    strcpy(dat_name, path);
    if (dat_index >= 0)
        strcpy(dat_name + dat_index, ".dat");

    extern u32 g_LoadedArchiveCount;  // TH10 DAT_00477850
    extern u32 g_LoadedArchiveTable;  // TH10 DAT_004923b0 (stride 0x10)
    if (static_cast<i32>(g_LoadedArchiveCount) <= 0)
        return 0;
    const u8 *entry = reinterpret_cast<const u8 *>(&g_LoadedArchiveTable);
    for (u32 index = 0; index < g_LoadedArchiveCount; ++index) {
        if (strcmp(*reinterpret_cast<const char *const *>(entry + 8U),
                   dat_name) == 0)
            return reinterpret_cast<const u32 *>(entry);
        entry += 0x10U;
    }
    return 0;
}

// TH10 0x00435800. Native ECX = path, stack = out size.
void *ReadLeafArchiveOrFileEcxStackAbi(const char *path, u32 *out_size)
{
    const u32 *record = FindArchiveRecordByPathEcxAbi(path);
    if (record != 0) {
        const char *slash = strchr(path, '/');
        const char *leaf = (slash != 0) ? slash + 1 : path;
        extern void *DecodePackedSectionEcxStackAbi(const char *section_name,
                                                    const void *section_list,
                                                    void *out_buffer);
        return DecodePackedSectionEcxStackAbi(leaf, record, out_size);
    }

    RawReader reader;
    reader.vtable = reinterpret_cast<const void *>(k_os_reader_vtable);
    reader.handle = reinterpret_cast<void *>(-1);
    reader.access = 0;
    OpenOsFileReaderEcxStackAbi(&reader, path, k_default_open_mode);
    u32 size = 0;
    if (reader.handle != reinterpret_cast<void *>(-1))
        size = GetFileSize(reader.handle, 0);
    extern u8 *ReadWholeArchiveReaderFile(void *reader, u32 max_bytes);
    u8 *contents = ReadWholeArchiveReaderFile(&reader, size);
    reader.vtable = reinterpret_cast<const void *>(k_os_reader_vtable);
    if (reader.handle != reinterpret_cast<void *>(-1))
        (void)CloseHandle(reader.handle);
    return contents;
}

// TH10 0x004358e0. Native ECX = path.
u32 GetLeafSizeArchiveOrFileEcxAbi(const char *path)
{
    const u32 *record = FindArchiveRecordByPathEcxAbi(path);
    if (record != 0) {
        const char *slash = strchr(path, '/');
        const char *leaf = (slash != 0) ? slash + 1 : path;
        return FindArchiveLeafEntryInsensitiveEaxEbxAbi(
            reinterpret_cast<const u32 *>(record), leaf);
    }

    RawReader reader;
    reader.vtable = reinterpret_cast<const void *>(k_os_reader_vtable);
    reader.handle = reinterpret_cast<void *>(-1);
    reader.access = 0;
    u32 size = 0;
    OpenOsFileReaderEcxStackAbi(&reader, path, k_default_open_mode);
    if (reader.handle != reinterpret_cast<void *>(-1)) {
        size = GetFileSize(reader.handle, 0);
        (void)CloseHandle(reader.handle);
    }
    return size;
}

// TH10 0x00435fa0. Native EAX = slot index.
void MarkArchiveSlotPendingEaxAbi(u32 slot_index)
{
    extern u32 *g_ArchiveSlotBase; // TH10 DAT_00477858 (3 words per slot)
    extern u32 *g_ArchiveSlotIndex; // TH10 DAT_0048f860
    g_ArchiveSlotIndex = reinterpret_cast<u32 *>(slot_index);
    u32 *const slot = g_ArchiveSlotBase + 3U * slot_index;
    slot[0] = 0x2000U;
    slot[1] = 0;
    slot[2] = 0;
}

// TH10 0x00436380. Native EAX = storage.
void ConstructMemoryReaderEaxAbi(void *storage)
{
    u32 *reader = static_cast<u32 *>(storage);
    reader[0] = k_memory_reader_vtable;
    reader[1] = 0;
    reader[2] = 0;
    reader[3] = 0;
}

// TH10 0x004363a0. Native thiscall (mode on stack).
void *DestroyMemoryReaderEcxAbi(void *object, u8 mode_flags)
{
    CloseMemoryReaderEcxAbi(object);
    if ((mode_flags & 1U) != 0U)
        FreeArchiveVector(object);
    return object;
}

// TH10 0x004363c0. Native thiscall.
void CloseMemoryReaderEcxAbi(void *object)
{
    u32 *reader = static_cast<u32 *>(object);
    void *base = reinterpret_cast<void *>(reader[3]);
    reader[0] = k_memory_reader_vtable;
    if (base != 0) {
        free(base);
        reader[3] = 0;
    }
    reader[3] = 0;
    reader[1] = 0;
    reader[2] = 0;
    reader[0] = k_reader_base_vtable;
}

// TH10 0x00436400. Native ECX = reader, stack = leaf path (+ unused word).
bool OpenMemoryReaderEcxStackAbi(void *reader_storage, const char *path)
{
    u32 *reader = static_cast<u32 *>(reader_storage);
    reader[3] = reinterpret_cast<u32>(
        ReadLeafArchiveOrFileEcxStackAbi(path, 0));
    reader[1] = GetLeafSizeArchiveOrFileEcxAbi(path);
    reader[2] = reader[3];
    return reader[3] != 0;
}

// TH10 0x00436430. Native thiscall.
void ReleaseMemoryReaderBufferEcxAbi(void *object)
{
    u32 *reader = static_cast<u32 *>(object);
    if (reader[3] != 0) {
        free(reinterpret_cast<void *>(reader[3]));
        reader[3] = 0;
    }
    reader[3] = 0;
    reader[1] = 0;
    reader[2] = 0;
}

// TH10 0x00436460. Native thiscall: stack = buffer, byte count.
u32 ReadMemoryReaderEcxAbi(void *reader_storage, void *buffer,
                           u32 byte_count)
{
    u32 *reader = static_cast<u32 *>(reader_storage);
    const u8 *cursor = reinterpret_cast<const u8 *>(reader[2]);
    const u32 remaining = reader[1] + reader[3] -
        static_cast<u32>(reinterpret_cast<const u32>(cursor));
    if (remaining < byte_count) {
        if (remaining == 0)
            return 0;
        memcpy(buffer, cursor, remaining);
        reader[2] += remaining;
        return remaining;
    }
    memcpy(buffer, cursor, byte_count);
    reader[2] += byte_count;
    return byte_count;
}

// TH10 0x004364f0. Native thiscall: stack = offset, origin.
bool SeekMemoryReaderEcxAbi(void *reader_storage, i32 offset, u32 origin)
{
    u32 *reader = static_cast<u32 *>(reader_storage);
    if (origin == 0) {
        if (offset >= 0 &&
            static_cast<u32>(offset) < reader[1]) {
            reader[2] = static_cast<u32>(offset) + reader[3];
            return true;
        }
        return false;
    }
    if (origin == 1) {
        if (static_cast<i32>(reader[1] + reader[3] - reader[2]) > offset) {
            reader[2] = static_cast<u32>(offset) + reader[2];
            return true;
        }
        return false;
    }
    if (origin == 2) {
        if (offset > 0 ||
            (offset == 0 &&
             static_cast<u32>(-offset) < reader[1])) {
            reader[2] = static_cast<u32>(offset) + reader[1] + reader[3];
            return true;
        }
    }
    return false;
}

// TH10 0x00436570. Native EDX = command line; stack = app name, wait flag.
i32 SpawnConsoleProcessEdxStackAbi(const char *command_line,
                                   const char *application_name,
                                   u32 wait_for_exit)
{
#pragma pack(push, 1)
    struct StartupInfo {
        u32 cb;
        char *reserved;
        char *desktop;
        char *title;
        u32 x;
        u32 y;
        u32 x_size;
        u32 y_size;
        u32 x_count_chars;
        u32 y_count_chars;
        u32 fill_attribute;
        u32 flags;
        u16 show_window;
        u16 cb_reserved2;
        u8 *lp_reserved2;
    };
    struct ProcessInformation {
        void *process;
        void *thread;
        u32 process_id;
        u32 thread_id;
    };
#pragma pack(pop)

    StartupInfo startup_info;
    startup_info.x = 0x80000000U;
    startup_info.y = 0x80000000U;
    startup_info.x_size = 0x80000000U;
    startup_info.y_size = 0x80000000U;
    startup_info.cb = 68;
    startup_info.reserved = 0;
    startup_info.desktop = 0;
    startup_info.title = 0;
    startup_info.x_count_chars = 80;
    startup_info.y_count_chars = 25;
    startup_info.fill_attribute = 0;
    startup_info.flags = 0;
    startup_info.show_window = 10;
    startup_info.cb_reserved2 = 0;
    startup_info.lp_reserved2 = 0;

    ProcessInformation process_information;
    if (CreateProcessA(application_name, const_cast<char *>(command_line),
                       0, 0, 0, k_create_no_window, 0, 0, &startup_info,
                       &process_information) == 0)
        return -1;
    if (wait_for_exit == 0)
        return 0;
    u32 exit_code = k_still_active;
    while (exit_code == k_still_active)
        (void)GetExitCodeProcess(process_information.process, &exit_code);
    return static_cast<i32>(exit_code);
}

// TH10 0x00436640. Native EAX = storage.
void ConstructResourceReaderEaxAbi(void *storage)
{
    u32 *reader = static_cast<u32 *>(storage);
    reader[0] = k_resource_reader_vtable;
    reader[1] = 0;
    reader[2] = 0;
    reader[3] = 0;
}

// TH10 0x00436660. Native thiscall (mode on stack).
void *DestroyResourceReaderEcxAbi(void *object, u8 mode_flags)
{
    CloseResourceReaderEcxAbi(object);
    if ((mode_flags & 1U) != 0U)
        FreeArchiveVector(object);
    return object;
}

// TH10 0x00436680. Native thiscall.
void CloseResourceReaderEcxAbi(void *object)
{
    u32 *reader = static_cast<u32 *>(object);
    void *base = reinterpret_cast<void *>(reader[3]);
    reader[0] = k_memory_reader_vtable;
    if (base != 0) {
        free(base);
        reader[3] = 0;
    }
    reader[3] = 0;
    reader[1] = 0;
    reader[2] = 0;
    reader[0] = k_reader_base_vtable;
}

// TH10 0x004366c0. Native ECX = reader, stack = resource name (+ unused).
bool OpenResourceReaderEcxStackAbi(void *reader_storage,
                                   const char *resource_name)
{
    u32 *reader = static_cast<u32 *>(reader_storage);
    CloseMemoryReaderEcxAbi(reader_storage);

    void *res_info = FindResourceA(0, resource_name,
        reinterpret_cast<const char *>(10));
    if (res_info == 0) {
        CloseMemoryReaderEcxAbi(reader_storage);
        return true;
    }
    void *res_data = LoadResource(0, res_info);
    if (res_data == 0) {
        CloseMemoryReaderEcxAbi(reader_storage);
        return true;
    }
    const void *locked = LockResource(res_data);
    if (locked == 0) {
        (void)FreeResource(0);
        CloseMemoryReaderEcxAbi(reader_storage);
        return true;
    }
    const u32 size = SizeofResource(0, res_info);
    reader[1] = size;
    void *copy = malloc(size);
    reader[3] = reinterpret_cast<u32>(copy);
    if (copy != 0) {
        memcpy(copy, locked, reader[1]);
        reader[2] = reader[3];
        (void)FreeResource(res_data);
        return true;
    }
    CloseMemoryReaderEcxAbi(reader_storage);
    return true;
}

} // namespace th10
