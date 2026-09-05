# Version Data Initialization Evidence

## Scope

This document records the initialization chain beginning at `0x00420100`,
the packed-data initializer at `0x00434c30`, and the resource loader at
`0x0044b360` in `resources/th10.exe`.  It also records their teardown,
failure reporting, and externally visible global state.  It does not assign
original source names to the archive, reader, or error-buffer types.

All addresses, register inputs, stack cleanup, field offsets, constants, and
control flow below were checked against the target disassembly.  Descriptive
names such as `PackedArchive` are semantic boundaries only.

## Top-Level Initializer: `0x00420100`

### ABI and result

`0x00420100` takes no caller-provided input and returns with a plain `ret`.
It has a stack-cookie-protected local frame containing a `0x100`-byte path
buffer and one `u32` size temporary.

```text
EAX =  0  if the packed archive and the version entry both load
EAX = -1  after either failure has been reported
```

The direct caller at `0x004201b2` deliberately discards this result and
continues startup.  A C++ reconstruction of that caller must preserve that
fact; turning this result into normal error propagation would alter the
observed control flow.

### Exact high-level flow

1. Put `"th10.dat"` (`0x0046e000`) in `EAX` and `0x00497990` in `ECX`, then
   call `0x00434c30`.
2. If that call reports failure in `AL`, report
   `"error : データファイルが存在しません\r\n"` through `0x0044b8e0` and
   return `-1`.  This path does **not** write `0x00492388` or `0x0049238c`.
3. Format `"th10_%.4x%c.ver"` (`0x0046dff0`) into the local `0x100`-byte
   buffer with the literal inputs `0x100` and `'a'`.  The resulting target
   name is `th10_0100a.ver`.
4. Call `0x0044b360` with that local name in `EAX`, a pointer to the local
   size in the first stack argument, and archive mode (`0`) in the second.
5. Unconditionally copy the local size to `0x00492388` and the returned
   pointer to `0x0049238c`.
6. If the returned pointer is non-null, return `0`.  Otherwise explicitly
   write zero to `0x0049238c`, report
   `"error : データのバージョンが違います\r\n"`, and return `-1`.

The second failure path therefore may leave `0x00492388` nonzero: archive
mode writes the matched entry's declared uncompressed size before the
allocation/decode attempt.  This is an instruction-level property, not an
inference about intended semantics.

## Packed Archive Initialization: `0x00434c30`

### ABI

The entry is a split-register boundary, not a normal C++ member call:

```text
EAX = requested archive filename (`"th10.dat"` here)
ECX = PackedArchive state (`0x00497990` here)
return = AL boolean; plain ret
```

It preserves `ESI` and `EDI`.  The state is first cleared by `0x00434d10`.
It then allocates exactly `0x0c` bytes, writes vtable `0x0046f230`, `-1` at
`+0x04`, and zero at `+0x08`, and stores that reader object at state `+0x0c`.
If allocation, parse, or pathname duplication fails, it calls the same clear
routine and returns false.  The final reader virtual call is made only after
those checks and its return value is ignored; `0x00434c30` then returns true.

The resulting safe partial layout is:

```text
PackedArchive +0x00  PackedArchiveEntry* entries
PackedArchive +0x04  u32                 entry_count
PackedArchive +0x08  char*               owned_archive_name
PackedArchive +0x0c  opaque FileReader*  reader
sizeof at least 0x10
```

`entries` is a heap vector whose allocation prefix stores a count at
`entries[-1]`; each element is `0x10` bytes and owns a name pointer at `+0`.
The later resource path proves `+0x04`, `+0x08`, and `+0x14` within an entry
are used as file-data offsets/sizes, but their exact source-level names should
remain conservative.

### Archive parse performed by `0x00434f70`

`0x00434f70` is called with the requested pathname as its one stack argument;
its actual state object is held in `ESI` by the caller.  It returns an `AL`
boolean and consumes four bytes (`ret 4`).  Its success path:

1. Calls reader vtable slot `+0x00` with the filename and `DAT_00474908`.
2. Reads 16 bytes through reader vtable slot `+0x08`.
3. Applies the target's reversible byte transform (`0x0044b0d0`) with
   `AL=0x1b`, increment `0x37`, block width `0x10`, and key length `0x10`.
4. Requires decoded dword zero to be `0x31414854` (`"THA1"` in memory).
5. Decodes the remaining header dwords with the literal arithmetic visible in
   the target: `-0x075bcd15`, `-0x3ade68b1`, and `+0xf7e7f8ac`.
6. Obtains the reader size through vtable `+0x14`, seeks through vtable
   `+0x18`, reads the encoded index, transforms it with `AL=0x3e`, increment
   `0x9b`, block width `0x80`, then LZSS-decompresses it via `0x00435dc0`.
7. Converts the decoded records to the owned `0x10`-byte entry vector via
   `0x004350d0`, storing that vector and the decoded count in the state.

The reader remains owned by state `+0x0c` after success.  `0x00434c30` copies
the requested pathname into state `+0x08` and invokes reader vtable slot
`+0x00` again with the owned copy and `DAT_00474908`; it ignores that call's
return and then reports success.  The target does not expose enough direct
type information to name these reader virtual methods beyond their observed
argument and ownership boundaries.

## Resource Loader: `0x0044b360`

