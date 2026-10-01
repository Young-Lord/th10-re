#include "AsciiHudOverlayUpdate.hpp"

#include "AsciiAnimationVm.hpp"
#include "EntityHelpers.hpp"
#include "PlayerShotData.hpp"
#include "VmRecord.hpp"

namespace th10 {

// TH10 DAT_00491c10: the main-chain render owner that owns the VM pool and
// the entity lists this routine spawns into.
extern void *g_MainChainRenderOwner;
// TH10 DAT_004776e0: ASCII manager; +0x8994 holds the ANM manager-work used
// for the auxiliary owner+0x9a48 VM reset.
extern void *g_AsciiManager;
// TH10 DAT_00477810: title screen state; +0x5c gates the script-79 spawn.
extern void *g_TitleScreen;
// TH10 DAT_00491fb8: shared status (8 = practice/result style HUD).
extern i32 g_MainChainSharedStatus;
// TH10 DAT_00474ca0: global mode flags; bit 0x20 selects the script-113 VM.
extern u32 g_GlobalModeFlags;
// TH10 DAT_00474c70: signed lives counter driving the visible-slot flags.
extern i32 g_PlayerLivesRemaining;
// TH10 DAT_00474c48: power gauge word (native reads it with movsx).
extern u16 g_PlayerPowerGaugeWord;
// TH10 DAT_00474c7c: active text layer / stage selector.
extern i32 g_ActiveTextLayer;
// TH10 DAT_00474c74: current difficulty index.
extern i32 g_TimelinePhase;
// TH10 DAT_00474c90: unnamed third gate of the script-79 spawn.
extern i32 g_HudGateState474c90;
// TH10 DAT_00491fc4: unnamed gate for the difficulty-script pair.
extern i32 g_HudGateFlag491fc4;

namespace {

inline u32 ReadU32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline void WriteU32(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

// Shared spawn core used by every entity this routine creates: allocate a
// 0x3ac pool VM, stamp render mode 15 (+0x20) and the 0x40000000 flag
// (+0x35c), bind the overlay script, and link it into the manager's active
// list with the wrapping id counter (native 0x449950 / 0x449870 / 0x4489d0
// sequence inlined at every spawn site).
u32 SpawnLinkedOverlayVm(void *anm_work, i32 script_id)
{
    u8 *const vm = static_cast<u8 *>(
        AllocatePoolVmEsiAbi(g_MainChainRenderOwner));
    VmRecord &vm_record = *reinterpret_cast<VmRecord *>(vm);
    vm_record.render_kind = 15U;
    vm_record.flags |= 0x40000000U;
    AssignPoolVmScriptEcxEaxAbi(vm, script_id);
    u32 id = 0;
    LinkEntityAndAssignIdEaxEsiAbi(&id, vm);
    return id;
}

} // namespace

// TH10 0x00413bc0.
void ResetAsciiHudOverlayEdiAbi(void *owner_memory)
{
    u8 *const owner = static_cast<u8 *>(owner_memory);
    void *const glyph_resource =
        *reinterpret_cast<void **>(owner + 0x9ec8);

    // Disable the two scheduler records (+8 / +0xc) by setting bit 1 of
    // their +4 flag word.
    u8 *const record_slots[2] = {owner + 8, owner + 0xc};
    for (u32 i = 0; i < 2U; ++i) {
        u8 *const record = *reinterpret_cast<u8 **>(record_slots[i]);
        if (record != 0)
            WriteU32(record, 4, ReadU32(record, 4) | 2U);
    }

    // Re-create the two permanent background entities; the second id is
    // discarded by the original.
    if (ReadU32(owner, 0x9e58) == 0U)
        WriteU32(owner, 0x9e58, SpawnLinkedOverlayVm(glyph_resource, 0));
    if (ReadU32(owner, 0x9e5c) == 0U)
        SpawnLinkedOverlayVm(glyph_resource, 1);

    // Rebind the glyph VM pool scripts unless the +0x36c bit-0 latch says
    // the pools are already armed. Script ids: +0x10 -> 10..19 paired with
    // +0x24c8 -> 20..29, +0x4980 -> 30..38, +0x6a8c -> 47..50,
    // +0x793c -> 80..81, +0x8094 -> 51..57.
    if ((owner[0x36c] & 1U) == 0U) {
        for (u32 i = 0; i < 10U; ++i) {
            AssignAnmScriptToVmEcxEaxBbxAbi(glyph_resource,
                                            owner + 0x10 + i * 0x3ac,
                                            static_cast<i32>(10 + i));
            AssignAnmScriptToVmEcxEaxBbxAbi(glyph_resource,
                                            owner + 0x24c8 + i * 0x3ac,
                                            static_cast<i32>(20 + i));
        }
        for (u32 i = 0; i < 9U; ++i)
            AssignAnmScriptToVmEcxEaxBbxAbi(glyph_resource,
                                            owner + 0x4980 + i * 0x3ac,
                                            static_cast<i32>(30 + i));
        for (u32 i = 0; i < 4U; ++i)
            AssignAnmScriptToVmEcxEaxBbxAbi(glyph_resource,
                                            owner + 0x6a8c + i * 0x3ac,
                                            static_cast<i32>(47 + i));
        for (u32 i = 0; i < 2U; ++i)
            AssignAnmScriptToVmEcxEaxBbxAbi(glyph_resource,
                                            owner + 0x793c + i * 0x3ac,
                                            static_cast<i32>(80 + i));
        for (u32 i = 0; i < 7U; ++i)
            AssignAnmScriptToVmEcxEaxBbxAbi(glyph_resource,
                                            owner + 0x8094 + i * 0x3ac,
                                            static_cast<i32>(51 + i));
    }

    // Visible life slots: enable the first `lives` of the nine 0x3ac-stride
    // flag words at +0x4cdc and clear the rest. The original enables the
    // counter verbatim, so a value above nine keeps setting flags.
    const i32 lives = g_PlayerLivesRemaining;
    for (i32 i = 0; i < lives; ++i)
        WriteU32(owner, 0x4cdc + static_cast<u32>(i) * 0x3ac,
                 ReadU32(owner, 0x4cdc + static_cast<u32>(i) * 0x3ac) | 2U);
    if (lives < 9) {
        for (i32 i = lives; i < 9; ++i)
            WriteU32(owner, 0x4cdc + static_cast<u32>(i) * 0x3ac,
                     ReadU32(owner, 0x4cdc + static_cast<u32>(i) * 0x3ac) &
                         ~2U);
    }

    // Life display: lives/20 into the +0x6a8c VM and the fractional part
    // scaled to 0..99 as tens/ones entries into the next two VMs.
    const i32 gauge = static_cast<i32>(
        static_cast<short>(g_PlayerPowerGaugeWord));
    const i32 whole = gauge / 20;
    const i32 scaled = (gauge % 20) * 100 / 20;
    InitializeAsciiAnimationVmEntry(owner + 0x6a8c,
                                    static_cast<u32>(whole + 8),
                                    glyph_resource);
    InitializeAsciiAnimationVmEntry(owner + 0x71e4,
                                    static_cast<u32>(scaled / 10 + 8),
                                    glyph_resource);
    InitializeAsciiAnimationVmEntry(owner + 0x7590,
                                    static_cast<u32>(scaled % 10 + 8),
                                    glyph_resource);

    // Mode-8 (shared status 8) or the 0x20 mode flag replace the standard
    // background pair with the script-113 practice overlay VM. Otherwise
    // the pair (0, 1) is spawned against the +0x9e80 resource.
    if (g_MainChainSharedStatus != 8 &&
        (g_GlobalModeFlags & 0x20U) == 0U) {
        void *const pair_resource =
            *reinterpret_cast<void **>(owner + 0x9e80);
        SpawnLinkedOverlayVm(pair_resource, 0);
        SpawnLinkedOverlayVm(pair_resource, 1);
    }
    if ((g_GlobalModeFlags & 0x20U) != 0U)
        SpawnLinkedOverlayVm(glyph_resource, 113);

    // Reset the auxiliary +0x9a48 VM with the ASCII manager's ANM work.
    AssignAnmScriptToVmEcxEaxBbxAbi(
        *reinterpret_cast<void **>(static_cast<u8 *>(g_AsciiManager) +
                                   0x8994),
        owner + 0x9a48, 0);

    // Stage-start text layer: spawn script 79 when the active layer is 1,
    // the title screen has no pending +0x5c state, and the third gate is
    // clear.
    if (g_ActiveTextLayer == 1 &&
        ReadU32(static_cast<u8 *>(g_TitleScreen), 0x5c) == 0U &&
        g_HudGateState474c90 == 0)
        SpawnLinkedOverlayVm(glyph_resource, 79);

    // Difficulty-keyed overlay pair when the 0x491fc4 gate is set. The
    // script-102 entity id is remembered at +0x9e50 and immediately
    // resolved to fire its state word (3 = consume).
    if (g_HudGateFlag491fc4 != 0) {
        WriteU32(owner, 0x9e50,
                 SpawnLinkedOverlayVm(
                     glyph_resource,
                     static_cast<i32>(102 + g_TimelinePhase)));
        SetEntityStateWordEaxEsiAbi(
            reinterpret_cast<u32 *>(owner + 0x9e50), 3);
    }

    // The script-107 twin always spawns; its id lands in +0x9e54, the
    // remembered +0x9e50 id is resolved again, and the +0x9e90 slot is
    // cleared unconditionally on every exit path.
    const u32 tail_id = SpawnLinkedOverlayVm(
        glyph_resource, static_cast<i32>(107 + g_TimelinePhase));
    // The original copies the id counter (now the script-107 id) here.
    WriteU32(owner, 0x9e54, tail_id);
    SetEntityStateWordEaxEsiAbi(reinterpret_cast<u32 *>(owner + 0x9e50), 3);
    WriteU32(owner, 0x9e90, 0U);
}

} // namespace th10
