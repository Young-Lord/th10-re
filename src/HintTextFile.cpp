// Hint-text file cluster (TH10 0x0041a200 template writer, 0x00419960
// parser, 0x00419040 list teardown, 0x00419120 tip-entity updater) plus the
// small helpers 0x0041a0a0 / 0x004198c0 / 0x00418d00 / 0x0041ab10 /
// 0x0041ab60 / 0x00419f40 / 0x0041ab70. Offsets and strings are taken from
// the native disassembly; the Shift-JIS header lines are embedded byte for
// byte. The tip record is 0x88 bytes: position floats at +0/+4, text at
// +0x1c (0x41-byte copy), list node at +0x0c (next +0x10, prev +0x14),
// count +0x60, align +0x64, base +0x68, time +0x6c, width +0x80, scale
// +0x7c, color bytes +0x84..+0x86, alpha +0x87.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "EntityHelpers.hpp"
#include "HintTextFile.hpp"
#include "Th10Types.hpp"

namespace th10 {

namespace {

// ---- globals ------------------------------------------------------------

extern u8 g_ExtraSaveSelectorDummy; // (not used here; keeps TU self-contained)
extern u32 g_HintBaseTable;         // TH10 off_4744b0 (61 {name,value} pairs)
extern u32 g_HintAlignTable;        // TH10 off_474494 (3 pairs)
extern u32 g_CurrentStageSceneId;   // TH10 DAT_00474cb4
extern void *g_TipEntityManager;    // TH10 DAT_00491c40
extern void *g_AsciiHudOwner;       // TH10 DAT_0047770c (script list holder)
extern u8 g_SceneRecordHolder;      // TH10 byte_477710 (dereferenced)

// ---- boundaries (native ABIs noted at each site) -------------------------

// TH10 0x452abc (MSVCRT _mkdir) as a toolchain-neutral boundary.
extern "C" i32 CreateHintDirectory(const char *path);
// TH10 0x0044b620. stdcall (file name); creates/truncates the file and
// stores the handle at DAT_00474c38; returns non-zero on failure (with the
// FormatMessageA/LocalFree diagnostics under the resource-loader lock).
extern i32 OpenHintTemplateFileStackAbi(const char *file_name);
// TH10 0x0044b740. usercall: ESI = length, EDX = line buffer; appends the
// line to the handle stored at DAT_00474c38.
extern void AppendHintTemplateLineEsiEdxAbi(i32 length, const char *line);
// TH10 0x0044b360. usercall: stack = (pointer to the file-name stack slot,
// 1); loads the whole file under the resource-loader lock and stores the
// size into the caller slot; returns the buffer (null on failure).
extern void *LoadHintFileContentsUsercall(const char *file_name,
                                          u32 *out_size);
// TH10 0x0041a090. EAX = string; the 75-byte trim variant used by the
// line reader / tip-line splitter.
extern char *TrimHintLineShortEaxAbi(char *text);
// TH10 0x0041a050. Splits the current line into the key/value buffers
// (native works on caller-stack buffers; modeled with explicit outputs).
extern void SplitTipLineUsercall(const char *line, char *key, char *value);
// TH10 0x0041ab70. usercall: EDX = entity (motion-block source qword),
// EAX = entity (motion-block base), ECX = {x, y} pair, stack = (8, 4).
// Seeds the tip-entity motion record at entity+0x180..0x1b8.
extern void ResetTipEntityMotionEdxEcxStackAbi(void *entity,
                                               const u32 *pair);
// TH10 0x00448db0. usercall: EDX = tip record, stack = (script id, out-id
// pointer, kind); returns the out pointer.
extern u32 *SpawnTipEntityEdxStackAbi(const void *tip, u32 script_id,
                                      u32 *out_id, i32 kind);
// TH10 0x004491c0. stdcall-ish (EAX = id, ECX/stack = manager, id); returns
// the entity for the id or null.
extern void *ResolveTipEntityEaxStackAbi(u32 id);
// TH10 0x00447bb0 / 0x00447a50 / 0x00447ae0. EAX = entity manager, stack =
// (color 0xffffff, text); center / left / right aligned tip text entities.
extern void AddTipTextCenteredEaxStackAbi(void *manager, u32 color,
                                          const char *text);
extern void AddTipTextLeftEaxStackAbi(void *manager, u32 color,
                                      const char *text);
extern void AddTipTextRightEaxStackAbi(void *manager, u32 color,
                                       const char *text);

// ---- field helpers -------------------------------------------------------

u32 LoadU32FromAddress(u32 address)
{
    return *reinterpret_cast<const u32 *>(address);
}

u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

i32 LoadI32At(const void *base, u32 offset)
{
    return static_cast<i32>(LoadU32At(base, offset));
}

void StoreI32At(void *base, u32 offset, i32 value)
{
    StoreU32At(base, offset, static_cast<u32>(value));
}

float LoadFloatAt(const void *base, u32 offset)
{
    return *reinterpret_cast<const float *>(
        static_cast<const u8 *>(base) + offset);
}

void StoreFloatAt(void *base, u32 offset, float value)
{
    *reinterpret_cast<float *>(static_cast<u8 *>(base) + offset) = value;
}

// Shift-JIS header lines of the generated template (byte-exact copies of
// 0x46d534 / 0x46d510 / 0x46d4d8 / 0x46d490).
const char kHintHeaderRule[] =
    "# ========================================================= \r\n";
const char kHintHeaderTitle[] =
    "# \x93\x8c\x95\xfb\x95\x97\x90\x5f\x98\x5e\x81\x40\x8d\x55\x97\xaa"
    "\x83\x71\x83\x93\x83\x67\x83\x74\x83\x40\x83\x43\x83\x8b \x0d\x0a";
const char kHintHeaderAuto[] =
    "# \x82\xb1\x82\xcc\x83\x74\x83\x40\x83\x43\x83\x8b\x82\xcd\x81\x41"
    "\x8e\xa9\x93\xae\x93\x49\x82\xc9\x90\xb6\x90\xac\x81\x41\x8f\xe3"
    "\x8f\x91\x82\xab\x82\xb3\x82\xea\x82\xdc\x82\xb7 \x0d\x0a";
const char kHintHeaderManual[] =
    "# \x8e\xa9\x95\xaa\x82\xc5\x8d\x55\x97\xaa\x83\x71\x83\x93\x83\x67"
    "\x82\xf0\x8d\x5c\x90\xac\x82\xb5\x82\xbd\x82\xa2\x8f\xea\x8d\x87"
    "\x82\xcd\x81\x41\x95\xca\x82\xcc\x83\x74\x83\x40\x83\x43\x83\x8b"
    "\x82\xf0\x8e\x67\x97\x70\x82\xb5\x82\xc4\x82\xad\x82\xbe\x82\xb3"
    "\x82\xa2 \x0d\x0a";

// strlen + append idiom used at every template line.
void AppendTemplateLine(char *buffer, const char *line)
{
    AppendHintTemplateLineEsiEdxAbi(static_cast<i32>(strlen(line)), line);
}

} // namespace

// TH10 0x0041a0a0.
char *TrimHintStringEaxAbi(char *text)
{
    // Leading strip: while the first character is whitespace, shift the
    // remainder left (the native repeats the strlen+rep movsb idiom).
    for (;;) {
        const char lead = text[0];
        if (lead != ' ' && lead != '\t' && lead != '\n' && lead != '\r')
            break;
        const size_t length = strlen(text);
        if (length == 0)
            break; // native: jle back to the lead test (no progress)
        memmove(text, text + 1, length);
        text[length] = '\0';
    }
    // Trailing strip: walk backwards from the end, NUL-terminating one
    // character earlier per whitespace character found.
    const int length = static_cast<int>(strlen(text));
    for (int index = length - 1; index >= 0; --index) {
        const char trail = text[index];
        if (trail != ' ' && trail != '\t' && trail != '\n'
            && trail != '\r')
            break;
        text[index + 1] = '\0'; // native writes [edx+eax+1] = 0
    }
    return text;
}

// TH10 0x004198c0.
u32 LookupHintKeywordEdxStackAbi(const char *name, const void *table,
                                 i32 count)
{
    const u8 *entries = static_cast<const u8 *>(table);
    for (i32 i = 0; i < count; ++i) {
        const char *entry_name = *reinterpret_cast<const char *const *>(
            entries + 8U * i);
        if (entry_name != 0 && strcmp(name, entry_name) == 0)
            return LoadU32At(entries, 8U * i + 4U);
    }
    return 0;
}

// TH10 0x00418d00.
void *ConstructTipRecordEdxAbi(void *record)
{
    memset(record, 0, 0x88U);
    StoreU32At(record, 0x0CU, reinterpret_cast<u32>(record));
    StoreU32At(record, 0x84U, 0xFFFFFFFFU);
    StoreU32At(record, 0x70U, 0xFFFFFFFFU);
    StoreI32At(record, 0x6CU, 300);
    StoreU32At(record, 0x7CU, 0x3F800000U); // 1.0f
    return record;
}

// TH10 0x0041ab10.
void AppendTipListNodeEaxEdxAbi(u32 *head_holder_minus4, void *node)
{
    // Native EAX starts at the list-head holder minus 4 so that EAX+4 is
    // the head slot; the walk then follows node->next (+4).
    u32 *cursor = head_holder_minus4;
    if (cursor[1] != 0U) {
        u32 *next;
        do {
            cursor = reinterpret_cast<u32 *>(cursor[1]);
            next = reinterpret_cast<u32 *>(cursor[1]);
        } while (next != 0);
    }
    const u32 tail_next = cursor[1];
    if (tail_next != 0U) {
        StoreU32At(node, 0x10U, tail_next);
        StoreU32At(reinterpret_cast<void *>(tail_next), 0x14U,
                   reinterpret_cast<u32>(node));
    }
    cursor[1] = reinterpret_cast<u32>(node);
    StoreU32At(node, 0x14U, reinterpret_cast<u32>(cursor));
}

// TH10 0x0041ab60.
u32 GetTipSlotEaxAbi(const void *tip)
{
    return LoadU32At(tip, 0x24U);
}

// TH10 0x00419f40.
const char *ReadHintTextLineUsercall(const char *cursor, char *line,
                                     u32 *remaining, u32 max_length)
{
    memset(line, 0, max_length);
    // Native quirk: the guard tests the freshly zeroed buffer, so the
    // read path always runs.
    if (line[0] == '\0') {
        for (;;) {
            const char *eol = strchr(cursor, '\n');
            if (eol == 0) {
                eol = strchr(cursor, '\r');
                if (eol == 0)
                    break;
            }
            size_t length = static_cast<size_t>(eol - cursor);
            if (eol - cursor >= static_cast<int>(max_length - 1U))
                length = max_length - 1U;
            memcpy(line, cursor, length);
            *remaining += static_cast<u32>(cursor - eol); // -= line length
            for (cursor = eol; *cursor == '\n' || *cursor == '\r';
                 ++cursor)
                --*remaining;
            const char *comment = strchr(line, '#');
            if (comment != 0) {
                // Native memsets from the '#' to the end of the buffer and
                // stores an explicit 0 (the memset tail is also the guard
                // feed for the short trim call).
                memset(const_cast<char *>(comment), 0,
                       max_length
                           - static_cast<u32>(comment - line));
                TrimHintLineShortEaxAbi(line);
            } else {
                TrimHintLineShortEaxAbi(line);
            }
            if (line[0] != '\0')
                return cursor;
        }
        // No more EOL: copy what is left and drain the counter.
        memcpy(line, cursor, *remaining);
        *remaining = 0;
    }
    return cursor;
}

// TH10 0x00419040.
void FreeHintTipListsEaxAbi(void *hint_state)
{
    u32 *cursor = reinterpret_cast<u32 *>(
        static_cast<u8 *>(hint_state) + 0x7CU);
    for (i32 i = 8; i != 0; --i) {
        // Paired heads 24 bytes apart (state+100+12*i, state+124+12*i).
        for (int pair = 0; pair < 2; ++pair) {
            u32 *head = reinterpret_cast<u32 *>(
                pair == 0 ? cursor - 6 : cursor);
            u32 node = *head;
            while (node != 0U) {
                const u32 next = LoadU32FromAddress(node + 4U);
                free(*reinterpret_cast<void **>(node));
                node = next;
            }
        }
        cursor += 3;
    }
}

// TH10 0x0041a200.
i32 WriteHintTextTemplateStackAbi(void *hint_state, const char *file_name)
{
    char *const buffer = static_cast<char *>(malloc(0x1000U));
    CreateHintDirectory("hint");
    if (OpenHintTemplateFileStackAbi(file_name) != 0) {
        free(buffer);
        return 0;
    }

    strcpy(buffer, kHintHeaderRule);
    AppendTemplateLine(buffer, buffer);
    strcpy(buffer, kHintHeaderTitle);
    AppendTemplateLine(buffer, buffer);
    strcpy(buffer, "#\r\n");
    AppendTemplateLine(buffer, buffer);
    strcpy(buffer, kHintHeaderAuto);
    AppendTemplateLine(buffer, buffer);
    strcpy(buffer, kHintHeaderManual);
    AppendTemplateLine(buffer, buffer);
    strcpy(buffer, "#\r\n");
    AppendTemplateLine(buffer, buffer);

    {
        const time_t now = time(0);
        const tm *stamp = localtime(&now);
        sprintf(buffer,
                "#                                Time-stamp: <%.4d/%.2d/"
                "%.2d %.2d:%.2d>\r\n",
                stamp->tm_year + 1900, stamp->tm_mon + 1, stamp->tm_mday,
                stamp->tm_hour, stamp->tm_min);
        AppendTemplateLine(buffer, buffer);
    }

    strcpy(buffer, "\r\n\r\n");
    AppendTemplateLine(buffer, buffer);
    sprintf(buffer, "Version = %s\r\n\r\n", "0.0");
    AppendTemplateLine(buffer, buffer);

    // Eight stage lists at hint_state+124 (12-byte stride); the paired
    // head 24 bytes earlier (state+100) belongs to the duplicate list.
    for (i32 stage = 0; stage < 8; ++stage) {
        const u8 *const slot = static_cast<const u8 *>(hint_state)
            + 0x7CU + 12U * static_cast<u32>(stage);
        u32 node = LoadU32At(slot, 0U);
        i32 tips = 0;
        while (node != 0U && tips < 255) {
            const u8 *const tip =
                reinterpret_cast<const u8 *>(LoadU32FromAddress(node));
            node = LoadU32FromAddress(node + 4U);
            if (LoadI32At(tip, 0x70U) == 0)
                continue; // tips with zero "count" are skipped entirely

            if (tips == 0) {
                strcpy(buffer, "# ================================== \r\n");
                AppendTemplateLine(buffer, buffer);
                sprintf(buffer, "Stage : %d\r\n\r\n", stage);
                AppendTemplateLine(buffer, buffer);
            }
            strcpy(buffer, "Tips\r\n");
            AppendTemplateLine(buffer, buffer);
            if (LoadI32At(tip, 0x70U) > 0) {
                sprintf(buffer, "\tRemain\t: %d\r\n",
                        LoadI32At(tip, 0x70U));
                AppendTemplateLine(buffer, buffer);
            }
            sprintf(buffer, "\tText\t: \"%s\"\r\n",
                    reinterpret_cast<const char *>(tip) + 0x1CU);
            AppendTemplateLine(buffer, buffer);
            sprintf(buffer, "\tPos\t\t: %d, %d\r\n",
                    static_cast<i32>(LoadFloatAt(tip, 0U)),
                    static_cast<i32>(LoadFloatAt(tip, 4U)));
            AppendTemplateLine(buffer, buffer);
            sprintf(buffer, "\tCount\t: %d\r\n", LoadI32At(tip, 0x60U));
            AppendTemplateLine(buffer, buffer);

            // Base: reverse lookup in the 61-entry keyword table.
            const char *base_name = 0;
            for (i32 i = 0; i < 61; ++i) {
                if (LoadU32At(&g_HintBaseTable, 8U * i + 8U)
                    == LoadU32At(tip, 0x68U)) {
                    base_name = reinterpret_cast<const char *>(
                        LoadU32At(&g_HintBaseTable, 8U * i + 4U));
                    break;
                }
            }
            sprintf(buffer, "\tBase\t: %s\r\n", base_name);
            AppendTemplateLine(buffer, buffer);

            // Align: reverse lookup in the 3-entry keyword table.
            const char *align_name = 0;
            for (i32 i = 0; i < 3; ++i) {
                if (LoadU32At(&g_HintAlignTable, 8U * i + 4U)
                    == LoadU32At(tip, 0x64U)) {
                    align_name = reinterpret_cast<const char *>(
                        LoadU32At(&g_HintAlignTable, 8U * i));
                    break;
                }
            }
            sprintf(buffer, "\tAlign\t: %s\r\n", align_name);
            AppendTemplateLine(buffer, buffer);

            sprintf(buffer, "\tTime\t: %d\r\n", LoadI32At(tip, 0x6CU));
            AppendTemplateLine(buffer, buffer);
            sprintf(buffer, "\tAlpha\t: %d\r\n", tip[0x87U]);
            AppendTemplateLine(buffer, buffer);
            sprintf(buffer, "\tColor\t: %d, %d, %d\r\n", tip[0x86U],
                    tip[0x85U], tip[0x84U]);
            AppendTemplateLine(buffer, buffer);
            sprintf(buffer, "\tScale\t: %.1f\r\n", LoadFloatAt(tip, 0x7CU));
            AppendTemplateLine(buffer, buffer);
            strcpy(buffer, "End\r\n\r\n");
            AppendTemplateLine(buffer, buffer);
            ++tips;
        }
        if (tips != 0) {
            strcpy(buffer, "StageEnd\r\n");
            AppendTemplateLine(buffer, buffer);
        }
    }

    free(buffer);
    return 0;
}

// TH10 0x00419960.
i32 ParseHintTextFileStackAbi(void *hint_state, const char *file_name,
                              i32 duplicate_flag)
{
    i32 stage = -1;
    i32 tip_count = 0;

    u32 size = 0;
    void *contents = LoadHintFileContentsUsercall(file_name, &size);
    if (contents == 0)
        return -1;

    char *const line = static_cast<char *>(malloc(0x1000U));
    char *const key = static_cast<char *>(malloc(0x1000U));
    char *const value = static_cast<char *>(malloc(0x1000U));

    const char *cursor = static_cast<const char *>(contents);
    if (size != 0U) {
        for (;;) {
            cursor = ReadHintTextLineUsercall(cursor, line, &size, 4096U);
            // Split "key : value" (the native runs the copy/strchr/trim
            // sequence twice back to back with identical results).
            strcpy(key, line);
            char *separator = strchr(key, ':');
            if (separator != 0) {
                strcpy(value, separator + 1);
                *separator = '\0';
                TrimHintStringEaxAbi(value);
            }
            TrimHintStringEaxAbi(key);

            if (strcmp(key, "Version") == 0) {
                if (strcmp(value, "0.0") != 0)
                    FreeHintTipListsEaxAbi(hint_state);
            } else if (strcmp(key, "Stage") == 0) {
                stage = static_cast<i32>(atol(value));
                tip_count = 0;
                if (stage <= 0 || stage >= 8)
                    stage = -1;
            } else if (strcmp(key, "StageEnd") == 0) {
                stage = -1;
            } else if (strcmp(key, "Tips") == 0 && stage > 0) {
                if (tip_count >= 255) {
                    stage = -1;
                } else {
                    void *const raw = operator new(0x88U);
                    void *tip = raw != 0
                        ? ConstructTipRecordEdxAbi(raw)
                        : 0;
                    ++tip_count;
                    StoreU32At(tip, 0x18U, 0U);
                    while (size > 0U) {
                        cursor = ReadHintTextLineUsercall(
                            cursor, line, &size, 4096U);
                        SplitTipLineUsercall(line, key, value);
                        if (strcmp(key, "End") == 0)
                            break;
                        if (strcmp(key, "Pos") == 0) {
                            char *comma = strchr(value, ',');
                            if (comma != 0) {
                                *comma = '\0';
                                TrimHintStringEaxAbi(value);
                                TrimHintStringEaxAbi(comma + 1);
                                StoreFloatAt(tip, 0U,
                                             static_cast<float>(
                                                 atol(value)));
                                StoreFloatAt(tip, 4U,
                                             static_cast<float>(
                                                 atol(comma + 1)));
                            }
                        } else if (strcmp(key, "Text") == 0) {
                            char *quote = strchr(value, '"');
                            if (quote != 0) {
                                const char *begin = quote + 1;
                                char *end =
                                    strrchr(quote + 1, '"');
                                if (end == 0)
                                    break;
                                *end = '\0';
                                strncpy(
                                    reinterpret_cast<char *>(tip) + 0x1CU,
                                    begin, 0x41U);
                            }
                        } else if (strcmp(key, "Count") == 0) {
                            StoreI32At(tip, 0x60U,
                                       static_cast<i32>(atol(value)));
                        } else if (strcmp(key, "Time") == 0) {
                            StoreI32At(tip, 0x6CU,
                                       static_cast<i32>(atol(value)));
                        } else if (strcmp(key, "Base") == 0) {
                            StoreU32At(
                                tip, 0x68U,
                                LookupHintKeywordEdxStackAbi(
                                    value, &g_HintBaseTable, 61));
                        } else if (strcmp(key, "Align") == 0) {
                            StoreU32At(
                                tip, 0x64U,
                                LookupHintKeywordEdxStackAbi(
                                    value, &g_HintAlignTable, 3));
                        } else if (strcmp(key, "Remain") == 0) {
                            StoreI32At(tip, 0x70U,
                                       static_cast<i32>(atol(value)));
                        } else if (strcmp(key, "Scale") == 0) {
                            const float scale = static_cast<float>(
                                atof(value));
                            // Native: nonzero keeps the parsed value,
                            // except sub-normal/negative magnitudes clamp
                            // to -1.0f, and zero becomes 1.0f.
                            if (scale == 0.0f)
                                StoreU32At(tip, 0x7CU, 0x3F800000U);
                            else if (scale < 1.1443742e-28f)
                                StoreU32At(tip, 0x7CU, 0xBF800000U);
                            else
                                StoreU32At(
                                    tip, 0x7CU,
                                    *reinterpret_cast<const u32 *>(
                                        &scale));
                        } else if (strcmp(key, "Color") == 0) {
                            char *comma = strchr(value, ',');
                            if (comma != 0) {
                                *comma = '\0';
                                TrimHintStringEaxAbi(value);
                                *reinterpret_cast<u8 *>(
                                    static_cast<u8 *>(tip) + 0x86U) =
                                    static_cast<u8>(atol(value));
                                char *comma2 = strchr(comma + 1, ',');
                                if (comma2 != 0) {
                                    *comma2 = '\0';
                                    TrimHintStringEaxAbi(comma + 1);
                                    *reinterpret_cast<u8 *>(
                                        static_cast<u8 *>(tip) + 0x85U) =
                                        static_cast<u8>(atol(comma + 1));
                                    TrimHintStringEaxAbi(comma2 + 1);
                                    *reinterpret_cast<u8 *>(
                                        static_cast<u8 *>(tip) + 0x84U) =
                                        static_cast<u8>(atol(comma2 + 1));
                                }
                            }
                        } else if (strcmp(key, "Alpha") == 0) {
                            *reinterpret_cast<u8 *>(
                                static_cast<u8 *>(tip) + 0x87U) =
                                static_cast<u8>(atol(value));
                        }
                    }
                    {
                        // Estimated rendered width: strlen * scale (the
                        // native computes the double product with a
                        // negative-length +64 correction that cannot fire
                        // for real strings).
                        const int text_length = static_cast<int>(strlen(
                            reinterpret_cast<const char *>(tip) + 0x1CU));
                        StoreFloatAt(
                            tip, 0x80U,
                            static_cast<float>(text_length)
                                * LoadFloatAt(tip, 0x7CU));
                    }
                    // Append to the stage list at state+124+12*stage
                    // (native EAX = holder-4, EDX = tip node at +0x0c).
                    AppendTipListNodeEaxEdxAbi(
                        reinterpret_cast<u32 *>(
                            static_cast<u8 *>(hint_state) + 0x7CU
                            + 12U * static_cast<u32>(stage) - 4U),
                        static_cast<u8 *>(tip) + 0x0CU);
                    if (duplicate_flag == 0) {
                        void *const raw_copy = operator new(0x88U);
                        void *copy = raw_copy != 0
                            ? ConstructTipRecordEdxAbi(raw_copy)
                            : 0;
                        memcpy(copy, tip, 0x88U);
                        StoreU32At(copy, 0x0CU,
                                   reinterpret_cast<u32>(copy));
                        StoreU32At(copy, 0x10U, 0U);
                        StoreU32At(copy, 0x14U, 0U);
                        // Duplicate lands in the paired list at
                        // state+100+12*stage (freed as [v1-24] by
                        // 0x00419040).
                        AppendTipListNodeEaxEdxAbi(
                            reinterpret_cast<u32 *>(
                                static_cast<u8 *>(hint_state) + 0x64U
                                + 12U * static_cast<u32>(stage) - 4U),
                            static_cast<u8 *>(copy) + 0x0CU);
                    }
                }
            }
            if (size <= 0U)
                break;
        }
    }

    free(line);
    free(key);
    free(value);
    free(contents);
    return 0;
}

// TH10 0x00419120.
void UpdateTipEntitiesUsercall(const u32 *walker, u8 style_byte,
                               void *scene_record, i32 free_flag)
{
    if (walker == 0)
        return;

    for (;;) {
        const u32 *next_walker = reinterpret_cast<const u32 *>(walker[1]);
        u8 *const tip = reinterpret_cast<u8 *>(walker[0]);

        const u32 base = LoadU32At(tip, 0x68U);
        if (base == 0U || base == g_CurrentStageSceneId)
            StoreI32At(tip, 0x60U, LoadI32At(tip, 0x60U) - 1);

        if (LoadI32At(tip, 0x60U) <= 0) {
            // Release the old scene handle for this ring slot.
            u32 *const ring_index_ptr = reinterpret_cast<u32 *>(
                static_cast<u8 *>(scene_record) + 0x1A4U);
            const u32 ring = *ring_index_ptr;
            u32 *const handle_slot = reinterpret_cast<u32 *>(
                static_cast<u8 *>(scene_record) + 0xDCU
                + 4U * ring);
            if (*handle_slot != 0U)
                ReleaseEntityById(g_TipEntityManager, *handle_slot);
            *handle_slot = 0U;

            StoreFloatAt(tip, 4U, LoadFloatAt(tip, 4U)
                + LoadFloatAt(tip, 0x7CU) * 2.0f);

            // Spawn the replacement entity (kind ring+2 below the small-
            // scale threshold, ring+12 above it).
            void *entity = 0;
            u32 spawned_id = 0;
            const float scale = LoadFloatAt(tip, 0x7CU);
            if (scale <= 0.81399995f) {
                SpawnTipEntityEdxStackAbi(
                    tip, LoadU32At(&g_SceneRecordHolder, 0U), &spawned_id,
                    static_cast<i32>(ring + 2U));
                *handle_slot = spawned_id;
                entity = spawned_id != 0U
                    ? ResolveTipEntityEaxStackAbi(spawned_id)
                    : 0;
                if (entity != 0)
                    *handle_slot = 0U; // native clears the slot after the
                                       // manual list search
                if (entity != 0) {
                    *reinterpret_cast<u8 *>(
                        static_cast<u8 *>(entity) + 0x3A0U) = 15;
                    *reinterpret_cast<u8 *>(
                        static_cast<u8 *>(entity) + 0x3A1U) = 15;
                }
                const u32 pair[2] = { LoadU32At(tip, 0x70U),
                                      LoadU32At(tip, 0x74U) };
                ResetTipEntityMotionEdxEcxStackAbi(entity, pair);
            } else if (scale > 448.0f) {
                SpawnTipEntityEdxStackAbi(
                    tip, LoadU32At(&g_SceneRecordHolder, 0U), &spawned_id,
                    static_cast<i32>(ring + 12U));
                *handle_slot = spawned_id;
                entity = ResolveTipEntityEaxStackAbi(spawned_id);
                if (entity == 0)
                    *handle_slot = 0U;
                if (entity != 0) {
                    *reinterpret_cast<u8 *>(
                        static_cast<u8 *>(entity) + 0x3A0U) = 30;
                    *reinterpret_cast<u8 *>(
                        static_cast<u8 *>(entity) + 0x3A1U) = 30;
                }
                const u32 pair[2] = { static_cast<u32>(
                                          scale * -3.5f),
                                      0U };
                ResetTipEntityMotionEdxEcxStackAbi(entity, pair);
            } else {
                SpawnTipEntityEdxStackAbi(
                    tip, LoadU32At(&g_SceneRecordHolder, 0U), &spawned_id,
                    static_cast<i32>(ring + 12U));
                *handle_slot = spawned_id;
                entity = ResolveTipEntityEaxStackAbi(spawned_id);
                if (entity == 0)
                    *handle_slot = 0U;
                const u8 alpha = static_cast<u8>(scale * 0.2f);
                if (entity != 0) {
                    *reinterpret_cast<u8 *>(
                        static_cast<u8 *>(entity) + 0x3A0U) = alpha;
                    *reinterpret_cast<u8 *>(
                        static_cast<u8 *>(entity) + 0x3A1U) = alpha;
                }
                const u32 pair[2] = { static_cast<u32>(scale * 192.0f),
                                      static_cast<u32>(scale * -3.5f) };
                ResetTipEntityMotionEdxEcxStackAbi(entity, pair);
            }

            // Publish position/width into the scene record and add the
            // text entity according to the alignment id.
            const u32 align = LoadU32At(tip, 0x64U);
            u32 *const pos_slot = reinterpret_cast<u32 *>(
                static_cast<u8 *>(scene_record) + 0x104U + 12U * ring);
            if (entity != 0) {
                pos_slot[0] = LoadU32At(entity, 0x340U);
                pos_slot[1] = LoadU32At(entity, 0x344U);
                pos_slot[2] = LoadU32At(entity, 0x348U);
            }
            StoreU32At(scene_record, 0x17CU + 4U * ring,
                       LoadU32At(tip, 0x80U));
            if (align == 0U) {
                AddTipTextCenteredEaxStackAbi(
                    g_TipEntityManager, 0xFFFFFFU,
                    reinterpret_cast<const char *>(tip) + 0x1CU);
            } else if (align == 1U) {
                StoreFloatAt(reinterpret_cast<u32 *>(
                                 pos_slot),
                             0U,
                             LoadFloatAt(pos_slot, 0U)
                                 + LoadFloatAt(tip, 0x80U) * 192.0f);
                AddTipTextLeftEaxStackAbi(
                    g_TipEntityManager, 0xFFFFFFU,
                    reinterpret_cast<const char *>(tip) + 0x1CU);
                if (entity != 0)
                    StoreU32At(entity, 0x35CU,
                               (LoadU32At(entity, 0x35CU) & 0xFFF3FFFFU)
                                   | 0x40000U);
            } else if (align == 2U) {
                StoreFloatAt(reinterpret_cast<u32 *>(pos_slot), 0U,
                             LoadFloatAt(pos_slot, 0U)
                                 - LoadFloatAt(tip, 0x80U) * 192.0f);
                AddTipTextRightEaxStackAbi(
                    g_TipEntityManager, 0xFFFFFFU,
                    reinterpret_cast<const char *>(tip) + 0x1CU);
                if (entity != 0)
                    StoreU32At(entity, 0x35CU,
                               (LoadU32At(entity, 0x35CU) & 0xFFF3FFFFU)
                                   | 0x80000U);
            }
            if (entity != 0) {
                StoreU32At(entity, 0x2FCU, LoadU32At(tip, 0x84U));
                *reinterpret_cast<u8 *>(
                    static_cast<u8 *>(entity) + 0x2FFU) = 0;
                StoreU32At(entity, 0x30CU, LoadU32At(tip, 0x6CU));
                StoreU32At(entity, 0x310U, tip[0x87U]);
            }
            *ring_index_ptr = (ring + 1U) % 10U;

            // Unlink the tip from its list (node at +0x0c: prev +0x14,
            // next +0x10) and free it when requested.
            const u32 prev = LoadU32At(tip, 0x14U);
            const u32 next = LoadU32At(tip, 0x10U);
            if (prev != 0U)
                StoreU32At(reinterpret_cast<void *>(prev), 0x10U, next);
            if (next != 0U)
                StoreU32At(reinterpret_cast<void *>(next), 0x14U, prev);
            StoreU32At(tip, 0x10U, 0U);
            StoreU32At(tip, 0x14U, 0U);
            if (free_flag != 0)
                free(tip);
        }

        if (next_walker == 0)
            break;
        walker = next_walker;
    }
}

} // namespace th10
