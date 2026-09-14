// ECL script test menu ("Spt Test", TH10 0x0042bad0-0x0042c5a0). A debug
// menu manager published at DAT_00477844 that enumerates ../../data/*.ecl,
// loads the selected script through the shared file loader, hands the
// decoded section list to the 0x1098-byte viewer object and runs the
// resulting context list (0x44fd10) until it drains. Sibling of the
// spell-practice select menu 0x0040a450 (src/EclSelectMenu.cpp) with a
// smaller 0x2e4-byte object and its own continuation worker entry
// 0x0042be10.
#include "ScriptTestMenu.hpp"

#include <stdio.h>

#include "AsciiManager.hpp"
#include "CallbackScheduler.hpp"
#include "EclScriptVm.hpp"
#include "GameManagerState.hpp"
#include "ManagerWork.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "ThreadControl.hpp"

namespace th10 {

namespace {

// Shared globals (addresses resolved against the binary).
extern void *g_AsciiManagerHost;                        // TH10 DAT_004776e0
extern CallbackScheduler *g_CallbackScheduler;          // TH10 DAT_00491be4
extern void *g_ScriptTestManagerSlot;                   // TH10 DAT_00477844
extern u32 g_ManagerSubGateFlags;                       // TH10 DAT_00474e36
extern u8 g_MenuInputFlagsByte;                         // TH10 DAT_00474e34
extern u32 g_ManagerConfirmFlagsSecond;                 // TH10 DAT_00474e38
extern void *g_MainChainRenderOwner;                    // TH10 DAT_00491c10
extern void *g_MainChainContextSlot;                    // TH10 DAT_00491c28

// TH10 0x00452706 (CRT malloc) / 0x00452422 (free) / 0x00452493 (operator
// new) / 0x004524a1 (operator delete).
extern void *AllocateResourceBuffer(u32 bytes);
extern void ReleaseResourceBuffer(void *pointer);
extern void *AllocateMainChainObject(u32 bytes);
extern void FreeMainChainObject(void *object);

// TH10 0x0044b360 (body in MainChainFileProbe.cpp): whole-file load with an
// optional size out-param and a filesystem mode flag.
extern void *LoadMainChainFile(const char *path, u32 *file_size,
                               i32 filesystem_mode);

// TH10 0x0044c1c0 with the native EDI worker entry fixed to 0x0042be10
// (EAX = control, stack = argument). The shared RegisterTimelineContinuation
// hardcodes the timeline worker, so this caller keeps a dedicated boundary.
extern void StartScriptTestContinuationWorker(void *control, void *argument);

// TH10 0x00450500: allocates the 0x103c-byte context-list manager (vtable
// 0x46d0d8), captures the caller's EDI (the 0x1098-byte viewer object) into
// manager +0x102c and initializes the embedded record at +4 through
// 0x00450470 with the script reference. Returns the manager or 0.
extern void *CreateEclContextListStackAbi(void *script_reference);

u32 LoadU32From(const void *address)
{
    return *static_cast<const u32 *>(address);
}

void StoreU32To(void *address, u32 value)
{
    *static_cast<u32 *>(address) = value;
}

float LoadF32From(const void *address)
{
    return *static_cast<const float *>(address);
}

// Virtual release calls: vtable slot 5 (offset 0x14) with the flag 1 — the
// context-list manager's refcount-style release.
typedef void (*ReleaseMethod)(void *self, u32 flag);

void ReleaseContextListManager(void *&slot)
{
    void *const manager = slot;
    if (manager != 0) {
        void **const vtable = *static_cast<void ***>(manager);
        reinterpret_cast<ReleaseMethod>(vtable[5])(manager, 1U);
    }
    slot = 0;
}

// Cursor seed used by sub-state 0: the native three-way compares run
// against a zeroed index register, so the seed is `value < 0 ? value - 1
// : 0` for every field.
u32 SeedCursorAgainstZero(i32 value)
{
    return static_cast<u32>(value < 0 ? value - 1 : 0);
}

// Shift a cursor record when either the gate-flag byte or the input byte
// carries the requested mask (raw byte tests, not the word poll).
void ShiftOnByteMasks(u8 *record, u8 low_byte, u8 mask)
{
    if (((low_byte & mask) != 0U)
        || ((g_MenuInputFlagsByte & mask) != 0U)) {
        ShiftManagerSelector(record, 1);
    }
}

void ShiftOnByteMasksDown(u8 *record, u8 low_byte, u8 mask)
{
    if (((low_byte & mask) != 0U)
        || ((g_MenuInputFlagsByte & mask) != 0U)) {
        ShiftManagerSelector(record, -1);
    }
}

// Win32 file enumeration (platform boundary), mirroring the sibling menu.
struct FindDataA {
    u32 attributes;
    u8 creation_time[8];
    u8 access_time[8];
    u8 write_time[8];
    u32 size_high;
    u32 size_low;
    u32 reserved[2];
    char file_name[260];
    char alternate_name[14];
};

extern "C" void *TH10_STDCALL FindFirstFileA(const char *file_name,
                                             FindDataA *data);
extern "C" i32 TH10_STDCALL FindNextFileA(void *handle, FindDataA *data);
extern "C" void TH10_STDCALL FindClose(void *handle);

// --- Scheduler callback wrappers -----------------------------------------

// TH10 0x0042c580: jmp 0x0042bfc0 (the ECX callback argument is the menu).
i32 TH10_FASTCALL ScriptTestUpdateCallback(void *arg)
{
    return UpdateScriptTestMenuEcxAbi(arg);
}

// TH10 0x0042c590: mov edi,ecx; call 0x0042c360.
i32 TH10_FASTCALL ScriptTestDrawCallback(void *arg)
{
    DrawScriptTestMenuEdiAbi(arg);
    return 1;
}

// --- Sub-state bodies -----------------------------------------------------

// Sub-state 0 tail (native 0x42c298): cursor seeds against the zeroed
// index register, run-flag seeds and the position-readout defaults.
void SeedMenuCursors(u8 *menu)
{
    StoreU32To(menu + 0x44U, 3U);
    StoreU32To(menu + 0x10cU, 1U);
    StoreU32To(menu + 0x3cU,
               SeedCursorAgainstZero(
                   static_cast<i32>(LoadU32From(menu + 0x44U))));
    StoreU32To(menu + 0x11cU, LoadU32From(menu + 0x38U));
    StoreU32To(menu + 0x1e4U, 1U);
    StoreU32To(menu + 0x114U,
               SeedCursorAgainstZero(
                   static_cast<i32>(LoadU32From(menu + 0x38U))));
    StoreU32To(menu + 0x1f4U, 1U);
    StoreU32To(menu + 0x2bcU, 1U);
    StoreU32To(menu + 0x1ecU, SeedCursorAgainstZero(1));
    StoreU32To(menu + 0x30U, 1U);
    StoreU32To(menu + 0x2c4U, 0U);
    StoreU32To(menu + 0x2c8U, 0x42000000U); // 32.0f
    StoreU32To(menu + 0x2ccU, 0U);
    StoreU32To(menu + 0x2d0U, 0x3e8U);      // 1000
}

// Sub-state 1, cursor 0 (native 0x42c154): shift the file-list cursor on
// the 0x80/0x40 byte masks, then the 0x1001 gate clears the continuation
// busy bit and schedules the 0x0042be10 load worker (sub-state 2).
void RunFileListConfirmPath(u8 *menu)
{
    u32 value = LoadU32From(menu + 0x114U);
    StoreU32To(menu + 0x118U, value);
    const u8 low_byte = static_cast<u8>(g_ManagerSubGateFlags & 0xffU);
    ShiftOnByteMasks(menu + 0x114U, low_byte, 0x80U);
    ShiftOnByteMasksDown(menu + 0x114U, low_byte, 0x40U);
    if ((g_ManagerSubGateFlags & 0x1001U) == 0U) {
        return;
    }
    StoreU32To(menu + 0x2d4U, LoadU32From(menu + 0x2d4U)
                                  & static_cast<u32>(-3));
    StartScriptTestContinuationWorker(menu + 0x10U, menu);
    StoreU32To(menu + 0x30U, 2U);
}

// Sub-state 1, cursor 1 (native 0x42c0ae): shift the script cursor on the
// 0x80/0x40 word polls, then the 0x1001 gate at DAT_00474e38 releases the
// previous context list and creates the new one from the viewer's entry
// table (sub-state 4).
void RunScriptListConfirmPath(u8 *menu)
{
    if (LoadU32From(menu + 0x2d8U) == 0U) {
        return;
    }
    const u32 select = LoadU32From(menu + 0x1ecU);
    StoreU32To(menu + 0x1f0U, select);
    if (PollMenuInputState(0x80U)) {
        ShiftManagerSelector(menu + 0x1ecU, 1);
    }
    if (PollMenuInputState(0x40U)) {
        ShiftManagerSelector(menu + 0x1ecU, -1);
    }
    if ((g_ManagerConfirmFlagsSecond & 0x1001U) == 0U) {
        return;
    }
    ReleaseContextListManager(*reinterpret_cast<void **>(menu + 0x2dcU));

    // The native re-reads the viewer without a null check; the sub-state
    // entry guard above already ensured it is present.
    const u8 *const viewer =
        reinterpret_cast<const u8 *>(LoadU32From(menu + 0x2d8U));
    const u32 index = LoadU32From(menu + 0x1ecU);
    const u32 entry_table = LoadU32From(viewer + 0x8cU);
    const u32 script_reference =
        LoadU32From(reinterpret_cast<const void *>(entry_table + index * 8U));
    StoreU32To(menu + 0x2dcU,
               reinterpret_cast<u32>(CreateEclContextListStackAbi(
                   reinterpret_cast<void *>(script_reference))));
    StoreU32To(menu + 0x30U, 4U);
}

// Sub-state 4 (native 0x42bff3): run the context list with delta 1.0 and
// drop back to sub-state 1 once its embedded record's head pointer is
// null. The native dereferences [[list+4]+4] unchecked — quirk preserved.
void RunContextList(u8 *menu)
{
    void *const list =
        reinterpret_cast<void *>(LoadU32From(menu + 0x2dcU));
    (void)RunEclContextListEdiStackAbi(list, 1.0f);

    const u32 updated = LoadU32From(menu + 0x2dcU);
    if (updated != 0U) {
        const u32 record = LoadU32From(reinterpret_cast<const void *>(
            updated + 4U));
        if (LoadU32From(reinterpret_cast<const void *>(record + 4U)) != 0U) {
            return;
        }
    }
    ReleaseContextListManager(*reinterpret_cast<void **>(menu + 0x2dcU));
    StoreU32To(menu + 0x30U, 1U);
}

} // namespace

// TH10 0x0042bb30 semantic body.
void *InitializeScriptTestManagerEdxAbi(void *manager)
{
    u8 *const bytes = static_cast<u8 *>(manager);

    // The native body seeds +0x14..+0x20, the +0x10 vtable (0x4703e4),
    // +0x3c, +0x44 (999), +0x10c (1), +0x110, +0x114, +0x11c (999),
    // +0x1a0, +0x1e4 (1), +0x1e8, +0x1f4 (999), +0x278, +0x2bc (1) and
    // +0x2c0 — all immediately erased by its trailing rep stos of 0xb9
    // dwords (dead stores, quirk preserved). Only the observable tail is
    // implemented.
    for (u32 i = 0; i < 0x2e4U; ++i) {
        bytes[i] = 0U;
    }
    StoreU32To(bytes, LoadU32From(bytes) | 2U);
    g_ScriptTestManagerSlot = manager;
    return manager;
}

// TH10 0x0042bbd0 semantic body.
i32 RegisterScriptTestSchedulerRecordsEbxAbi(void *manager)
{
    ChainElem *const update =
        CallbackSchedulerApi::Create(ScriptTestUpdateCallback);
    update->flags |= ChainElemFlag_Enabled;
    update->arg = manager;
    (void)CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler,
                                                      update, 5);
    StoreU32To(static_cast<u8 *>(manager) + 0x8U,
               reinterpret_cast<u32>(update));

