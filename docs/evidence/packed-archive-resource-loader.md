# Packed Archive Resource Loader Evidence

## Scope

This document recovers the C++ interface, ownership, and synchronization
boundaries of the generic resource loader at `0x0044b360` and the packed-entry
loader at `0x00434dd0` in `resources/th10.exe`.  It complements
`packed-archive-lifecycle.md`: that document owns archive opening and teardown;
this one covers extraction of an already indexed entry and the public loader
which selects archive or direct-file input.

All names below are descriptive.  In particular, `PackedArchive` and
`ArchiveReader` are C++ reconstruction names, not recovered source names.

## Confirmed Persistent State

The packed branch of `0x0044b360` uses the singleton state at `0x00497990`.
The parts needed by both functions are:

```text
PackedArchive +0x00  PackedArchiveEntry* entries
PackedArchive +0x04  u32                 entry_count
PackedArchive +0x0c  ArchiveReader*      reader

PackedArchiveEntry +0x00  char* entry_name
PackedArchiveEntry +0x04  u32   stored_offset
PackedArchiveEntry +0x08  u32   declared_size
```

The lookup helpers at `0x00434f30` and `0x0044b360` walk `entry_count`
records in `0x10` byte steps and compare `entry_name` case-insensitively via
`0x0046054e`.  `0x00434dd0` additionally reads its stored end from
`matched_entry + 0x14`, rather than from a conventional field inside the first
`0x10` bytes.  The parser's terminal record makes this address valid for the
last normal entry.  The exact source-level representation of this packed range
table is not yet proven; a C++ reconstruction must retain an explicit
`stored_end` accessor rather than silently assert that the record itself is a
normal independent `0x18` byte structure.

`PackedArchive::Clear()` owns the index, copied names, and `reader`; it is
separate from every extracted byte buffer.  See `packed-archive-lifecycle.md`
for the clear/destruction ordering.

## Public Loader: `0x0044b360`

### ABI

```text
EAX       = const char* requested_name
stack +04 = u32* optional_out_size
stack +08 = i32 source_mode
return EAX = caller-owned byte buffer, or null
ret 8
```

This is not an ordinary C++ member ABI.  A semantic C++ implementation should
expose a normal function/method and use a small ABI thunk only where an
object-level match requires it.

### Lock Protocol

The public loader begins every call with:

```text
EnterCriticalSection(0x004922a4)
+0x0049231e
```

It performs all archive lookup, archive seeking/reading/decoding, direct file
opening, allocation, reading, and handle closing while the critical section is
held.  Each normal return path then executes:

```text
LeaveCriticalSection(0x004922a4)
--0x0049231e
```

The byte at `0x0049231e` is a nesting observation counter, not a lock token or
resource status.  It is incremented after entering and decremented after
leaving, so the decrement is intentionally outside the lock.  It is an `u8`
and may wrap under sufficiently deep recursion.  The critical section itself
is recursive on Win32; the byte does not add recursion safety.

`0x00434dd0` does not touch either global.  Its reader seek/read sequence is
therefore serialized only when a caller such as `0x0044b360` holds this lock.
A direct C++ `PackedArchive::LoadEntry` method should either document that the
archive lock is already held or acquire the same subsystem lock at its public
boundary.  It must not rely on `0x00434dd0` to protect concurrent `Clear()` or
reader cursor changes.

### `source_mode == 0`: Packed Archive

The loader derives a leaf name without allocating a copy:

1. It finds the last `'\\'`; when present, the candidate begins one byte after
   it.
2. It then finds the last `'/'` in that candidate; when present, the candidate
   begins one byte after it.
3. It searches `0x00497990.entries` case-insensitively for that leaf name.

This intentionally handles mixed separators by stripping the last backslash
first and then the last slash in the remaining suffix.  It does not normalize
case or modify the caller's string.

On no match, it writes zero through `optional_out_size` when non-null and
returns null.  On a match, it first writes `entry.declared_size` through that
pointer, if present.  That size publication occurs before allocation, reader
I/O, byte transformation, or decompression.  A null return may therefore be
paired with a nonzero output size.

