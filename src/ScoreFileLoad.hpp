#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Score-save state lifecycle loader side (the writer 0x0042b1e0 lives in
// src/ScoreSave.cpp). The 0x1dcb4-byte state is:
//   +0x0000  pointer to the loaded "scoreth10.dat" image (0x18-byte header
//            record: +0 magic 0x30314854 "TH10", +8 word version 3, +0xc
//            default 0x100, +0x10 packed size, +0x14 unpacked size; the
//            packed body starts at +0x18)
//   +0x0004  decompressed body scratch buffer
//   +0x0008  seven 0x437c-byte stage-record slots (12-byte sub-header:
//            +0 word magic 0x5243 "CR", +2 word 0, +4 checksum, +8 size
//            0x437c, +0xc stage index, payload from +0x10)
//   +0x1d86c 0x448-byte clear-data region (magic 0x5453 "ST", size 0x448)

// TH10 0x0044b0d0. Native __userpurge (initial key in AL; stack = buffer,
// size, key step, block size, size again). Exact inverse of
// ScrambleReplayImageUserpurgeAbi (0x0044b220, src/ReplayPackedCodec.cpp):
// copies min(size_again, size) bytes to a scratch buffer, then per block
// emits the scratch bytes linearly into the buffer's odd offsets in
// descending order followed by its even offsets in descending order, each
// output byte being the source byte XOR the running key (key advances by
// key_step per emitted byte). The scrambled span is
// size - (skipped_tail + (size & 1)) with
// skipped_tail = (size % block_size >= block_size / 4) ? 0 : size % block_size.
// When remaining < block_size the native permanently overwrites its
// block_size parameter with the remainder (unobservable — last block).
void UnscramblePackedImageUserpurgeAbi(u8 initial_key, void *buffer,
                                       i32 size, u8 key_step, i32 block_size,
                                       i32 size_again);

// TH10 0x0042b030. Native EBX = state, plain ret. Validates the loaded
// header record ([state]), unscrambles the packed body at header+0x18 with
// the 0xac/0x35-over-0x10 parameters, decompresses it into
// malloc(unpacked * 4) scratch published at state+4, then walks the section
// records: 0x5243 stage records are checksum-verified and copied into
// state+8 + record_index * 0x437c, 0x5453 clear-data records into
// state+0x1d86c. Any failure (or a null header) frees the header and
// installs a fresh blank "TH10" record. Returns 0 in every path.
i32 LoadScoreRecordFileEbx(void *save_state);

// TH10 0x0042ae60. Native EAX = zero-initialized state, returns it. Loads
// "scoreth10.dat" (mode 1) into the header pointer, seeds the clear-data
// region and the seven stage slots with their default records, then runs
// the file body loader 0x0042b030.
void *InitializeScoreSaveStateEaxAbi(void *state);

// TH10 0x0042af20. malloc(0x1dcb4) + 0x0042ae60, publishing DAT_0047783c
// (0 on any failure). SEH frame elided.
void *CreateScoreSaveState();

// TH10 0x0042af90. Frees the header record and scratch of the published
// state, the state itself, and clears DAT_0047783c.
void DestroyScoreSaveStateGlobal();

// TH10 0x0042afe0. Native ESI = state, stack = free flag (`ret 4`).
// Frees header and scratch, then the state itself when flag bit 0 is set.
void *ReleaseScoreSaveStateEsiStackAbi(void *state, i32 free_flag);

} // namespace th10
