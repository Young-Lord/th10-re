#include "ManagerReleaseWrappers.hpp"

#include "CallbackScheduler.hpp"
#include "EntityHelpers.hpp"
#include "Th10Platform.hpp"
#include "TitleCalcCluster.hpp"

namespace th10 {

// TH10 0x00424ed0. In-place destructor of the DAT_00477834 player-state
// block: removes the two scheduler records at +8/+0xc under the scheduler
// lock, clears the block global, then either (mode-flag bit 0 set) releases
// the resource-matched entities for the +0x10 word and republishes the
// +0x45c pointer into the player shot entry cache, or releases the
// render-owner large slot at +0x3AD08C (block freed with the shared delete),
// frees +0x45c through the CRT free and clears the cache. The +0x36c buffer
// is CRT-freed and cleared on both paths.
void DestroyPlayerStateBlockInPlace(void *object);

// TH10 0x00405f70. In-place destructor of the DAT_004776f0 effect manager
// root: removes the two scheduler records at +8/+0xc under the scheduler
// lock, releases the resource-matched entities for the +0x3E0B50 word,
// clears the root global, and runs the eh vector destructor iterator over
// 2001 0x7F0-byte records at +0x60 (scalar dtor frees the record's +0x360
// buffer through the CRT free).
void DestroyEffectManagerRootInPlace(void *root);

// TH10 0x00405620. In-place destructor of the DAT_004776ec game context:
// removes the two scheduler records at +8/+0xc under the scheduler lock and
// clears the context global.
void DestroyGameContextInPlace(void *context);

// TH10 0x0041adf0. In-place destructor of the DAT_00477818 bullet manager:
// removes the two scheduler records at +8/+0xc under the scheduler lock,
// clears the manager global, and runs the eh vector destructor iterator
// over 2198 0x3F0-byte records at +0x14 (scalar dtor frees the record's
// +0x358 buffer through the CRT free).
void DestroyBulletManagerInPlace(void *manager);

// TH10 0x0042b570. In-place destructor of the DAT_00477840 0x840-byte
// main-chain object: removes the two scheduler records at +8/+0xc under the
// scheduler lock, clears the object global, and frees the +0x370 buffer
// through the CRT free.
void DestroyMainChainObject840InPlace(void *object);

// TH10 0x0040d530. Native EAX = the DAT_00477704 ASCII HUD conditional
// state. Runs the 0x409f90 sub-block release, removes the two scheduler
// records at +8/+0xc under the scheduler lock, frees the 0x0c..0x88
// non-null pointers of the +0x54 sub-object through the CRT free, tears the
// sub-object down (vtable 0x46D0F0, +0x8C buffer freed) and releases its
// allocation, then either clears the state global or first releases the
// four render-owner large slots at +0x3AD090..+0x3AD09C (indices 9..12 of
// the signed slot guard). The native body leaves zero in EAX on both
// return paths; the value is never consumed by any caller.
void DestroyAsciiHudConditionalStateEax(void *state);

namespace {

extern void FreeMainChainObject(void *object); // TH10 0x004524a1
extern void ReleaseResourceBuffer(void *pointer); // TH10 0x00452422

extern void DestroyAsciiHudOwnerInPlace(void *object); // TH10 0x004145f0 (implemented in TitleGameManagerLifecycle.cpp)

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern Win32CriticalSection g_CallbackSchedulerLock; // TH10 DAT_00492274
extern u8 g_CallbackSchedulerActivityDepth; // TH10 DAT_0049231c
extern u32 g_GlobalModeFlags; // TH10 DAT_00474ca0
extern void *g_OptionPositionBase; // TH10 DAT_00477834 (this player-state block)
extern void *g_PlayerShotEntryCache; // TH10 DAT_00491bf0
extern void *g_EffectManagerRoot; // TH10 DAT_004776f0
extern void *g_GameContext; // TH10 DAT_004776ec
extern void *g_BulletManagerSlot; // TH10 DAT_00477818
extern void *g_MainChainObject840; // TH10 DAT_00477840
extern void *g_AsciiHudConditionalState; // TH10 DAT_00477704

extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);

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

} // namespace

void ReleaseAsciiHudOwnerEsiAbi(void *owner)
{
    if (owner != 0) {
        DestroyAsciiHudOwnerInPlace(owner);
        FreeMainChainObject(owner);
    }
}

void ReleasePlayerStateBlockEsiAbi(void *block)
{
    if (block != 0) {
        DestroyPlayerStateBlockInPlace(block);
        FreeMainChainObject(block);
    }
}

