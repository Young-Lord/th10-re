# Score Save Writer — TH10 0x0042B1E0

Reconstructed as `th10::SaveScoreRecordFileEbx` in `src/ScoreSave.cpp/.hpp`
(semantic body; the register ABI is documented below and mirrored at the
wired call site in `src/TitleGameManagerLifecycle.cpp`, function
`TeardownTitleScreenStackAbi` / 0x00417c80).

## ABI

```text
0x0042B1E0:  EBX = score-save state (the value of DAT_0047783c)
             prologue: sub esp,0xC; epilogue retn (cdecl-like, no cleanup)
             returns -1 (header record null / opener failed) or 0
```

The caller loads `EBX` from the pointer variable `DAT_0047783c`
(`g_TitleScoreSaveRecord`); unlike 0x00429b60 there is no ECX/EDX argument
and no stack argument.

## Save-State Layout (EBX)

| Offset | Meaning |
| ------ | ------- |
| `+0x0000` | pointer to the 0x18-byte score header record, written to the file first; `+0x10` = uncompressed image size (compressor out-size), `+0x14` = total size published as payload end + 0x430, `+0x04` republished as compressed size + 0x18 |
| `+0x0008` | seven 0x437c-byte stage-record slots; each: `+0` word magic `0x5243`, `+4` checksum dword, `+8` stage-index dword, payload from `+0xc` |
| `+0x1d86c` | 0x448-byte clear-data region; `+4` = its checksum dword |

## Behavior

1. Null-checks `[EBX]` (the header record pointer); -1 when null.
2. `malloc(0x200000)` scratch image and copies the current 0x18 header record
   to `block[0..0x17]` — **dead data**: the compressor input starts at
   `block+0x18` and the file's first block is the (later-updated) record
   itself, so this copy is never used. Quirk preserved.
3. Walks the seven 0x437c-byte stage slots. A slot is serialized only when
   its magic word is `0x5243`; for each matching slot, in place:
   - stores the stage index (the **raw loop counter**, 0..6 — it advances for
     skipped slots too, so numbering has gaps when a magic is missing) at
     `record+8`,
   - recomputes the checksum at `record+4` (below),
   - copies the full 0x437c bytes into the image at the running offset, which
     advances **only for matching slots**.
4. Stage-record checksum (native loop 0x42b253): 2878 iterations of six
   bytes stepping from `record+0xc`, i.e. the byte sum of `record+8 ..
   record+0x437b` (the magic word at +0 and the checksum field itself at +4
   are excluded).
5. Clear-data checksum (native loop 0x42b2c0): 272 iterations of four bytes
   stepping from `state+0x1d876`, i.e. the byte sum of `region+8 ..
   region+0x447`; stored at `state+0x1d870` before the 0x448-byte region is
   appended to the image.
6. Publishes the total size `header+0x14 = payload end + 0x430`. The
   compressor therefore runs over **0x430 bytes of uninitialized scratch**
   beyond the copied data — native quirk preserved.
7. `CompressReplayImageStdcallAbi` (0x004359b0) over `block+0x18` with size
   `header+0x14`, compressed size stored into `header+0x10`; then
   `header+0x04 = header+0x10 + 0x18` (same "+0x18 secondary header" pattern
   as the replay writer).
8. One scramble pass through `ScrambleReplayImageUserpurgeAbi` (0x0044b220):
   initial key `AL=0xAC`, key step `0x35`, 0x10-byte blocks (the replay
   writer's two passes 0x3d/0x7a/0x80 and 0xaa/0xe1/0x400 are the sibling
   usage of the same helper).
9. Opens `scoreth10.dat` through `OpenReplayFileForWriteStdcallAbi`
   (0x0044b620); on a nonzero return, returns -1 **without freeing the
   scratch image or the packed buffer** (leak quirk preserved; the opener
   releases its own lock on failure).
10. Writes the 0x18-byte header record, then the packed body
    (`header+0x10` bytes) through `WriteFile`, re-testing the published
    handle global `DAT_00474C38` against -1 before each write. A short write
    runs the shared inline failure epilogue (`CloseHandle` +
    `LeaveCriticalSection(stru_4922A4)` + `dec byte_49231E`) **without
    invalidating the handle global**, so the following guard re-tests a
    closed handle — quirk preserved. After the body write, a final
    handle-valid check closes and unlocks.
11. Frees the packed buffer (if non-null) and the scratch image; returns 0.

## Preserved quirks

- Dead 0x18-byte header copy to the front of the scratch image (step 2).
- Stage index is the raw loop counter; image offset advances only on magic
  matches, so a missing magic both skips data and leaves an index gap.
- Compressor size includes 0x430 uninitialized trailing bytes (step 6).
- Opener-failure return leaks both heap buffers (step 9).
- Short-write path leaves the (now closed) handle value in `DAT_00474C38`.

## Boundaries

- `0x004359B0` `CompressReplayImageStdcallAbi`, `0x0044B220`
  `ScrambleReplayImageUserpurgeAbi`, `0x0044B620`
  `OpenReplayFileForWriteStdcallAbi` — extern ABI boundaries shared with
  `src/ReplaySave.cpp` (not yet reconstructed in `src/`).
- `WriteFile` / `CloseHandle` / `LeaveCriticalSection` / `malloc` / `free` —
  CRT/Win32 platform boundaries modeled semantically.