    ChainElem *const draw =
        CallbackSchedulerApi::Create(ScriptTestDrawCallback);
    draw->flags |= ChainElemFlag_Enabled;
    draw->arg = manager;
    (void)CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw,
                                               0x27);
    StoreU32To(static_cast<u8 *>(manager) + 0xcU,
               reinterpret_cast<u32>(draw));
    return 0;
}

// TH10 0x0042bf30 semantic body.
void *CreateScriptTestManager()
{
    void *manager = AllocateMainChainObject(0x2e4U);
    if (manager != 0) {
        manager = InitializeScriptTestManagerEdxAbi(manager);
    }

    // Native quirk: the registration runs even for the null manager; its
    // manager+8 store then faults, exactly as in the original.
    if (RegisterScriptTestSchedulerRecordsEbxAbi(manager) != 0) {
        if (manager != 0) {
            DestroyScriptTestMenuStackAbi(manager);
            FreeMainChainObject(manager);
        }
        return 0;
    }
    return manager;
}

// TH10 0x0042bad0 semantic body.
void ReleaseScriptTestMenuFileNamesEsiAbi(void *manager)
{
    u8 *const bytes = static_cast<u8 *>(manager);
    if (LoadU32From(bytes + 0x34U) == 0U) {
        return;
    }
    const u32 count = LoadU32From(bytes + 0x38U);
    if (static_cast<i32>(count) > 0) {
        void **const names =
            reinterpret_cast<void **>(LoadU32From(bytes + 0x34U));
        for (u32 i = 0; i < count; ++i) {
            if (names[i] != 0) {
                ReleaseResourceBuffer(names[i]);
                names[i] = 0;
            }
        }
    }
    void *const list =
        reinterpret_cast<void *>(LoadU32From(bytes + 0x34U));
    if (list != 0) {
        ReleaseResourceBuffer(list);
    }
    StoreU32To(bytes + 0x34U, 0U);
    StoreU32To(bytes + 0x34U, 0U); // native stores the clear twice (quirk)
}