For a nonzero declared size, `0x0044b360` allocates exactly that many bytes
through `0x00452706` and passes the allocation to `0x00434dd0`.  A failed
allocation returns null with the already-published declared size.  A matched
zero-size entry also returns null: the loader does not call `0x00434dd0` with
a null output buffer for this path.

The returned non-null buffer belongs to the loader's caller and is released by
the target heap free route (`0x00452422`).  It is not adopted by the archive.

### `source_mode != 0`: Direct File

Any nonzero mode bypasses the archive and calls:

```text
CreateFileA(requested_name, GENERIC_READ, FILE_SHARE_READ, null,
            OPEN_EXISTING, 0x08000080, null)
GetFileSize(handle, null)
allocate(file_size)
ReadFile(handle, buffer, file_size, &actual_bytes, null)
CloseHandle(handle)
```

The target ignores the Boolean return from `ReadFile`; after a successful
allocation it returns the buffer even when the read call reports failure.  If
an output pointer is supplied, it receives the `actual_bytes` local after the
call.  A file-open or allocation failure returns null.  The allocation is
caller-owned on every non-null return, as in archive mode.  This direct branch
does not initialize `optional_out_size` before file open, so it differs from
the archive no-match path.

## Archive Entry Load: `0x00434dd0`

### ABI

```text
ECX       = const char* requested_leaf_name
stack +04 = PackedArchive* archive
stack +08 = u8* supplied_output
return EAX = supplied_output, decoded output, or null
ret 8
```

It returns null immediately if `archive->reader` is null.  Otherwise it uses
`0x00434f30` to perform a fresh case-insensitive index lookup.  Consequently
the loader does not accept a previously found entry as authority: any C++
version should keep lookup and extraction coupled to the same stable archive
snapshot/lock.

### Read and Decode Flow

For a matched entry, the function computes:

```text
stored_size = StoredEnd(entry) - entry.stored_offset
declared_size = entry.declared_size
```

If `stored_size == declared_size` and `supplied_output` is non-null, it reads
directly into `supplied_output`.  In every other case it allocates a temporary
buffer of `stored_size` bytes.  It then calls reader vtable `+0x18` to seek to
`stored_offset` with a zero second argument, followed by vtable `+0x08` to
read `stored_size` bytes.

A seek failure or read failure makes the function return null.  Any buffer
chosen for the immediate read is released on that failure path.  In the direct
read case this includes a non-null caller-supplied buffer.  This is an unusual
but concrete ownership rule: callers must treat a null result as meaning that
the supplied buffer may already have been freed.

After a successful read, `0x0044b0d0` transforms the stored bytes in place.
Its parameters are derived from a byte sum of `entry_name` and a twelve-byte
record selected from global tables at `0x00474bd8..0x00474be0`.  The transform
is an implementation boundary here; it does not change buffer ownership.

When stored and declared sizes differ, the function invokes `0x00435dc0` with
the transformed encoded bytes, `stored_size`, `supplied_output`, and
`declared_size`.  The encoded temporary buffer is then freed.  When the sizes
match, it returns the transformed read buffer directly.  Thus, in the public
loader's normal archive path:

```text
uncompressed entry: allocated output is filled and returned directly
compressed entry:  allocated output is decode destination; encoded input is temporary
```

The target does not check an output-size return from the decode routine.  The
public loader publishes the index's declared size, not a decoder-produced byte
count.

The only proven decode-failure behavior is the pointer propagated from
`0x00435dc0`, after the temporary encoded buffer is released.  The entry
loader does not visibly free a non-null supplied decode destination in this
branch.  This differs from seek/read failure and is a reason to preserve the
original allocation/failure contract in an ABI-matching implementation rather
than replacing it with a blanket RAII assumption without first recovering the
decompressor's return contract.

## Ownership Table

