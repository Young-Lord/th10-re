// Score-file formats: the per-run high-score insertion (TH10 0x00421fa0),
// the scoreth10.dat section loader/decoder (0x00434dd0 with its 0x00434f30
// lookup helper) and the manager-draw body that renders the score/replay
// screens (0x00433b30).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "AsciiManager.hpp"
#include "PackedArchive.hpp"
#include "ScoreFileFormats.hpp"
#include "Th10Types.hpp"

namespace th10 {

namespace {

// ASCII-only case-insensitive compare; the native code calls msvcrt _stricmp
// for the scoreth10.dat section-name lookup.
int StrIcmp(const char *a, const char *b) {
    for (;;) {
        int ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
        if (ca != cb)
            return ca - cb;
        if (ca == 0)
            return 0;
    }
}

// TH10 DAT_00474C44 (current run score) and friends; DAT_00477708 holds the
// slow-rate doubles at +0x24/+0x2c.
const u32 kCurrentRunScore = 0x474C44U;   // DAT_00474C44
const u32 kScoreCharaSlot = 0x474C74U;    // DAT_00474C74
const u32 kScoreByte7C = 0x474C7CU;       // DAT_00474C7C
const u32 kScoreByte90 = 0x474C90U;       // DAT_00474C90
const u32 kSlowRateStats = 0x477708U;     // DAT_00477708
const u32 kGameModeObject = 0x477838U;    // DAT_00477838

extern void *g_AsciiManagerHost; // TH10 DAT_004776E0

extern const char *g_ReplayCharacterNames[]; // TH10 off_4746DC
extern const char *g_ReplayRankNames[];      // TH10 off_4746F4
extern const char *g_ReplayStageNames[];     // TH10 off_474744
extern const char *g_ScoreStageAllName;      // TH10 off_474764 ("All")

// TH10 asc_46E354: the eight-space blank name.
const char kBlankName[9] = "        ";

u32 LoadU32(u32 address)
{
    return *reinterpret_cast<const u32 *>(address);
}

i32 LoadI32(u32 address)
{
    return *reinterpret_cast<const i32 *>(address);
}

// TH10 0x0044b0d0 (reconstructed in PackedArchive.cpp): XOR stream transform
// with the DAT_00474bd8 key record selected by the entry-name byte sum.
void TransformPackedBytesInPlace(const PackedArchiveEntry *entry, u8 *bytes,
                                 u32 byte_count);

// TH10 0x00435dc0 (reconstructed in PackedArchive.cpp): LZSS-style decoder.
u8 *DecompressPackedBytes(const u8 *encoded, u32 encoded_size, u8 *output,
                          u32 output_size);

// Native branchless selected-row color: setnz/dec/and 0x7F7E80/add
// 0xFF808080 — a selected entry turns 0xFF808080, everything else
// 0xFFFFFF00.
u32 EntryColor(u32 selected, u32 index)
{
    return (selected != index) ? 0xFF808080U : 0xFFFFFF00U;
}

// Replay/score header fields shared by both screen bodies:
//   +0x00 name, +0x0c time_t, +0x48 slow-rate float, +0x50 chara,
//   +0x54 shot type, +0x58 rank, +0x5c final stage value.
void AppendScoreLine(AsciiManager &ascii, const Float3 &position,
                     i32 entry_number, const char *name,
                     const time_t &stamp, u32 chara, u32 shot, u32 rank,
                     const char *stage_name, float slow_rate)
{
    time_t captured = stamp;
    tm *date = localtime(&captured);
    ascii.AddFormatText(
        &position, "No.%.2d %s %.2d/%.2d/%.2d %.2d:%.2d %s %s %s %2.1f%%",
        entry_number, name, date->tm_year % 100, date->tm_mon + 1,
        date->tm_mday, date->tm_hour, date->tm_min,
        g_ReplayCharacterNames[3 * chara + shot], g_ReplayRankNames[rank],
        stage_name, static_cast<double>(slow_rate));
}

} // namespace

// TH10 0x00421fa0. Native EDI-ABI body.
i32 InsertScoreRecordEdi(void *score_table)
{
    u8 *const table = static_cast<u8 *>(score_table);
    const i32 chara = static_cast<i32>(LoadU32(kScoreCharaSlot));
    i32 score = LoadI32(kCurrentRunScore);
    u8 *const character_block = table + 240U * static_cast<u32>(chara);

    // Rank the run: skip every stored score still strictly greater.
    i32 rank = 0;
    while (rank < 10) {
        if (LoadI32(reinterpret_cast<u32>(character_block +
                                          24U * static_cast<u32>(rank) +
                                          0x10U)) <= score)
            break;
        ++rank;
    }
    if (rank >= 10)
        return -1;

    // Shift records rank..8 down one slot. Only the 24-byte payload region
    // at +0x10 moves; the 16 bytes ahead of each record stay untouched.
    for (i32 slot = 9; slot > rank; --slot) {
        memcpy(character_block + 24U * static_cast<u32>(slot) + 0x10U,
               character_block + 24U * static_cast<u32>(slot - 1) + 0x10U,
               24);
        // Native reloads DAT_00474C74 each iteration (cannot change).
    }

    u8 *const record =
        character_block + 24U * static_cast<u32>(rank);

    // Native reloads DAT_00474C44 here (same value as the scan used).
    score = LoadI32(kCurrentRunScore);
    *reinterpret_cast<i32 *>(record + 0x10) = score;
    record[0x15] = static_cast<u8>(LoadU32(kScoreByte90));
    record[0x14] = static_cast<u8>(LoadU32(kScoreByte7C));
    time(reinterpret_cast<time_t *>(record + 0x20));
    memcpy(record + 0x16, kBlankName, sizeof(kBlankName));

    // Unchecked dereference of DAT_00477708 — quirk preserved.
    const u8 *const stats =
        reinterpret_cast<const u8 *>(LoadU32(kSlowRateStats));
    const double slow_rate =
        100.0 - *reinterpret_cast<const double *>(stats + 0x24) /
                    *reinterpret_cast<const double *>(stats + 0x2c) * 100.0;
    *reinterpret_cast<float *>(record + 0x24) =
        static_cast<float>(slow_rate);

    return rank;
}

namespace {

// TH10 0x00434f30 (native __usercall: EAX = section-list object, EBX =
// name). Case-insensitive linear search over 0x10-byte entries whose first
// dword is the name; returns the entry or 0.
const u32 *FindSectionEntryByNameEaxEbxAbi(const void *section_list,
                                           const char *name)
{
    const u32 *entries =
        *reinterpret_cast<const u32 *const *>(section_list);
    if (entries == 0)
        return 0;
    i32 count = *reinterpret_cast<const i32 *>(
        static_cast<const u8 *>(section_list) + 4);
    if (count <= 0)
        return 0;
    for (;;) {
        if (StrIcmp(name,
                       reinterpret_cast<const char *>(entries[0])) == 0)
            return entries;
        --count;
        entries += 4;
        if (count <= 0)
            return 0;
    }
}

} // namespace

// TH10 0x00434dd0. Native ECX + two stack args (retn 8) body.
void *DecodePackedSectionEcxStackAbi(const char *section_name,
                                     const void *section_list,
                                     void *out_buffer)
{
    const u8 *const list = static_cast<const u8 *>(section_list);

    // Native checks the reader pointer before anything else.
    if (*reinterpret_cast<void *const *>(list + 0x0c) == 0)
        return 0;

    const u32 *const entry =
        FindSectionEntryByNameEaxEbxAbi(section_list, section_name);
    if (entry == 0)
        return 0;

    // Packed size = entry[+0x14] - entry[+4]. Because the lookup strides
    // 0x10 bytes, +0x14 is the *next* entry's start offset: sections are
    // contiguous and the last entry reads one dword past the table — native
    // quirk preserved.
    const u32 packed_size = entry[5] - entry[1];
    const u32 unpacked_size = entry[2];

    // Reuse the caller's buffer only when the section is stored unpacked;
    // otherwise (or with no buffer) a scratch block is allocated.
    u8 *block;
    if (packed_size != unpacked_size || out_buffer == 0) {
        block = static_cast<u8 *>(malloc(packed_size));
        if (block == 0)
            return 0;
    } else {
        block = static_cast<u8 *>(out_buffer);
    }

    void *const reader = *reinterpret_cast<void *const *>(list + 0x0c);
    typedef i32 (*VirtualMethod)(void *self, u32 argument, u32 extra);
    void **const vtable = *reinterpret_cast<void ***>(reader);
    const VirtualMethod seek_to_offset =
        reinterpret_cast<VirtualMethod>(vtable[6]); // vtable+0x18
    const VirtualMethod read_bytes =
        reinterpret_cast<VirtualMethod>(vtable[2]); // vtable+0x08

    if (seek_to_offset(reader, entry[1], 0) == 0 ||
        read_bytes(reader, reinterpret_cast<u32>(block), packed_size) == 0) {
        // Native frees the block here even when it aliased the caller's
        // out_buffer — quirk preserved.
        if (block != 0)
            free(block);
        return 0;
    }

    // Unscramble with the key record picked by the name byte sum; the
    // section entries reuse the packed-archive entry layout.
    const char *const name = reinterpret_cast<const char *>(entry[0]);
    PackedArchiveEntry key_entry;
    key_entry.owned_name = const_cast<char *>(name);
    key_entry.stored_offset = entry[1];
    key_entry.declared_size = entry[2];
    key_entry.field_0c = entry[3];
    TransformPackedBytesInPlace(&key_entry, block, packed_size);

    void *result;
    if (packed_size == unpacked_size)
        result = block;
    else
        result = DecompressPackedBytes(block, packed_size,
                                       static_cast<u8 *>(out_buffer),
                                       unpacked_size);

    if (block != out_buffer && block != 0)
        free(block);
    return result;
}

// TH10 0x00433b30. Native stdcall (retn 4) body.
i32 RunManagerDrawBody10(void *manager)
{
    u8 *const bytes = static_cast<u8 *>(manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    AsciiManager &ascii = *static_cast<AsciiManager *>(g_AsciiManagerHost);

    const i32 mode = static_cast<i32>(words[0x20 / 4]);
    if (mode == 2) {
        // Replay list: 25 rows, x = 56, y starting at 80 stepping 15.
        ascii.text_mode = 1;
        Float3 position;
        position.x = 56.0f;
        position.y = 80.0f;
        position.z = 0.0f;

        const u32 selected_row = words[0x24 / 4];
        u32 *const record_slots =
            words + (0x59e4 / 4);
        for (u32 row = 0; row < 25; ++row) {
            ascii.color = EntryColor(selected_row, row);
            u32 *const record =
                reinterpret_cast<u32 *>(record_slots[row]);
            if (record != 0) {
                const u8 *const header =
                    reinterpret_cast<const u8 *>(record[0x18 / 4]);
                AppendScoreLine(
                    ascii, position, static_cast<i32>(row) + 1,
                    reinterpret_cast<const char *>(header),
                    *reinterpret_cast<const time_t *>(header + 0x0c),
                    *reinterpret_cast<const u32 *>(header + 0x50),
                    *reinterpret_cast<const u32 *>(header + 0x54),
                    *reinterpret_cast<const u32 *>(header + 0x58),
                    g_ReplayStageNames[*reinterpret_cast<const u32 *>(
                        header + 0x5c)],
                    *reinterpret_cast<const float *>(header + 0x48));
            } else {
                ascii.AddFormatText(
                    &position,
                    "No.%.2d -------- --/--/-- --:-- ------- ------- ---"
                    " ---%%",
                    static_cast<i32>(row) + 1);
            }
            position.y += 15.0f; // TH10 flt_470C08
        }
    } else if (mode == 3) {
        // Selected high-score detail line.
        const i32 selected = static_cast<i32>(words[0x59dc / 4]);
        Float3 position;
        position.x = 56.0f;
        position.y = 240.0f; // TH10 flt_470BFC
        position.z = 0.0f;
        if (static_cast<i32>(words[0x2b4 / 4]) < 10) {
            // Scroll-in animation: y = (10 - mgr+0x2B8) * (15*sel + 0x50 -
            // 240) * 0.1 + 240 (TH10 flt_470C1C / flt_470C18).
            const float animated =
                (10.0f - *reinterpret_cast<const float *>(bytes + 0x2b8)) *
                (static_cast<float>(15 * selected + 0x50) - 240.0f) * 0.1f;
            position.y = animated + 240.0f;
        }

        // Unchecked dereference chain DAT_00477838 -> +0x18 — quirk shared
        // with the replay writer.
        const u8 *const save_context =
            reinterpret_cast<const u8 *>(LoadU32(kGameModeObject));
        const u8 *const header =
            reinterpret_cast<const u8 *>(
                *reinterpret_cast<const u32 *>(save_context + 0x18));
        AppendScoreLine(
            ascii, position, selected + 1, kBlankName,
            *reinterpret_cast<const time_t *>(header + 0x0c),
            *reinterpret_cast<const u32 *>(header + 0x50),
            *reinterpret_cast<const u32 *>(header + 0x54),
            *reinterpret_cast<const u32 *>(header + 0x58),
            g_ScoreStageAllName,
            *reinterpret_cast<const float *>(header + 0x48));

        if (static_cast<i32>(words[0x2b4 / 4]) >= 10) {
            // Comment editor: the entered name plus a grid of the alphabet
            // template with an insertion caret.
            const char *const comment =
                *reinterpret_cast<char *const *>(0x4746D8U);
            const i32 length = static_cast<i32>(strlen(comment));
            char *const name_buffer =
                reinterpret_cast<char *>(bytes + 0x58dc);
            const i32 cursor = static_cast<i32>(words[0x58e8 / 4]);
            const u32 grid_cursor = words[0x58f4 / 4];

            ascii.color = 0xFFFFFFFFU;
            Float3 line_position;
            line_position.x = 112.0f; // TH10 0x42E00000
            line_position.y = 240.0f;
            line_position.z = 0.0f;
            ascii.AddFormatText(&line_position, "%s", name_buffer);

            Float3 caret_position;
            caret_position.x = static_cast<float>(cursor * 9) + 112.0f;
            if (cursor == 8)
                caret_position.x -= 9.0f; // TH10 flt_470C10
            caret_position.y = 240.0f;
            caret_position.z = 0.0f;
            ascii.color = 0xFFFFFF00U;
            ascii.AddFormatText(&caret_position, "_");

            ascii.color = 0xFFFFFFFFU;
            Float3 cell_position;
            cell_position.x = 212.0f; // TH10 0x43540000
            cell_position.y = 360.0f; // TH10 0x43B40000
            cell_position.z = 0.0f;
            for (i32 index = 0; index < length; ++index) {
                ascii.color = EntryColor(grid_cursor,
                                         static_cast<u32>(index));
                char cell;
                if (index < length - 3) {
                    cell = comment[index];
                } else if (index == length - 3) {
                    cell = static_cast<char>(0x81);
                } else {
                    // Last two cells: 0x80 then 0x81.
                    cell = static_cast<char>(0x7f +
                                             (index != length - 2 ? 1 : 0));
                }
                ascii.AddFormatText(&cell_position, "%c",
                                    static_cast<i32>(cell));
                if (static_cast<u32>(index) % 13U == 12U) {
                    cell_position.x = 212.0f;
                    cell_position.y += 16.0f; // TH10 flt_470B48
                } else {
                    cell_position.x += 18.0f; // TH10 flt_470C0C
                }
            }
        }
    } else {
        return 1; // other sub-states draw nothing here
    }

    ascii.color = 0xFFFFFFFFU;
    ascii.text_mode = 0;
    return 1;
}

} // namespace th10
