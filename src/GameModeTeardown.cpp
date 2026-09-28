#include "GameModeTeardown.hpp"

#include "CallbackScheduler.hpp"
#include "EntityHelpers.hpp"
#include "GlobalLifecycleManager.hpp"
#include "ManagerReleaseWrappers.hpp"
#include "ScoreSave.hpp"
#include "ThreadControl.hpp"
#include "Th10Platform.hpp"
#include "TitleCalcCluster.hpp"

namespace th10 {

namespace {

extern void FreeMainChainObject(void *object); // TH10 0x004524a1 (j__free)
extern void ReleaseResourceBuffer(void *pointer); // TH10 0x00452422 (free)

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern void *g_GameModeObject; // TH10 DAT_00477838
extern void *g_GameStateManager; // TH10 DAT_00477830
extern void *g_SpellBulletBase; // TH10 DAT_004776f4
extern void *g_AsciiManagerHost; // TH10 DAT_004776e0
extern GlobalLifecycleManager *g_GlobalLifecycleManager; // TH10 DAT_00477820
extern void *g_TitleScoreSaveRecord; // TH10 DAT_0047783c
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4

u32 LoadU32From(const void *address)
{
    const u8 *const bytes = static_cast<const u8 *>(address);
    return static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8)
         | (static_cast<u32>(bytes[2]) << 16)
         | (static_cast<u32>(bytes[3]) << 24);
}

void StoreU32To(void *address, u32 value)
{
    u8 *const bytes = static_cast<u8 *>(address);
    bytes[0] = static_cast<u8>(value);
    bytes[1] = static_cast<u8>(value >> 8);
    bytes[2] = static_cast<u8>(value >> 16);
    bytes[3] = static_cast<u8>(value >> 24);
}

// The native eh vector destructor iterator (0x004525ff) receives the scalar
// destructor as a __thiscall callee (ECX = record). The reconstruction
// models it as a plain pointer-to-function taking the record.
void RunRecordVectorDestructorIterator(void *array, u32 stride, u32 count)
{
    u8 *record = static_cast<u8 *>(array);
    for (u32 i = 0; i < count; ++i) {
        DestroyTitleScreenVmRecordInPlace(record);
        record += stride;
    }
}

// Scheduler-record removal exactly as 0x00449f60 is bracketed by every
// destructor in this cluster: global scheduler critical section plus the
// DAT_0049231c activity-depth byte.
void RemoveSchedulerRecordSynchronized(void *base, u32 offset)
{
    ChainElem *const record = *reinterpret_cast<ChainElem **>(
        static_cast<u8 *>(base) + offset);
    if (record == 0) {
        return;
    }
    CallbackSchedulerApi::RemoveSynchronized(g_CallbackScheduler, record);
}

// Large render-owner slot word: release the slot contents through
// 0x00447810, free the slot block itself with the shared delete, and clear
// the publishing word.
void ReleaseRenderOwnerSlotWord(u32 *slot_word)
{
    const u32 slot = *slot_word;
    if (slot == 0U) {
        return;
    }
    ReleaseLargeRenderOwnerSlotEdiAbi(
        reinterpret_cast<void *>(slot));
    FreeMainChainObject(reinterpret_cast<void *>(slot));
    *slot_word = 0U;
}

} // namespace

// Scalar destructor 0x0042ac60 passed to the eh vector destructor iterator
// by 0x004294a0 (__thiscall ECX = the 0x24-byte record): unlinks the record
// from the doubly-linked chain through its +0x1c/+0x20 links and clears
// them. The chain neighbours are not fixed up beyond the direct links.
// Exported (out of the anonymous namespace) so the replay demo record pool
// in ReplayContextHelpers.cpp can bind the same 0x0042ac60 destructor.
void UnlinkGameModeChainRecordInPlace(void *record)
{
    u32 *const words = static_cast<u32 *>(record);
    const u32 previous = words[7];
    if (previous != 0U) {
        StoreU32To(reinterpret_cast<void *>(previous + 8U), words[8]);
    }
    const u32 next = words[8];
    if (next != 0U) {
        StoreU32To(reinterpret_cast<void *>(next + 4U), words[7]);
    }
    words[7] = 0U;
    words[8] = 0U;
}

