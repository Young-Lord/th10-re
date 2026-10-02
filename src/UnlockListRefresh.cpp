// TH10 0x00432690 — refresh of the paginated selection list drawn by the
// game-manager state-B body (0x00431ee0, invoked from its page-shift
// paths at 0x4321ba / 0x4322e5 / 0x432369). The visible rows are the
// consecutive matches of the manager's category value (+0xfc) in the
// 110-byte table at 0x4743c0, starting after 10*(N-1) matches where
// N = manager+0x1d4. Each row's save entry (0x90-byte stride, name at
// 0x47783c+0x19a8c, entry dword at +0x19b10) is rendered through the
// 0x00447bb0 text-entity entry together with two dwords of the per-
// difficulty score block (save + 8 + mgr+0x24*0x437c + index*0x90 + 0x61c
// / +0x620). Rows past the last entry (or past the table) are blanked and
// their +0x5d4 entity handles released (slot zeroed when stale).
#include <stdio.h>
#include <string.h>

#include "EntityHelpers.hpp"
#include "GameManagerObject.hpp"
#include "Th10Types.hpp"
#include "UnlockListRefresh.hpp"

namespace th10 {

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

const u32 kCategoryTable = 0x4743C0U;    // 110-byte category byte table
const u32 kScoreSaveState = 0x47783CU;   // DAT_0047783c
const u32 kEntryNameBase = 0x19A8CU;     // save + name (0x90 stride)
const u32 kEntryFlagBase = 0x19B10U;     // save + entry dword (+0x84)
const u32 kTableEntries = 0x6EU;         // 110

u32 LoadU32(u32 address)
{
    return *reinterpret_cast<const u32 *>(address);
}

u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(const_cast<void *>(base)) + offset);
}

i32 LoadI32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const i32 *>(
        static_cast<const u8 *>(const_cast<void *>(base)) + offset);
}

signed char LoadByteAt(u32 address, u32 offset)
{
    return static_cast<signed char>(
        *reinterpret_cast<const u8 *>(address + offset));
}

// TH10 0x00447bb0 (native ESI = row entity, stack = (owner, color, format,
// ...)). Text-entity submission boundary; implemented elsewhere.
extern void AddFormattedTextEntityEsiStackAbi(void *entity, void *owner,
                                              u32 color, const char *format,
                                              ...);

// Blank one row: draw a single space (native format at 0x46cfd4) in white.
void BlankRow(void *entity)
{
    AddFormattedTextEntityEsiStackAbi(entity, g_MainChainRenderOwner,
                                      0xFFFFFFFFU, " ");
}

} // namespace

