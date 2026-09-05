#pragma once

#include <stddef.h>

#include "Th10Platform.hpp"

namespace th10 {

struct ArchiveReader;

struct PackedArchiveEntry {
    char *owned_name;
    u32 stored_offset;
    u32 declared_size;
    u32 field_0c;
};

class PackedArchive {
public:
    bool OpenAndIndex(const char *archive_path);
    void Clear();

    // The shared ResourceLoader critical section must remain held throughout.
    u8 *LoadLeafLocked(const char *leaf_name, u8 *supplied_output);

    PackedArchiveEntry *entries;
    u32 entry_count;
    char *archive_name;
    ArchiveReader *reader;

private:
    friend class ResourceLoader;

    bool ParseAndBuildIndex(const char *archive_path);
    const PackedArchiveEntry *FindLeafCaseInsensitive(const char *leaf_name) const;
    u32 StoredEnd(const PackedArchiveEntry *entry) const;
};

class ResourceLoader {
public:
    explicit ResourceLoader(PackedArchive *archive);

    u8 *Load(const char *requested_name, u32 *optional_out_size,
             i32 source_mode);

private:
    PackedArchive *archive;
};

// Semantic singleton boundary for TH10's packed/direct resource loader.
// The native EAX-plus-stack entry remains a separate thunk concern.
u8 *LoadPackedResource(const char *requested_name, u32 *optional_out_size,
                       i32 source_mode);

typedef char AssertPackedArchiveEntrySize[
    sizeof(PackedArchiveEntry) == 0x10 ? 1 : -1];
typedef char AssertPackedArchiveSize[
    sizeof(PackedArchive) == 0x10 ? 1 : -1];
typedef char AssertPackedArchiveEntriesOffset[
    offsetof(PackedArchive, entries) == 0x0 ? 1 : -1];
typedef char AssertPackedArchiveReaderOffset[
    offsetof(PackedArchive, reader) == 0xc ? 1 : -1];

} // namespace th10
