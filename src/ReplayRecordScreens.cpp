// TH10 0x00422660 — draw body of the replay-save / replay-viewer screens
// over the 0x3ac-byte calculation record, plus the shared name-entry grid
// renderer 0x004224c0. The record fields used here mirror the calculation
// side (0x004236f0): +0x14 timer, +0x24 cursor, +0x1e8 replay-present gate,
// +0x1ec the 25 parsed replay records (each record's +0x18 points at a
// replay header: +0x00 name, +0x0c time_t, +0x50 chara, +0x54 shot,
// +0x58 rank, +0x5c stage) and +0x2b4 the typed name.
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "AsciiManager.hpp"
#include "ReplayRecordScreens.hpp"
#include "Th10Types.hpp"

namespace th10 {

namespace {

extern void *g_AsciiManagerHost; // TH10 DAT_004776e0

// TH10 DAT_00474c68/6c/74 (current character, character slot and shot) and
// TH10 DAT_00474c7c (current stage).
const u32 kRunChara = 0x474C68U;
const u32 kRunCharaSlot = 0x474C6CU;
const u32 kRunShot = 0x474C74U;
const u32 kRunStage = 0x474C7CU;
const u32 kGameModeObject = 0x477838U; // DAT_00477838
const u32 kScoreSaveState = 0x47783CU; // DAT_0047783c

// Name tables (pointer arrays in .rdata).
const u32 kReplayCharaNames = 0x4746DCU; // off_4746dc
const u32 kReplayRankNames = 0x474708U;  // off_474708 (detail/ranking table)
const u32 kReplayStageNames = 0x474744U; // off_474744
const u32 kScoreStageNames = 0x47471CU;  // off_47471c (ranking table)

u32 LoadU32(u32 address)
{
    return *reinterpret_cast<const u32 *>(address);
}

u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

i32 LoadI32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const i32 *>(
        static_cast<const u8 *>(base) + offset);
}

const char *LoadNamePointer(u32 table, u32 index)
{
    return *reinterpret_cast<const char *const *>(table + 4U * index);
}

// Branchless native row color (setnz/dec/and 0x7f7e80/add 0xff808080):
// the selected row turns 0xffffff00, everything else 0xff808080.
u32 RowColor(u32 selected, u32 index)
{
    return (selected != index) ? 0xFF808080U : 0xFFFFFF00U;
}

i32 StrLen(const char *text)
{
    return static_cast<i32>(strlen(text));
}

} // namespace

// TH10 0x004224c0. Native `retn 0x10` stdcall (record, x, y, z).
void DrawNameEntryGridStackAbi(void *record, float x, float y, float z)
{
    AsciiManager &ascii = *static_cast<AsciiManager *>(g_AsciiManagerHost);
    u8 *const bytes = static_cast<u8 *>(record);

    const char *const charset = *reinterpret_cast<const char *const *>(
        0x4746D8U); // off_4746d8 (pointer to the alphabet template)
    const i32 length = StrLen(charset);

    // The typed name at the caller-supplied position.
    Float3 position;
    position.x = x;
    position.y = y;
    position.z = z;
    ascii.AddFormatText(&position, "%s",
                        reinterpret_cast<const char *>(bytes + 0x2B4U));

    // Insertion caret: x + 9 * cursor, pulled back one cell at cursor 8
    // (TH10 flt_470c10 = 9.0).
    const i32 name_cursor =
        *reinterpret_cast<const i32 *>(bytes + 0x1E0U);
    float caret_x = x + static_cast<float>(name_cursor * 9);
    if (name_cursor == 8) {
        caret_x -= 9.0f;
    }
    Float3 caret_position;
    caret_position.x = caret_x;
    caret_position.y = y;
    caret_position.z = z;
    ascii.color = 0xFFFFFF00U;
    ascii.AddFormatText(&caret_position, "_");
    ascii.color = 0xFFFFFFFFU;

    // The alphabet grid is drawn at a fixed origin (112, 320), 13 columns,
    // stepping x by 18 (flt_470c0c) and y by 16 (flt_470b48) every 13th
    // cell. The last three cells are the space/delete/END markers encoded
    // as bytes 0x81 / 0x7f / 0x80.
    Float3 grid_position;
    grid_position.x = 112.0f; // TH10 0x42e00000
    grid_position.y = 320.0f; // TH10 0x43a00000
    grid_position.z = 0.0f;
    for (i32 index = 0; index < length; ++index) {
        const u32 grid_cursor = *reinterpret_cast<const u32 *>(bytes + 0xFCU);
        ascii.color = RowColor(grid_cursor, static_cast<u32>(index));
        i32 cell;
        if (index < length - 3) {
            cell = static_cast<signed char>(charset[index]);
        } else if (index == length - 3) {
            cell = 0x81;
        } else {
            cell = 0x7f + (index != length - 2 ? 1 : 0);
        }
        ascii.AddFormatText(&grid_position, "%c", cell);
        if (index % 13 == 12) {
            grid_position.x = 112.0f;
            grid_position.y += 16.0f;
        } else {
            grid_position.x += 18.0f;
        }
    }
    ascii.color = 0xFFFFFFFFU;
}