void ReleaseEffectManagerRootEsiAbi(void *root)
{
    if (root != 0) {
        DestroyEffectManagerRootInPlace(root);
        FreeMainChainObject(root);
    }
}

void ReleaseGameContextEsiAbi(void *context)
{
    if (context != 0) {
        DestroyGameContextInPlace(context);
        FreeMainChainObject(context);
    }
}

void ReleaseBulletManagerEsiAbi(void *manager)
{
    if (manager != 0) {
        DestroyBulletManagerInPlace(manager);
        FreeMainChainObject(manager);
    }
}

void ReleaseMainChainObject840EsiAbi(void *object)
{
    if (object != 0) {
        DestroyMainChainObject840InPlace(object);
        FreeMainChainObject(object);
    }
}

void ReleaseAsciiHudConditionalStateEsiAbi(void *state)
{
    if (state != 0) {
        DestroyAsciiHudConditionalStateEax(state);
        FreeMainChainObject(state);
    }
}

void *EnableAsciiHudConditionalRecordsEaxAbi(void *manager)
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

void ReleaseEclSelectMenuNamesEsiAbi(void *menu)
{
    u8 *const base = static_cast<u8 *>(menu);
    const u32 array = LoadU32From(base + 0x34U);
    if (array == 0U) {
        return;
    }
    const u32 count = LoadU32From(base + 0x38U);
    for (u32 i = 0; i < count; ++i) {
        const u32 name = LoadU32From(reinterpret_cast<const void *>(array + 4U * i));
        if (name != 0U) {
            ReleaseResourceBuffer(reinterpret_cast<void *>(name));
            StoreU32To(reinterpret_cast<void *>(array + 4U * i), 0U);
        }
    }
    if (array != 0U) {
        ReleaseResourceBuffer(reinterpret_cast<void *>(array));
        StoreU32To(base + 0x34U, 0U);
    }
    StoreU32To(base + 0x34U, 0U); // native clears the slot twice
}

void DestroyTitleScreenVmRecordInPlace(void *record)
{
    u8 *const base = static_cast<u8 *>(record);
    const u32 buffer = LoadU32From(base + 0x358U);
    if (buffer != 0U) {
        ReleaseResourceBuffer(reinterpret_cast<void *>(buffer));
    }
    StoreU32To(base + 0x358U, 0U);
}

void ReleaseLargeRenderOwnerSlotEdiAbi(void *slot_block)
{
    extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
    u8 *const block = static_cast<u8 *>(slot_block);
    if (LoadU32From(block + 0x108U) == 0U) {
        return;
    }
    ReleaseEntitiesUsingResourceEaxEdxAbi(g_MainChainRenderOwner,
                                          reinterpret_cast<u32>(slot_block));

    typedef void (TH10_STDCALL *ReleaseFn)(void *object);
    const u32 entries = LoadU32From(block + 0x120U);
    const i32 count = static_cast<i32>(LoadU32From(block + 0x10cU));
    for (i32 i = 0; i < count; ++i) {
        const u32 entry = entries + static_cast<u32>(i) * 16U;
        const u32 object = LoadU32From(reinterpret_cast<const void *>(entry));
        if (object != 0U) {
            void **const vtbl =
                *static_cast<void ***>(reinterpret_cast<void *>(object));
            reinterpret_cast<ReleaseFn>(vtbl[2])(
                reinterpret_cast<void *>(object)); // vtbl +0x08 Release
            StoreU32To(reinterpret_cast<void *>(entry), 0U);
        }
        const u32 extra = LoadU32From(reinterpret_cast<const void *>(entry + 4U));
        if (extra != 0U) {
            ReleaseResourceBuffer(reinterpret_cast<void *>(extra));
            StoreU32To(reinterpret_cast<void *>(entry + 4U), 0U);
        }
    }

    const u32 buffer_slots[5] = {0x120U, 0x118U, 0x11cU, 0x12cU, 0x108U};
    for (u32 i = 0; i < 5U; ++i) {
        const u32 buffer = LoadU32From(block + buffer_slots[i]);
        if (buffer != 0U) {
            ReleaseResourceBuffer(reinterpret_cast<void *>(buffer));
            StoreU32To(block + buffer_slots[i], 0U);
        }
    }
}

