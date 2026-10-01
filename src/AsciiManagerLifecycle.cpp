#include "AsciiManager.hpp"
#include "AsciiAnimationVm.hpp"
#include "AsciiGlyphRenderer.hpp"

#include <string.h>

#include "CallbackScheduler.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "MainChainRender.hpp"
#include "ManagerWork.hpp"

namespace th10 {

namespace {

typedef i32 (TH10_STDCALL *D3DSetViewportFn)(D3D9Device *,
                                               const D3DViewport *);

extern AsciiManager *g_AsciiManager; // TH10 DAT_004776e0
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern D3D9Device *g_MainChainD3D9Device; // TH10 DAT_00491c30
extern u32 g_AsciiActiveViewIsDefault; // TH10 DAT_00491fb0
extern MainChainCameraWork g_AsciiDefaultView;
extern MainChainCameraWork g_AsciiAlternateView;

extern void *AllocateAsciiManagerMemory(u32 bytes); // TH10 0x452493
extern void FreeAsciiManagerMemory(void *pointer); // TH10 0x4524a1
extern void ReleaseAsciiManagerBuffer(void *pointer); // TH10 0x452422
extern void AppendAsciiResourceLoadError(); // TH10 0x44b810

void *GetD3DSlot(D3D9Device *device, u32 index)
{
    return device->vtable[index];
}

u8 *ManagerBytes(AsciiManager *manager)
{
    return reinterpret_cast<u8 *>(manager);
}

void **ManagerPointer(AsciiManager *manager, u32 offset)
{
    return reinterpret_cast<void **>(ManagerBytes(manager) + offset);
}

u32 *ManagerWord(AsciiManager *manager, u32 offset)
{
    return reinterpret_cast<u32 *>(ManagerBytes(manager) + offset);
}

i32 TH10_FASTCALL ResetAsciiStringsCallback(void *manager)
{
    AsciiManager *const ascii = static_cast<AsciiManager *>(manager);
    ascii->num_strings = 0;
    *ManagerWord(ascii, 0x8970) = 0;
    ++*ManagerWord(ascii, 0x8990);
    return 0;
}

i32 TH10_FASTCALL DrawAsciiStringsCallback(void *manager)
{
    return DrawAsciiManagerSecondaryStrings(static_cast<AsciiManager *>(manager));
}

i32 TH10_FASTCALL DrawAsciiPrimaryStringsCallback(void *manager)
{
    return DrawAsciiManagerPrimaryStrings(static_cast<AsciiManager *>(manager));
}

void ConfigureAsciiView(MainChainCameraWork *view, u32 default_view)
{
    UpdateMainChainCameraWorkEdiAbi(view);
    (void)reinterpret_cast<D3DSetViewportFn>(GetD3DSlot(
        g_MainChainD3D9Device, 47))(g_MainChainD3D9Device, &view->viewport);
    g_AsciiActiveViewIsDefault = default_view;
}

void RemoveAsciiCallback(ChainElem *record)
{
    if (record != 0)
        CallbackSchedulerApi::RemoveSynchronized(g_CallbackScheduler, record);
}

void ReleaseAsciiResourceSlot(void *owner, u32 slot)
{
    LargeRenderOwnerLayout &owner_ref = *static_cast<LargeRenderOwnerLayout *>(owner);
    void **const slot_pointer = &owner_ref.work_slots[slot];
    ManagerWorkPartial *const work = static_cast<ManagerWorkPartial *>(
        *slot_pointer);
    if (work == 0)
        return;
    ReleaseManagerWorkContents(work);
    FreeAsciiManagerMemory(work);
    *slot_pointer = 0;
}

void *GetAsciiResourceGlyph(void *resource, u32 glyph_index)
{
    return static_cast<u8 *>(resource) + 0x118 + glyph_index * 0x44;
}

} // namespace

AsciiManager *ConstructAsciiManager(AsciiManager *manager)
{
    // These transient stores/reads precede the native whole-object clear.
    static const u32 first_flags[] = {
        0x080, 0x0c4, 0x110, 0x13c, 0x188, 0x1c4, 0x210, 0x23c, 0x38c
    };
    static const u32 second_flags[] = {
        0x42c, 0x470, 0x4bc, 0x4e8, 0x534, 0x570, 0x5bc, 0x5e8, 0x738
    };
    u8 *const bytes = ManagerBytes(manager);
    *reinterpret_cast<void **>(bytes) = reinterpret_cast<void *>(0x0046cb14);
    for (u32 index = 0; index != sizeof(first_flags) / sizeof(first_flags[0]); ++index)
        *ManagerWord(manager, first_flags[index]) &= ~1U;
    memset(bytes + 0x14, 0, 0x3ac);
    *reinterpret_cast<u16 *>(bytes + 0x398) = 0xffff;
    for (u32 index = 0; index != sizeof(second_flags) / sizeof(second_flags[0]); ++index)
        *ManagerWord(manager, second_flags[index]) &= ~1U;
    memset(bytes + 0x3c0, 0, 0x3ac);
    *reinterpret_cast<u16 *>(bytes + 0x744) = 0xffff;

    memset(manager, 0, sizeof(*manager));
    *ManagerWord(manager, 0x4) |= 2;
    g_AsciiManager = manager;
    manager->color = 0xffffffffU;
    manager->scale_x = 1.0f;
    manager->scale_y = 1.0f;
    manager->unknown_8984 = 0;
    manager->unknown_898c = 9;
    return manager;
}

i32 InitializeAsciiManager(AsciiManager *manager)
{
    void *const owner = g_MainChainRenderOwner;
    ChainElem *record;
    *ManagerPointer(manager, 0x8994) = RequestManagerWork(
        reinterpret_cast<ManagerWorkOwnerPartial *>(owner), 2, "ascii.anm");
    if (*ManagerPointer(manager, 0x8994) == 0)
        goto failed;
    *ManagerPointer(manager, 0x899c) = RequestManagerWork(
        reinterpret_cast<ManagerWorkOwnerPartial *>(owner), 0, "text.anm");
    if (*ManagerPointer(manager, 0x899c) == 0)
        goto failed;
    *ManagerPointer(manager, 0x8998) = RequestManagerWork(
        reinterpret_cast<ManagerWorkOwnerPartial *>(owner), 3, "capture.anm");
    if (*ManagerPointer(manager, 0x8998) == 0)
        goto failed;

    record = CallbackSchedulerApi::Create(ResetAsciiStringsCallback);
    record->flags &= ~ChainElemFlag_Enabled;
    record->arg = manager;
    (void)CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler, record, 4);
    *reinterpret_cast<ChainElem **>(ManagerBytes(manager) + 0xc) = record;

