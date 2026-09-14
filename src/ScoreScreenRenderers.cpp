// Score-screen renderers: the four draw bodies dispatched by the
// game-manager draw controller 0x0042d260 for states 9 / 0xB / 0xC / 0xF
// (TH10 0x00431410, 0x004329f0, 0x00431ba0, 0x00433230). The sibling
// state-0x10 body 0x00433b30 lives in ScoreFileFormats.cpp.
//
// Score-record addressing: every renderer reads the same 24-byte record
// payload described by InsertScoreRecordEdi (score +0x10, DAT_00474c7c byte
// +0x14, DAT_00474c90 byte +0x15, 9-byte name +0x16, time_t +0x20, slow-rate
// float +0x24) through a base pointer that sits eight bytes below the
// payload start. Throughout this file `record` is that native base R, so the
// payload fields appear at R+0x18 (score), R+0x1c / R+0x1d (the two byte
// fields), R+0x1e (name), R+0x28 (time) and R+0x2c (slow rate); a record
// counts as occupied when the time dword at R+0x28 is non-zero.
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "AsciiManager.hpp"
#include "ScoreScreenRenderers.hpp"
#include "Th10Types.hpp"

namespace th10 {

namespace {

// Shared render/globals (same externs the sibling modules declare).
extern void *g_AsciiManagerHost;   // TH10 DAT_004776e0
extern void *g_ScoreSaveState;     // TH10 DAT_0047783c
extern u32 g_PlayerCharacter;      // TH10 DAT_00474c68 (0=Reimu, 1=Marisa)
extern u32 g_PlayerShotType;       // TH10 DAT_00474c6c
extern u32 g_CurrentDifficulty;    // TH10 DAT_00474c74

extern const char *g_ReplayCharacterNames[]; // TH10 off_4746dc
extern const char *g_ReplayRankNames[];      // TH10 off_4746f4
extern const char *g_ReplayStageNames[];     // TH10 off_474744
// Long stage-name table (TH10 off_474718: "X", "test  ", "Stage 1".."Stage
// 6", "Extra  "). The renderers read its dwords from 0x47471c onward, i.e.
// element [stage_byte + 1]; a negative signed stage byte indexes before the
// table exactly as the native code does.
extern const char *g_ScoreStageNames[9];     // TH10 off_474718
// Pointer variable at TH10 off_4746d8; it holds the 76-character alphabet
// template used by the name-entry grid.
extern const char *g_AlphabetTemplate;       // TH10 off_4746d8

u32 LoadU32(const void *address)
{
    return *reinterpret_cast<const u32 *>(address);
}

i32 LoadI32(const void *address)
{
    return *reinterpret_cast<const i32 *>(address);
}

// Native branchless selected-row color (setne/dec/and 0x7F7E80/add
// 0xFF808080): the selected entry renders 0xFFFFFF00, everything else
// 0xFF808080.
u32 EntryColor(u32 selected, u32 index)
{
    return (selected != index) ? 0xFF808080U : 0xFFFFFF00U;
}

// Replay header (the +0x18 pointer inside a manager +0x59e4 slot):
// +0x00 name, +0x0c time_t, +0x10 dword, +0x48 slow-rate float,
// +0x50 chara, +0x54 shot, +0x58 rank, +0x5c stage, +0x60 dword.
void AppendReplayLine(AsciiManager &ascii, const Float3 &position,
                      i32 entry_number, const void *header)
{
    const u8 *const bytes = static_cast<const u8 *>(header);
    const time_t stamp =
        *reinterpret_cast<const time_t *>(bytes + 0x0c);
    tm *date = localtime(const_cast<time_t *>(&stamp));
    ascii.AddFormatText(
        &position, "No.%.2d %s %.2d/%.2d/%.2d %.2d:%.2d %s %s %s %2.1f%%",
        entry_number, reinterpret_cast<const char *>(bytes),
        date->tm_year % 100, date->tm_mon + 1, date->tm_mday,
        date->tm_hour, date->tm_min,
        g_ReplayCharacterNames[3U * LoadU32(bytes + 0x50) +
                               LoadU32(bytes + 0x54)],
        g_ReplayRankNames[LoadU32(bytes + 0x58)],
        g_ReplayStageNames[LoadU32(bytes + 0x5c)],
        static_cast<double>(*reinterpret_cast<const float *>(bytes + 0x48)));
}

// Populated score-file record, format string TH10 0x46ee84
// "%2d  %s  %9ld%d  %.4d/%.2d/%.2d %.2d:%.2d  %s  %2.1f%%". The date prints
// the full year (tm_year + 1900); the stage name comes from the signed byte
// at R+0x1c and the trailing %d from the signed byte at R+0x1d (the
// DAT_00474c90 analog, the score's trailing digit).
void AppendScoreFileLine(AsciiManager &ascii, const Float3 &position,
                         i32 entry_number, const u8 *record)
{
    const time_t stamp =
        *reinterpret_cast<const time_t *>(record + 0x28);
    tm *date = localtime(const_cast<time_t *>(&stamp));
    ascii.AddFormatText(
        &position, "%2d  %s  %9ld%d  %.4d/%.2d/%.2d %.2d:%.2d  %s  %2.1f%%",
        entry_number, reinterpret_cast<const char *>(record + 0x1e),
        static_cast<long>(LoadI32(record + 0x18)),
        static_cast<i32>(static_cast<signed char>(record[0x1d])),
        date->tm_year + 1900, date->tm_mon + 1, date->tm_mday,
        date->tm_hour, date->tm_min,
        g_ScoreStageNames[static_cast<signed char>(record[0x1c]) + 1],
        static_cast<double>(*reinterpret_cast<const float *>(record + 0x2c)));
}

// Empty score-file record, format string TH10 0x46ee50
// "%2d  %s  %9ld%d  ----/--/-- --:--  Stage -  ---%%". The stored score and
// trailing-digit byte are printed even for untouched records.
void AppendScoreFileEmptyLine(AsciiManager &ascii, const Float3 &position,
                              i32 entry_number, const u8 *record)
{
    ascii.AddFormatText(
        &position, "%2d  %s  %9ld%d  ----/--/-- --:--  Stage -  ---%%",
        entry_number, reinterpret_cast<const char *>(record + 0x1e),
        static_cast<long>(LoadI32(record + 0x18)),
        static_cast<i32>(static_cast<signed char>(record[0x1d])),
        static_cast<double>(*reinterpret_cast<const float *>(record + 0x2c)));
}

} // namespace

// TH10 0x00431410. Native EDI-ABI body (plain retn); the dispatcher at
// 0x0042d2a8 relies on the manager still being in EDI. Always returns 1.
// Draws the six per-stage separator rows under the score list for the
// current (shot + 3 * character) block. Clear-state pairs live in the
// score-save block at score dword +0x4dc / cleared byte +0x4e1, eight bytes
// per (difficulty, stage) pair (48 bytes per difficulty, stage 1..6).
i32 RunManagerDrawBody9(void *manager)
{
    u8 *const bytes = static_cast<u8 *>(manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);

    const i32 state = static_cast<i32>(words[0x20 / 4]);
    if (state < 2 || state > 3)
        return 1;

    AsciiManager &ascii = *static_cast<AsciiManager *>(g_AsciiManagerHost);
    ascii.text_mode = 1U;

    const i32 timer = static_cast<i32>(words[0x2b4 / 4]);
    if (timer < 10 && state != 3) {
        ascii.color = 0xFFFFFFFFU;
        ascii.text_mode = 0U;
        return 1;
    }

    const u8 *const save = static_cast<const u8 *>(g_ScoreSaveState);
    const u32 block = 0x437cU * (g_PlayerShotType + 3U * g_PlayerCharacter);

    Float3 position;
    position.x = (g_PlayerCharacter != 0U) ? 296.0f : 168.0f;
    position.y = 152.0f;
    position.z = 0.0f;

    for (u32 stage = 1; stage <= 6; ++stage) {
        const u32 pair_offset = 48U * g_CurrentDifficulty + 8U * stage;
        const u8 cleared =
            save[block + pair_offset + 0x4e1U];

        if (words[0x24 / 4] != stage - 1U) {
            ascii.color = 0xFF808080U;
        } else if (cleared == 0U) {
            ascii.color = 0xFFDFDFDFU;
        } else {
            // Selected and cleared rows blink black while the state-3
            // detail screen is up: signed-mod-4 phase of the +0x2b4 timer
            // >= 2 renders 0xFF000000 (native and/or/dec/inc idiom).
            i32 phase = static_cast<i32>(words[0x2b4 / 4] & 0x80000003U);
            if (phase < 0) {
                --phase;
                phase |= static_cast<i32>(0xFFFFFFFCU);
                ++phase;
            }
            ascii.color =
                (state == 3 && phase >= 2) ? 0xFF000000U : 0xFFFFFF00U;
        }

        const char *stage_name = g_ScoreStageNames[stage + 1];
        if (cleared != 0U) {
            // TH10 0x46effc "%s  %.8d0": the stored dword is the score
            // without its trailing zero digit, printed with a literal 0.
            ascii.AddFormatText(&position, "%s  %.8d0", stage_name,
                                LoadI32(save + block + pair_offset + 0x4dcU));
        } else {
            // TH10 0x46f008 "%s  ---------".
            ascii.AddFormatText(&position, "%s  ---------", stage_name);
        }
        position.y += 18.0f; // TH10 flt_470C0C
    }

    ascii.color = 0xFFFFFFFFU;
    ascii.text_mode = 0U;
    return 1;
}

// TH10 0x004329f0. Native EDI-ABI body (plain retn). Always returns 1.
// State 0xB (extra-mode unlock menu) sub-state 2 list. The menu's shot-type
// cursor (+0x24) selects the 0x437c-byte block and the difficulty cursor
// (+0xfc) the 0xf0-byte run of ten 24-byte records inside it; +0x1d4
// non-zero suppresses the record list entirely.
i32 RunManagerDrawBodyB(void *manager)
{
    u8 *const bytes = static_cast<u8 *>(manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);

    if (words[0x20 / 4] != 2U)
        return 1;

    AsciiManager &ascii = *static_cast<AsciiManager *>(g_AsciiManagerHost);
    ascii.text_mode = 1U;

    const u8 *const save = static_cast<const u8 *>(g_ScoreSaveState);
    const u32 shot_block = 0x437cU * words[0x24 / 4];
    const u32 difficulty = words[0xfc / 4];

    Float3 position;
    position.x = 48.0f;
    position.y = 160.0f;
    position.z = 0.0f;

    if (words[0x1d4 / 4] == 0U) {
        u32 fade = 0xffU;
        u32 record_offset = difficulty * 0xf0U;
        for (i32 row = 1; row <= 10; ++row) {
            // Rows fade from white downward: color 0xFFvvvvFF with
            // v = 0xff - 0x10 * (row - 1) (alpha and the blue channel stay
            // at 0xff via the native OR of 0xFF0000FF).
            ascii.color = 0xFF0000FFU | (((fade << 8) | fade) << 8);
            const u8 *record = save + shot_block + record_offset;
            if (LoadU32(record + 0x28) != 0U)
                AppendScoreFileLine(ascii, position, row, record);
            else
                AppendScoreFileEmptyLine(ascii, position, row, record);
            position.y += 18.0f; // TH10 flt_470C0C
            fade -= 0x10U;
            record_offset += 0x18U;
        }
    }

    // Totals block at x = 324: play count (+0x4c8), play time (+0x4cc) and
    // the per-difficulty play count (+0x4d0 + 4 * difficulty).
    ascii.color = 0xFFFFFFFFU;
    position.x = 324.0f;
    position.y = 378.0f;
    ascii.AddFormatText(&position, "    %5d",
                        LoadI32(save + shot_block + 0x4c8U));

    position.y = 396.0f;
    const i32 play_time = LoadI32(save + shot_block + 0x4ccU);
    // Native division chain (signed magic constants / 60, / 3600,
    // / 216000) prints t / 216000 : (t / 3600) % 60 : (t / 60) % 60. The
    // middle field is hours % 60 rather than minutes or seconds — quirk
    // preserved.
    ascii.AddFormatText(&position, "%3d:%.2d:%.2d",
                        play_time / 216000, (play_time / 3600) % 60,
                        (play_time / 60) % 60);

    position.y = 412.0f;
    ascii.AddFormatText(&position, "    %5d",
                        LoadI32(save + shot_block + 0x4d0U +
                                4U * difficulty));

    ascii.color = 0xFFFFFFFFU;
    ascii.text_mode = 0U;
    return 1;
}

// TH10 0x00431ba0. Native stdcall (manager as stack argument, retn 4).
// Always returns 1. Score-file viewer body: sub-state 2 renders the 25
// replay rows, sub-state 4 renders the selected entry's detail line plus
// (once the +0x2b4 timer reaches 10) its seven per-stage rows.
i32 RunManagerDrawBodyC(void *manager)
{
    u8 *const bytes = static_cast<u8 *>(manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    AsciiManager &ascii = *static_cast<AsciiManager *>(g_AsciiManagerHost);

    const i32 state = static_cast<i32>(words[0x20 / 4]);
    u32 *const slots = words + (0x59e4 / 4);

    if (state == 2) {
        // Replay list: 25 rows, x = 58, y = 80 stepping 15 (flt_470C08).
        ascii.text_mode = 1U;
        Float3 position;
        position.x = 58.0f;
        position.y = 80.0f;
        position.z = 0.0f;

        const u32 selected_row = words[0x24 / 4];
        for (u32 row = 0; row < 25; ++row) {
            ascii.color = EntryColor(selected_row, row);
            const u32 *const record =
                reinterpret_cast<const u32 *>(slots[row]);
            if (record != 0) {
                AppendReplayLine(ascii, position,
                                 static_cast<i32>(row) + 1,
                                 reinterpret_cast<const u8 *>(
                                     record[0x18 / 4]));
            } else {
                // TH10 0x46ef74.
                ascii.AddFormatText(
                    &position,
                    "No.%.2d -------- --/--/-- --:-- ------- ------- ---"
                    " ---%%",
                    static_cast<i32>(row) + 1);
            }
            position.y += 15.0f; // TH10 flt_470C08
        }
    } else if (state == 4) {
        // Selected-entry detail. The slot is dereferenced without a null
        // check — quirk preserved.
        const i32 selected = static_cast<i32>(words[0x59dc / 4]);
        const u32 *const record =
            reinterpret_cast<const u32 *>(slots[selected]);
        const u8 *const header =
            reinterpret_cast<const u8 *>(record[0x18 / 4]);

        Float3 position;
        position.x = 80.0f;
        position.y = 80.0f;
        position.z = 0.0f;
        if (static_cast<i32>(words[0x2b4 / 4]) < 10) {
            // Scroll-in animation:
            // y = (10 - mgr+0x2B8) * (15 * selected) * 0.1 + 80
            // (TH10 flt_470C1C / flt_470C18 / flt_470C28).
            const float animated =
                *reinterpret_cast<const float *>(bytes + 0x2b8);
            position.y = (10.0f - animated) *
                             static_cast<float>(15 * selected) * 0.1f +
                         80.0f;
        }

        ascii.text_mode = 1U;
        AppendReplayLine(ascii, position, selected + 1, header);

        if (static_cast<i32>(words[0x2b4 / 4]) < 10) {
            ascii.color = 0xFFFFFFFFU;
            ascii.text_mode = 0U;
            return 1;
        }

        // Per-stage rows at x = 220, y = 128 stepping 18: sub-records live
        // inside the record at 0x24 + 0x24 * (stage - 1) with the played
        // flag at +0xb0 and an optional spell-history pointer at +0xd4
        // (whose +0xc / +0x1b4 dwords feed the "%.8d%d" split). Stages 6
        // and 7 (and any row without history) fall back to the header
        // totals at header+0x10 / header+0x60.
        position.x = 220.0f;
        position.y = 128.0f;
        position.z = 0.0f;
        u32 sub_offset = 0x24U;
        for (i32 stage = 1; stage <= 7; ++stage, sub_offset += 0x24U) {
            ascii.color = EntryColor(words[0x24 / 4],
                                     static_cast<u32>(stage) - 1U);
            const u8 *const sub =
                reinterpret_cast<const u8 *>(record) + sub_offset;
            const char *stage_name = g_ScoreStageNames[stage + 1];
            if (LoadU32(sub + 0xb0) == 0U) {
                // TH10 0x46f008 "%s  ---------".
                ascii.AddFormatText(&position, "%s  ---------", stage_name);
            } else if (stage < 6 && LoadU32(sub + 0xd4) != 0U) {
                // TH10 0x46ef68 "%s  %.8d%d".
                const u8 *const history =
                    reinterpret_cast<const u8 *>(LoadU32(sub + 0xd4));
                ascii.AddFormatText(&position, "%s  %.8d%d", stage_name,
                                    LoadI32(history + 0x0c),
                                    LoadI32(history + 0x1b4));
            } else {
                ascii.AddFormatText(&position, "%s  %.8d%d", stage_name,
                                    LoadI32(header + 0x10),
                                    LoadI32(header + 0x60));
            }
            position.y += 18.0f; // TH10 flt_470C0C
        }
    }

    ascii.color = 0xFFFFFFFFU;
    ascii.text_mode = 0U;
    return 1;
}

// TH10 0x00433230. Native stdcall (manager as stack argument, retn 4).
// Always returns 1. State 0xF sub-state 2: the ten high-score records of
// the current (shot + 3 * character) block for the selected difficulty
// (DAT_00474c74 * 0xf0), then the name-entry editor unless the flag at
// manager+0x58ec is set.
i32 RunManagerDrawBodyF(void *manager)
{
    u8 *const bytes = static_cast<u8 *>(manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    AsciiManager &ascii = *static_cast<AsciiManager *>(g_AsciiManagerHost);

    if (words[0x20 / 4] != 2U)
        return 1;

    const u8 *const save = static_cast<const u8 *>(g_ScoreSaveState);
    const u32 block = 0x437cU * (g_PlayerShotType + 3U * g_PlayerCharacter);
    const u32 difficulty_base = 0xf0U * g_CurrentDifficulty;
    const u32 name_entry_done = words[0x58ec / 4];

    Float3 position;
    position.x = 48.0f;
    position.y = 160.0f;
    position.z = 0.0f;
    ascii.text_mode = 1U;

    u32 fade = 0xffU;
    for (i32 row = 0; row < 10; ++row) {
        if (name_entry_done != 0U) {
            // Whole list fades (0xFFvvvvFF) while no name entry is pending.
            ascii.color = 0xFF0000FFU | (((fade << 8) | fade) << 8);
        } else if (words[0x24 / 4] == static_cast<u32>(row)) {
            ascii.color = 0xFFFFFFFFU;
        } else {
            ascii.color = 0xFF404040U;
        }

        const u8 *const record =
            save + block + difficulty_base + 0x18U * row;
        if (LoadU32(record + 0x28) != 0U)
            AppendScoreFileLine(ascii, position, row + 1, record);
        else
            AppendScoreFileEmptyLine(ascii, position, row + 1, record);
        position.y += 18.0f; // TH10 flt_470C0C
        fade -= 0x10U;       // native loop runs while fade > 0x5f (10 rows)
    }

    if (name_entry_done != 0U)
        // Native quirk: this early return leaves color at the last row
        // tint and text_mode at 1 (the common tail is skipped).
        return 1;

    // Name-entry editor: the 9-byte buffer at manager+0x58dc renders on the
    // cursor row (x = 84, y = 18 * cursor + 160), with the caret "_" in
    // yellow nine pixels per entered character (pulled back one cell when
    // the cursor sits past the last character, cursor == 8).
    const i32 cursor_row = static_cast<i32>(words[0x24 / 4]);
    const float entry_y = static_cast<float>(cursor_row) * 18.0f + 160.0f;

    ascii.color = 0xFFFFFFFFU;
    position.x = 84.0f; // TH10 0x42a80000 constant
    position.y = entry_y;
    ascii.AddFormatText(&position, "%s",
                        reinterpret_cast<const char *>(bytes + 0x58dc));

    const i32 cursor = static_cast<i32>(words[0x58e8 / 4]);
    float caret_x = static_cast<float>(cursor) * 9.0f + 84.0f;
    if (cursor == 8)
        caret_x -= 9.0f; // TH10 flt_470C10
    ascii.color = 0xFFFFFF00U;
    position.x = caret_x;
    ascii.AddFormatText(&position, "_");

    // Alphabet template grid: 13 columns stepping 18 pixels (flt_470C0C)
    // from (212, 360), dropping 16 pixels (flt_470B48) and resetting x on
    // every 13th cell. The last three cells are forced to the bytes 0x81 /
    // 0x7f / 0x80 (the native's 0x7f + (index != length - 2) for the final
    // two). Cell selection uses manager+0x58f4 with the shared EntryColor
    // idiom.
    ascii.color = 0xFFFFFFFFU;
    const char *const alphabet = g_AlphabetTemplate;
    const i32 length = static_cast<i32>(strlen(alphabet));
    if (length > 0) {
        position.x = 212.0f;
        position.y = 360.0f;
        for (i32 index = 0; index < length; ++index) {
            ascii.color = EntryColor(words[0x58f4 / 4],
                                     static_cast<u32>(index));
            char cell;
            if (index < length - 3)
                cell = alphabet[index];
            else if (index == length - 3)
                cell = static_cast<char>(0x81);
            else
                cell = static_cast<char>(0x7f + (index != length - 2));
            ascii.AddFormatText(&position, "%c", cell);
            if (index % 13 == 12) {
                position.x = 212.0f;
                position.y += 16.0f; // TH10 flt_470B48
            } else {
                position.x += 18.0f; // TH10 flt_470C0C
            }
        }
    }

    ascii.color = 0xFFFFFFFFU;
    return 1;
}

} // namespace th10