// TH10 0x0042bfc0 semantic body.
i32 UpdateScriptTestMenuEcxAbi(void *menu_object)
{
    u8 *const menu = static_cast<u8 *>(menu_object);
    const i32 state = static_cast<i32>(LoadU32From(menu + 0x30U));

    if (state == 0) {
        ReleaseScriptTestMenuFileNamesEsiAbi(menu);

        u32 count = 0U;
        FindDataA data;
        void *handle = FindFirstFileA("../../data/*.ecl", &data);
        if (handle != reinterpret_cast<void *>(-1)) {
            do {
                ++count;
            } while (FindNextFileA(handle, &data) != 0);
        }
        FindClose(handle);
        StoreU32To(menu + 0x38U, count);
        if (count != 0U) {
            void **const names = static_cast<void **>(
                AllocateResourceBuffer(4U * count));
            // Native quirk: the malloc result is stored unchecked; a
            // failure writes through null below, as in the original.
            StoreU32To(menu + 0x34U, reinterpret_cast<u32>(names));
            void *scan = FindFirstFileA("../../data/*.ecl", &data);
            for (u32 index = 0U; index < count; ++index) {
                // Inline strlen of cFileName followed by a same-length
                // copy including the terminator.
                u32 length = 0U;
                while (data.file_name[length] != '\0') {
                    ++length;
                }
                names[index] = AllocateResourceBuffer(length + 1U);
                char *const out = static_cast<char *>(names[index]);
                for (u32 i = 0; i <= length; ++i) {
                    out[i] = data.file_name[i];
                }
                if (FindNextFileA(scan, &data) == 0) {
                    break;
                }
            }
            FindClose(scan);
        }
        SeedMenuCursors(menu);
        return 1;
    }

    if (state == 1) {
        // Menu cursor (+0x3c): byte-mask shifts on 0x20/0x10, then the
        // dispatch on the shifted value.
        u32 value = LoadU32From(menu + 0x3cU);
        StoreU32To(menu + 0x40U, value);
        const u8 low_byte = static_cast<u8>(g_ManagerSubGateFlags & 0xffU);
        ShiftOnByteMasks(menu + 0x3cU, low_byte, 0x20U);
        ShiftOnByteMasksDown(menu + 0x3cU, low_byte, 0x10U);
        value = LoadU32From(menu + 0x3cU);
        if (value == 0U) {
            RunFileListConfirmPath(menu);
        } else if (value == 1U) {
            RunScriptListConfirmPath(menu);
        } else if (value == 2U) {
            if ((g_ManagerSubGateFlags & 0x1001U) != 0U) {
                RequestGameStateTransitionEaxStackAbi(g_MainChainContextSlot,
                                                      3);
            }
        }
        return 1;
    }

    if (state == 4) {
        RunContextList(menu);
    }
    return 1;
}

