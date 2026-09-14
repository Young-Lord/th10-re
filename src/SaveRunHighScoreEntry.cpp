// TH10 0x00423570 — post-run high-score publication. This is the routine the
// post-run replay-save menu (0x004236f0, mode 7 non-practice path) calls as
// its former `SaveRunHighScoreEntry` boundary; the mode-10 epilogue of the
// same menu repeats part of this setup inline. All fixed globals and offsets
// are from the raw disassembly; see docs/evidence/save-run-high-score-entry.md.
#include <string.h>

#include "GameManagerState.hpp"
#include "ScoreFileFormats.hpp"
#include "Th10Types.hpp"
#include "SaveRunHighScoreEntry.hpp"

namespace th10 {

namespace {

// ------------------------------------------------------------- globals

extern u32 g_SceneModeSelector;       // TH10 DAT_00474c7c (stage selector)
extern u32 g_SceneModeSelectorMirror; // TH10 DAT_00474c80 (mirror dword)
extern u32 g_RunChara;                // TH10 DAT_00474c68
extern u32 g_RunCharaSlot;            // TH10 DAT_00474c6c
extern void *g_ScoreSaveState;        // TH10 DAT_0047783c (save data)
extern void *g_PublishedModeRecord;   // TH10 DAT_00477848

// TH10 0x4746d8 (DAT_004746d8): a char* variable pointing at the entry-name
// charset at 0x0046e2f8 ("ABCDEFGHIJKLMNOPQRST..."). The native loads the
// pointer value, never the variable's bytes.
extern const char *g_MenuCharset; // TH10 DAT_004746d8 -> 0x0046e2f8

// Nine spaces at TH10 0x0046e354 (the blank saved-name sentinel).
const char *const kNineSpaces = reinterpret_cast<const char *>(0x46E354U);
// Alternate published mode-record tables in .data.
const u32 kExtraModeRecordTable = 0x474908U;
const u32 kNormalModeRecordTable = 0x4748d8U;

// ------------------------------------------------------------ accessors

inline u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

inline i32 LoadI32At(const void *base, u32 offset)
{
    return static_cast<i32>(LoadU32At(base, offset));
}

inline void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

} // namespace

// TH10 0x00423570 (native stdcall `ret 4`).
void SaveRunHighScoreEntry(void *record_arg)
{
    u8 *const record = static_cast<u8 *>(record_arg);

    // Extra-stage swap: file the score under stage 8 with the alternate
    // published mode record while the selector reads 7 with the extra flag.
    if (g_SceneModeSelector == 7U && LoadU32At(record, 0x1e4) != 0U) {
        g_PublishedModeRecord =
            reinterpret_cast<void *>(kExtraModeRecordTable);
        g_SceneModeSelector = 8U;
        g_SceneModeSelectorMirror = 8U;
    }

    // Score-save table of the finished run's character.
    u8 *const score_table = static_cast<u8 *>(g_ScoreSaveState) + 8U
        + 0x437cU * (g_RunCharaSlot + 3U * g_RunChara);
    const i32 rank = InsertScoreRecordEdi(score_table);

    // Native restore guard: it re-checks the selector against 7, which the
    // swap above has already set to 8, so this branch never fires after the
    // swap (preserved verbatim).
    if (g_SceneModeSelector == 7U && LoadU32At(record, 0x1e4) != 0U) {
        g_PublishedModeRecord =
            reinterpret_cast<void *>(kNormalModeRecordTable);
        g_SceneModeSelector = 7U;
        g_SceneModeSelectorMirror = 7U;
    }

    if (rank < 0) {
        // The run did not place: keep the replay-save menu closed.
        StoreU32At(record, 0x1e8, 1U);
        return;
    }

    // Rank accepted: cursor A becomes the replay slot selector (max 25).
    StoreU32At(record, 0x2c, 0x19U);
    StoreU32At(record, 0xf4, 1U);
    i32 clamped_rank = rank;
    {
        const i32 max = LoadI32At(record, 0x2c);
        if (max != 0 && clamped_rank >= max) {
            clamped_rank = max - 1;
        }
    }
    StoreU32At(record, 0x24, static_cast<u32>(clamped_rank));

    // Cursor B: clamp the current value against its pre-existing maximum,
    // then refresh that maximum with the charset length.
    {
        const i32 old_max = LoadI32At(record, 0x104);
        if (old_max < 0) {
            StoreU32At(record, 0xfc, static_cast<u32>(old_max - 1));
        } else {
            StoreU32At(record, 0xfc, 0U);
        }
    }
    StoreU32At(record, 0x104, static_cast<u32>(strlen(g_MenuCharset)));
    StoreU32At(record, 0x1cc, 1U);

    // Copy the saved replay name over the record's name buffer; when it
    // differs from the nine-space sentinel, shift cursor B by -1 (native
    // polarity: `je` skips the 0x0044bea0 shift only on equality).
    char *const name = reinterpret_cast<char *>(record + 0x2b4);
    const char *const saved_name = reinterpret_cast<const char *>(
        static_cast<u8 *>(g_ScoreSaveState) + 0x1d878U);
    strcpy(name, saved_name);
    if (memcmp(name, kNineSpaces, 9U) != 0) {
        (void)ShiftManagerSelector(record + 0xfc, -1);
    }

    // Trim trailing spaces inside the fixed 8-character window (the native
    // counter starts at 8, so a longer stored name still yields at most 8).
    i32 length = 8;
    while (length > 0 && name[length - 1] == ' ') {
        --length;
    }
    StoreU32At(record, 0x1e0, static_cast<u32>(length));
    StoreU32At(record, 0x1e8, 0U);
}

} // namespace th10