### ABI

This entry also has a mixed ABI:

```text
EAX = const char* requested_name
stack +0x04 = u32* optional_out_size
stack +0x08 = i32 source_mode
return EAX = heap buffer or null
ret 8
```

It locks `DAT_004922a4`, increments byte `DAT_0049231e`, and guarantees the
matching unlock/decrement before every normal return.  The byte is a nesting
counter, not a durable file status: nested callers can wrap its `u8` value.

### Mode `0`: packed archive

Archive mode strips the final `\\` and then the final `/` from the requested
name and performs a case-insensitive lookup against `PackedArchive.entries`.
It writes the matching entry's `+0x08` dword to `optional_out_size` whenever
that pointer is non-null, including when later allocation or decoding fails.
It then calls `0x00434dd0` to read/decode the entry through the still-owned
reader and returns the resulting heap buffer.

`0x00434dd0` locates the same entry, reads the byte interval
`entry[+0x04]..entry[+0x14)`, and returns it directly when that interval length
equals entry `+0x08`; otherwise it runs the target LZSS routine with the
declared size as output capacity.  Temporary encoded storage is freed before
return.  Therefore the returned buffer is caller-owned, whereas the archive
index and reader remain global-state-owned.

### Nonzero mode: direct filesystem read

Any nonzero `source_mode` takes a distinct disk path:

```text
CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, null, OPEN_EXISTING,
            0x08000080, null)
GetFileSize(handle, null)
malloc(file_size)
ReadFile(handle, buffer, file_size, &actual_bytes, null)
CloseHandle(handle)
```

The returned allocation is accepted without checking `ReadFile`'s boolean
result.  When supplied, `optional_out_size` receives `actual_bytes` after the
call.  This direct-file branch is relevant to the generic loader but is not
used by `0x00420100`, which always passes zero.

## Globals and Lifetime

| Address | Confirmed role | Ownership/lifetime fact |
| --- | --- | --- |
| `0x00497990` | `PackedArchive` state | Cleared before initialization and by shutdown; owns entry vector, copied archive path, and reader. |
| `0x00497994` | `PackedArchive + 0x04` | Entry count; read by archive-mode lookup. |
| `0x00492388` | Version data size | Written after every attempted version-entry load; shutdown does not clear it. |
| `0x0049238c` | Version data heap pointer | Written after the attempt; explicitly zeroed on version-entry failure and freed/zeroed at shutdown. |
| `0x004922a4` | Shared resource-loader critical section | Locked by `0x0044b360`; reused by other resource helpers. |
| `0x0049231e` | Shared resource-loader nesting byte | Incremented/decremented inside the loader lock. |
| `0x004922bc` | Error-buffer critical section | Locked by the failure reporter. |
| `0x0049231f` | Error-buffer nesting byte | Incremented/decremented by both formatting reporters. |

At `0x00420293`, shutdown frees non-null `0x0049238c` and zeros that pointer.
At `0x0042038a`, it clears `0x00497990` via `0x00434d10`: free state `+0x08`,
destroy/free every entry and its owned name, call reader vtable `+0x1c` with
argument `1`, and zero all four fields.  No shutdown write to `0x00492388` is
present.

## Failure Reporter: `0x0044b8e0`

`0x00420100` sets `EDI=0x00474f70` before calling this cdecl/varargs helper;
the format string is the first stack argument.  It is therefore not safe to
declare it as an ordinary `thiscall` function.

The reporter locks `DAT_004922bc`, formats into a 512-byte local buffer with
`vsprintf`, and appends it to the state passed in `EDI` only when it fits below
`EDI + 0x1fff`.  The confirmed state fields are:

```text
ErrorBuffer +0x2000  char* append_cursor
ErrorBuffer +0x2004  u8    changed = 1 after every report attempt
sizeof at least 0x2005
```

On a fitting message, it copies the NUL-terminated text, advances the cursor
by the text length excluding its terminator, and writes a fresh NUL there.
Even an over-capacity message sets `changed`.  The function then unlocks and
decrements `DAT_0049231f`; it has no observable error return.

## Implementable C++ Boundary

The appropriate first C++ boundary is a semantic subsystem, not a direct
attempt to force the original split-register or `EDI` ABIs into normal member
functions:

```cpp
class PackedArchive {
public:
    bool OpenAndIndex(const char* archive_name);
    unsigned char* LoadEntryByLeafName(const char* name, unsigned long* size);
    void Clear();
};

int InitializeVersionData();
```

`InitializeVersionData()` should own only the orchestration above: clear/open
the persistent archive, request the fixed version filename, publish the
separately allocated buffer/size globals in the observed order, and append the
two fixed failure messages.  `PackedArchive::Clear()` must remain separate
from freeing the published version buffer, because the target does so in
separate shutdown steps.

For an ABI-sensitive build, preserve thin wrappers at the original boundaries:
`EAX`/`ECX` for `0x00434c30`, `EAX` plus two stack arguments for `0x0044b360`,
and `EDI` plus cdecl varargs for `0x0044b8e0`.  The semantic C++ methods above
should not pretend those are ordinary `thiscall` calls.  The decompression and
byte-transform helpers can remain external implementation boundaries until
their C++ versions are independently recovered; this does not prevent a
correct ownership and startup-flow reconstruction.