// TH10 0x0042c360 semantic body.
i32 DrawScriptTestMenuEdiAbi(void *menu_object)
{
    u8 *const menu = static_cast<u8 *>(menu_object);
    AsciiManager &ascii = *static_cast<AsciiManager *>(g_AsciiManagerHost);

    // Header line at the origin.
    {
        Float3 position;
        position.x = 0.0f;
        position.y = 0.0f;
        position.z = 0.0f;
        ascii.AddFormatTextSelected(&position, "Spt Test\n");
    }

    const i32 state = static_cast<i32>(LoadU32From(menu + 0x30U));
    if (state == 3) {
        // Position readout of the running script.
        ascii.color = 0xffa0a0a0U;
        Float3 position;
        position.x = 32.0f;  // TH10 0x42000000
        position.y = 16.0f;  // TH10 0x41800000
        position.z = 0.0f;
        ascii.AddFormatTextSelected(
            &position, "Pos %.3d %.3d",
            static_cast<i32>(LoadU32From(menu + 0x2c8U)),
            static_cast<i32>(LoadU32From(menu + 0x2c4U)));
        ascii.color = 0xffffffffU;
        return 1;
    }

    if (state == 2) {
        // Loading line with the selected file name.
        ascii.color = 0xffff4040U;
        Float3 position;
        position.x = 32.0f;
        position.y = 16.0f;
        position.z = 0.0f;
        const u32 index = LoadU32From(menu + 0x114U);
        void **const names =
            reinterpret_cast<void **>(LoadU32From(menu + 0x34U));
        ascii.AddFormatTextSelected(&position, "Loading %s",
                                    static_cast<char *>(names[index]));
        ascii.color = 0xffffffffU;
        return 1;
    }

    if (state == 1) {
        // File list line (42, 16).
        Float3 position;
        position.x = 42.0f;  // TH10 0x42280000
        position.y = 16.0f;
        position.z = 0.0f;
        const u32 count = LoadU32From(menu + 0x38U);
        if (count != 0U) {
            const u32 index = LoadU32From(menu + 0x114U);
            void **const names =
                reinterpret_cast<void **>(LoadU32From(menu + 0x34U));
            ascii.AddFormatTextSelected(&position, "File %s",
                                        static_cast<char *>(names[index]));
        } else {
            ascii.AddFormatTextSelected(&position, "File not found.");
        }

        // Script list line (42, 24): the viewer's entry table when loaded.
        position.y = 24.0f; // TH10 0x41d00000
        const u32 viewer = LoadU32From(menu + 0x2d8U);
        if (viewer != 0U) {
            const u32 index = LoadU32From(menu + 0x1ecU);
            const u32 entry_table =
                LoadU32From(reinterpret_cast<const void *>(viewer + 0x8cU));
            ascii.AddFormatTextSelected(
                &position, "Ecl %s",
                reinterpret_cast<const char *>(LoadU32From(
                    reinterpret_cast<const void *>(entry_table
                                                   + index * 8U))));
        } else {
            ascii.AddFormatTextSelected(&position, "Ecl not load");
        }

        // Quit line (42, 36).
        position.y = 36.0f; // TH10 0x42100000
        ascii.AddFormatTextSelected(&position, "Quit");

        // Cursor caret under the selected menu row.
        position.x = 32.0f;
        position.y = static_cast<float>(
                         static_cast<i32>(LoadU32From(menu + 0x3cU)))
                         * 10.0f // TH10 flt_00470c1c
                     + 16.0f;  // TH10 flt_00470b48
        ascii.AddFormatTextSelected(&position, ">");

        ascii.color = 0xffffffffU;
    }
    return 1;
}

