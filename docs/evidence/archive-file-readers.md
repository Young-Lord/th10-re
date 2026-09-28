# Archive file readers (0x00434c10-0x004366c0)

Module: `src/ArchiveFileReaders.cpp` / `src/ArchiveFileReaders.hpp`

The three polymorphic leaf readers behind `ArchiveReader`:

- vtable `PTR_FUN_0046f230` (OS file handle): `{vtable, HANDLE, access}`;
  slots: +0x00 open 0x00435370, +0x04 close 0x00435460, +0x08 read
  0x00435490, +0x0c write 0x004354d0, +0x10 tell 0x00435510, +0x14 size
  0x00435530, +0x18 seek 0x00435550.
- vtable `PTR_FUN_0046f308` (memory) and `PTR_FUN_0046f328` (RT_RCDATA
  resource) share the body shape `{vtable, size, cursor, base}`; their
  close paths free `+0x0c` and downgrade to the base vtable `0046f210`.

Leaf resolution (`0x004356f0`) builds `"<text before first />.dat"` and
scans the loaded-archive table `DAT_004923b0` (count `DAT_00477850`,
stride 0x10, name at +0x08). The loaders `0x00435800`/`0x004358e0` decode
archive leaves through 0x00434dd0 (`DecodePackedSectionEcxStackAbi`,
already registered) and fall back to whole-file reads through
`ReadWholeArchiveReaderFile`.

`0x00434c30` opens an archive onto a context slot: reset (0x00434d10),
allocate reader, decode index (native 0x00434f70, EAX = reader, stack =
path; modeled as the `DecodeArchiveHeaderEaxStackAbi` boundary), duplicate
the path (0x00435220) and open through vtable slot 0 with the mode string
from `DAT_00474908`.

`0x00434d10` frees the context: `+0x08` path copy, the eh-vector at
`+0x00` (count at base-4, stride 0x10, element dtor 0x00435250) via the
MSVCP60 `eh vector destructor iterator` 0x004525ff, and the reader at
`+0x0c` through vtable +0x1c.

`0x00436570` launches a child process with `CREATE_NO_WINDOW`, a minimal
`STARTUPINFO` (x/y/size = 0x80000000, show window 10) and optionally polls
`GetExitCodeProcess` until it leaves `STILL_ACTIVE` (259).
