# Ending MIDI Block Loader (TH10 0x43aad0 / 0x43aa60)

Reconstruction: `src/EndingMidiBlockLoader.cpp`
(`LoadMidiBlockRowsEbxStackAbi` and `FreeMidiBlockRowsEsiAbi`).

Subsystem identification: the record is the ending music sequencer
already partially covered by `EndingMidiSequencer.cpp` (0x43b110
`InterpretEndingMidiEventBlockStackAbi`; the same cluster 0x43a730/
0x43aeb0 was reconstructed inline there). Neighbors confirm it: 0x43ad70
opens/closes midiOut (IAT 0x4662ac midiOutOpen, 0x4662a8 midiOutReset,
0x4662a4 midiOutClose), 0x43ad10 parses the big-endian varint delta of
each row at descriptor `+4` with the data cursor at `+0x14`, and
`+0x114`/`+0x13c` hold the midiOut handle pair.

## Native ABI

- 0x43aad0: EBX = sequencer record, stack = block index (ret 4);
  /GS-protected (cookie at 0x473660, check via 0x458ea5). Calls the row
  release helper 0x43aa60 unconditionally first. Returns 0 on success,
  -1 when the blob pointer is null.
- 0x43aa60: ESI = sequencer record; frees every row payload and the row
  table, then zeroes `+0x138` and `+0x118`.

## Sequencer record fields

- `+0x10` selected block index (set from the stack argument)
- `+0x98 + i*4` raw file blob pointers (loaded by 0x43a9b0 via the
  packed-file loader 0x44b360; 0x43ac90 loads slot 31)
- `+0x114` scratch buffer freed by the slot-31 loader
- `+0x118` row count, `+0x11c`/`+0x120` header words
- `+0x124` timing base, always reset to 1000000 (0xf4240)
- `+0x138` row descriptor table: `rows * 32` bytes; per row
  `{u32 used; u32 pad; u32 byte_size(+8); u32 pad; u8 *data(+0x10); ...}`;
  the playback side reads the varint delta at `+4` and the data cursor at
  `+0x14` (initialized by 0x43ace0/0x43ad10).

## Blob container (big-endian)

- `+0x00` dword (read but unused by the loader)
- `+0x04` dword; its big-endian low 16 bits (bytes at +6/+7) are the skip
  count applied after `+0x0e`
- `+0x08` u16 word 1 -> `+0x11c`
- `+0x0a` u16 row count -> `+0x118`
- `+0x0c` u16 word 3 -> `+0x120`
- `+0x0e + skip` row data

Each row: dword (read but unused), big-endian dword byte size, then that
many payload bytes. All words/dwords are byte-swapped by the native
through stack scratch bytes; the reconstruction uses explicit be16/be32
helpers.

## Loader body

1. Free previous rows (0x43aa60), fetch the blob at
   `+0x98 + index*4`; null blob -> return -1.
2. Publish the three big-endian header words into `+0x11c/+0x118/+0x120`.
3. malloc `rows * 0x20` bytes (0x452706) into `+0x138` and zero-fill.
4. Per row: `byte_size` = big-endian dword at cursor+4; descriptor gets
   `used = 1` at `+0`, `byte_size` at `+8`, and a fresh malloc payload at
   `+0x10` filled by the inline rep movs copy; the cursor advances by
   `8 + byte_size` and the descriptor offset by 0x20.
5. `+0x10` = block index, `+0x124` = 1000000, return 0.

## Verification notes

- Disassembly 0x43aad0-0x43ac8d (448 bytes), 0x43aa60-0x43aac2.
- Heap helpers: 0x452706 = malloc over the CRT heap handle at 0x477364
  (cdecl; the explicit `add esp,4` at 0x43ac50 confirms caller cleanup),
  0x452422 = free.
- Stack-slot cross-check: the descriptor offset counter and the loop
  counter live in distinct slots relative to the shifting esp (the malloc
  wrapper is cdecl, so the loop body runs at esp-4 after each call); this
  rules out an apparent "raw dword row-offset quirk" — the descriptors
  advance by exactly 0x20 per row.
