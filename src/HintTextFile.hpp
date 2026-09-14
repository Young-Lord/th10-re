#pragma once

#include "Th10Platform.hpp"
#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0041a200. Native stdcall, two stack arguments (the hint-text state
// and the template file name), ret 8, returns 0. Writes the automatically
// generated hint template (mkdir "hint", header/time-stamp/Version lines,
// then every non-empty tip of the 8 stage lists serialized as
// Tips/Remain/Text/Pos/Count/Base/Align/Time/Alpha/Color/Scale/End blocks
// with Stage/StageEnd separators) unless the target file already exists.
i32 WriteHintTextTemplateStackAbi(void *hint_state, const char *file_name);

// TH10 0x00419960. Native stdcall, three stack arguments (hint state, file
// name, duplicate flag), ret 0xc, returns 0 (or -1 when the file cannot be
// opened). Parses the "key : value" hint text (Version/Stage/StageEnd/Tips
// blocks with Pos/Text/Count/Time/Base/Align/Remain/Scale/Color/Alpha/End
// fields) into freshly allocated 0x88-byte tip records appended to the
// stage lists; a Version other than "0.0" clears the lists first, and with
// the duplicate flag clear every tip is copied into the paired list.
i32 ParseHintTextFileStackAbi(void *hint_state, const char *file_name,
                              i32 duplicate_flag);

// TH10 0x00419040. Native EAX = the hint-text state; plain ret. Frees the
// 8 pairs of stage tip lists (heads at state+100+12*i and
// state+124+12*i), releasing every list node's tip record.
void FreeHintTipListsEaxAbi(void *hint_state);

// TH10 0x0041a0a0. Native EAX = the string; plain ret, returns EAX. Trims
// leading and trailing space/tab/LF/CR in place.
char *TrimHintStringEaxAbi(char *text);

// TH10 0x004198c0. Native EDX = the name to look up, stack = (table,
// count) of {name, value} pairs; ret 8. Returns the value of the first
// matching entry or 0.
u32 LookupHintKeywordEdxStackAbi(const char *name, const void *table,
                                 i32 count);

// TH10 0x00418d00. Native fastcall EDX = the 0x88-byte record to
// initialize. Zeroes the record, seeds the list node self pointer (+0x0c),
// the -1 defaults at +0x84/+0x70, the 300 at +0x6c and the 1.0f at +0x7c;
// returns the record.
void *ConstructTipRecordEdxAbi(void *record);

// TH10 0x0041ab10. Native EAX = the list-head holder minus 4, EDX = the
// node (tip + 0x0c: next at +0x10, prev at +0x14); plain ret. Appends the
// node at the tail of the singly/pairwise linked list.
void AppendTipListNodeEaxEdxAbi(u32 *head_holder_minus4, void *node);

// TH10 0x0041ab60. Native EAX = tip record; plain ret. Returns tip+0x24.
u32 GetTipSlotEaxAbi(const void *tip);

// TH10 0x00419f40. Native usercall: EAX = the read cursor into the file
// buffer, stack = (destination line buffer, remaining-size pointer, max
// length). Reads the next non-empty, comment-stripped line, advancing the
// cursor and draining the remaining counter; returns the new cursor.
const char *ReadHintTextLineUsercall(const char *cursor, char *line,
                                     u32 *remaining, u32 max_length);

// TH10 0x00419120. Native usercall: EAX = the tip list walker (pairs of
// {tip, next}), EDI = the text style byte, stack = (scene record,
// free-flag). For every tip whose base scene id is 0 or the current one
// the countdown at tip+0x60 is decremented; on expiry the old scene entity
// handle is released, a replacement entity is spawned/resolved, positioned
// by the tip scale, the text/color/time/alpha fields are published into
// the scene record slots and the tip is unlinked (and freed when the
// free-flag is set).
void UpdateTipEntitiesUsercall(const u32 *walker, u8 style_byte,
                               void *scene_record, i32 free_flag);

} // namespace th10