namespace {

// Removes the scheduler chain record stored at base+offset under the
// scheduler lock, matching the shared destructor idiom (0x00449f60 bracketed
// by DAT_00492274 and the DAT_0049231c activity-depth byte). The record
// slot itself is not cleared, matching the native destructors.
void RemoveManagerSchedulerRecord(void *base, u32 offset)
{
    ChainElem *const record = *reinterpret_cast<ChainElem **>(
        static_cast<u8 *>(base) + offset);
    if (record == 0) {
        return;
    }
    EnterCriticalSection(&g_CallbackSchedulerLock);
    ++g_CallbackSchedulerActivityDepth;
    CallbackSchedulerApi::Remove(g_CallbackScheduler, record);
    LeaveCriticalSection(&g_CallbackSchedulerLock);
    --g_CallbackSchedulerActivityDepth;
}

// Scalar destructor of the 0x7F0-byte effect-root record array (TH10
// 0x00405de0): frees the record's +0x360 buffer through the CRT free and
// clears the slot.
void DestroyEffectRootRecordInPlace(void *record)
{
    u32 *const words = static_cast<u32 *>(record);
    if (words[0x360 / 4] != 0U) {
        ReleaseResourceBuffer(reinterpret_cast<void *>(words[0x360 / 4]));
    }
    words[0x360 / 4] = 0U;
}

// Scalar destructor of the 0x3F0-byte bullet-manager record array (TH10
// 0x0041ad60): frees the record's +0x358 buffer through the CRT free and
// clears the slot.
void DestroyBulletManagerRecordInPlace(void *record)
{
    u32 *const words = static_cast<u32 *>(record);
    if (words[0x358 / 4] != 0U) {
        ReleaseResourceBuffer(reinterpret_cast<void *>(words[0x358 / 4]));
    }
    words[0x358 / 4] = 0U;
}

// Inlined body of the eh vector destructor iterator (0x004525ff): runs the
// scalar destructor over count stride-sized records starting at array.
void RunVectorDestructorIterator(void *array, u32 stride, u32 count,
                                 void (*destroy)(void *))
{
    u8 *record = static_cast<u8 *>(array);
    for (u32 i = 0; i < count; ++i) {
        destroy(record);
        record += stride;
    }
}

} // namespace

// TH10 0x00405620. In-place destructor of the DAT_004776ec game context.
void DestroyGameContextInPlace(void *context)
{
    RemoveManagerSchedulerRecord(context, 0x08U);
    RemoveManagerSchedulerRecord(context, 0x0cU);
    g_GameContext = 0; // DAT_004776ec
}

// TH10 0x00405f70. In-place destructor of the DAT_004776f0 effect manager
// root. The +0x3E0B50 word is the root's shared resource pointer; the
// native clears the global between pushing the iterator arguments and the
// iterator call.
void DestroyEffectManagerRootInPlace(void *root)
{
    RemoveManagerSchedulerRecord(root, 0x08U);
    RemoveManagerSchedulerRecord(root, 0x0cU);
    ReleaseEntitiesUsingResourceEaxEdxAbi(
        g_MainChainRenderOwner,
        LoadU32From(static_cast<const u8 *>(root) + 0x3E0B50U));
    g_EffectManagerRoot = 0; // DAT_004776f0
    RunVectorDestructorIterator(static_cast<u8 *>(root) + 0x60U, 0x7F0U,
                                0x7D1U, DestroyEffectRootRecordInPlace);
}

// TH10 0x0041adf0. In-place destructor of the DAT_00477818 bullet manager.
void DestroyBulletManagerInPlace(void *manager)
{
    RemoveManagerSchedulerRecord(manager, 0x08U);
    RemoveManagerSchedulerRecord(manager, 0x0cU);
    g_BulletManagerSlot = 0; // DAT_00477818
    RunVectorDestructorIterator(static_cast<u8 *>(manager) + 0x14U, 0x3F0U,
                                0x896U, DestroyBulletManagerRecordInPlace);
}

// TH10 0x0042b570. In-place destructor of the DAT_00477840 0x840-byte
// main-chain object. The native reads +0x370 before clearing the global.
void DestroyMainChainObject840InPlace(void *object)
{
    u8 *const bytes = static_cast<u8 *>(object);
    const u32 buffer = LoadU32From(bytes + 0x370U);
    g_MainChainObject840 = 0; // DAT_00477840
    if (buffer != 0U) {
        ReleaseResourceBuffer(reinterpret_cast<void *>(buffer));
    }
    StoreU32To(bytes + 0x370U, 0U);
}