// TH10 0x00432690. Native ECX = game manager; always returns 0.
void RefreshStateBSelection(void *game_manager)
{
    GameManager &mgr = *reinterpret_cast<GameManager *>(game_manager);

    // Skip the first 10*N-10 category matches (10 rows per category; the
    // scan never bounds-checks the 110-byte table — native quirk kept).
    const i32 target = 10 * mgr.state_b_page_cursor - 10;
    u32 index = 0;
    if (target > 0) {
        i32 matches = 0;
        while (matches < target) {
            if (LoadByteAt(kCategoryTable, index) == mgr.cursor_b_value) {
                ++matches;
            }
            ++index;
        }
    }

    u32 row = 0;
    u32 *handle_slot = mgr.state_b_option_handles;
    mgr.state_row_counter = 0;

    for (;;) {
        // Advance to the next category match.
        while (index < kTableEntries
               && LoadByteAt(kCategoryTable, index)
                      != mgr.cursor_b_value) {
            ++index;
        }
        if (row >= 10U) {
            return;
        }
        if (index >= kTableEntries) {
            // Table exhausted: blank every remaining row (the native
            // 0x432750 loop over 10-row remaining rows).
            for (u32 remaining = 10U - row; remaining != 0U; --remaining) {
                const u32 handle = *handle_slot;
                u8 *entity = 0;
                if (handle != 0U) {
                    entity = FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                                   handle);
                    if (entity == 0) {
                        *handle_slot = 0;
                    }
                }
                BlankRow(entity);
                ++handle_slot;
            }
            return;
        }

        // The matched row: name at save+0x19a8c+index*0x90, entry dword at
        // save+0x19b10+index*0x90.
        const u32 entry_offset = index * 0x90U;
        const u8 *const save =
            reinterpret_cast<const u8 *>(LoadU32(kScoreSaveState));
        const u32 entry_flag =
            *reinterpret_cast<const u32 *>(save + kEntryFlagBase
                                           + entry_offset);
        const bool has_entry = (entry_flag != 0U);

        // Per-difficulty score block (native ecx = save + 8 +
        // mgr+0x24*0x437c); the row dwords live at +0x61c / +0x620 of the
        // 0x90-stride entry.
        const u32 block_offset =
            static_cast<u32>(mgr.cursor_a.value) * 0x437CU + 8U;

        char name[43];
        if (has_entry) {
            // Copy the entry name (unbounded native byte copy — quirk
            // kept) and pad it to 42 characters.
            const char *const source = reinterpret_cast<const char *>(
                save + kEntryNameBase + entry_offset);
            for (u32 i = 0;; ++i) {
                name[i] = source[i];
                if (source[i] == '\0') {
                    break;
                }
            }
            const i32 length = static_cast<i32>(strlen(name));
            if (length < 0x2A) {
                memset(name + length, ' ',
                       static_cast<u32>(0x2A - length));
            }
            name[0x2A] = '\0';
        }

        // Re-resolve the row handle; stale handles are zeroed.
        const u32 handle = *handle_slot;
        u8 *entity = 0;
        if (handle != 0U) {
            entity = FindEntityEdxStackAbi(g_MainChainRenderOwner, handle);
            if (entity == 0) {
                *handle_slot = 0;
            }
        }

        if (has_entry) {
            const u8 *const base =
                reinterpret_cast<const u8 *>(LoadU32(kScoreSaveState))
                + block_offset + entry_offset;
            const u32 marker =
                *reinterpret_cast<const u32 *>(base + 0x61CU);
            // Branchless native color: (marker != 0) ? 0xffff80 : 0xefefef
            // (neg/sbb/and 0x100f91/add 0xefefef chain).
            const u32 color = (marker != 0U) ? 0xFFFF80U : 0xEFEFEFU;
            AddFormattedTextEntityEsiStackAbi(
                entity, g_MainChainRenderOwner, color,
                "No.%3d %s %4d/%4d", static_cast<i32>(index) + 1, name,
                static_cast<i32>(*reinterpret_cast<const u32 *>(base
                                                                 + 0x61CU)),
                static_cast<i32>(*reinterpret_cast<const u32 *>(base
                                                                 + 0x620U)));
        } else {
            const u8 *const base =
                reinterpret_cast<const u8 *>(LoadU32(kScoreSaveState))
                + block_offset + entry_offset;
            AddFormattedTextEntityEsiStackAbi(
                entity, g_MainChainRenderOwner, 0x808080U,
                "No.%3d "
                "\x81\x48\x81\x48\x81\x48\x81\x48\x81\x48"
                "\x81\x48\x81\x48\x81\x48\x81\x48\x81\x48"
                "\x81\x48\x81\x48\x81\x48\x81\x48\x81\x48"
                "\x81\x48\x81\x48\x81\x48\x81\x48\x81\x48"
                " %4d/%4d",
                static_cast<i32>(index) + 1,
                static_cast<i32>(*reinterpret_cast<const u32 *>(base
                                                                 + 0x61CU)),
                static_cast<i32>(*reinterpret_cast<const u32 *>(base
                                                                 + 0x620U)));
        }

        ++index;
        ++handle_slot;
        ++row;
        mgr.state_row_counter += 1U;
    }
}

} // namespace th10
