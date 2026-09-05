# Replay Save Writer — TH10 0x00429B60

Reconstructed as `th10::CommitReplaySave` in `src/ReplaySave.cpp/.hpp`
(semantic body; the register ABI is documented below and mirrored at the
single wired call site in `src/GameManagerStateBodies.cpp`).

## ABI

```text
0x00429B60:  ECX = save context (game-mode object, DAT_00477838)
             EDX = file name relative to the replay dir ("th10_NN.rpy")
             stack arg0 = player name buffer (char*)
             prologue: sub esp,0x124 + /GS cookie; epilogue xor eax,eax; retn 4
```

Callers (both `mov ecx, DAT_00477838`, EDX = local filename buffer, push
name): `0x0043399D` inside the replay-save naming screen `0x00433570`
(`RunManagerStateBody10`) and `0x00423F16` inside the post-run save path
`0x004236F0`.

## Save Context Layout (ECX)

| Offset | Meaning |
| ------ | ------- |
| `+0x14` | pointer to the 0x24-byte secondary header written first (`+0x0c` = compressed size + 0x24, `+0x1c` = compressed size, `+0x20` = uncompressed image size) |
| `+0x18` | pointer to the 100-byte replay header: `+0x00` player name (8 bytes), `+0x0c` time_t, `+0x10` score, `+0x48` slow-rate float, `+0x4c` stage count, `+0x50` chara, `+0x54` shot type, `+0x58` rank, `+0x5c` final stage value |
| `+0x1c` | eight dword stage-record pointers (records are 0x1c4 bytes) |
| `+0x40` | eight frame-list heads, stride 0x0c; node = `{record, next}` |

## Behavior

1. `strcpy`s the player name into `header+0` and pads with spaces (`0x20`)
   up to 8 bytes.
2. `_mkdir("replay")`, then `sprintf(path, "replay/%s", EDX)`.
3. Sizing pass over stages 0..7: for each non-null stage record, records the
   first (`first_stage`; the native "unset" sentinel is index 0) and last
   non-null index, zeroes `stage+8`, adds 452 plus, per list node, the
   body size `u32@entry+0x6304 - entry - 0x5464` and `6 * frames` where
   `frames = (i32@entry+0x5460 - entry)/6` into both the running total and
   `stage+8` (frames also into `stage+4`).
4. Finalizes the header: `+0x4c` stage count, `+0x10` = DAT_00474C44
   (current score), `+0x48` float =
   `100.0 - *(double*)(DAT_00477708+0x24) / *(double*)(DAT_00477708+0x2c) * 100.0`
   (unchecked deref of DAT_00477708).
5. `malloc(total)` and serializes: 100-byte header, then per stage the
   0x1c4-byte record, the `6*frames` frame bytes from each node entry, and
   the tail region `[entry+0x5464, u32@entry+0x6304)` copied with **unsigned**
   length (a negative length wraps to a huge copy — quirk preserved).
6. Compresses with `0x004359B0` (LZSS-style, stdcall `(image, size,
   &out_size)`), frees the plain image, then applies two `0x0044B220`
   scramble passes (`__userpurge`, initial key in AL; stack = buffer, size,
   key step, block size, size): pass 1 `AL=0x3D, step=0x7A, block=0x80`,
   pass 2 `AL=0xAA, step=0xE1, block=0x400`.
7. Publishes `secondary+0x20` = image size, `+0x1c` = compressed size,
   `+0x0c` = compressed size + 0x24, opens the file with `0x0044B620`
   (CreateFileA GENERIC_WRITE/SHARE_READ/OPEN_ALWAYS under critical section
   `stru_4922A4`, handle published at DAT_00474C38, depth byte
   `byte_49231E`; the opener's return value is ignored) and writes the
   0x24-byte secondary header and then the packed body.
8. Appends two text chunks in a `malloc(0xFFFF)` scratch buffer, each with
   tag `'USER'` (`0x52455355`) at +0, byte size at +4, type byte at +8 and
   4-aligned text from +12:
   - **type 0**: SJIS title `東方風神録　リプレイファイル情報\r\n` (format
     string `0x46E5E0`), then `Version %s\r\n` ("1.00a"), `Name %s\r\n`,
     `Date %.2d/%.2d/%.2d %.2d:%.2d\r\n` (from `localtime(header+0x0c)`,
     year taken `% 100`), `Chara %s\r\n`
     (`off_4746DC[3*chara + shot]`), `Rank %s\r\n` (`off_4746F4[rank]`), a
     stage line, `Score %d\r\n`, and `Slow Rate %2.2f\r\n` (header float
     promoted to double) plus the trailing NUL.
   - **type 1**: the comment template `0x46E4F8`
     (`コメントを書けます`) plus NUL.
9. Always returns 0.

### Stage line selection (chunk 1)

```text
if header+0x5c <= 7:
    first_stage == last_stage:
        first_stage == 7 ? "Extra Stage\r\n" : "Stage %d\r\n" (first_stage)
    else:
        sprintf("Stage %d ", first_stage)   # no CRLF, last stage is DEAD:
                                            # the native pushes both values but
                                            # the format only consumes one
else:
    first_stage == 7 ? "Extra Stage Clear\r\n" : "Stage All Clear\r\n"
```

With no non-null stage records, first/last both stay 0 and the line reads
`Stage 0`.

## Preserved quirks

- Short-write failure path (`CloseHandle` + `LeaveCriticalSection` +
  `dec byte_49231E`) does **not** invalidate DAT_00474C38, so subsequent
  `handle != -1` guards re-test a closed handle and can write to it.
- The two-arg stage branch's second argument is dead and the trailing CRLF
  is missing.
- Unsigned-wrapping tail copy length (see step 5).
- Unchecked deref of `*(ctx+0x14)`, `*(ctx+0x18)`, `header` fields and
  `DAT_00477708`.
- `first_stage`'s "unset" sentinel collides with stage index 0.
- The 0xFFFF chunk buffer is fully zeroed by the native `rep stosd`/`stosw`/
  `stosb` (0xFFFC + 2 + 1 bytes); the decompiler's extra stores are
  redundant.

## Boundaries

- `0x004359B0` `CompressReplayImageStdcallAbi`, `0x0044B220`
  `ScrambleReplayImageUserpurgeAbi`, `0x0044B620`
  `OpenReplayFileForWriteStdcallAbi` — extern ABI boundaries (not yet
  reconstructed in `src/`).
- `WriteFile` / `CloseHandle` / `LeaveCriticalSection` / `_mkdir` /
  `sprintf` / `malloc` / `free` / `localtime` — CRT/Win32 platform
  boundaries modeled semantically.