| Object | Created/owned by | Transfer and release rule |
| --- | --- | --- |
| Archive index, copied names, reader | `PackedArchive` | Remain owned until `PackedArchive::Clear()`; never transferred by either loader. |
| Public archive output | `0x0044b360` before calling `0x00434dd0` | Transfers to caller only on non-null return; freed internally on uncompressed read failure. |
| Encoded read buffer | `0x00434dd0` when direct output cannot be used | Freed internally after transform/decode, including read failure. |
| Direct-file buffer | `0x0044b360` | Transfers to caller on non-null return; its file handle is always closed after allocation/read attempt. |
| `optional_out_size` | Caller-owned storage | Archive mode writes zero on no match and declared size on match before later failures; direct-file mode writes after `ReadFile` only. |

## Conservative C++ Boundary

The semantic API should separate an archive's externally synchronized loading
operation from direct files:

```cpp
class PackedArchive {
public:
    // Requires the shared resource-loader lock to remain held for lookup,
    // seek, read, transform, and decode.
    unsigned char* LoadLeafLocked(const char* leaf_name,
                                  unsigned char* supplied_output);
};

class ResourceLoader {
public:
    unsigned char* Load(const char* requested_name,
                        unsigned long* optional_out_size,
                        int source_mode);
};
```

`ResourceLoader::Load` should own the recursive critical section and expose
the target's mode split.  `PackedArchive::LoadLeafLocked` should retain the
explicit supplied-output parameter because it captures the real distinction
between directly returned stored bytes and a temporary compressed input.  A
more idiomatic RAII implementation may wrap only the confirmed successful
return ownership.  It must account for the target's asymmetric failure paths
before claiming behavioral or binary equivalence.

For a codegen-sensitive implementation, keep an external thunk for the
`EAX`-plus-two-stack-arguments ABI at `0x0044b360` and a separate `ECX` plus
two-stack-arguments thunk at `0x00434dd0`; neither should be falsely declared
as a normal `__thiscall` member function.

## Resolved Native Leaves (implemented in `src/PackedArchive.cpp`)

The byte transform at `0x0044b0d0` and the LZSS-style decompressor at
`0x00435dc0` were ported into the anonymous namespace of `PackedArchive.cpp`
and are now reached by `PackedArchive::LoadLeafLocked`.

`0x0044b0d0` selects one of eight twelve-byte key records by the low three
bits of the byte sum of the matched entry name.  The used fields start at
`DAT_00474bd8 + 0x01` (per-byte key step, `u8`), `+0x04` (block size, `u32le`)
and `+0x08` (transform span, `u32le`), each advanced by `0x0c` per record.
Record values: step `37 e9 51 19 cd 34 97 37`, block
`0x40 0x40 0x80 0x400 0x200 0x80 0x80 0x400`, span
`0x2800 0x3000 0x3200 0x7800 0x2800 0x3200 0x2800 0x2000`.  Spans are exact
multiples of their block sizes.

`0x00435dc0` decodes MSB-first tokens: a set flag precedes an 8-bit literal,
a clear flag precedes a 13-bit absolute history offset and a 4-bit match
length (stored code means count + 3).  The `0x2000`-byte ring at
`DAT_0048f868` is written with a cursor initialized to one; matches read
`ring[(offset + i) & 0x1fff]`.  A zero offset terminates the stream.  The
decoder feeds zero bits past the encoded end.

## Native Archive Open and Index Decode (mapping notes)

The reader object used by the leaf loader is a 0xc-byte object whose virtual
methods are published through `PTR_FUN_0046f230` (0x0046f230).  Layout:
`+0` vtable, `+4` OS file handle, `+8` access flags.  Methods identified:
slot `+0x00` = `0x00435370` mode-string open (`r`/`w`/`a`) then `CreateFileA`;
slot `+0x08` = `0x00435490` `ReadFile(handle, buf, size, &actual, 0)` when the
access field is `GENERIC_READ`; slot `+0x18` = `0x00435550`
`SetFilePointer(handle, offset, 0, method)` when the handle is not `-1`;
slot `+0x1c` = `0x00435320` release/free (delegates to `0x00435340`).
The archive open at `0x00434c30` allocates the reader inline
(`operator_new(0xc)`, vtable `0x0046f230`) and `0x00434f70` performs the index
decode: it reads a 16-byte header, transforms it with the fixed key set
`(key 0x37, block 0x10, span 0x10)`, checks the signature dword
`0x31414854` ("THA1"), seeks to `file_size - index_size`, reads the trailing
index blob, transforms it with `(key 0x9b, block 0x80, span = index_size)`,
LZSS-decompresses it via `0x00435dc0`, and hands the decoded records to
`0x004350d0`.

