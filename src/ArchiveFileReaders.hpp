#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Reader objects are the 0xc/0x10-byte polymorphic sources behind
// ArchiveReader (vtables PTR_FUN_0046f230 file-handle, 0046f308 memory,
// 0046f328 embedded resource). Layouts:
//   file-handle: +0 vtable, +4 HANDLE, +8 access flags
//   memory:      +0 vtable, +4 size, +8 cursor, +0c base allocation
//   resource:    +0 vtable, +4 size, +8 cursor, +0c base allocation

// TH10 0x00434c10. Zero-fills a four-dword slot (vtable constructor entry).
void ZeroFillReaderSlotEcxAbi(void *slot);

// TH10 0x00434c30. EAX = path, ECX = context. Resets the context, allocates
// the file-handle reader, decodes the archive index for the path, duplicates
// the path into the context and opens the reader through vtable slot 0.
bool OpenArchiveReaderOnPathEaxCcxAbi(const char *path, void *context);

// TH10 0x00434d10. ESI = context. Frees the context path copy (+0x08), runs
// the eh-vector destructor over the entry array at +0x00 (count at base-4,
// stride 0x10, per-entry dtor 0x00435250), releases the reader at +0x0c
// through vtable +0x1c and clears the slot.
void ReleaseVersionDataSlotEsiAbi(void *context);

// TH10 0x00434d70. ESI = allocation, stack = mode flags. Bit 1 selects the
// eh-vector path (destruct entries, drop the 4-byte header); otherwise the
// first entry's string is freed. Bit 0 additionally frees the allocation.
void *ReleaseArchiveEntryArrayEsiAbi(void *allocation, u8 mode_flags);

// TH10 0x00434ef0. EAX = {base, count} list, EBX = name. Case-insensitive
// scan over 0x10-byte entries; returns entry+0x08 or 0.
u32 FindArchiveLeafEntryInsensitiveEaxEbxAbi(const u32 *list, const char *name);

// TH10 0x00435250. Frees the string pointed to by the slot and clears it.
void ReleaseArchiveEntryStringEcxAbi(char **slot);

// TH10 0x00435270. EAX = cursor slot. Advances by one dword and returns the
// dword at the new position.
u32 AdvanceArchiveIndexCursorEaxAbi(u32 *cursor);

// TH10 0x00435300. EAX = storage. File-handle reader constructor.
void ConstructOsFileReaderEaxAbi(void *storage);

// TH10 0x00435320. Destructor wrapper: releases the handle, then frees the
// object when bit 0 of the mode is set.
void *DestroyOsFileReaderEcxAbi(void *object, u8 mode_flags);

// TH10 0x00435340. Closes the OS handle when open (not -1) and downgrades
// the vtable to the base 0046f210.
void CloseOsFileReaderEcxAbi(void *object);

// TH10 0x00435370. ECX = reader, stack = path, mode string. Parses the mode
// characters ('r'/'w'/'a'), optionally deletes the target for 'w', resolves
// the path and CreateFileA's it (FILE_FLAG_NO_BUFFERING|NORMAL attributes,
// share read). 'a' seeks to end. Returns 1 on success.
bool OpenOsFileReaderEcxStackAbi(void *reader, const char *path,
                                 const char *mode);

// TH10 0x00435490. ECX = reader, stack = buffer, byte count. Reads only when
// the access flags equal GENERIC_READ (0x80000000); returns actual bytes.
u32 ReadOsFileReaderEcxAbi(void *reader, void *buffer, u32 byte_count);

// TH10 0x00435550. ECX = reader, stack = distance, origin. SetFilePointer
// wrapper; the native result is ignored and 1 is returned when open.
bool SeekOsFileReaderEcxAbi(void *reader, i32 distance, u32 origin);

// TH10 0x00435610. ECX = destination buffer (>= 0x104 bytes), stack = leaf
// name. Absolute paths (containing ':') are moved back over the destination;
// otherwise the executable directory is prepended.
void ResolveLeafPathEcxStackAbi(char *destination, const char *leaf);

// TH10 0x004356a0. Base-object destructor: restores vtable 0046f210 and
// frees the object when bit 0 of the mode is set.
void *ReleaseReaderBaseObjectEcxAbi(void *object, u8 mode_flags);

// TH10 0x004356f0. ECX = leaf path. Builds "<prefix>.dat" from the text
// before the first '/' and scans the loaded archive table (DAT_004923b0,
// count DAT_00477850, stride 0x10, name at +0x08). Returns the record or 0.
const u32 *FindArchiveRecordByPathEcxAbi(const char *path);

// TH10 0x00435800. ECX = leaf path, stack = out buffer. Archive records are
// decoded through 0x00434dd0; otherwise the raw file is read whole.
void *ReadLeafArchiveOrFileEcxStackAbi(const char *path, u32 *out_size);

// TH10 0x004358e0. ECX = leaf path. Same lookup; returns the payload size.
u32 GetLeafSizeArchiveOrFileEcxAbi(const char *path);

// TH10 0x00435fa0. EAX = slot index. Stores the index in DAT_0048f860 and
// resets the three request words of the slot (0x2000/0/0).
void MarkArchiveSlotPendingEaxAbi(u32 slot_index);

// TH10 0x00436380. EAX = storage. Memory reader constructor.
void ConstructMemoryReaderEaxAbi(void *storage);

// TH10 0x004363a0. Memory reader destructor wrapper.
void *DestroyMemoryReaderEcxAbi(void *object, u8 mode_flags);

// TH10 0x004363c0. Frees the base allocation and clears the reader fields.
void CloseMemoryReaderEcxAbi(void *object);

// TH10 0x00436400. ECX = reader, stack = leaf path (plus one unused word).
// Loads the leaf, storing size/cursor/base. Returns base != 0.
bool OpenMemoryReaderEcxStackAbi(void *reader, const char *path);

// TH10 0x00436430. Frees the base allocation at +0x0c and clears the fields.
void ReleaseMemoryReaderBufferEcxAbi(void *object);

// TH10 0x00436460. ECX = reader, stack = buffer, byte count. Bounded copy
// from the cursor; partial reads return what remains.
u32 ReadMemoryReaderEcxAbi(void *reader, void *buffer, u32 byte_count);

// TH10 0x004364f0. ECX = reader, stack = offset, origin (0 set/1 cur/2 end).
bool SeekMemoryReaderEcxAbi(void *reader, i32 offset, u32 origin);

// TH10 0x00436570. EDX = command line, stack = app name, wait flag. Launches
// CREATE_NO_WINDOW; when wait is set, polls GetExitCodeProcess until it
// leaves STILL_ACTIVE. Returns -1 on launch failure.
i32 SpawnConsoleProcessEdxStackAbi(const char *command_line,
                                   const char *application_name,
                                   u32 wait_for_exit);

// TH10 0x00436640. EAX = storage. Embedded-resource reader constructor.
void ConstructResourceReaderEaxAbi(void *storage);

// TH10 0x00436660. Resource reader destructor wrapper.
void *DestroyResourceReaderEcxAbi(void *object, u8 mode_flags);

// TH10 0x00436680. Frees the resource copy and clears the reader fields.
void CloseResourceReaderEcxAbi(void *object);

// TH10 0x004366c0. ECX = reader, stack = resource name (plus one unused
// word). Loads RT_RCDATA (type 10), copies it into a heap buffer and sets
// size/cursor/base. Always returns 1.
bool OpenResourceReaderEcxStackAbi(void *reader, const char *resource_name);

} // namespace th10
