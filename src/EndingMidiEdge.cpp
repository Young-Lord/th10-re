// TH10 ending-MIDI player edge functions around the already-registered
// loader/sequencer/player bodies (EndingMidiBlockLoader / EndingMidiPlayer /
// EndingMidiSequencer).
//
// Sequencer record layout reminders (from the registered modules):
//   +0x10 selected block index, +0x98+4i raw file blobs (32 slots),
//   +0x114 scratch, +0x138 row table, +0x140 timer id, +0x144 timer period,
//   +0x13c MIDI out handle, +0x148 running flag.
#include <stdlib.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"

namespace th10 {

void FreeMidiBlockRowsEsiAbi(void *sequencer);       // 0x43aa60 (registered)
i32 LoadMidiBlockRowsEbxStackAbi(void *sequencer, i32 block_index); // 0x43aad0

namespace {

extern "C" u32 TH10_STDCALL midiOutReset(void *handle);
extern "C" u32 TH10_STDCALL midiOutClose(void *handle);
extern "C" u32 TH10_STDCALL midiOutUnprepareHeader(void *handle,
                                                   void *header,
                                                   u32 header_size);
extern "C" i32 TH10_STDCALL timeKillEvent(u32 timer_id);
extern "C" i32 TH10_STDCALL timeEndPeriod(u32 milliseconds);

// TH10 0x0044b810: formatted error print (ECX = console FILE* 0x474f70).
extern void PrintErrorToConsoleEcxAbi(void *console, const char *format,
                                      const char *argument);

extern void *g_EndingMidiErrorConsole; // TH10 0x00474f70

const u32 k_player_vtable_running = 0x0046f80cU;
const u32 k_player_vtable_stopped = 0x0046f810U;
const u32 k_block_slot_count = 32U;
const u32 k_midi_header_size = 0x40U;

} // namespace

// TH10 0x0043aeb0 twin (the registered copy lives inside
// EndingMidiSequencer.cpp's anonymous namespace, so this module carries its
// own body): find the header in the 32-slot ring at +0x14, unprepare it and
// release the data pointer and the header itself.
static void ReleaseMidiHeaderRingEntry(void *record, void *header)
{
    u8 *ctx = static_cast<u8 *>(record);
    for (u32 slot = 0; slot < k_block_slot_count; ++slot) {
        void **ring = reinterpret_cast<void **>(ctx + 0x14U + 4U * slot);
        if (*ring != header)
            continue;
        *ring = 0;
        void *midi_out = *reinterpret_cast<void *const *>(ctx + 0x13cU);
        (void)midiOutUnprepareHeader(midi_out, header, k_midi_header_size);
        void *data = *reinterpret_cast<void *const *>(header);
        if (data != 0) {
            free(data);
            *reinterpret_cast<void **>(header) = 0;
        }
        free(header);
        return;
    }
}

// TH10 0x0043ae20. Native EDI = sequencer record. Side reset: releases
// every prepared SysEx header, stops the timer, closes the MIDI device and
// clears the selected block index. Returns -1 when no row table is loaded.
i32 ResetSequencerSideStateEaxAbi(void *sequencer)
{
    u8 *record = static_cast<u8 *>(sequencer);
    if (*reinterpret_cast<const u32 *>(record + 0x138U) == 0U)
        return -1;

    for (u32 slot = 0; slot < k_block_slot_count; ++slot) {
        void *header = *reinterpret_cast<void *const *>(
            record + 0x14U + 4U * slot);
        if (header != 0)
            ReleaseMidiHeaderRingEntry(record, header);
    }

    const u32 timer_id = *reinterpret_cast<const u32 *>(record + 0x4U);
    if (timer_id != 0U)
        (void)timeKillEvent(timer_id);
    (void)timeEndPeriod(*reinterpret_cast<const u32 *>(record + 0x8U));
    *reinterpret_cast<u32 *>(record + 0x4U) = 0;

    void *midi_out = *reinterpret_cast<void *const *>(record + 0x13cU);
    if (midi_out != 0) {
        (void)midiOutReset(midi_out);
        (void)midiOutClose(midi_out);
        *reinterpret_cast<void **>(record + 0x13cU) = 0;
    }
    *reinterpret_cast<u32 *>(record + 0x10U) = 0xffffffffU;
    return 0;
}

namespace {

} // namespace

// TH10 0x0043a910. Native EAX = player record. Tears the player down:
// side reset, row release, blob slots, MIDI device, timer and both time
// period restores (the second one repeats the same period value).
void DestroyEndingMidiPlayerEaxAbi(void *player)
{
    u8 *record = static_cast<u8 *>(player);
    *reinterpret_cast<u32 *>(record) = k_player_vtable_running;

    ResetSequencerSideStateEaxAbi(record);
    FreeMidiBlockRowsEsiAbi(record);

    void **slot = reinterpret_cast<void **>(record + 0x98U);
    for (u32 index = 0; index < k_block_slot_count; ++index) {
        if (*slot != 0) {
            free(*slot);
            *slot = 0;
        }
        *slot = 0;
        ++slot;
    }

    void *midi_out = *reinterpret_cast<void *const *>(record + 0x13cU);
    if (midi_out != 0) {
        (void)midiOutReset(midi_out);
        (void)midiOutClose(midi_out);
        *reinterpret_cast<void **>(record + 0x13cU) = 0;
    }

    const u32 timer_id = *reinterpret_cast<const u32 *>(record + 0x140U);
    *reinterpret_cast<u32 *>(record) = k_player_vtable_stopped;
    if (timer_id != 0U)
        (void)timeKillEvent(timer_id);
    const u32 period = *reinterpret_cast<const u32 *>(record + 0x144U);
    (void)timeEndPeriod(period);
    *reinterpret_cast<u32 *>(record + 0x140U) = 0;
    (void)timeEndPeriod(period);
}

// TH10 0x0043a9b0. Native EAX = sequencer record, ESI = slot index,
// stack = path (ret 4). Reloads raw file blob slot; when the slot is the
// currently selected block the side state is reset first. Failure prints
// "error : MIDI File %s" and returns -1.
i32 LoadEndingMidiFileBlobEaxEsiStackAbi(void *sequencer, u32 slot_index,
                                         const char *path)
{
    u8 *record = static_cast<u8 *>(sequencer);
    const u32 selected = *reinterpret_cast<const u32 *>(record + 0x10U);
    if (selected == slot_index)
        ResetSequencerSideStateEaxAbi(record);

    void **slot = reinterpret_cast<void **>(record + 0x98U + 4U * slot_index);
    if (*slot != 0) {
        free(*slot);
        *slot = 0;
    }
    *slot = 0;

    extern void *LoadMainChainFile(const char *path, u32 *file_size,
                                   i32 filesystem_mode);
    void *blob = LoadMainChainFile(path, 0, 0);
    *slot = blob;
    if (blob != 0)
        return 0;

    PrintErrorToConsoleEcxAbi(&g_EndingMidiErrorConsole, "error : MIDI File ",
                              path);
    return -1;
}

// TH10 0x0043ac90. Native AL = path, ECX = sequencer. Loads block 31 (the
// drum/percussion track) and drops the +0x114 scratch buffer afterwards.
i32 LoadEndingMidiPercussionEcxAbi(const char *path, void *sequencer)
{
    if (LoadEndingMidiFileBlobEaxEsiStackAbi(sequencer, 31U, path) != 0)
        return -1;
    (void)LoadMidiBlockRowsEbxStackAbi(sequencer, 31);

    u8 *record = static_cast<u8 *>(sequencer);
    void *scratch = *reinterpret_cast<void *const *>(record + 0x114U);
    if (scratch != 0) {
        free(scratch);
        *reinterpret_cast<void **>(record + 0x114U) = 0;
    }
    *reinterpret_cast<void **>(record + 0x114U) = 0;
    return 0;
}

// TH10 0x0043af00. Native EAX = sequencer record, EDX = span. Re-arms the
// tempo window state at +0x2c8..0x2e4.
void BeginEndingMidiTempoSpanEaxEdxAbi(void *sequencer, u32 span)
{
    u32 *record = static_cast<u32 *>(sequencer);
    record[178] = 0;      // +0x2c8 accumulator clear
    record[185] = span;   // +0x2e4 span
    record[186] = 0;      // +0x2e8
    record[183] = 0;      // +0x2dc
    record[184] = 1;      // +0x2e0
}

} // namespace th10
