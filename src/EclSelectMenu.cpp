#include "EclSelectMenu.hpp"

#include "AsciiHudOverlayUpdate.hpp"
#include "ConditionalStateObject.hpp"
#include "EclScriptLibrary.hpp"
#include "GameManagerState.hpp"
#include "ManagerReleaseWrappers.hpp"
#include "PlayerOptionRecords.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "TitleCalcCluster.hpp"
#include "Th10Platform.hpp"

namespace th10 {

// RequestGameStateTransitionEaxStackAbi's controller (TH10 DAT_00491c28,
// the main-chain context).
extern u8 *g_MainChainContext; // TH10 DAT_00491c28

namespace {

// Input state bank at 0x474e30: current key words at +0/+4, gate byte at
// +4, gate flag dword at +6, saved words at +0x2c/+0x32.
const u32 k_input_bank = 0x474e30U;

u16 LoadU16At(u32 offset)
{
    const u8 *const base = reinterpret_cast<const u8 *>(k_input_bank);
    return static_cast<u16>(static_cast<u16>(base[offset])
                            | (static_cast<u16>(base[offset + 1]) << 8));
}

void StoreU16At(u32 offset, u16 value)
{
    u8 *const base = reinterpret_cast<u8 *>(k_input_bank);
    base[offset] = static_cast<u8>(value);
    base[offset + 1] = static_cast<u8>(value >> 8);
}

u32 LoadU32At(u32 offset)
{
    const u8 *const base = reinterpret_cast<const u8 *>(k_input_bank);
    return static_cast<u32>(base[offset])
         | (static_cast<u32>(base[offset + 1]) << 8)
         | (static_cast<u32>(base[offset + 2]) << 16)
         | (static_cast<u32>(base[offset + 3]) << 24);
}

u32 LoadU32Ptr(u32 address)
{
    const u8 *const bytes = reinterpret_cast<const u8 *>(address);
    return static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8)
         | (static_cast<u32>(bytes[2]) << 16)
         | (static_cast<u32>(bytes[3]) << 24);
}

u32 LoadU32From(const void *address)
{
    return LoadU32Ptr(reinterpret_cast<u32>(address));
}

void StoreU32To(void *address, u32 value)
{
    u8 *const bytes = static_cast<u8 *>(address);
    bytes[0] = static_cast<u8>(value);
    bytes[1] = static_cast<u8>(value >> 8);
    bytes[2] = static_cast<u8>(value >> 16);
    bytes[3] = static_cast<u8>(value >> 24);
}

// --- Boundary set -------------------------------------------------------
// Most native callees read their manager from ESI/EAX/EDI; the manager is
// an explicit argument here.

extern void CleanupEffectManagerEsiBoundary(void *manager); // TH10 0x00405ed0, native ESI = manager
extern void CleanupAsciiHudOverlayEsiBoundary(void *manager); // TH10 0x004148e0, native ESI = manager

// Continuation worker start with the native thread entry 0x0040a340; the
// shared RegisterTimelineContinuation (0x0044c1c0) hardcodes a different
// entry, so this caller keeps a dedicated boundary.
extern void StartEclMenuContinuationWorker(void *control, void *argument); // TH10 0x0044c1c0 with native EDI = 0x0040a340, EAX = control, stack = argument

// TH10 0x0040ac20. Native __thiscall ECX = the 0x474e30 input bank.
// Clears the +4 gate bytes, then for each of the 16 bits of the saved
// word at +0x2c either advances the matching u16 repeat counter at +0x38
// (wrapping by -8 with the +0x30 repeat flag at >= 26) or resets it, and
// finally computes the pressed (+0x32... no: +0x32 holds the flags word)
// pressed/released words from the current (+0x2c) and previous (+0x2e)
// saved words: pressed = cur & (cur ^ prev), released = (cur ^ prev) &
// ~cur. Returns the released mask.
u16 AdvanceInputBankTriggersEcxAbi(void *bank)
{
    u8 *const base = static_cast<u8 *>(bank);
    base[4U] = 0U;
    base[5U] = 0U;

    const u16 current = LoadU16At(0x2cU);
    u16 counters[16];
    for (u32 i = 0; i < 16U; ++i) {
        counters[i] = LoadU16At(0x38U + i * 2U);
    }
    u32 repeat_flags = LoadU16At(0x30U);
    u16 bits = current;
    for (u32 i = 0; i < 16U; ++i, bits >>= 1) {
        if ((bits & 1U) != 0U) {
            ++counters[i];
            if (counters[i] >= 0x1aU) {
                repeat_flags |= 1U;
                counters[i] = static_cast<u16>(counters[i] - 8U);
            }
        } else {
            counters[i] = 0U;
        }
    }
    for (u32 i = 0; i < 16U; ++i) {
        StoreU16At(0x38U + i * 2U, counters[i]);
    }
    StoreU16At(0x30U, repeat_flags);

    const u16 previous = LoadU16At(0x2eU);
    const u16 diff = static_cast<u16>(current ^ previous);
    StoreU16At(0x32U, static_cast<u16>(current & diff));
    const u16 released = static_cast<u16>(diff & static_cast<u16>(~current));
    StoreU16At(0x34U, released);
    return released;
}

// --- Win32 file enumeration (platform boundary) -------------------------
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

extern void *AllocateResourceBuffer(u32 bytes); // TH10 0x00452706 (CRT malloc)
extern void *g_AsciiHudConditionalState; // TH10 DAT_00477704
extern u32 g_ManagerSubGateFlags; // TH10 DAT_00474e36
extern u8 g_MenuInputFlagsByte; // TH10 DAT_00474e34

// Disables the scheduler-record enabled bit (word at record+4, bit 1) of
// the two records at manager+8 / manager+12.
void DisableManagerSchedulerRecords(void *manager)
{
    u8 *const base = static_cast<u8 *>(manager);
    const u32 mask = static_cast<u32>(-3); // ~2
    for (u32 slot = 8U; slot <= 0xcU; slot += 4U) {
        const u32 record = LoadU32From(base + slot);
        if (record != 0U) {
            const u32 flags =
                LoadU32From(reinterpret_cast<const void *>(record + 4));
            StoreU32To(reinterpret_cast<void *>(record + 4), flags & mask);
        }
    }
}

// TH10 0x0040a010. Native EAX = option-position manager: re-enables both
// scheduler records (+8/+0xc, word at record+4 |= 2) and rebuilds the
// player option records (0x00426f70).
i32 EnableOptionRecordsAndRebuildEaxAbi(void *manager)
{
    u8 *const base = static_cast<u8 *>(manager);
    for (u32 slot = 8U; slot <= 0xcU; slot += 4U) {
        const u32 record = LoadU32From(base + slot);
        const u32 flags = LoadU32From(reinterpret_cast<const void *>(record + 4));
        StoreU32To(reinterpret_cast<void *>(record + 4), flags | 2U);
    }
    RebuildPlayerOptionRecords(manager);
    return 0; // the native tail-calls 0x00426f70; its return value is unused
}

// TH10 0x00409e20. Native EAX = manager: re-enables the scheduler records
// at +8/+0xc when present (word at record+4 |= 2) and returns the +0xc
// record (or the manager when absent).
void *EnableManagerSchedulerRecordsEaxAbi(void *manager)
{
    u8 *const base = static_cast<u8 *>(manager);
    const u32 first = LoadU32From(base + 8U);
    if (first != 0U) {
        const u32 flags = LoadU32From(reinterpret_cast<const void *>(first + 4));
        StoreU32To(reinterpret_cast<void *>(first + 4), flags | 2U);
    }
    const u32 second = LoadU32From(base + 0xcU);
    if (second != 0U) {
        const u32 flags = LoadU32From(reinterpret_cast<const void *>(second + 4));
        StoreU32To(reinterpret_cast<void *>(second + 4), flags | 2U);
        return reinterpret_cast<void *>(second);
    }
    return manager;
}

// Cursor record seed used by state 0: the native three-way compare of the
// seed value against the count (equal keeps the count, greater yields
// zero, less yields max-1).
u32 SeedCursor(u32 max_value, u32 count)
{
    if (max_value == count) {
        return count;
    }
    return (max_value > count) ? 0U : (max_value - 1U);
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

// State 1, second cursor (+0x114): up/down on 0x80/0x40, then the 0x1001
// gate runs the light teardown sweep and starts the continuation worker
// (sub-state 2).
void RunFirstCursorZeroPath(u8 *menu)
{
    u32 value = LoadU32From(menu + 0x114U);
    StoreU32To(menu + 0x118U, value);
    const u8 low_byte = static_cast<u8>(g_ManagerSubGateFlags & 0xffU);
    ShiftOnByteMasks(menu + 0x114U, low_byte, 0x80U);
    ShiftOnByteMasksDown(menu + 0x114U, low_byte, 0x40U);
    if ((g_ManagerSubGateFlags & 0x1001U) == 0U) {
        return;
    }
    ReleaseAsciiHudOwnerEsiAbi(*reinterpret_cast<void **>(0x47770cU));
    ReleasePlayerStateBlockEsiAbi(*reinterpret_cast<void **>(0x477834U));
    ReleaseEffectManagerRootEsiAbi(*reinterpret_cast<void **>(0x4776f0U));
    ReleaseGameContextEsiAbi(*reinterpret_cast<void **>(0x4776ecU));
    ReleaseBulletManagerEsiAbi(*reinterpret_cast<void **>(0x477818U));
    ReleaseMainChainObject840EsiAbi(*reinterpret_cast<void **>(0x477840U));
    ReleaseAsciiHudConditionalStateEsiAbi(*reinterpret_cast<void **>(0x477704U));
    const u32 word = LoadU32From(menu + 0x684U) & static_cast<u32>(-3);
    StoreU32To(menu + 0x684U, word);
    StartEclMenuContinuationWorker(menu + 0x10U, menu);
    StoreU32To(menu + 0x30U, 2U);
}

// State 1, spell select cursor (+0x1ec): word polls 0x80/0x40, then the
// 0x10010000 gate tears the managers down and spawns the ECL script object
// (sub-state 4).
void RunSpellSelectPath(u8 *menu)
{
    u32 select = LoadU32From(menu + 0x1ecU);
    StoreU32To(menu + 0x1f0U, select);
    if (PollMenuInputState(0x80U)) {
        ShiftManagerSelector(menu + 0x1ecU, 1);
    }
    if (PollMenuInputState(0x40U)) {
        ShiftManagerSelector(menu + 0x1ecU, -1);
    }
    if ((g_ManagerSubGateFlags & 0x10010000U) == 0U) {
        return;
    }
    EnableOptionRecordsAndRebuildEaxAbi(*reinterpret_cast<void **>(0x477834U));
    ResetOptionPositionRecordsEsiAbi(*reinterpret_cast<void **>(0x477834U));
    void *const effect_root = *reinterpret_cast<void **>(0x4776f0U);
    EnableManagerSchedulerRecordsEaxAbi(effect_root);
    CleanupEffectManagerEsiBoundary(effect_root);
    void *const hud_state = *reinterpret_cast<void **>(0x477704U);
    EnableAsciiHudConditionalRecordsEaxAbi(hud_state);
    ReleaseAsciiHudConditionalState(hud_state);
    ResetAsciiHudOverlayEdiAbi(*reinterpret_cast<void **>(0x47770cU));
    void *const bullet_slot = *reinterpret_cast<void **>(0x477818U);
    EnableManagerSchedulerRecordsEaxAbi(bullet_slot);
    for (u32 i = 0; i < 0x21cea0U; ++i) {
        static_cast<u8 *>(bullet_slot)[0x14U + i] = 0U;
    }

    u8 descriptor[0x40];
    for (u32 i = 0; i < sizeof(descriptor); ++i) {
        descriptor[i] = 0U;
    }
    select = LoadU32From(menu + 0x1ecU);
    // The +0x54 name registry and its +0x8c name table are modeled
    // ConditionalState / ConditionalNameRegistry fields; the entry index
    // (the menu cursor at +0x1ec) is unbounded natively, so the 8-byte
    // entry read itself stays RAW (no bounds check in the native either).
    ConditionalState &hud_cond =
        *reinterpret_cast<ConditionalState *>(g_AsciiHudConditionalState);
    ConditionalNameRegistry *const registry =
        static_cast<ConditionalNameRegistry *>(hud_cond.name_registry_0054);
    const void *const name_table = registry->name_table_008c;
    const u32 script_id = LoadU32From(
        static_cast<const u8 *>(name_table) + select * 8U);
    StoreU32To(descriptor + 0x14U, 10000U);
    CreateEclScriptObjectEaxStackAbi(reinterpret_cast<const u32 *>(descriptor),
                                     hud_state, static_cast<i32>(script_id));
    StoreU32To(menu + 0x30U, 4U);
}

} // namespace

i32 UpdateEclSelectMenuStackAbi(void *menu_object)
{
    u8 *const menu = static_cast<u8 *>(menu_object);
    const u32 state = LoadU32From(menu + 0x30U);

    if (state == 0U) {
        ReleaseEclSelectMenuNamesEsiAbi(menu);
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
            StoreU32To(menu + 0x34U, reinterpret_cast<u32>(names));
            void *scan = FindFirstFileA("../../data/*.ecl", &data);
            for (u32 index = 0U; index < count; ++index) {
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
        StoreU32To(menu + 0x44U, 3U);       // a1[17]
        StoreU32To(menu + 0x10cU, 1U);      // a1[67]
        StoreU32To(menu + 0x3cU, SeedCursor(3U, count));  // a1[15]
        StoreU32To(menu + 0x11cU, count);   // a1[71]
        StoreU32To(menu + 0x1e4U, 1U);      // a1[121]
        StoreU32To(menu + 0x114U, SeedCursor(count, count)); // a1[69]
        StoreU32To(menu + 0x1f4U, 1U);      // a1[125]
        StoreU32To(menu + 0x2bcU, 1U);      // a1[175]
        StoreU32To(menu + 0x1ecU, SeedCursor(1U, count)); // a1[123]
        StoreU32To(menu + 0x30U, 1U);       // a1[12]
        StoreU32To(menu + 0x674U, 0U);      // a1[413]
        StoreU32To(menu + 0x678U, 0x42000000U); // a1[414] (32.0f)
        StoreU32To(menu + 0x67cU, 0U);      // a1[415]
        StoreU32To(menu + 0x680U, 1000U);   // a1[416]
        return 1;
    }

    if (state == 1U) {
        // First cursor (+0x3c): byte-mask shifts on 0x20/0x10, dispatch on
        // the shifted value.
        u32 value = LoadU32From(menu + 0x3cU);
        StoreU32To(menu + 0x40U, value);
        const u8 low_byte = static_cast<u8>(g_ManagerSubGateFlags & 0xffU);
        ShiftOnByteMasks(menu + 0x3cU, low_byte, 0x20U);
        ShiftOnByteMasksDown(menu + 0x3cU, low_byte, 0x10U);
        value = LoadU32From(menu + 0x3cU);
        if (value == 0U) {
            RunFirstCursorZeroPath(menu);
        } else if (value == 1U) {
            RunSpellSelectPath(menu);
        } else if (value == 2U) {
            if ((g_ManagerSubGateFlags & 0x1001U) != 0U) {
                RequestGameStateTransitionEaxStackAbi(g_MainChainContext, 3);
            }
        }
        return 1;
    }

    if (state == 4U) {
        // Snapshot the current input words into the saved pair.
        const u16 current = LoadU16At(0U);
        const u16 saved = LoadU16At(0x2cU);
        StoreU16At(0x2cU, current);
        StoreU16At(0x2eU, saved);
        AdvanceInputBankTriggersEcxAbi(reinterpret_cast<void *>(k_input_bank));
        const u16 flags16 = static_cast<u16>(LoadU32At(6U) & 0xffffU);
        StoreU16At(0x32U, flags16);
        if ((flags16 & 8U) == 0U) {
            return 1;
        }
        DisableManagerSchedulerRecords(*reinterpret_cast<void **>(0x477834U));
        ResetOptionPositionRecordsEsiAbi(*reinterpret_cast<void **>(0x477834U));
        void *const effect_root = *reinterpret_cast<void **>(0x4776f0U);
        DisableManagerSchedulerRecords(effect_root);
        CleanupEffectManagerEsiBoundary(effect_root);
        void *const hud_state = *reinterpret_cast<void **>(0x477704U);
        DisableManagerSchedulerRecords(hud_state);
        ReleaseAsciiHudConditionalState(hud_state);
        void *const bullet_slot = *reinterpret_cast<void **>(0x477818U);
        DisableManagerSchedulerRecords(bullet_slot);
        for (u32 i = 0; i < 0x21cea0U; ++i) {
            static_cast<u8 *>(bullet_slot)[0x14U + i] = 0U;
        }
        StoreU32To(menu + 0x30U, 1U);
        return 1;
    }

    return 1;
}

} // namespace th10