`0x004350d0(decoded, count, trailing_boundary)` builds the 0x10-byte entry
array with a leading allocation-count dword.  For every record it duplicates
the NUL-terminated name, advances past the name padded to a four-byte
boundary, then copies the three dwords offset / declared size / field_0c.
The terminal record receives `stored_offset = trailing_boundary` and a zero
declared size.  This confirms the decoded-record layout assumed by
`PackedArchive::BuildArchiveEntries` in `src/PackedArchive.cpp`.

## Implementation Status Update

`src/PackedArchive.cpp` now defines the reader object and the direct-file
helpers that earlier versions kept as unannotated externs.  The reader object
is an `ArchiveReaderState` mirroring the native 0xc-byte layout (vtable field
+0, OS handle +4, access flags +8):
`CreateArchiveReader` allocates and seeds the closed state, `DestroyArchiveReader`
closes the handle and optionally frees, `OpenArchiveReaderIgnoredResult` applies
the read-open CreateFileA parameters, `SeekArchiveReader` maps to SetFilePointer
(returning false on a closed handle), and `ReadArchiveReader` returns whether
ReadFile produced bytes.  `OpenDirectResourceFile` / `GetDirectResourceFileSize`
/ `ReadDirectResourceFileIgnoredResult` / `CloseDirectResourceFile` reproduce the
loader's nonzero-mode branch.

The only remaining external leaf of this subsystem is `DecodeArchiveIndex`,
whose native implementation is the pair `0x00434f70` + `0x004350d0` described
above.

## Reader Virtual Method Table (complete slot map)

`PTR_FUN_0046f230` publishes the nine methods of the archive reader object
(0xc bytes: vtable, OS handle at +4, access flags at +8):

| offset | address | role |
| --- | --- | --- |
| +0x00 | 0x00435370 | open(path, mode-string r/w/a); calls +0x04 first |
| +0x04 | 0x00435460 | close(): CloseHandle, handle = -1, access = 0 |
| +0x08 | 0x00435490 | read(buf, size): returns bytes read (0 unless access == GENERIC_READ) |
| +0x0c | 0x004354d0 | write(buf, size): full-write boolean when access == GENERIC_WRITE |
| +0x10 | 0x00435510 | tell(): current position, or 0 on a closed handle |
| +0x14 | 0x00435530 | file size, or 0 on a closed handle |
| +0x18 | 0x00435550 | seek(offset, method): SetFilePointer, always reports success |
| +0x1c | 0x00435320 | release (via 0x00435340 close) then free when the argument is set |
| +0x20 | 0x00435580 | guarded whole-file read: +0x14 size bound test then malloc/read |

`src/PackedArchive.cpp` models the four methods used by the loader (open/read/
seek/release) directly with Win32 calls instead of a virtual dispatch table.

The four remaining reader vtable methods (close `0x00435460`, write
`0x004354d0`, tell `0x00435510`, file size `0x00435530`) are implemented in
`src/PackedArchive.cpp` as `CloseArchiveReaderFile`, `WriteArchiveReader`,
`TellArchiveReaderPosition`, and `GetArchiveReaderFileSize`.

## DecodeArchiveIndex Implementation (0x00434f70)

`DecodeArchiveIndex` is now implemented inside `src/PackedArchive.cpp` and
replaces the last external leaf of the subsystem.  It opens the reader for
reading, reads the 16-byte header and transforms it with the fixed key set
`(0x37 / 0x10 / 0x10)`, validates the `"THA1"` signature, then derives the
entry count, the trailing index blob size and the decoded index size from the
shifted header words (`- 135792468`, `- 987654321`, `- 123456789`), seeks to
`file_size - index_size`, reads and transforms the blob with
`(0x9b / 0x80 / blob_size)` and LZSS-decompresses it.  The record-table build
is already covered by `BuildArchiveEntries` (mirror of `0x004350d0`).