// TH10 0x0042be10 / 0x0042be20 semantic body.
i32 RunScriptTestLoadContinuation(void *menu_object)
{
    // The worker entry ignores its argument and re-reads the global.
    u8 *const menu = static_cast<u8 *>(*reinterpret_cast<void *const *>(
        0x477844U));

    // sprintf(local, "%s", selected file name).
    char local_name[0x104];
    const u32 index = LoadU32From(menu + 0x114U);
    void **const names =
        reinterpret_cast<void **>(LoadU32From(menu + 0x34U));
    sprintf(local_name, "%s", static_cast<const char *>(names[index]));

    ReleaseContextListManager(*reinterpret_cast<void **>(menu + 0x2dcU));
    StoreU32To(menu + 0x2dcU, 0U); // native stores the clear twice (quirk)
    void *const previous_list =
        reinterpret_cast<void *>(LoadU32From(menu + 0x2e0U));
    if (previous_list != 0) {
        ReleaseResourceBuffer(previous_list);
        StoreU32To(menu + 0x2e0U, 0U);
    }

    // Reload the file through the shared whole-file loader.
    StoreU32To(menu + 0x2e0U,
               reinterpret_cast<u32>(
                   LoadMainChainFile(local_name, 0, 0)));

    // 0x1098-byte viewer object: vtable 0x46d0f0 and the native dead
    // stores at +0x1090/+0x1094 ahead of the full zeroing.
    void *viewer = AllocateMainChainObject(0x1098U);
    if (viewer != 0) {
        u8 *const bytes = static_cast<u8 *>(viewer);
        StoreU32To(bytes, 0x46d0f0U);
        StoreU32To(bytes + 0x1090U, 0U);
        StoreU32To(bytes + 0x1094U, 0U);
        for (u32 i = 0; i < 0x1098U; ++i) {
            bytes[i] = 0U; // erases the stores above (dead-store quirk)
        }
    }

    StoreU32To(menu + 0x2d8U, reinterpret_cast<u32>(viewer));
    // Native quirk: the viewer vtable call is issued unchecked, so an
    // allocation failure dereferences null exactly like the original.
    void **const viewer_vtable = *static_cast<void ***>(viewer);
    reinterpret_cast<void (*)(void *self, void *sections)>(
        viewer_vtable[0])(viewer,
                          reinterpret_cast<void *>(
                              LoadU32From(menu + 0x2e0U)));

    // Seed the script cursor from the viewer's script count; a non-positive
    // count seeds index - 1 (i.e. -1 for an empty viewer).
    const u32 script_count =
        LoadU32From(reinterpret_cast<const void *>(
            LoadU32From(menu + 0x2d8U) + 8U));
    StoreU32To(menu + 0x1f4U, script_count);
    i32 cursor = 0;
    if (static_cast<i32>(script_count) <= 0) {
        cursor = static_cast<i32>(script_count) - 1;
    }
    StoreU32To(menu + 0x1ecU, static_cast<u32>(cursor));

    StoreU32To(menu + 0x30U, 1U);
    return 0;
}

