// TH10 spell/bullet manager (DAT_004776f4, g_SpellBulletBase) lifecycle.
//
// The 0x37b0-byte manager owns two pools of 0x3ac-byte VM records
// (8 at +0x778, 5 at +0x24d8) plus the spell-card story state at
// +0x3788/+0x378c and the practice tutorial timeline handle at +0x774.

#include "Th10Types.hpp"
#include "CallbackScheduler.hpp"
#include "AsciiRenderModeDispatcher.hpp"
#include "SpellBulletVtable.hpp"
#include "TimelineRenderObjects.hpp"

namespace th10 {

extern CallbackScheduler *g_CallbackScheduler; // TH10 ds:0x491be4
extern void *g_SpellBulletBase;                // TH10 DAT_004776f4
extern void *g_MainChainRenderOwner;           // TH10 ds:0x491c10

// TH10 0x00408af0 (declared in GameModeTeardown.hpp).
void DestroySpellBulletBaseInPlace(void *object);

// Scheduler callback adapters (defined at the bottom of this file).
i32 TH10_FASTCALL SpellCardStoryStateCalcThunk(void *manager);
i32 TH10_FASTCALL SpellBulletPoolTickThunk(void *manager);
i32 TH10_FASTCALL SpellBulletPoolDispatchThunk(void *manager);

// TH10 0x00408910. Native stdcall, one stack argument = the 0x37b0-byte
// manager (ret 4). Constructs the two eh vector record pools (8 x 0x3ac
// at +0x778, 5 x 0x3ac at +0x24d8; scalar ctor 0x402050 / dtor 0x401ff0
// through 0x45252d), clears the +0x3744 flag bit 0, wipes the whole
// 0xdec-dword manager (native order quirk: after the pool construction),
// sets bit 1 of the first dword, publishes DAT_004776f4 and returns the
// manager.
void *InitSpellBulletManagerStackAbi(void *manager) {
    u8 *bytes = static_cast<u8 *>(manager);

    // Pool A: 8 records at +0x778; Pool B: 5 records at +0x24d8. Both use
    // the shared 0x3ac record init (0x402050).
    extern void *InitTitleScreenVmRecordEcxAbi(void *record);
    u8 *pool_a = bytes + 0x778;
    for (int i = 0; i < 8; ++i) {
        InitTitleScreenVmRecordEcxAbi(pool_a + i * 0x3ac);
    }
    u8 *pool_b = bytes + 0x24d8;
    for (int i = 0; i < 5; ++i) {
        InitTitleScreenVmRecordEcxAbi(pool_b + i * 0x3ac);
    }

    *reinterpret_cast<u32 *>(bytes + 0x3744) &= ~1u;
    u32 *wipe = reinterpret_cast<u32 *>(bytes);
    for (int i = 0; i < 0xdec; ++i) {
        wipe[i] = 0;
    }
    *wipe |= 2u;
    g_SpellBulletBase = manager;
    return manager;
}

// TH10 0x004089c0. Native EBX = manager. Registers three scheduler
// records with the manager as their argument: calc 0x409220 (the
// spell-card story-state thunk) at priority 0x16 on the calculation
// chain, draw 0x409230 at priority 0xe and draw 0x409270 at priority
// 0x25, both on the draw chain. Elements land at manager+8 / manager+0xc
// / manager+0x37ac. Then lazily initializes the +0x3734 timer record
// (sentinel -999999, default rate pointer 0x476f78, flag bit 0) and
// hard-resets it to {-1, 0, 0}. Returns 0.
i32 RegisterSpellBulletSchedulerRecordsEbxAbi(void *manager) {
    u8 *bytes = static_cast<u8 *>(manager);

    ChainElem *calc = CallbackSchedulerApi::Create(
        SpellCardStoryStateCalcThunk);
    calc->flags &= ~ChainElemFlag_Enabled;
    calc->arg = manager;
    CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler, calc,
                                                0x16);
    *reinterpret_cast<ChainElem **>(bytes + 0x8) = calc;

    ChainElem *draw_a = CallbackSchedulerApi::Create(
        SpellBulletPoolTickThunk);
    draw_a->flags &= ~ChainElemFlag_Enabled;
    draw_a->arg = manager;
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw_a, 0xe);
    *reinterpret_cast<ChainElem **>(bytes + 0xc) = draw_a;

    ChainElem *draw_b = CallbackSchedulerApi::Create(
        SpellBulletPoolDispatchThunk);
    draw_b->flags &= ~ChainElemFlag_Enabled;
    draw_b->arg = manager;
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw_b,
                                         0x25);
    *reinterpret_cast<ChainElem **>(bytes + 0x37ac) = draw_b;

    if ((*reinterpret_cast<u32 *>(bytes + 0x3744) & 1) == 0) {
        *reinterpret_cast<u32 *>(bytes + 0x3738) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x3734) = 0xfff0bdc1U;
        *reinterpret_cast<u32 *>(bytes + 0x373c) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x3740) = 0x476f78U;
        *reinterpret_cast<u32 *>(bytes + 0x3744) |= 1;
    }
    *reinterpret_cast<u32 *>(bytes + 0x3738) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x373c) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x3734) = 0xffffffffU;
    return 0;
}

