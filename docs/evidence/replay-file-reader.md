# Replay sequential-file reader

Covers TH10 0x0044b6b0, 0x0044b790, 0x0044b7e0 — module
`src/ReplayFileReader.{hpp,cpp}`. The trio is the read-side counterpart of
`OpenReplayFileForWriteStdcallAbi` (0x0044b620, ReplaySave.cpp); the sole
caller is `LoadReplayViewRecord` (0x0042a200).

## 0x0044b6b0 OpenReplayFileForReadStdcallAbi

stdcall, ret 4. Enters the shared resource-loader critical section
(stru_4922a4), bumps the nesting byte byte_49231e, and calls CreateFileA
with GENERIC_READ (0x80000000), share mode 1, no security attributes,
OPEN_EXISTING (3) and flags 0x8000080
(FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN). The handle is
published to DAT_00474c38. On success returns 0.

Failure path: GetLastError, FormatMessageA with 0x1300
(FROM_SYSTEM | FROM_HMODULE | ALLOCATE_BUFFER) and language 0x400 into the
stack pointer-to-buffer, then LocalFree of that buffer — the message is
never surfaced. Leaves the critical section, decrements the nesting byte
and returns -1.

## 0x0044b790 ReadReplayFileBlockEdiAbi

The byte count travels in EDI (call sites: 0x24 for the header block, then
the packed size at header+0x1c). Returns 0 immediately while
DAT_00474c38 is -1. Otherwise mallocs (CRT 0x452706) the requested size;
when the allocation fails the file handle is closed and 0 returned — the
critical section and nesting byte are untouched on that path. A single
ReadFile fills the block; both the API result and the transferred count
are ignored. The buffer is handed to the caller, which validates the
"t10r" / version-5 header itself.

## 0x0044b7e0 CloseReplayFileReader

Closes the file handle when it is not -1, leaves the critical section and
decrements the nesting byte. Always returns 0. Note this closes the FILE,
not the handed-out block; LoadReplayViewRecord keeps the second block
until the decompressed body has been produced.

## Verification

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` (see report).
Reference disassembly: `build/reference/0044b6b0_sub_44B6B0.asm`,
`0044b790_sub_44B790.asm`, `0044b7e0_sub_44B7E0.asm`; native caller
`build/reference/0042a200_LoadReplayViewRecord.asm` lines 0x42a269-0x42a2a5.