// TH10 0x00422660. Native `retn 4` stdcall draw body.
i32 DrawReplayRecordScreensStackAbi(void *record)
{
    AsciiManager &ascii = *static_cast<AsciiManager *>(g_AsciiManagerHost);
    u8 *const bytes = static_cast<u8 *>(record);
    ascii.text_mode = 1; // +0x8988

    // Native byte table at 0x422a7c over mode-10: modes 10/17 -> 0, 11/18
    // -> 1, 12/19 -> 2, 13..16 -> the bare epilogue.
    static const u8 kModeMap[10] = {0, 1, 2, 3, 3, 3, 3, 0, 1, 2};
    const u32 selector = static_cast<u32>(LoadI32At(bytes, 0x04U)) - 10U;

    if (selector <= 9U) {
        switch (kModeMap[selector]) {
        case 0: { // ---------------------------------------- replay list
            Float3 position;
            position.x = 48.0f; // TH10 0x42400000
            position.y = 64.0f; // TH10 0x42800000
            position.z = 0.0f;
            const u32 selected = LoadU32At(bytes, 0x24U);
            u32 *const slots = reinterpret_cast<u32 *>(bytes + 0x1ECU);
            for (u32 row = 0; row < 25U; ++row) {
                ascii.color = RowColor(selected, row);
                const u32 parsed = slots[row];
                if (parsed != 0U) {
                    const u8 *const header =
                        *reinterpret_cast<u8 *const *>(parsed + 0x18U);
                    time_t stamp =
                        *reinterpret_cast<const time_t *>(header + 0x0CU);
                    const tm *const date = localtime(&stamp);
                    const u32 chara =
                        *reinterpret_cast<const u32 *>(header + 0x50U);
                    const u32 shot =
                        *reinterpret_cast<const u32 *>(header + 0x54U);
                    const u32 rank =
                        *reinterpret_cast<const u32 *>(header + 0x58U);
                    const u32 stage =
                        *reinterpret_cast<const u32 *>(header + 0x5CU);
                    ascii.AddFormatText(
                        &position,
                        "No.%.2d %s %.2d/%.2d/%.2d %s %s %s",
                        static_cast<i32>(row) + 1,
                        reinterpret_cast<const char *>(header),
                        date->tm_year % 100, date->tm_mon + 1,
                        date->tm_mday,
                        LoadNamePointer(kReplayCharaNames, 3U * chara + shot),
                        LoadNamePointer(kReplayRankNames, rank),
                        LoadNamePointer(kReplayStageNames, stage));
                } else {
                    ascii.AddFormatText(
                        &position,
                        "No.%.2d ------------ --/--/-- ----- - St-",
                        static_cast<i32>(row) + 1);
                }
                position.y += 15.0f; // TH10 flt_470c08
            }
            break;
        }

        case 1: { // --------------------------------- selected replay detail
            const i32 timer =
                *reinterpret_cast<const i32 *>(bytes + 0x14U);
            const i32 cursor = static_cast<i32>(LoadU32At(bytes, 0x24U));
            float y;
            if (timer < 10) {
                // Scroll-in: y = (224 - (cursor*15 + 64)) * timer * 0.1
                //             + (cursor*15 + 64)
                // (TH10 flt_470b4c / flt_470bc8 / flt_470c18).
                const float base = static_cast<float>(cursor) * 15.0f
                                   + 64.0f;
                y = (224.0f - base) * static_cast<float>(timer) * 0.1f
                    + base;
            } else {
                y = 224.0f; // TH10 0x43600000
            }

            // Name entry overlay (native Float3 (102, y, 0); 0x42cc0000).
            DrawNameEntryGridStackAbi(record, 102.0f, y, 0.0f);

            // Detail line of the currently loaded replay header: the
            // unchecked chain DAT_00477838 -> +0x18 (quirk shared with the
            // replay writer).
            const u8 *const save_context =
                reinterpret_cast<const u8 *>(LoadU32(kGameModeObject));
            const u8 *const header = reinterpret_cast<const u8 *>(
                *reinterpret_cast<const u32 *>(save_context + 0x18U));
            time_t stamp =
                *reinterpret_cast<const time_t *>(header + 0x0CU);
            const tm *const date = localtime(&stamp);
            const u32 chara =
                *reinterpret_cast<const u32 *>(header + 0x50U);
            const u32 shot = *reinterpret_cast<const u32 *>(header + 0x54U);
            const u32 rank = *reinterpret_cast<const u32 *>(header + 0x58U);

            Float3 position;
            position.x = 48.0f;
            position.y = y;
            position.z = 0.0f;
            ascii.AddFormatText(
                &position,
                "No.%.2d         %.2d/%.2d/%.2d %s %s %s",
                cursor + 1, date->tm_year % 100, date->tm_mon + 1,
                date->tm_mday,
                LoadNamePointer(kReplayCharaNames, 3U * chara + shot),
                LoadNamePointer(kReplayRankNames, rank),
                LoadNamePointer(kReplayStageNames, LoadU32(kRunStage)));
            break;
        }

        case 2: { // ---------------------------------------- score ranking
            Float3 heading;
            heading.x = 48.0f;
            heading.y = 64.0f;
            heading.z = 0.0f;
            ascii.AddFormatText(&heading, "            Score Ranking!!");

            const i32 cursor = static_cast<i32>(LoadU32At(bytes, 0x24U));
            const float y = static_cast<float>(cursor) * 18.0f + 96.0f;
            if (*reinterpret_cast<const i32 *>(bytes + 0x1E8U) == 0) {
                // Name entry overlay at (75, y, 0) while no replay gates it
                // (TH10 0x42960000 = 75.0).
                DrawNameEntryGridStackAbi(record, 75.0f, y, 0.0f);
            }
            const u32 selected =
                (*reinterpret_cast<const i32 *>(bytes + 0x1E8U) != 0)
                    ? 0xFFFFFFFFU
                    : 0U;

            Float3 position;
            position.x = 48.0f;
            position.y = 96.0f; // TH10 0x42c00000
            position.z = 0.0f;
            const u32 block =
                3U * LoadU32(kRunChara) + LoadU32(kRunCharaSlot);
            const u32 shot = LoadU32(kRunShot);
            const u8 *const save =
                reinterpret_cast<const u8 *>(LoadU32(kScoreSaveState));
            for (u32 row = 0; row < 10U; ++row) {
                ascii.color = RowColor(selected, row);
                // One 24-byte record per (row, shot) cell of the character
                // block: +0x18 score, +0x1c stage byte, +0x1d byte, +0x1e
                // 9-byte name, +0x28 time_t.
                const u32 offset =
                    block * 0x437CU + 24U * (row + 10U * shot);
                const u8 *const entry = save + offset;
                const u32 stamp =
                    *reinterpret_cast<const u32 *>(entry + 0x28U);
                if (stamp != 0U) {
                    time_t seen = static_cast<time_t>(stamp);
                    const tm *const date = localtime(&seen);
                    const signed char stage_byte =
                        static_cast<signed char>(entry[0x1CU]);
                    ascii.AddFormatText(
                        &position, "%2d %s %.9ld%d %.2d/%.2d/%.2d %s",
                        static_cast<i32>(row) + 1,
                        reinterpret_cast<const char *>(entry + 0x1EU),
                        static_cast<long>(
                            *reinterpret_cast<const u32 *>(entry + 0x18U)),
                        static_cast<i32>(
                            static_cast<signed char>(entry[0x1DU])),
                        date->tm_year % 100, date->tm_mon + 1,
                        date->tm_mday,
                        LoadNamePointer(kScoreStageNames,
                                        static_cast<u32>(stage_byte)));
                } else {
                    ascii.AddFormatText(
                        &position, "%2d %s %.9ld%d --/--/-- Stage -",
                        static_cast<i32>(row) + 1,
                        reinterpret_cast<const char *>(entry + 0x1EU),
                        static_cast<long>(*reinterpret_cast<const u32 *>(
                            entry + 0x18U)),
                        static_cast<i32>(
                            static_cast<signed char>(entry[0x1DU])));
                }
                position.y += 18.0f; // TH10 flt_470c0c
            }
            break;
        }

        default: // modes 13..16: bare epilogue
            break;
        }
    }

    ascii.color = 0xFFFFFFFFU;
    ascii.text_mode = 0;
    return 1;
}

} // namespace th10
