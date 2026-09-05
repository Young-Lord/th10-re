# Packed Archive Lifecycle Evidence

## Scope

This document recovers the ownership boundary formed by `0x00434d10`,
`0x00434f70`, and `0x004350d0` in `resources/th10.exe`.  It is sufficient to
model a C++ `PackedArchive` class, including clearing an existing index,
opening and indexing an archive, and ownership transfer for the resulting
records.  It deliberately does not recover the byte transform, decompressor,
or the source-file format beyond the fields needed for that lifetime model.

The names in this document are descriptive.  They are not claims about the
original source identifiers.

## Persistent State

The callers hold the archive state in `ESI` for the three functions here.
The confirmed layout is:

```text
PackedArchive +0x00  PackedArchiveEntry* entries
PackedArchive +0x04  u32                 entry_count
PackedArchive +0x08  char*               archive_name
PackedArchive +0x0c  OpaqueReader*       reader
sizeof(PackedArchive) >= 0x10

PackedArchiveEntry +0x00  char* owned_name
PackedArchiveEntry +0x04  u32   field_04
PackedArchiveEntry +0x08  u32   field_08
PackedArchiveEntry +0x0c  u32   field_0c
sizeof(PackedArchiveEntry) == 0x10
```

`entries` is not the allocation address.  Its backing allocation begins four
bytes earlier, and the preceding dword is an element count used by the
destruction helper.  The parser allocates storage for `entry_count + 1`
records, so this prefix includes the extra terminal record.  The persistent
`entry_count` field excludes that terminal record.

Every normal entry owns its own copied NUL-terminated name.  The observed
entry destructor at `0x00435250` only releases `owned_name` through
`0x00452422`, then stores zero in that field.  The other three dwords are not
released by this destructor and must not be modeled as independently owned
pointers.

## Clear: `0x00434d10`

### ABI

This is an internal register-boundary helper, not an ordinary callable C++
member function:

```text
ESI = PackedArchive*
return = plain ret
```

It preserves `EDI`; it conditionally saves `EBX` only while disposing the
entry vector.

### Exact release sequence

`Clear` is idempotent for the four confirmed state fields and follows this
order:

1. Read `archive_name` (`+0x08`).  If non-null, pass it to `0x00452422`, then
   write zero to `+0x08`.
2. Read `entries` (`+0x00`).  If non-null, read the count prefix at
   `entries[-1]`, run the element destructor `0x00435250` across that many
   `0x10`-byte records through `0x004525ff`, then free `entries - 4` through
   `0x004524a1`.
3. Write zero to `entries` (`+0x00`).
4. Read `reader` (`+0x0c`).  If non-null, invoke vtable slot `+0x1c` with a
   single stack argument equal to `1`.
5. Write zero to `reader` (`+0x0c`) and `entry_count` (`+0x04`).

The function writes `+0x08` to zero twice when it was originally null or
after it was freed.  This is an instruction-level detail with no additional
observable ownership meaning.

The reader's slot `+0x1c` is the reader object's in-place/destructive release
boundary for this class.  `Clear` does not separately call `0x004524a1` on
the reader pointer.  A C++ model must therefore give the reader a single
`DestroyOrRelease(true)` wrapper and must not add a second outer free.

## Open and Index: `0x00434c30`

Although outside the requested three-entry set, this direct caller establishes
the required class-level ownership contract for `0x00434f70` and is included
only for that reason.

### ABI and result

```text
EAX = requested archive path
ECX = PackedArchive*
return AL = success boolean
plain ret
```

It saves and restores `ESI` and `EDI`.  Its first action is
`0x00434d10(this)`, so successful reopening always replaces, rather than
merges with, a prior archive.

It allocates exactly `0x0c` bytes through `0x00452493`, initializes that
object as follows, and publishes it immediately at `this->reader`:

```text
OpaqueReader +0x00  vtable = 0x0046f230
OpaqueReader +0x04  -1
OpaqueReader +0x08  0
```

It then calls `0x00434f70(path)`.  On parser success it duplicates `path`
with `0x00435220`, publishes the returned allocation at `archive_name`, and
calls reader vtable slot `+0x00` once more with the copied path and global
`0x00474908`.  That last virtual return value is ignored.  Only parser
success plus successful pathname duplication determines the boolean result.

