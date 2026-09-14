// TH10 0x00421180 / 0x00421070 / 0x00421300 — spawn and stop management of
// the three background effect VMs owned by the 0x491c28 game-manager slot.
// 0x00421070 and 0x00421300 are the functions previously kept as the
// `LeaveGameManagerGate` / `EnterGameManagerGate` boundaries in
// TitleSceneSetup.cpp; 0x00421180 is their spawn counterpart (no direct
// reference to it remains in the shipped binary — no E8/E9 rel32 call and no
// absolute dword reference anywhere in the image — so it is reconstructed
// from the body and its twins). All fixed addresses are from the raw
// disassembly; see docs/evidence/game-manager-gate-vms.md.
#include <string.h>

#include "EntityHelpers.hpp"
#include "Th10Types.hpp"
#include "GameManagerGateVms.hpp"

namespace th10 {

namespace {

// ------------------------------------------------------------- globals

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern u32 g_ManagerBackgroundVmIdA; // TH10 DAT_00477824
extern u32 g_ManagerBackgroundVmIdB; // TH10 DAT_00477828
extern u32 g_ManagerBackgroundVmIdC; // TH10 DAT_0047782c
extern u32 g_ManagerSlotAuxFlag;     // TH10 DAT_00491bec

// ------------------------------------------------------------ accessors

inline u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

inline void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

// Writes the stop word at entity+0x304 and, when the entity's +0x18 count is
// zero, repeats it over the +0x14 {entity, next} child chain (native inline
// shared by both stop twins).
void StopEntityAndChildren(u8 *entity, u16 stop_word)
{
    *reinterpret_cast<u16 *>(entity + 0x304) = stop_word;
    if (LoadU32At(entity, 0x18) != 0U) {
        return;
    }
    u32 node = LoadU32At(entity, 0x14);
    while (node != 0U) {
        u8 *const child = reinterpret_cast<u8 *>(LoadU32At(
            reinterpret_cast<const void *>(node), 0x0));
        *reinterpret_cast<u16 *>(child + 0x304) = stop_word;
        node = LoadU32At(reinterpret_cast<const void *>(node), 0x4);
    }
}

// Shared stop body: stop word and post-stop latch differ between the twins
// (0x00421070: word 1, latch 0; 0x00421300: word 2, latch 2).
void StopGameManagerBackgroundVms(void *slot_arg, u16 stop_word,
                                  u32 stop_latch)
{
    u8 *const slot = static_cast<u8 *>(slot_arg);
    if (LoadU32At(slot, 0x6fc) == 1U) {
        u32 *const id_slots[3] = {&g_ManagerBackgroundVmIdA,
                                  &g_ManagerBackgroundVmIdB,
                                  &g_ManagerBackgroundVmIdC};
        for (u32 index = 0; index != 3U; ++index) {
            u8 *const entity = FindEntityEdxStackAbi(
                g_MainChainRenderOwner, *id_slots[index]);
            if (entity != 0) {
                StopEntityAndChildren(entity, stop_word);
            }
        }
        // The native stops all three first, then clears the id slots and the
        // latch (order preserved).
        for (u32 index = 0; index != 3U; ++index) {
            *id_slots[index] = 0U;
        }
        StoreU32At(slot, 0x6fc, stop_latch);
    }
    if (g_ManagerSlotAuxFlag != 0U) {
        g_ManagerSlotAuxFlag = 0U;
    }
}

} // namespace

// TH10 0x00421180 (native stdcall `ret 8`).
void SpawnGameManagerBackgroundVmsStackAbi(void *slot_arg,
                                           const float position[3])
{
    u8 *const slot = static_cast<u8 *>(slot_arg);
    if (LoadU32At(slot, 0x6fc) == 0U) {
        // The script binds run with ECX = the slot's +0x3c8 manager-work
        // (consumed by the 0x00449870 script-bind boundary; the semantic
        // body feeds its bind context internally).
        void *const manager_work =
            reinterpret_cast<void *>(LoadU32At(slot, 0x3c8));
        (void)manager_work;
        u32 *const id_slots[3] = {&g_ManagerBackgroundVmIdA,
                                  &g_ManagerBackgroundVmIdB,
                                  &g_ManagerBackgroundVmIdC};
        for (u32 script = 0; script != 3U; ++script) {
            u8 *const vm = static_cast<u8 *>(AllocatePoolVmEsiAbi(g_MainChainRenderOwner));
            StoreU32At(vm, 0x35c, LoadU32At(vm, 0x35c) | 0x40000000U);
            StoreU32At(vm, 0x20, 0xfU);
            AssignPoolVmScriptEcxEaxAbi(vm, static_cast<i32>(script));
            // Native uses the tail-append 0x004489d0, not the list-B
            // front-insertion variant.
            LinkEntityAndAssignIdEaxEsiAbi(id_slots[script], vm);
        }
        StoreU32At(slot, 0x6fc, 1U);

        // Publish the caller's position into all three VMs (0x004492f0,
        // native ESI = float3 read from the second stack argument).
        for (u32 index = 0; index != 3U; ++index) {
            SetEntityPositionDirectEsiAbi(g_MainChainRenderOwner,
                                          *id_slots[index], position);
        }
    }

    // Render-owner viewport-state init (both paths): while the owner's first
    // signed dword is negative (idle), mark it 8 and store the two 640x480
    // rects.
    u8 *const owner = static_cast<u8 *>(g_MainChainRenderOwner);
    if (static_cast<i32>(LoadU32At(owner, 0x0)) < 0) {
        StoreU32At(owner, 0x00, 8U);
        StoreU32At(owner, 0x2c, 0U);
        StoreU32At(owner, 0x30, 0U);
        StoreU32At(owner, 0x34, 0x280U); // 640
        StoreU32At(owner, 0x38, 0x1e0U); // 480
        StoreU32At(owner, 0x3c, 0U);
        StoreU32At(owner, 0x40, 0U);
        StoreU32At(owner, 0x44, 0x280U); // 640
        StoreU32At(owner, 0x48, 0x1e0U); // 480
    }
}

// TH10 0x00421070 (native stdcall `ret 4`).
void LeaveGameManagerGateStackAbi(void *slot)
{
    StopGameManagerBackgroundVms(slot, 1, 0U);
}

// TH10 0x00421300 (native stdcall `ret 4`).
void EnterGameManagerGateStackAbi(void *slot)
{
    StopGameManagerBackgroundVms(slot, 2, 2U);
}

} // namespace th10
