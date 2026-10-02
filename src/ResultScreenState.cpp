// TH10 0x4172e0 — result-screen state transitions (the state machine that
// rebuilds the result-screen score digits and the two banner entity slots of
// the ASCII HUD record). FinishSpellCardPracticeEaxAbi (0x409c00) enters it
// with mode 0 after adding a captured spell's bonus to the score.
//
// Native entry: EAX = mode, stack = {state owner (DAT_0047770c), bonus},
// `ret 8`. Modes 0..6 dispatch through the jump table at 0x417560; mode 5 is
// a clean no-op (it jumps straight into the epilogue) and modes above 6 fall
// through from the `ja` at 0x4172eb — both preserved.
//
// Layout of the state owner (the ASCII HUD record):
//   +0x9df4..+0x9e10  eight u32 entity handles, one per score digit
//                     (most significant digit first)
//   +0x9e14           first banner handle
//   +0x9e18           second banner handle
//   +0x9ec8           resource pointer handed to the spawn/bind helpers
#include "ResultScreenState.hpp"

#include "AsciiAnimationVm.hpp"
#include "AsciiHudOwner.hpp"
#include "EntityHelpers.hpp"
#include "GameManagerState.hpp"
#include "TimelineRenderObjects.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

// TH10 0x449590. Set-twin of ClearEntityFlag2ByHandleSlot (GameMangerState /
// PostRunReplaySaveMenu keep local copies too): resolve the handle stored at
// *handle_slot through DAT_00491c10 and set bit 2 of the +0x35c flag word,
// propagating to the +0x14 child chain while the +0x18 count is zero.
void SetEntityFlagWord2ByHandleSlot(u32 *handle_slot)
{
    u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                       *handle_slot);
    if (entity == 0) {
        return;
    }
    VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
    vm.flags |= 2U;
    if (vm.parent_link != 0) {
        return;
    }
    void *node = vm.first_child;
    while (node != 0) {
        u8 *child = reinterpret_cast<u8 *>(node);
        VmRecord &child_vm = *reinterpret_cast<VmRecord *>(child);
        child_vm.flags |= 2U;
        // The walk's next slot (node+4) is not a modeled chain field; kept raw.
        node = *reinterpret_cast<void **>(child + 4);
    }
}

// Mode 0 (0x4172f9): rebuild the whole result screen. The +0x9e14 banner is
// released and respawned from script 0x47, then eight score-digit VMs are
// allocated (scripts 0x27..0x2e), linked, and fed their digit entry
// (bonus digit + 8) through the ASCII animation VM initializer. Leading zero
// digits have the +0x35c bit-2 visibility flag cleared, the rest set.
void RunResultScreenScoreRebuild(u8 *owner, i32 bonus)
{
    AsciiHudOwner &hud = *reinterpret_cast<AsciiHudOwner *>(owner);

    ReleaseEntityById(g_MainChainRenderOwner, hud.first_banner_handle);
    hud.first_banner_handle = 0U;

    i32 *const respawn = SpawnSetupEffectVmListABack(0x47, 0xfU);
    // Unchecked dereference of the spawned record (native quirk).
    hud.first_banner_handle = static_cast<u32>(*respawn);

    u32 nonzero_digit_seen = 0;
    i32 value = bonus;
    i32 divisor = 0x989680; // 10^7: the most significant of eight digits

    for (u32 digit = 0; digit != 8; ++digit) {
        u32 *const slot = &hud.result_digit_handles[digit];

        ReleaseEntityById(g_MainChainRenderOwner, *slot);
        *slot = 0;

        u8 *const vm = static_cast<u8 *>(
            AllocatePoolVmEsiAbi(g_MainChainRenderOwner));
        VmRecord &vm_rec = *reinterpret_cast<VmRecord *>(vm);
        vm_rec.render_kind = 0xfU; // kind 0xf
        vm_rec.flags |= 0x40000000U;
        // The native call also carries the +0x9ec8 bind context in ECX; that
        // manager-work bind tail lives inside the semantic body.
        AssignPoolVmScriptEcxEaxAbi(vm, static_cast<i32>(0x27U + digit));

        u32 id = 0;
        LinkEntityAndAssignIdEaxEsiAbi(&id, vm);
        *slot = id;

        const i32 quotient = value / divisor;
        const i32 remainder = value % divisor;

        u8 *const entity = FindEntityEdxStackAbi(g_MainChainRenderOwner, id);
        if (entity != 0) {
            (void)InitializeAsciiAnimationVmEntry(
                entity, static_cast<u32>(quotient + 8),
                reinterpret_cast<VmRecord *>(entity)->bound_resource);
        }

        if (quotient != 0) {
            nonzero_digit_seen = 1;
        }
        if (nonzero_digit_seen != 0) {
            SetEntityFlagWord2ByHandleSlot(slot);
        } else {
            ClearEntityFlag2ByHandleSlot(slot);
        }

        value = remainder;
        divisor /= 10;
    }
}

// Modes 1 (0x417441) and 6 (0x417520) share one tail at 0x41753a: release the
// +0x9e14 banner, respawn it from `script` (kind 0xf) and store its id.
void RespawnFirstBanner(u8 *owner, i32 script)
{
    AsciiHudOwner &hud = *reinterpret_cast<AsciiHudOwner *>(owner);
    ReleaseEntityById(g_MainChainRenderOwner, hud.first_banner_handle);
    i32 *const respawn = SpawnSetupEffectVmListABack(script, 0xfU);
    hud.first_banner_handle = 0U;
    hud.first_banner_handle = static_cast<u32>(*respawn);
}

// Modes 2/3/4 (0x417460/0x4174a0/0x4174e0): release the +0x9e18 banner,
// respawn it from `script` (kind 0xf) and store its id.
void RespawnSecondBanner(u8 *owner, i32 script)
{
    AsciiHudOwner &hud = *reinterpret_cast<AsciiHudOwner *>(owner);
    ReleaseEntityById(g_MainChainRenderOwner, hud.second_banner_handle);
    i32 *const respawn = SpawnSetupEffectVmListABack(script, 0xfU);
    hud.second_banner_handle = 0U;
    hud.second_banner_handle = static_cast<u32>(*respawn);
}

} // namespace

void ApplyResultScreenStateEaxStackAbi(i32 mode, void *state_owner,
                                      i32 bonus)
{
    u8 *const owner = static_cast<u8 *>(state_owner);

    switch (mode) {
    case 0:
        RunResultScreenScoreRebuild(owner, bonus);
        break;
    case 1:
        RespawnFirstBanner(owner, 0x48);
        break;
    case 2:
        RespawnSecondBanner(owner, 0x49);
        break;
    case 3:
        RespawnSecondBanner(owner, 0x4a);
        break;
    case 4:
        RespawnSecondBanner(owner, 0x4b);
        break;
    case 5:
        // Native quirk: mode 5 jumps straight into the epilogue.
        break;
    case 6:
        RespawnFirstBanner(owner, 0x4c);
        break;
    default:
        // `ja 0x417559`: out-of-range modes fall straight to the epilogue.
        break;
    }
}

} // namespace th10