If reader allocation fails, the state was already cleared and the function
returns false directly.  If parsing or pathname duplication fails, it invokes
`Clear` and returns false.  Thus the public semantic operation can promise an
empty archive on every false return, even though the lower-level parser alone
cannot.

## Parse and Build Index: `0x00434f70`

### ABI

```text
ESI       = PackedArchive*
stack +04 = requested archive path
return AL = success boolean
ret 4
```

It requires `this->reader` to be non-null.  A null reader returns false before
register saves or any mutation.

The reader is used to open/read the archive, obtain its size, and seek/read
the encoded index.  After the format checks and decode steps, the function:

1. Stores the decoded entry count directly at `this->entry_count`.
2. Calls `0x004350d0(decoded_index, entry_count, trailing_boundary)`.
3. Stores its return at `this->entries`.
4. On non-null `entries`, frees the temporary encoded and decoded index
   buffers and returns true.

`trailing_boundary` is computed as the reader-reported length minus one
decoded header dword.  It is stored in the terminal record by the builder.
The source-level meaning of that boundary is not yet proven.

On every failure after entering the parser, it frees the temporary encoded and
decoded buffers when present, invokes reader vtable slot `+0x1c` with `1`,
clears only `this->reader`, and returns false.  It does **not** clear
`entry_count`, `entries`, or `archive_name` on that local failure path.
`OpenAndIndex` immediately calls `Clear` after receiving that false result,
which supplies the public all-fields cleanup.  Calling this parser as an
independent C++ method would otherwise expose an invalid partial state and is
therefore not an appropriate public API.

## Entry-Vector Builder: `0x004350d0`

### ABI

```text
stack +04 = decoded index records
stack +08 = decoded entry count
stack +0c = trailing boundary
return EAX = entries pointer, or null
ret 12
```

It is a standalone allocation/conversion routine; it does not access
`PackedArchive` directly.  It computes `count + 1`, allocates
`4 + (count + 1) * 0x10` bytes through `0x00452493`, writes that count to the
first dword, and returns the address four bytes later.  It uses the target's
array-construction helper to initialize the first dword of every record to
zero before filling it.

For each of the first `count` decoded records it duplicates a variable-length
name through `0x00452706`, stores that allocation at entry `+0x00`, advances
over the name rounded up to four-byte alignment, and copies three following
dwords into entry `+0x04`, `+0x08`, and `+0x0c`.

An individual name allocation may return null.  The builder still stores that
null and continues processing; it does not turn that allocation failure into
a null builder result.  A faithful C++ recovery must not accidentally make
per-entry name allocation a transactional failure unless later evidence proves
that the target allocator cannot return null in this use.

After the loop, the extra record remains with a null `owned_name` and receives
the supplied trailing boundary at `+0x04` plus zero at `+0x08`.  The target
does not write its `+0x0c` after the zero-initializing constructor.  This is a
sentinel-like storage record, but its exact logical role should remain
unnamed.

If the outer allocation fails, the builder returns null.  No decoded-index
buffer ownership transfers to it: `0x00434f70` remains responsible for freeing
that temporary on both builder success and failure.  Conversely, once a
non-null vector is stored in `PackedArchive.entries`, `Clear` exclusively owns
both its allocation and every successfully copied name.

## C++ Reconstruction Boundary

The appropriate semantic class keeps the parser private and represents the
prefix/sentinel allocation explicitly rather than substituting a normal
`std::vector`, whose allocation and destruction layout are different:

```cpp
struct PackedArchiveEntry {
    char* owned_name; // released individually
    unsigned long field_04;
    unsigned long field_08;
    unsigned long field_0c;
};

class PackedArchive {
public:
    bool OpenAndIndex(const char* archive_path);
    void Clear();

private:
    bool ParseAndBuildIndex(const char* archive_path);

    PackedArchiveEntry* entries_;
    unsigned long entry_count_;
    char* archive_name_;
    OpaqueReader* reader_;
};
```

`OpenAndIndex` owns the transaction boundary: clear first, install the reader,
delegate parsing, copy the archive path, and clear on any later failure.
`ParseAndBuildIndex` owns only reader-driven parsing and its temporary encoded
and decoded buffers.  `Clear` owns only persistent state.  Keeping those three
responsibilities distinct reproduces the target's release order and avoids
both reader double-free and temporary-index ownership leaks without requiring
decoder internals in the initial C++ reconstruction.