// TH10 0x00408c90. Native stdcall (stack: ret 8 with one argument,
// unused by the body). operator new(0x37b0) + init + register; on a
// registration failure destroys (0x408af0), frees and returns null;
// otherwise returns the manager. The native runs the registration even
// for a null allocation (quirk preserved by the same order).
void *CreateSpellBulletManager() {
    void *manager = ::operator new(0x37b0U);
    if (manager != 0) {
        InitSpellBulletManagerStackAbi(manager);
    }
    const i32 result = RegisterSpellBulletSchedulerRecordsEbxAbi(manager);
    if (result != 0) {
        if (manager != 0) {
            DestroySpellBulletBaseInPlace(manager);
            ::operator delete(manager);
        }
        return 0;
    }
    return manager;
}

// TH10 0x004091c0. Native EBX = manager. When the +0x378c story flag has
// bit 0 set, dispatches the animation render mode over every record of
// pool A (8 at +0x778) and pool B (5 at +0x24d8) against the render
// owner. Always returns 1.
i32 DispatchSpellBulletPoolsEbxAbi(void *manager) {
    u8 *bytes = static_cast<u8 *>(manager);
    if ((*reinterpret_cast<u32 *>(bytes + 0x378c) & 1) == 0) {
        return 1;
    }
    u8 *record = bytes + 0x778;
    for (int i = 0; i < 8; ++i, record += 0x3ac) {
        DispatchAsciiAnimationVmRenderMode(record, g_MainChainRenderOwner);
    }
    record = bytes + 0x24d8;
    for (int i = 0; i < 5; ++i, record += 0x3ac) {
        DispatchAsciiAnimationVmRenderMode(record, g_MainChainRenderOwner);
    }
    return 1;
}

// TH10 0x00409230. Native ECX = manager. When the +0x378c story flag has
// bit 0 set, dispatches the animation render mode for the +0x10 and
// +0x3bc records. Always returns 1.
i32 DispatchSpellBulletPairEcxEcxAbi(void *manager) {
    u8 *bytes = static_cast<u8 *>(manager);
    if ((*reinterpret_cast<u32 *>(bytes + 0x378c) & 1) == 0) {
        return 1;
    }
    DispatchAsciiAnimationVmRenderMode(bytes + 0x10, g_MainChainRenderOwner);
    DispatchAsciiAnimationVmRenderMode(bytes + 0x3bc,
                                       g_MainChainRenderOwner);
    return 1;
}

// TH10 0x00409220. Calc-chain thunk: forwards ECX to the spell-card
// story-state updater (0x408d60) and passes its return through.
i32 TH10_FASTCALL SpellCardStoryStateCalcThunk(void *manager) {
    return UpdateSpellCardStoryStateEcxAbi(manager);
}

// TH10 0x00409230 thunk adapter (draw chain): ECX passthrough.
i32 TH10_FASTCALL SpellBulletPoolTickThunk(void *manager) {
    return DispatchSpellBulletPairEcxEcxAbi(manager);
}

// TH10 0x00409270. Draw-chain thunk: forwards ECX (as EBX) to the pool
// dispatcher 0x4091c0.
i32 TH10_FASTCALL SpellBulletPoolDispatchThunk(void *manager) {
    return DispatchSpellBulletPoolsEbxAbi(manager);
}

// TH10 0x0040cea0. Native ESI = manager. Raises the +0x378c flag bit 4
// (0x10), releases the practice tutorial timeline handle at +0x774
// (0x4492a0) and clears it.
void ReleaseSpellBulletTutorialHandleEsiAbi(void *manager) {
    u8 *bytes = static_cast<u8 *>(manager);
    *reinterpret_cast<u32 *>(bytes + 0x378c) |= 0x10u;
    const i32 handle = *reinterpret_cast<i32 *>(bytes + 0x774);
    ReleaseTimelineHandle(g_MainChainRenderOwner, handle);
    *reinterpret_cast<u32 *>(bytes + 0x774) = 0;
}

// TH10 0x0040ced0. Native EAX = manager: bit 3 (0x8) of the +0x378c
// story flags.
i32 GetSpellBulletStoryFlag3EaxAbi(void *manager) {
    return (*reinterpret_cast<u32 *>(
                static_cast<u8 *>(manager) + 0x378c) >> 3) & 1;
}

} // namespace th10
