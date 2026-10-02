#include "ManagerCreation.hpp"

#include "AsciiHudOwner.hpp"
#include "ManagerReleaseWrappers.hpp"
#include "PlayerObjectLifecycle.hpp"
#include "PlayerRecord.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

extern void FreeMainChainObject(void *object); // TH10 0x004524a1
// DestroyEffectManagerRootInPlace (TH10 0x00405f70) comes from
// ManagerReleaseWrappers.hpp; keep the local declaration for the HUD
// destructor which has no shared header yet.
extern void DestroyAsciiHudOwnerInPlace(void *object); // TH10 0x004145f0

// TH10 0x0045252d: `eh vector constructor iterator'
// (void* elems, uint size, int count, ctor_each, ctor_first)
extern "C" void TH10_STDCALL EhVectorConstructorIterator(
    void *elements, u32 size, i32 count, void *ctor_each, void *ctor_first);
extern i32 LoadEffectManagerContent(); // TH10 0x00405e20
extern i32 LoadAsciiHudOwnerContent(); // TH10 0x00413980

} // namespace

void *CreateEffectManagerRoot()
{
    extern void *g_EffectManagerRoot; // TH10 DAT_004776f0
    void *const object = ::operator new(0x3e0b54U);
    if (object != 0) {
        EhVectorConstructorIterator(static_cast<u8 *>(object) + 0x60U,
                                    0x7f0U, 2001,
                                    reinterpret_cast<void *>(0x405d00U),
                                    reinterpret_cast<void *>(0x405de0U));
        u8 *const bytes = static_cast<u8 *>(object);
        // Native order quirk: the full-object wipe runs after the array
        // construction above.
        for (u32 i = 0; i < 0x3e0b54U; ++i) {
            bytes[i] = 0U;
        }
        g_EffectManagerRoot = object;
    }
    if (LoadEffectManagerContent() == 0) {
        return object;
    }
    if (object != 0) {
        DestroyEffectManagerRootInPlace(object);
        FreeMainChainObject(object);
    }
    return 0;
}

void *CreateAsciiHudOwner()
{
    void *const object = ::operator new(0x9ed0U);
    void *constructed = 0;
    if (object != 0) {
        constructed = ConstructAsciiHudOwnerEaxAbi(object);
    }
    if (LoadAsciiHudOwnerContent() == 0) {
        return constructed;
    }
    if (constructed != 0) {
        DestroyAsciiHudOwnerInPlace(constructed);
        FreeMainChainObject(constructed);
    }
    return 0;
}

void *CreatePlayerStateBlock()
{
    extern void DestroyPlayerStateBlockInPlace(void *object); // TH10 0x00424ed0
    extern void *ConstructPlayerStateBlockEaxAbi(void *object); // TH10 0x004246c0
    void *const object = ::operator new(0x4478U);
    void *constructed = 0;
    if (object != 0) {
        constructed = ConstructPlayerStateBlockEaxAbi(object);
    }
    if (InitializePlayerObject(constructed) == 0) {
        return constructed;
    }
    if (constructed != 0) {
        DestroyPlayerStateBlockInPlace(constructed);
        FreeMainChainObject(constructed);
    }
    return 0;
}


namespace {

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

void *ConstructAsciiHudOwnerEaxAbi(void *object)
{
    extern void *g_AsciiHudOwner; // TH10 DAT_0047770c
    extern void ConstructBlankVmRecord(void *record); // TH10 0x00402050
    u8 *const base = static_cast<u8 *>(object);
    AsciiHudOwner &hud = *reinterpret_cast<AsciiHudOwner *>(object);
    // Everything from the array construction down to the 0x9ed0 wipe is a
    // deliberate dead-store block (the wipe erases all of it), so those
    // accesses stay raw per the conversion rules. Only the live
    // flags_0000 store after the wipe is converted.
    const u32 array_slots[6] = {0x10U, 0x24c8U, 0x4980U, 0x6a8cU, 0x793cU,
                                0x8094U};
    const u32 array_counts[6] = {10U, 10U, 9U, 4U, 2U, 7U};
    for (u32 i = 0; i < 6U; ++i) {
        EhVectorConstructorIterator(base + array_slots[i], 0x3acU,
                                    static_cast<i32>(array_counts[i]),
                                    reinterpret_cast<void *>(0x402050U),
                                    reinterpret_cast<void *>(
                                        &DestroyTitleScreenVmRecordInPlace));
    }
    const u32 flag_slots[9] = {0x9ad4U, 0x9b38U, 0x9be4U, 0x9c50U, 0x9cfcU,
                               0x9d98U, 0x9e24U, 0x9e90U, 0x9f60U};
    for (u32 i = 0; i < 9U; ++i) {
        StoreU32To(base + flag_slots[i],
                   LoadU32From(base + flag_slots[i]) & ~1U);
    }
    for (u32 i = 0; i < 0x3acU; ++i) {
        base[0x9a28U + i] = 0U;
    }
    base[0x9d8cU] = 0xffU;
    base[0x9d8dU] = 0xffU;
    StoreU32To(base + 0x9e70U, LoadU32From(base + 0x9e70U) & ~1U);
    for (u32 i = 0; i < 0x9ed0U; ++i) {
        base[i] = 0U;
    }
    hud.flags_0000 |= 2U;
    g_AsciiHudOwner = object;
    return object;
}

void *ConstructPlayerStateBlockEsiAbi(void *object)
{
    extern void *g_OptionPositionBase; // TH10 DAT_00477834
    u8 *const base = static_cast<u8 *>(object);
    PlayerRecord &player = *reinterpret_cast<PlayerRecord *>(base);
    const u32 early_flags[9] = {0x80U, 0xc4U, 0x110U, 0x13cU, 0x188U,
                                0x1c4U, 0x210U, 0x23cU, 0x38cU};
    for (u32 i = 0; i < 9U; ++i) {
        StoreU32To(base + early_flags[i],
                   LoadU32From(base + early_flags[i]) & ~1U);
    }
    for (u32 i = 0; i < 0x3acU; ++i) {
        base[0x14U + i] = 0U;
    }
    base[0x398U] = 0xffU;
    base[0x399U] = 0xffU;
    const u32 mid_flags[3] = {0x470U, 0x484U, 0x498U};
    for (u32 i = 0; i < 3U; ++i) {
        StoreU32To(base + mid_flags[i],
                   LoadU32From(base + mid_flags[i]) & ~1U);
    }
    PlayerShotRecord *shot = player.shots;
    for (i32 i = 0; i < 128; ++i) {
        shot->timer_latch &= ~1U;
        ++shot;
    }
    const u32 late_flags[4] = {0x3320U, 0x33b8U, 0x3450U, 0x34e8U};
    for (u32 i = 0; i < 4U; ++i) {
        StoreU32To(base + late_flags[i],
                   LoadU32From(base + late_flags[i]) & ~1U);
    }
    u32 cursor = 0x3560U;
    for (i32 i = 0; i < 33; ++i) {
        StoreU32To(base + cursor, LoadU32From(base + cursor) & ~1U);
        cursor += 0x6cU;
    }
    player.deathbomb_timer.flags &= ~1U;
    for (u32 i = 0; i < 0x4478U; ++i) {
        base[i] = 0U;
    }
    g_OptionPositionBase = static_cast<u8 *>(object);
    return object;
}

} // namespace th10