// TH10 0x0042bc30 semantic body (native stack argument, `ret 4`, SEH frame
// elided).
void DestroyScriptTestMenuStackAbi(void *menu_object)
{
    u8 *const bytes = static_cast<u8 *>(menu_object);

    ReleaseScriptTestMenuFileNamesEsiAbi(bytes);

    // Both scheduler records are removed inside the shared scheduler lock
    // with the activity-depth byte bracketed.
    ChainElem *const update =
        reinterpret_cast<ChainElem *>(LoadU32From(bytes + 0x8U));
    if (update != 0) {
        CallbackSchedulerApi::RemoveSynchronized(g_CallbackScheduler, update);
    }
    ChainElem *const draw =
        reinterpret_cast<ChainElem *>(LoadU32From(bytes + 0xcU));
    if (draw != 0) {
        CallbackSchedulerApi::RemoveSynchronized(g_CallbackScheduler, draw);
    }

    // Decoded section list (malloc domain).
    void *const section_list =
        reinterpret_cast<void *>(LoadU32From(bytes + 0x2e0U));
    if (section_list != 0) {
        ReleaseResourceBuffer(section_list);
        StoreU32To(bytes + 0x2e0U, 0U);
    }

    // Viewer object: restore the base vtable, free its +0x8c buffer and
    // operator-delete the object.
    u8 *const viewer =
        reinterpret_cast<u8 *>(LoadU32From(bytes + 0x2d8U));
    if (viewer != 0) {
        StoreU32To(viewer, 0x46d0f0U);
        void *const viewer_buffer =
            reinterpret_cast<void *>(LoadU32From(viewer + 0x8cU));
        if (viewer_buffer != 0) {
            ReleaseResourceBuffer(viewer_buffer);
            StoreU32To(viewer + 0x8cU, 0U);
        }
        FreeMainChainObject(viewer);
        StoreU32To(bytes + 0x2d8U, 0U);
    }

    // Running context-list manager: virtual release with flag 1.
    ReleaseContextListManager(*reinterpret_cast<void **>(bytes + 0x2dcU));

    // Large render-owner slots at +0x3ad088 then +0x3ad084.
    const u32 slot_offsets[2] = { 0x3ad088U, 0x3ad084U };
    for (u32 i = 0; i < 2U; ++i) {
        const u32 offset = slot_offsets[i];
        u8 *const slot_base = static_cast<u8 *>(g_MainChainRenderOwner);
        u8 *const slot =
            reinterpret_cast<u8 *>(LoadU32From(slot_base + offset));
        if (slot != 0) {
            ReleaseManagerWorkContents(
                reinterpret_cast<ManagerWorkPartial *>(slot));
            FreeMainChainObject(slot);
            StoreU32To(slot_base + offset, 0U);
        }
    }

    // Global clear, vtable restore at +0x10 and thread-control stop.
    u8 *const control = bytes + 0x10U;
    g_ScriptTestManagerSlot = 0;
    StoreU32To(control, 0x4703e4U);
    StopThreadControl(reinterpret_cast<ThreadControl *>(control));
}

// TH10 0x0042bf80 semantic body (`ret 4`).
void *DeleteScriptTestMenuEsiStackAbi(void *manager, i32 free_flag)
{
    DestroyScriptTestMenuStackAbi(manager);
    if ((free_flag & 1) != 0) {
        FreeMainChainObject(manager);
    }
    return manager;
}

// TH10 0x0042bfa0 semantic body (plain ret).
void ReleaseScriptTestMenuEsiAbi(void *manager)
{
    if (manager != 0) {
        DestroyScriptTestMenuStackAbi(manager);
        FreeMainChainObject(manager);
    }
}

} // namespace th10