    record = CallbackSchedulerApi::Create(DrawAsciiPrimaryStringsCallback);
    record->flags &= ~ChainElemFlag_Enabled;
    record->arg = manager;
    (void)CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, record, 48);
    *reinterpret_cast<ChainElem **>(ManagerBytes(manager) + 0x10) = record;

    record = CallbackSchedulerApi::Create(DrawAsciiStringsCallback);
    record->flags &= ~ChainElemFlag_Enabled;
    record->arg = manager;
    (void)CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, record, 38);
    *reinterpret_cast<ChainElem **>(ManagerBytes(manager) + 0x89a8) = record;

    ResetAsciiAnimationVmRecord(ManagerBytes(manager) + 0x14);
    *ManagerPointer(manager, 0x31c) = *ManagerPointer(manager, 0x8994);
    (void)InitializeAsciiAnimationVmEntry(ManagerBytes(manager) + 0x14, 0,
                                           *ManagerPointer(manager, 0x8994));
    ResetAsciiAnimationVmRecord(ManagerBytes(manager) + 0x3c0);
    *ManagerPointer(manager, 0x6c8) = *ManagerPointer(manager, 0x8994);
    (void)InitializeAsciiAnimationVmEntry(ManagerBytes(manager) + 0x3c0, 98,
                                           *ManagerPointer(manager, 0x8994));
    return 0;

failed:
    AppendAsciiResourceLoadError();
    return -1;
}

