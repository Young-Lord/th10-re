// TH10 0x43aad0 (+ row-release helper 0x43aa60) - MIDI block loader of the
// ending music player (see EndingMidiSequencer for the event interpreter
// and playback side, 0x43ad70 midiOutOpen/reset, 0x43ad10 varint parse).
//
// The sequencer record keeps an array of raw file blobs at +0x98 (loaded by
// 0x43a9b0 through the packed-file loader 0x44b360) and the parsed block
// table at +0x138:
//   +0x10  currently selected block index
//   +0x98 + i*4   raw file blob pointers
//   +0x114  scratch buffer (freed alongside)
//   +0x118  block/row count
//   +0x11c  block header word 1 (big-endian)
//   +0x120  block header word 3 (big-endian)
//   +0x124  timing base, always reset to 1000000
//   +0x138  row descriptor table: rows * 32 bytes, one
//           {u32 used; u32 pad; u32 byte_size; u32 pad; u8 *data; ...}
//           descriptor per row (the event side reads the varint delta at
//           +4 and the data cursor at +0x14)
//
// The file blob is a big-endian container:
//   +0x00 dword (read but unused by the loader)
//   +0x04 dword (big-endian; its low 16 bits = skip count after +0x0e)
//   +0x08 u16 word 1 -> +0x11c
//   +0x0a u16 row count -> +0x118
//   +0x0c u16 word 3 -> +0x120
//   +0x0e + skip      row data
// Each row: dword (read but unused), big-endian dword byte size, then that
// many payload bytes.
#include <string.h>

#include "Th10Types.hpp"

namespace th10 {

namespace {

// TH10 0x00452706 - malloc through the CRT heap handle at 0x477364.
void *MallocHeap452706(u32 bytes);
// TH10 0x00452422 - free.
void FreeHeap452422(void *block);
// TH10 0x0043ae20 - side reset invoked when the same index is reloaded.
void ResetSequencerSideStateEaxAbi(void *sequencer);

// Big-endian dword (row sizes are stored big-endian).
u32 LoadBE32(const u8 *base)
{
    return (static_cast<u32>(base[0]) << 24) | (static_cast<u32>(base[1]) << 16)
         | (static_cast<u32>(base[2]) << 8) | static_cast<u32>(base[3]);
}

// Big-endian u16 (the native packs it through stack scratch bytes).
u16 LoadBE16(const u8 *base)
{
    return static_cast<u16>(static_cast<u16>(base[0]) << 8
                            | static_cast<u16>(base[1]));
}

} // namespace

// TH10 0x43aa60. Native ESI = sequencer record. Frees every row payload,
// then the row descriptor table, and clears the count.
void FreeMidiBlockRowsEsiAbi(void *sequencer_raw)
{
    u8 *const sequencer = static_cast<u8 *>(sequencer_raw);
    const i32 rows = static_cast<i32>(
        *reinterpret_cast<const u32 *>(sequencer + 0x118U));
    u8 *const table = *reinterpret_cast<u8 **>(sequencer + 0x138U);

    if (rows > 0 && table != 0) {
        for (i32 index = 0; index != rows; ++index) {
            u8 *const descriptor = table + static_cast<u32>(index) * 0x20U;
            void *const data = *reinterpret_cast<void **>(descriptor + 0x10U);
            if (data != 0) {
                FreeHeap452422(data);
                *reinterpret_cast<void **>(descriptor + 0x10U) = 0;
            }
        }
    }

    if (table != 0)
        FreeHeap452422(table);
    *reinterpret_cast<void **>(sequencer + 0x138U) = 0;
    *reinterpret_cast<u32 *>(sequencer + 0x118U) = 0;
}

// TH10 0x43aad0. Native EBX = sequencer record, stack = block index
// (ret 4); /GS-protected. Parses the blob at sequencer+0x98+index*4 into
// the row table. Returns 0 on success, -1 when the blob pointer is null.
i32 LoadMidiBlockRowsEbxStackAbi(void *sequencer_raw, i32 block_index)
{
    u8 *const sequencer = static_cast<u8 *>(sequencer_raw);

    // Native calls the row release helper unconditionally first.
    FreeMidiBlockRowsEsiAbi(sequencer);

    const u32 blob = *reinterpret_cast<const u32 *>(
        reinterpret_cast<u32>(sequencer) + 0x98U
        + static_cast<u32>(block_index) * 4U);
    if (blob == 0U)
        return -1;

    const u8 *const data = reinterpret_cast<const u8 *>(blob);
    *reinterpret_cast<u32 *>(sequencer + 0x11cU) = LoadBE16(data + 8U);
    const u16 rows = LoadBE16(data + 0x0aU);
    *reinterpret_cast<u32 *>(sequencer + 0x118U) = rows;
    *reinterpret_cast<u32 *>(sequencer + 0x120U) = LoadBE16(data + 0x0cU);

    // Skip count = big-endian low word of the +4 dword.
    const u16 skip = LoadBE16(data + 6U);
    const u8 *cursor = data + 0x0eU + skip;

    // Row descriptor table: rows * 32 bytes, zero-filled.
    u8 *const table = static_cast<u8 *>(MallocHeap452706(rows * 0x20U));
    *reinterpret_cast<u8 **>(sequencer + 0x138U) = table;
    memset(table, 0, static_cast<u32>(rows) * 0x20U);

    u32 descriptor_offset = 0U;
    for (u16 row = 0; row != rows; ++row) {
        const u32 byte_size = LoadBE32(cursor + 4U);
        u8 *const descriptor = table + descriptor_offset;

        *reinterpret_cast<u32 *>(descriptor + 8U) = byte_size;
        u8 *const payload = static_cast<u8 *>(MallocHeap452706(byte_size));
        *reinterpret_cast<u32 *>(descriptor + 0x10U)
            = reinterpret_cast<u32>(payload);
        *reinterpret_cast<u32 *>(descriptor) = 1U;
        memcpy(payload, cursor + 8U, byte_size);

        cursor += 8U + byte_size;
        descriptor_offset += 0x20U;
    }

    *reinterpret_cast<u32 *>(sequencer + 0x10U)
        = static_cast<u32>(block_index);
    *reinterpret_cast<u32 *>(sequencer + 0x124U) = 0xf4240U; // 1000000
    return 0;
}

} // namespace th10
