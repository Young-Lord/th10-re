// ASCII HUD owner lifecycle: the EH-scoped VM-record array constructor
// (0x00413810) and the resource release path (0x00414370) of the
// 0x9ED0-byte owner record published at DAT_004770C.
#include "AsciiHudOwnerLifecycle.hpp"

#include "EntityHelpers.hpp"
#include "ManagerReleaseWrappers.hpp"
#include "Th10Types.hpp"
#include "VmRecord.hpp"

#include <stdlib.h>
#include <string.h>
namespace th10 {

namespace {

// TH10 DAT_004770C: the HUD owner record (constructed here).
void *g_AsciiHudOwnerRecord;

// TH10 DAT_00491C40: the resource owner whose render slot and entity
// lists back the HUD (its +0x3acb9c slot, +0x72d674/+0x72d67c lists).
u8 *g_AsciiHudResourceOwner;

// TH10 DAT_00491C18: the text mirror pointer kept alive across the owner
// teardown (or re-published in replay/demo mode).
void *g_AsciiHudTextMirror;

// TH10 DAT_00474CD0 mode flags: bit 0 / bit 3 (mask 9) select the
// replay/demo release paths.
u32 g_HudOwnerModeFlags;

// TH10 0x402050 boundary: the VM-record array element constructor used by
// the `eh vector constructor iterator` helper (0x45252d).
void ConstructTitleScreenVmRecordCtor(void *record);

// TH10 0x4136C0 boundary: releases the glyph batch state that shares the
// +0x9eb8 buffer before it is freed.
void ReleaseHudGlyphBatchBoundary();

// TH10 0x447810 comes from ManagerReleaseWrappers.hpp
// (ReleaseLargeRenderOwnerSlotEdiAbi).

// TH10 0x45252d `eh vector constructor iterator` (CRT).
void EhVectorConstructorIterator(void *array, u32 element_size, i32 count,
                                 void (*ctor)(void *), void (*dtor)(void *));

} // namespace

namespace {

// Faithful reimplementation of the native two-list scan used by
// 0x414370 (list A at +0x72d674, list B at +0x72d67c).
u32 *FindEntityNodeTwoLists(u32 id)
{
    u32 *node = *reinterpret_cast<u32 **>(g_AsciiHudResourceOwner +
                                          0x72d674);
    if (node != 0) {
        while (*reinterpret_cast<u32 *>(*reinterpret_cast<u8 **>(node)) !=
               id) {
            node = *reinterpret_cast<u32 **>(node + 1);
            if (node == 0) {
                node = *reinterpret_cast<u32 **>(
                    g_AsciiHudResourceOwner + 0x72d67c);
                if (node == 0)
                    return 0;
                while (*reinterpret_cast<u32 *>(
                           *reinterpret_cast<u8 **>(node)) != id) {
                    node = *reinterpret_cast<u32 **>(node + 1);
                    if (node == 0)
                        return 0;
                }
                return node;
            }
        }
        return node;
    }
    node = *reinterpret_cast<u32 **>(g_AsciiHudResourceOwner + 0x72d67c);
    if (node == 0)
        return 0;
    while (*reinterpret_cast<u32 *>(*reinterpret_cast<u8 **>(node)) != id) {
        node = *reinterpret_cast<u32 **>(node + 1);
        if (node == 0)
            return 0;
    }
    return node;
}

} // namespace

// FUNCTION: TH10 0x00413810
void *ConstructAsciiHudOwnerRecords(void *record_memory)
{
    u8 *const record = static_cast<u8 *>(record_memory);

    // Six 0x3AC-byte VM-record arrays, constructed through the CRT
    // iterator with ctor 0x402050 and dtor 0x401ff0.
    const u32 arrays[6] = { 0x10U, 0x24c8U, 0x4980U, 0x6a8cU, 0x793cU,
                            0x8094U };
    const i32 counts[6] = { 10, 10, 9, 4, 2, 7 };
    for (u32 index = 0; index != 6U; ++index) {
        EhVectorConstructorIterator(record + arrays[index], 0x3acU,
                                    counts[index],
                                    ConstructTitleScreenVmRecordCtor,
                                    DestroyTitleScreenVmRecordInPlace);
    }

    // Nine ready flags of the +0x9a48 block cleared; the first one is
    // read from +0x9ab4 first (native quirk: the loaded value feeds the
    // store even though nothing else observes it).
    {
        const u32 flag_offsets[9] = {
            0x6cU, 0xb0U, 0xfcU, 0x128U, 0x174U, 0x1b0U, 0x1fcU, 0x228U,
            0x378U
        };
        u32 first = *reinterpret_cast<u32 *>(record + 0x9ab4) & ~1U;
        *reinterpret_cast<u32 *>(record + 0x9a48 + flag_offsets[0]) = first;
        for (u32 index = 1; index != 9U; ++index)
            *reinterpret_cast<u32 *>(record + 0x9a48 + flag_offsets[index]) &=
                ~1U;
        // The block itself is wiped right after (0x3ac bytes), which
        // overwrites the flag stores above - native order preserved.
        memset(record + 0x9a48, 0, 0x3ac);
        *reinterpret_cast<u16 *>(record + 0x9dcc) = 0xffffU;
    }

    *reinterpret_cast<u32 *>(record + 0x9e70) &= ~1U;
    // The final 0x9ED0-byte wipe erases everything written so far,
    // including the six arrays and the +0x9a48 block - preserved as-is.
    memset(record, 0, 0x9ed0);
    *reinterpret_cast<u32 *>(record) |= 2U;
    g_AsciiHudOwnerRecord = record;
    return record;
}

// FUNCTION: TH10 0x00414370
void ReleaseAsciiHudOwnerResources(void *record_memory)
{
    u8 *const record = static_cast<u8 *>(record_memory);

    if ((g_HudOwnerModeFlags & 9U) != 0U) {
        ReleaseEntitiesUsingResourceEaxEdxAbi(
            g_AsciiHudResourceOwner,
            *reinterpret_cast<u32 *>(record + 0x9e80));
    } else {
        u32 *const slot =
            reinterpret_cast<u32 *>(g_AsciiHudResourceOwner + 0x3acb9c);
        if (*slot != 0) {
            ReleaseLargeRenderOwnerSlotEdiAbi(
                *reinterpret_cast<void **>(slot));
            free(*reinterpret_cast<void **>(slot));
            *slot = 0;
        }
    }

    void *glyph_buffer = *reinterpret_cast<void **>(record + 0x9eb8);
    *reinterpret_cast<u32 *>(record + 0x9e80) = 0;
    if (glyph_buffer != 0) {
        ReleaseHudGlyphBatchBoundary();
        free(glyph_buffer);
        *reinterpret_cast<void **>(record + 0x9eb8) = 0;
    }

    if ((g_HudOwnerModeFlags & 9U) != 0U) {
        g_AsciiHudTextMirror = *reinterpret_cast<void **>(record + 0x9ebc);
    } else {
        if (*reinterpret_cast<void **>(record + 0x9ebc) != 0) {
            free(*reinterpret_cast<void **>(record + 0x9ebc));
            *reinterpret_cast<void **>(record + 0x9ebc) = 0;
        }
        *reinterpret_cast<void **>(record + 0x9ebc) = 0;
        g_AsciiHudTextMirror = 0;
    }

    void *parent = *reinterpret_cast<void **>(record + 8);
    if (parent != 0)
        *reinterpret_cast<u32 *>(static_cast<u8 *>(parent) + 4) &= ~2U;

    ReleaseEntityById(g_AsciiHudResourceOwner,
                      *reinterpret_cast<u32 *>(record + 0x9e54));
    *reinterpret_cast<u32 *>(record + 0x9e54) = 0;
    ReleaseEntityById(g_AsciiHudResourceOwner,
                      *reinterpret_cast<u32 *>(record + 0x9e58));
    *reinterpret_cast<u32 *>(record + 0x9e58) = 0;
    ReleaseEntityById(g_AsciiHudResourceOwner,
                      *reinterpret_cast<u32 *>(record + 0x9e64));
    *reinterpret_cast<u32 *>(record + 0x9e64) = 0;

    for (u32 offset = 0x9e70U; offset != 0x9e9cU + 4U; offset += 4U)
        *reinterpret_cast<u32 *>(record + offset) = 0;

    // Eight tracked-entity slots at +0x9e34: find each id in the two
    // resource-owner lists and flag the entity (and its children when
    // entity+0x18 == 0) with +0x35c bit 0x4000000, then clear the slot.
    u32 *slot = reinterpret_cast<u32 *>(record + 0x9e34);
    i32 remaining = 8;
    do {
        const u32 id = *slot;
        if (id != 0) {
            u32 *node = FindEntityNodeTwoLists(id);
            if (node != 0) {
                u32 *entity = *reinterpret_cast<u32 **>(node);
                if (entity != 0) {
                    *reinterpret_cast<u32 *>(entity + 0x35c) |= 0x4000000U;
                    if (*reinterpret_cast<u32 *>(entity + 0x18) == 0U) {
                        u32 *child =
                            *reinterpret_cast<u32 **>(entity + 0x14);
                        for (; child != 0;
                             child = *reinterpret_cast<u32 **>(child + 1))
                            reinterpret_cast<VmRecord *>(*child)->flags |=
                                0x4000000U;
                    }
                }
            }
        }
        *slot++ = 0;
        --remaining;
    } while (remaining != 0);

    *reinterpret_cast<u32 *>(record + 0x9e90) = 0;
}

} // namespace th10