// TH10 0x004294a0. In-place destructor of the DAT_00477838 game-mode object.
void DestroyGameModeObjectInPlace(void *object)
{
    u8 *const base = static_cast<u8 *>(object);

    // +0x14 scratch buffer: released through the shared delete but the slot
    // is intentionally NOT cleared (native quirk).
    FreeMainChainObject(*reinterpret_cast<void **>(base + 0x14U));

    for (i32 i = 0; i < 8; ++i) {
        FreeGameModeChainEntriesEaxEcxAbi(i, base);
    }

    // +0x18 buffer: freed and cleared.
    FreeMainChainObject(*reinterpret_cast<void **>(base + 0x18U));
    StoreU32To(base + 0x18U, 0U);

    // Eight entry-array pointers at +0x1c: each freed through the shared
    // delete (the native free call is unconditional; free(0) is a no-op)
    // and cleared.
    u32 *entry = reinterpret_cast<u32 *>(base + 0x1cU);
    for (i32 i = 0; i < 8; ++i) {
        FreeMainChainObject(reinterpret_cast<void *>(*entry));
        *entry++ = 0U;
    }

    RemoveSchedulerRecordSynchronized(base, 0x08U);
    RemoveSchedulerRecordSynchronized(base, 0x1ccU);
    RemoveSchedulerRecordSynchronized(base, 0x0cU);

    if (base == g_GameModeObject) {
        g_GameModeObject = 0; // DAT_00477838
    }

    // eh vector destructor iterator over eight 0x24-byte records at +0xa0;
    // the scalar destructor unlinks each record's chain entry.
    u8 *record = base + 0xa0U;
    for (u32 i = 0; i < 8U; ++i) {
        UnlinkGameModeChainRecordInPlace(record);
        record += 0x24U;
    }
}

void DestroyUnknownMainChainObjectInPlace(void *object)
{
    DestroyGameModeObjectInPlace(object);
}

void DestroyOpaqueMainChainManagerInPlace(void *object)
{
    DestroyGameModeObjectInPlace(object);
}

void DestroyDemoParseObject(void *parsed)
{
    DestroyGameModeObjectInPlace(parsed);
}

// TH10 0x00422220. In-place destructor of the DAT_00477830 game-state
// manager. The native returns zero in EAX, but no caller consumes the
// value (it is used as a plain destructor function pointer), so the body
// is void like the other destructor bodies in this cluster.
void DestroyGameStateObjectInPlace(void *object)
{
    u32 *const words = static_cast<u32 *>(object);

    RemoveSchedulerRecordSynchronized(words, 0x08U);
    RemoveSchedulerRecordSynchronized(words, 0x0cU);

    // 25 game-mode sub-objects at +0x1ec: each non-null one is torn down
    // with 0x004294a0 and freed with the shared delete. The native leaves
    // the slot words untouched here (they are re-established on the next
    // game-mode construction).
    u32 *slot = reinterpret_cast<u32 *>(words) + 0x1ecU / 4U;
    for (u32 i = 0; i < 25U; ++i) {
        const u32 sub_object = *slot;
        if (sub_object != 0U) {
            DestroyGameModeObjectInPlace(reinterpret_cast<void *>(sub_object));
            FreeMainChainObject(reinterpret_cast<void *>(sub_object));
        }
        ++slot;
    }

    // +0x1dc published entity handle: the native inlines 0x004492a0 here —
    // the id is resolved over the list-A/list-B roots at
    // render-owner+0x72dad4 / +0x72dadc, the +0x35c release flag is set,
    // and (while entity+0x18 is clear) the same flag is applied to every
    // child in the +0x14 list — which is exactly the reconstructed
    // ReleaseEntityById body.
    const u32 entity_id = words[0x1dcU / 4U];
    if (entity_id != 0U) {
        ReleaseEntityById(g_MainChainRenderOwner, entity_id);
    }
    words[0x1dcU / 4U] = 0U;

    g_GameStateManager = 0; // DAT_00477830
}

// TH10 0x00408af0. In-place destructor of the DAT_004776f4 spell/bullet
// base. The native is a real C++ destructor with an EH scope table; the
// unwinding path only releases the outer allocation, which the callers
// already model.
void DestroySpellBulletBaseInPlace(void *object)
{
    u8 *const base = static_cast<u8 *>(object);

    // The three published entity handles are soft-released and cleared.
    // (The native caches LeaveCriticalSection in EBP after the first
    // removal and calls through it for the remaining two; that register
    // reuse is semantically identical to a direct call.)
    for (u32 offset = 0x768U; offset <= 0x770U; offset += 4U) {
        u32 *const handle = reinterpret_cast<u32 *>(base + offset);
        if (*handle != 0U) {
            ReleaseEntityById(g_MainChainRenderOwner, *handle);
        }
        *handle = 0U;
    }

    RemoveSchedulerRecordSynchronized(base, 0x08U);
    RemoveSchedulerRecordSynchronized(base, 0x0cU);
    RemoveSchedulerRecordSynchronized(base, 0x37acU);

    g_SpellBulletBase = 0; // DAT_004776f4

    // Three eh vector destructor iterators over 0x3ac-byte VM records,
    // destroyed in native order by the shared scalar record dtor 0x00401ff0.
    RunRecordVectorDestructorIterator(base + 0x24d8U, 0x3acU, 5U);
    RunRecordVectorDestructorIterator(base + 0x77cU, 0x3acU, 8U);
    RunRecordVectorDestructorIterator(base + 0x10U, 0x3acU, 2U);
}