void DestroyAsciiManagerInPlace(AsciiManager *manager)
{
    *reinterpret_cast<void **>(ManagerBytes(manager)) =
        reinterpret_cast<void *>(0x0046cb14);
    RemoveAsciiCallback(*reinterpret_cast<ChainElem **>(ManagerBytes(manager) + 0xc));
    RemoveAsciiCallback(*reinterpret_cast<ChainElem **>(ManagerBytes(manager) + 0x10));
    RemoveAsciiCallback(*reinterpret_cast<ChainElem **>(ManagerBytes(manager) + 0x89a8));
    ReleaseAsciiResourceSlot(g_MainChainRenderOwner, 2);
    ReleaseAsciiResourceSlot(g_MainChainRenderOwner, 0);
    ReleaseAsciiResourceSlot(g_MainChainRenderOwner, 3);
    g_AsciiManager = 0;
    if (*ManagerPointer(manager, 0x718) != 0)
        ReleaseAsciiManagerBuffer(*ManagerPointer(manager, 0x718));
    *ManagerPointer(manager, 0x718) = 0;
    if (*ManagerPointer(manager, 0x36c) != 0)
        ReleaseAsciiManagerBuffer(*ManagerPointer(manager, 0x36c));
    *ManagerPointer(manager, 0x36c) = 0;
}

AsciiManager *CreateAsciiManager()
{
    AsciiManager *manager = static_cast<AsciiManager *>(
        AllocateAsciiManagerMemory(sizeof(AsciiManager)));
    if (manager != 0)
        manager = ConstructAsciiManager(manager);
    if (InitializeAsciiManager(manager) != 0) {
        if (manager != 0) {
            DestroyAsciiManagerInPlace(manager);
            FreeAsciiManagerMemory(manager);
        }
        return 0;
    }
    return manager;
}

