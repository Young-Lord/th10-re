# Replay / Score Packed-Image Codec

Reconstructed as `th10::CompressReplayImageStdcallAbi` (TH10 0x004359b0) and
`th10::ScrambleReplayImageUserpurgeAbi` (TH10 0x0044b220) in
`src/ReplayPackedCodec.cpp/.hpp`. Both were previously extern ABI boundaries
declared by `src/ReplaySave.cpp` and `src/ScoreSave.cpp`; the C++ names match
those declarations, so both writers bind automatically. The compressor's
binary-search-tree helpers 0x00436000 / 0x00436210 / 0x00436260 / 0x004362b0 /
0x00436330 are implemented in the same file.

## 0x004359b0 — `CompressReplayImageStdcallAbi(image, size, out_size)`

Native stdcall, `retn 0xc`. Callers: `CommitReplaySave` (0x00429b60) and
`SaveScoreRecordFileEbx` (0x0042b1e0).

Okumura-style LZSS encoder:

- Parameters: ring `N = 0x2000` (`byte_48F868`, the same buffer the decoder
  0x00435dc0 uses; data starts at cursor one), lookahead `F = 18`,
  `THRESHOLD = 2`, 12-bit match positions, 4-bit length codes storing
  `length - 3`.
- Bitstream: MSB-first; a set flag bit precedes an 8-bit literal, a clear
  flag bit precedes 12 position bits then 4 length bits. The flag byte is
  flushed whenever the mask wraps under bit 0x01 (native `shr bl,1` /
  `test` / store sequence).
- Tree storage: one {parent, left, right} dword triple per ring position at
  `DAT_00477858 + 12 * node`; node 0 is NIL, node 0x2000 is the root
  sentinel whose right-child slot *is* `DAT_0048f860`. The zeroing pass
  clears nodes 0..0x1fff only; the root sentinel is then seeded with node 1
  (`DAT_0048f860 = 1`, `tree[1].parent = 0x2000`, children 0 — the explicit
  stores at 0x435a62..0x435a7c land inside node 1's fields).
- `InsertNode` (0x00436000, fastcall ECX = previous position [unused],
  EDX = position, stack = out match position): compares 18 ring bytes
  (native walks them in six 6-byte chunks), tracks the best position, and
  either attaches a new leaf or, on a full 18-byte match, evicts the
  previous owner and takes over its parent link and subtree. Returns the
  best length.
- `DeleteNode` (0x00436210, fastcall ECX): parent == 0 means absent. With
  both children present it splices in the rightmost node of the left
  subtree (0x00436330) after recursively deleting it; otherwise the single
  child (or NIL 0 for a leaf) moves up (0x00436260 — for a leaf the write
  goes into `tree[0].parent`, which is scratch — quirk). 0x004362b0 performs
  the two-children splice.
- Encoder loop (all signed compares): clamp `match_length` (and its saved
  copy `var_1c`) to the remaining length; literal when `<= 2`, match token
  otherwise. Per emitted symbol: delete the ring slot at
  `(r + 18) & 0x1fff`, consume one input byte into it (or decrement the
  remaining count once the input is exhausted), advance `r`, and re-insert
  while input remains. `match_length` resumes from the last insert result.
- Tail: one more clear flag bit plus twelve zero position bits; the mask
  wraps flush the pending flag byte.
- Output buffer is `malloc(2 * size)` and is never freed by the encoder;
  `*out_size` is only written on the success path (malloc failure returns
  null with `out_size` untouched).

### Preserved quirks

- The two `byte == -1` (getc EOF) checks are dead: bytes are zero-extended
  before the compare, so input is always fully consumed.
- The native reuses its match-length register (EBP) as scratch while setting
  flag bits in the match branch and reloads it from `var_1c` on every set
  length bit; the two always hold the same clamped value, so the reload is
  unobservable.
- Trailing partial-block / flag handling means an empty input still produces
  512 bytes of zero output (`*out_size = 512`).
- Negative `size` wraps through the unsigned `malloc(2 * size)`.

## 0x0044b220 — `ScrambleReplayImageUserpurgeAbi(key, buffer, size, key_step, block_size, size_again)`

Native `__userpurge` (initial key in AL; the five stack arguments). Callers:
replay writer passes `0x3D/0x7A/0x80` and `0xAA/0xE1/0x400`; score writer
passes `0xAC/0x35/0x10`. Native EAX returns the buffer (dropped by all
callers; the C++ ABI is `void`).

- Copies `min(size_again, size)` bytes to a `malloc` scratch buffer.
- The scrambled span is `size - (skipped_tail + (size & 1))` where
  `skipped_tail = (size % block_size >= block_size / 4) ? 0 : size % block_size`
  (signed ops) — a short trailing block is only transformed when it is less
  than a quarter block, and the odd trailing byte is always skipped.
- Per block of `n = min(block_size, remaining)` bytes the output sequence is
  the block's odd offsets in descending order (`n-1, n-3, ...`, `(n+1)/2`
  bytes) followed by its even offsets descending (`n-2, n-4, ...`, `n/2`
  bytes); each output byte is `key ^ source` and the key advances by
  `key_step` per emitted byte (mod 256).
- When `remaining < block_size` the native *permanently* overwrites its
  `block_size` parameter with `remaining`; unobservable because that block
  is always the last one — noted, preserved by using a local copy.

## Boundaries

- `malloc` / `free` / `memcpy` / `memset` — CRT platform boundaries.
- The callers' file-write pipeline is documented in
  `docs/evidence/replay-save.md` and `docs/evidence/score-save.md`.
