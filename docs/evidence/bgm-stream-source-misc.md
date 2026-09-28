# BGM stream source misc

Covers TH10 0x0044dbb0 — added to module `src/BgmRuntime.{cpp,hpp}`
(struct `BgmStreamSourceLayout`, the 0x94-byte stream source owned by
`TransitionSoundAdapterLayout::owned_auxiliary`).

## 0x0044dbb0 ClearBgmStreamSourceStateEaxAbi

Native EAX = stream source. Four dwords are zeroed in the native write
order: the descriptor slot at +0x90, the record head at +0x00, the
+0x2c counter (`initial_byte_count`), and the memory-backed flag at
+0x7c. Every other field — the file handle (+0x8c), memory pointers
(+0x80/+0x84/+0x88), ownership mode (+0x78) and the remaining
bookkeeping — is deliberately left stale, matching the native teardown
helper that sits between `RewindBgmStreamAdapter` (0x44dad0) and
`InitializeBgmStreamSource` (0x44dbf0). No direct cross-references
remain in the retail binary; registered from the neighborhood and field
usage (the follow-on native at 0x44dbd0 closes the +0x8c handle when
+0x78 == 1).

## Verification

Reference disassembly `build/reference/0044dbb0_sub_44DBB0.asm`; layout
cross-checked against `InitializeBgmStreamSource` (a4[30..36] dword
indices) and `RewindBgmStreamSource`/`CopyBgmStreamSourceData` field
accesses.