// TH10 0x0041f930. Releases the two coordinated render-owner slots.
void ReleaseGlobalLifecycleCoordinatedSlots()
{
    u8 *const owner = static_cast<u8 *>(g_MainChainRenderOwner);
    ReleaseRenderOwnerSlotWord(reinterpret_cast<u32 *>(owner + 0x3ad084U));
    ReleaseRenderOwnerSlotWord(reinterpret_cast<u32 *>(owner + 0x3ad088U));
}

// TH10 0x00401260. Base-object in-place destructor of the DAT_004776e0
// ASCII manager host (vtable planted first, as in the native destructor).
void DestroyAsciiManagerHostInPlace(void *host)
{
    u8 *const base = static_cast<u8 *>(host);

    StoreU32To(base, 0x0046cb14U); // base vtable off_46cb14

    RemoveSchedulerRecordSynchronized(base, 0x0cU);
    RemoveSchedulerRecordSynchronized(base, 0x10U);
    RemoveSchedulerRecordSynchronized(base, 0x89a8U);

    u8 *const owner = static_cast<u8 *>(g_MainChainRenderOwner);
    ReleaseRenderOwnerSlotWord(reinterpret_cast<u32 *>(owner + 0x3ad074U));
    ReleaseRenderOwnerSlotWord(reinterpret_cast<u32 *>(owner + 0x3ad06cU));
    ReleaseRenderOwnerSlotWord(reinterpret_cast<u32 *>(owner + 0x3ad078U));

    // +0x718 buffer: the native reads it, clears the published host global
    // DAT_004776e0, frees the buffer through the CRT free, and clears the
    // slot. The buffer word is cleared on all paths.
    const u32 buffer = LoadU32From(base + 0x718U);
    g_AsciiManagerHost = 0;
    if (buffer != 0U) {
        ReleaseResourceBuffer(reinterpret_cast<void *>(buffer));
    }
    StoreU32To(base + 0x718U, 0U);

    const u32 scratch = LoadU32From(base + 0x36cU);
    if (scratch != 0U) {
        ReleaseResourceBuffer(reinterpret_cast<void *>(scratch));
    }
    StoreU32To(base + 0x36cU, 0U);
}

// TH10 0x0041fb50. In-place destructor of the 0x3f0-byte global lifecycle
// manager published at DAT_00477820.
void DestroyGlobalLifecycleManagerInPlace(void *manager)
{
    GlobalLifecycleManager *const lifecycle =
        static_cast<GlobalLifecycleManager *>(manager);

    // The native passes manager+0x10 (the embedded ThreadControl) in ESI to
    // 0x0044c150; the reconstructed ThreadControl body takes the pointer.
    StopThreadControl(reinterpret_cast<ThreadControl *>(
        &lifecycle->thread_control_0010));

    RemoveSchedulerRecordSynchronized(lifecycle, 0x08U);
    RemoveSchedulerRecordSynchronized(lifecycle, 0x0cU);

    // Expanded ReleaseGlobalLifecycleCoordinatedGlobals boundary: the
    // coordinated slot pair, the front slot, the ASCII manager host
    // (torn down and freed), the second coordinated slot, and the
    // score-save record flush/release.
    ReleaseGlobalLifecycleCoordinatedSlots();

    ReleaseRenderOwnerSlotWord(reinterpret_cast<u32 *>(
        static_cast<u8 *>(g_MainChainRenderOwner) + 0x3ad070U));

    g_GlobalLifecycleManager = 0; // DAT_00477820
    if (g_AsciiManagerHost != 0) {
        DestroyAsciiManagerHostInPlace(g_AsciiManagerHost);
        FreeMainChainObject(g_AsciiManagerHost);
    }

    ReleaseRenderOwnerSlotWord(reinterpret_cast<u32 *>(
        static_cast<u8 *>(g_MainChainRenderOwner) + 0x3ad06cU));

    // Flush scoreth10.dat state and release the score-save record: the
    // native hands DAT_0047783c to 0x0042b1e0 in EBX, then frees the two
    // record buffers through the CRT free and the record itself through
    // the shared delete, and clears the global unconditionally.
    SaveScoreRecordFileEbx(g_TitleScoreSaveRecord);
    void **const record =
        static_cast<void **>(g_TitleScoreSaveRecord);
    if (record != 0) {
        if (record[0] != 0) {
            ReleaseResourceBuffer(record[0]);
            record[0] = 0;
        }
        if (record[1] != 0) {
            ReleaseResourceBuffer(record[1]);
            record[1] = 0;
        }
        FreeMainChainObject(record);
    }

    // +0x388 owned buffer: CRT-freed and cleared after the global clear.
    const u32 owned = LoadU32From(static_cast<const u8 *>(manager) + 0x388U);
    g_TitleScoreSaveRecord = 0; // DAT_0047783c
    if (owned != 0U) {
        ReleaseResourceBuffer(reinterpret_cast<void *>(owned));
    }
    lifecycle->owned_buffer = 0;

    lifecycle->thread_control_0010 = reinterpret_cast<void *>(0x004703e4U);
    StopThreadControl(reinterpret_cast<ThreadControl *>(
        &lifecycle->thread_control_0010));
}

} // namespace th10