// TH10 0x00424ed0. In-place destructor of the DAT_00477834 player-state
// block.
void DestroyPlayerStateBlockInPlace(void *object)
{
    u8 *const block = static_cast<u8 *>(object);
    RemoveManagerSchedulerRecord(block, 0x08U);
    RemoveManagerSchedulerRecord(block, 0x0cU);
    g_OptionPositionBase = 0; // DAT_00477834

    if ((g_GlobalModeFlags & 1U) != 0U) {
        // Keep-alive path: only release the resource-matched entities and
        // republish the +0x45c pointer into the shot entry cache.
        ReleaseEntitiesUsingResourceEaxEdxAbi(
            g_MainChainRenderOwner,
            LoadU32From(block + 0x10U));
        g_PlayerShotEntryCache = // DAT_00491bf0
            reinterpret_cast<void *>(LoadU32From(block + 0x45cU));
    } else {
        u32 *const slot = reinterpret_cast<u32 *>(
            static_cast<u8 *>(g_MainChainRenderOwner) + 0x3AD08CU);
        if (*slot != 0U) {
            ReleaseLargeRenderOwnerSlotEdiAbi(reinterpret_cast<void *>(*slot));
            FreeMainChainObject(reinterpret_cast<void *>(*slot));
            *slot = 0U;
        }
        const u32 entry = LoadU32From(block + 0x45cU);
        if (entry != 0U) {
            ReleaseResourceBuffer(reinterpret_cast<void *>(entry));
            StoreU32To(block + 0x45cU, 0U);
        }
        g_PlayerShotEntryCache = 0; // DAT_00491bf0
    }

    const u32 work = LoadU32From(block + 0x36cU);
    if (work != 0U) {
        ReleaseResourceBuffer(reinterpret_cast<void *>(work));
    }
    StoreU32To(block + 0x36cU, 0U);
}

// TH10 0x0040d530. Native EAX = the DAT_00477704 ASCII HUD conditional
// state.
void DestroyAsciiHudConditionalStateEax(void *state)
{
    u8 *const bytes = static_cast<u8 *>(state);
    ReleaseAsciiHudConditionalState(state); // TH10 0x00409f90

    RemoveManagerSchedulerRecord(bytes, 0x08U);
    RemoveManagerSchedulerRecord(bytes, 0x0cU);

    const u32 sub = LoadU32From(bytes + 0x54U);
    if (sub != 0U) {
        // The native frees each non-null +0x0c..+0x88 word without clearing
        // the slots.
        for (u32 offset = 0x0cU; offset < 0x8cU; offset += 4U) {
            const u32 pointer = LoadU32From(reinterpret_cast<const u8 *>(sub)
                                            + offset);
            if (pointer != 0U) {
                ReleaseResourceBuffer(reinterpret_cast<void *>(pointer));
            }
        }
        // TH10 0x0040d680 (native ESI = the sub-object): plants the
        // 0x46D0F0 vtable, frees the +0x8C buffer through the CRT free and
        // clears the slot, then the sub-object allocation is released.
        u8 *const sub_bytes = reinterpret_cast<u8 *>(sub);
        StoreU32To(sub_bytes, 0x0046D0F0U);
        const u32 tail = LoadU32From(sub_bytes + 0x8cU);
        if (tail != 0U) {
            ReleaseResourceBuffer(reinterpret_cast<void *>(tail));
            StoreU32To(sub_bytes + 0x8cU, 0U);
        }
        FreeMainChainObject(reinterpret_cast<void *>(sub));
    }
    StoreU32To(bytes + 0x54U, 0U);

    if ((g_GlobalModeFlags & 9U) != 0U) {
        g_AsciiHudConditionalState = 0; // DAT_00477704
    } else {
        // Release the four render-owner large slots 0x3AD090..0x3AD09C
        // (indices 9..12 of the native signed slot guard).
        for (u32 i = 0; i < 4U; ++i) {
            const u32 index = 9U + i;
            if (static_cast<i32>(index) < 0 ||
                index >= 0x21U) {
                continue;
            }
            u32 *const slot = reinterpret_cast<u32 *>(
                static_cast<u8 *>(g_MainChainRenderOwner)
                + 0x3AD090U + i * 4U);
            if (*slot != 0U) {
                ReleaseLargeRenderOwnerSlotEdiAbi(
                    reinterpret_cast<void *>(*slot));
                FreeMainChainObject(reinterpret_cast<void *>(*slot));
                *slot = 0U;
            }
        }
        g_AsciiHudConditionalState = 0; // DAT_00477704
    }
}

} // namespace th10