// FUNCTION: TH10 0x00401a50 (secondary queue body; native 0x00401520 is a
// 0xa-byte thunk that moves ECX into EBX and calls this body).
// EBX = AsciiManager; iterates the secondary 0x68-byte string queue at
// +0x6f6c (count at +0x8970). The +0x370 mode word is first masked with
// 0xffd7fffe and ORed with 0x140001. Each entry refreshes the working
// position (+0x348..+0x350) and scales (+0x50/+0x54), sets the +0x370 bit 3,
// and advances the glyph X cursor by (i32)+0x898c * entry scale_x. A change
// of the entry's GUI mode (+0x5c) flushes pending vertices and switches the
// default/alternate camera views through the device SetViewport slot.
// Newlines advance Y by 14 * scale_y and reset X to the entry origin; spaces
// advance only; any other byte selects the glyph at ascii.anm + 0x118 +
// (byte - 0x20) * 0x44 (stored at +0x3a8), stores the entry color at +0x310,
// and draws through the unscaled path when scale_x is exactly 1.0 else the
// scaled path. When the loop ends on a nonzero GUI mode (or the queue was
// empty), the default view is restored. Always returns 1.
i32 DrawAsciiManagerSecondaryStrings(AsciiManager *manager)
{
    u8 *const bytes = ManagerBytes(manager);
    void *const vm = bytes + 0x14;
    *ManagerWord(manager, 0x370) = (*ManagerWord(manager, 0x370) & 0xffd7fffeU) |
        0x00140001U;
    u32 current_gui_mode = 1;
    const i32 count = *reinterpret_cast<const i32 *>(ManagerBytes(manager) + 0x8970);
    for (i32 index = 0; index < count; ++index) {
        const AsciiManagerString &entry = manager->secondary_strings[index];
        memcpy(bytes + 0x348, &entry.position, sizeof(entry.position));
        *reinterpret_cast<float *>(bytes + 0x50) = entry.scale_x;
        *reinterpret_cast<float *>(bytes + 0x54) = entry.scale_y;
        *ManagerWord(manager, 0x370) |= 8;
        const float advance = static_cast<float>(manager->unknown_898c) * entry.scale_x;
        if (current_gui_mode != entry.is_gui) {
            current_gui_mode = entry.is_gui;
            FlushRenderOwnerPendingVertices(
                reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
            ConfigureAsciiView(current_gui_mode != 0 ? &g_AsciiAlternateView :
                &g_AsciiDefaultView, current_gui_mode == 0);
        }
        for (const u8 *character = reinterpret_cast<const u8 *>(entry.text);
             *character != 0; ++character) {
            if (*character == '\n') {
                *reinterpret_cast<float *>(bytes + 0x34c) += 14.0f * entry.scale_y;
                *reinterpret_cast<float *>(bytes + 0x348) = entry.position.x;
            } else {
                if (*character != ' ') {
                    *reinterpret_cast<void **>(bytes + 0x3a8) = GetAsciiResourceGlyph(
                        *ManagerPointer(manager, 0x8994), *character - 0x20);
                    *ManagerWord(manager, 0x310) = entry.color;
                    if (*reinterpret_cast<float *>(bytes + 0x50) == 1.0f)
                        DrawAsciiAnimationVmUnscaled(vm);
                    else
                        DrawAsciiAnimationVmScaled(vm);
                }
                *reinterpret_cast<float *>(bytes + 0x348) += advance;
            }
        }
    }
    if (current_gui_mode != 0) {
        FlushRenderOwnerPendingVertices(
            reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
        ConfigureAsciiView(&g_AsciiDefaultView, 1);
    }
    return 1;
}

// FUNCTION: TH10 0x00401760 (primary queue body; the priority-48 callback at
// 0x00401510 forwards here).
i32 DrawAsciiManagerPrimaryStrings(AsciiManager *manager)
{
    u8 *const bytes = ManagerBytes(manager);
    void *const vm = bytes + 0x14;
    *ManagerWord(manager, 0x370) = (*ManagerWord(manager, 0x370) & 0xffd7ffffU) |
        0x00140001U;
    u32 current_gui_mode = 1;
    for (i32 index = 0; index < manager->num_strings; ++index) {
        const AsciiManagerString &entry = manager->strings[index];
        memcpy(bytes + 0x348, &entry.position, sizeof(entry.position));
        *reinterpret_cast<float *>(bytes + 0x50) = entry.scale_x;
        *reinterpret_cast<float *>(bytes + 0x54) = entry.scale_y;
        *ManagerWord(manager, 0x370) |= 8;

        const bool narrow_font = entry.is_selected == 1;
        const float line_advance = (narrow_font ? 9.0f : 14.0f) * entry.scale_y;
        const float glyph_advance = narrow_font ? 7.0f :
            static_cast<float>(manager->unknown_898c) * entry.scale_x;
        if (current_gui_mode != entry.is_gui) {
            current_gui_mode = entry.is_gui;
            FlushRenderOwnerPendingVertices(
                reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
            ConfigureAsciiView(current_gui_mode != 0 ? &g_AsciiAlternateView :
                &g_AsciiDefaultView, current_gui_mode == 0);
        }

        for (const u8 *character = reinterpret_cast<const u8 *>(entry.text);
             *character != 0; ++character) {
            if (*character == '\n') {
                *reinterpret_cast<float *>(bytes + 0x34c) += line_advance;
                *reinterpret_cast<float *>(bytes + 0x348) = entry.position.x;
                continue;
            }
            if (*character != ' ') {
                void *const glyph = GetAsciiResourceGlyph(
                    *ManagerPointer(manager, 0x8994),
                    98 * entry.is_selected + *character - 0x20);
                *reinterpret_cast<void **>(bytes + 0x3a8) = glyph;
                *ManagerWord(manager, 0x60) =
                    *reinterpret_cast<u32 *>(static_cast<u8 *>(glyph) + 0x34);
                *ManagerWord(manager, 0x64) =
                    *reinterpret_cast<u32 *>(static_cast<u8 *>(glyph) + 0x30);

                if (entry.text_mode != 0) {
                    *ManagerWord(manager, 0x310) = entry.color & 0xff000000U;
                    *(bytes + 0x313) = static_cast<u8>(entry.color >> 25);
                    *reinterpret_cast<float *>(bytes + 0x348) += 2.0f;
                    *reinterpret_cast<float *>(bytes + 0x34c) += 2.0f;
                    DrawAsciiAnimationVmUnscaled(vm);
                    *reinterpret_cast<float *>(bytes + 0x348) -= 2.0f;
                    *reinterpret_cast<float *>(bytes + 0x34c) -= 2.0f;
                }
                *ManagerWord(manager, 0x310) = entry.color;
                DrawAsciiAnimationVmUnscaled(vm);
            }
            *reinterpret_cast<float *>(bytes + 0x348) += glyph_advance;
        }
    }
    if (current_gui_mode != 0) {
        FlushRenderOwnerPendingVertices(
            reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
        ConfigureAsciiView(&g_AsciiDefaultView, 1);
    }
    return 1;
}

} // namespace th10
