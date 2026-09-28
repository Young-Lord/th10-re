// TH10 timeline stage-effect spawn presets (0x00448e80-0x00449590).
//
// All six spawn variants allocate a preset text slot node from the
// DAT_00491c10 owner (0x00449950), set the +0x35c 0x40000000 flag and the
// +0x20 word to zero, seed the position triple at +0x340..0x348 from the
// EDI source (the positioned variants add 224.0f/16.0f first), bind the
// ANM script through 0x0043e710 (ECX = ANM work, EAX = node, EBX = script
// index) and finally link the node (list-A front, list-B, or list-B front;
// the positioned twins pass an offset position triple).
// 0x00449590 stops a timeline entity tree through its handle.
#include "Th10Types.hpp"
#include "Th10Platform.hpp"
#include "StageEffectHelpers.hpp"
#include "EntityHelpers.hpp"
#include "PlayerShotData.hpp"
namespace th10 {

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

const float k_positioned_offset_x = 224.0f; // TH10 0x00470b4c
const float k_positioned_offset_y = 16.0f;  // TH10 0x00470b48

u32 ReadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

void WriteU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

// Common spawn prologue; returns the new node address.
u32 *SpawnPresetNodeCommon(const u32 *source_triple, void *anm_work,
                           i32 script_index)
{
    // Native 0x00449950 AllocatePoolVmEsiAbi: ESI = owner, node in EAX.
    extern void *AllocatePoolVmEsiAbi(void *owner);
    u32 *node = static_cast<u32 *>(
        AllocatePoolVmEsiAbi(g_MainChainRenderOwner));
    if (node == 0)
        return 0;

    node[0x35cU / 4U] |= 0x40000000U;
    node[0x20U / 4U] = 0;
    node[0x340U / 4U] = source_triple[0];
    node[0x344U / 4U] = source_triple[1];
    node[0x348U / 4U] = source_triple[2];
    AssignAnmScriptToVmEcxEaxBbxAbi(anm_work, node, script_index);
    return node;
}

} // namespace

// TH10 0x00448e80. Native EDI = position triple, stack (ret 0xc) = ANM
// work, out id, script index. Links through list-A front (0x00448a50).
void SpawnStageEffectPresetFrontEdiStackAbi(const u32 *source_triple,
                                            void *anm_work, u32 *out_id,
                                            i32 script_index)
{
    u32 *node = SpawnPresetNodeCommon(source_triple, anm_work,
                                      script_index);
    if (node == 0)
        return;
    LinkEntityFrontAndAssignIdEaxEsiAbi(out_id, node);
}

// TH10 0x00448ee0. Positioned twin: adds (+224, +16) before the copy.
void SpawnStageEffectPositionedFrontEdiStackAbi(const u32 *source_triple,
                                                void *anm_work, u32 *out_id,
                                                i32 script_index)
{
    u32 adjusted[3];
    adjusted[0] = reinterpret_cast<const float &>(source_triple[0]) +
                  k_positioned_offset_x;
    adjusted[1] = reinterpret_cast<const float &>(source_triple[1]) +
                  k_positioned_offset_y;
    adjusted[2] = source_triple[2];
    u32 *node = SpawnPresetNodeCommon(adjusted, anm_work, script_index);
    if (node == 0)
        return;
    LinkEntityFrontAndAssignIdEaxEsiAbi(out_id, node);
}

// TH10 0x00448fb0. List-B variant (0x00448ac0).
void SpawnStageEffectPresetListBEdiStackAbi(const u32 *source_triple,
                                            void *anm_work, u32 *out_id,
                                            i32 script_index)
{
    u32 *node = SpawnPresetNodeCommon(source_triple, anm_work,
                                      script_index);
    if (node == 0)
        return;
    AttachEffectVmToListB(out_id, node, g_MainChainRenderOwner);
}

// TH10 0x00449010. Positioned list-B variant.
void SpawnStageEffectPositionedListBEdiStackAbi(const u32 *source_triple,
                                                void *anm_work, u32 *out_id,
                                                i32 script_index)
{
    u32 adjusted[3];
    adjusted[0] = reinterpret_cast<const float &>(source_triple[0]) +
                  k_positioned_offset_x;
    adjusted[1] = reinterpret_cast<const float &>(source_triple[1]) +
                  k_positioned_offset_y;
    adjusted[2] = source_triple[2];
    u32 *node = SpawnPresetNodeCommon(adjusted, anm_work, script_index);
    if (node == 0)
        return;
    AttachEffectVmToListB(out_id, node, g_MainChainRenderOwner);
}

// TH10 0x004490e0. List-B front variant (0x00448b40).
void SpawnStageEffectPresetListBFrontEdiStackAbi(const u32 *source_triple,
                                                 void *anm_work, u32 *out_id,
                                                 i32 script_index)
{
    u32 *node = SpawnPresetNodeCommon(source_triple, anm_work,
                                      script_index);
    if (node == 0)
        return;
    AttachEffectVmToListBFront(out_id, node, g_MainChainRenderOwner);
}

// TH10 0x00449140. Positioned list-B front variant.
void SpawnStageEffectPositionedListBFrontEdiStackAbi(
    const u32 *source_triple, void *anm_work, u32 *out_id, i32 script_index)
{
    u32 adjusted[3];
    adjusted[0] = reinterpret_cast<const float &>(source_triple[0]) +
                  k_positioned_offset_x;
    adjusted[1] = reinterpret_cast<const float &>(source_triple[1]) +
                  k_positioned_offset_y;
    adjusted[2] = source_triple[2];
    u32 *node = SpawnPresetNodeCommon(adjusted, anm_work, script_index);
    if (node == 0)
        return;
    AttachEffectVmToListBFront(out_id, node, g_MainChainRenderOwner);
}

// TH10 0x00449590. Native EAX = handle slot. Resolves the timeline handle
// and stops the entity tree: sets the 0x2 bit on the node (+0x35c) and,
// when the node has no children list (slot 6 zero), walks the child list
// (slot 5) setting the same bit on every child's +0x35c.
void StopTimelineEntityTreeEaxAbi(void *handle_slot)
{
    extern void *ResolveTimelineHandle(void *owner, i32 handle);
    u32 *slot = static_cast<u32 *>(handle_slot);
    u32 *node = static_cast<u32 *>(ResolveTimelineHandle(
        g_MainChainRenderOwner, static_cast<i32>(slot[0])));
    if (node == 0)
        return;

    node[0x35cU / 4U] |= 2U;
    if (node[6] != 0U)
        return;
    u32 *child = reinterpret_cast<u32 *>(node[5]);
    while (child != 0) {
        *reinterpret_cast<u32 *>(*child + 0x35cU) |= 2U;
        child = reinterpret_cast<u32 *>(child[1]);
    }
}

} // namespace th10
