# Score File Load — scoreth10.dat Reader and State Lifecycle (0x42af20 / 0x42ae60 / 0x42b030 / 0x42af90 / 0x42afe0) and the 0x44b0d0 Unscramble

Reconstructed in `src/ScoreFileLoad.cpp/.hpp`. This is the read side of
`docs/evidence/score-save.md` (writer 0x0042b1e0 in src/ScoreSave.cpp): it
rebuilds the 0x1dcb4-byte score-save state published at `DAT_0047783c` from
`scoreth10.dat`. The creator 0x0042af20 is called from the game-mode
teardown cluster (0x0041fab1).

## State layout

| Offset | Meaning |
| ------ | ------- |
| `+0x0000` | pointer to the loaded file image — the 0x18-byte header record (`+0` magic 0x30314854 "TH10", `+8` word 3, `+0xc` default 0x100, `+0x10` packed size, `+0x14` unpacked size; packed body at `+0x18`) |
| `+0x0004` | decompressed body scratch buffer |
| `+0x0008` | seven 0x437c-byte stage-record slots (12-byte sub-header: `+0` word magic 0x5243 "CR", `+2` word 0, `+4` checksum, `+8` size 0x437c, `+0xc` stage index, payload from `+0x10`) |
| `+0x1d86c` | 0x448-byte clear-data region (magic 0x5453 "ST", size 0x448) |

## 0x0044b0d0 — `UnscramblePackedImageUserpurgeAbi(key, buffer, size, key_step, block_size, size_again)`

Native `__userpurge` (initial key in AL; five stack arguments), implemented
in this module. It is the **exact inverse** of `ScrambleReplayImageUserpurgeAbi`
(0x0044b220, src/ReplayPackedCodec.cpp): 0x44b220 reads the scratch copy at
the block's odd offsets descending then even offsets descending and writes
the buffer linearly, while 0x44b0d0 reads the scratch copy **linearly** and
writes the buffer's odd offsets descending then even offsets descending —
same permutation, same per-byte key stream (`key += key_step`), so writer
then loader round-trips.

- Copies `min(size_again, size)` bytes to a malloc scratch buffer (malloc
  failure skips the whole transform).
- Transformed span `= size - (skipped_tail + (size & 1))` with
  `skipped_tail = (size % block_size >= block_size / 4) ? 0 : size % block_size`
  (the native computes `block_size/4` via the sign-mask `and 3`, exact for
  positive block sizes).
- Per block `n = min(block_size, span)`: output positions `n-1, n-3, ...`
  (`(n+1)/2` bytes) then `n-2, n-4, ...` (`n/2` bytes), each
  `key ^ scratch[linear]`. When `span < block_size` the native permanently
  overwrites its `block_size` parameter (unobservable — last block).
- The scratch source pointer and the destination block base both advance
  across blocks without reset (the destination base is the previous block
  end, saved at 0x44b18e).
- Free of the scratch on every path.

Note on the earlier disassembly confusion: the odd/even count formulas use
`cdq` results (zero for positive blocks), not the `size_again` counter —
`[esp+0x2c]` only serves as the secondary loop guard (`remaining >
0`), decremented per block.

## 0x0042b030 — `LoadScoreRecordFileEbx(save_state)` (native EBX, plain ret)

Always returns 0.

1. Null header at `[state]` → install a fresh blank record (step 5) and
   return.
2. Header checks: dword `+0` must be `0x30314854` ("TH10") and word `+8`
   must be 3; failure → free the header, install a fresh blank record.
3. Unscramble the packed body at `header+0x18` in place:
   `UnscramblePackedImageUserpurgeAbi(0xac, header+0x18, [header+0x10],
   0x35, 0x10, [header+0x10])` — the reverse of the writer's pass.
4. `malloc([header+0x14] * 4)` — a deliberate **4x over-allocation**
   (`shl eax,2` quirk) — stored at `state+4`, then
   `DecompressPackedBytes(header+0x18, [header+0x10], body, [header+0x14])`.
5. `[header+0x14] <= 0` → return 0 **without** the fresh-record fallback
   (quirk).
6. Section walk (`remaining = [header+0x14]`, record = body):
   - magic 0x5243 ("CR"): word `+2` must be 0, then the byte sum of
     `record+8 .. record+0x437b` (0xb3e iterations of six bytes) must equal
     `[record+4]` and `[record+8]` must be 0x437c — then the record is
     copied (0x10df dwords) into `state + 8 + [record+0xc] * 0x437c`. The
     stage index comes **straight from the record with no bounds check**
     (quirk).
   - magic 0x5453 ("ST"): word `+2` zero, byte sum of `record+8 ..
     record+0x447` (0x110 iterations of four bytes) vs `[record+4]`,
     `[record+8] == 0x448` — copied (0x112 dwords) into `state+0x1d86c`.
   - Records whose checks fail are still skipped by their declared size;
     an **unknown** magic fails the whole load.
   - Advance: `remaining -= [record+8]`; negative → failure; the loop ends
     when `remaining == 0` → return 0.
7. Fresh blank record (failure and null paths): free the old header
   (failure path only), `malloc(0x18)`, zero six dwords, magic "TH10",
   word `+8 = 3` (16-bit store), `[+0xc] = 0x100`. The `state+4` scratch
   from steps 4-6 is **not freed** on these paths (leak quirk).

## 0x0042ae60 — `InitializeScoreSaveStateEaxAbi(state)`

Zeroes 0x1dcb4 bytes, loads `scoreth10.dat` through `LoadMainChainFile`
(0x0044b360, mode 1; the size out-param points at a caller stack slot and
is ignored) into `[state]`, seeds the clear-data region (0x0042add0
boundary) and the seven stage slots (0x0042acb0 boundary, stack arg `ret 4`)
with their default records, then runs `LoadScoreRecordFileEbx`. Returns the
state.

## 0x0042af20 — `CreateScoreSaveState`

`operator new(0x1dcb4)` (0x00452493) → 0x0042ae60 → publish `DAT_0047783c`;
allocation failure publishes 0 and returns 0. SEH frame elided.

## 0x0042af90 — `DestroyScoreSaveStateGlobal`

Frees `[state]` and `[state+4]` of the published state (clearing both),
`operator delete(state)` and clears `DAT_0047783c`.

## 0x0042afe0 — `ReleaseScoreSaveStateEsiStackAbi(state, free_flag)`

Native ESI = state, stack = flag (`ret 4`): the same two frees with field
clears, then `operator delete(state)` when flag bit 0 is set; returns the
state pointer.

## Boundaries

- `0x0044b360` `LoadMainChainFile`, `0x00435dc0` `DecompressPackedBytes`,
  `0x0042acb0` `InitializeScoreStageSlotStackAbi`, `0x0042add0`
  `InitializeClearDataRegionEdxAbi` — extern boundaries (the first two
  reconstructed elsewhere, the last two remain in the score-defaults
  domain).
- `operator new/delete` (0x00452493/0x004524a1) and malloc/free
  (0x00452706/0x00452422) — platform boundaries.
