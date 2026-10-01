#include "StageEffectHelpers.hpp"

#include "AsciiAnimationVm.hpp"
#include "EntityHelpers.hpp"
#include "BgmRuntime.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "PlayerShotData.hpp"
#include "VmRecord.hpp"

#include <cmath>
#include <string.h>

namespace th10 {

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern float g_FrameTimeScale; // TH10 DAT_00476f78
extern i32 g_ActiveTextLayer; // TH10 DAT_00474c7c
extern u32 g_TextStyleFlag; // TH10 DAT_00474c84
extern i32 g_TextStyleAlt; // TH10 DAT_00474c8c
extern i32 g_TextStyleDefault; // TH10 DAT_00474c88

// TH10 0x452493: operator new.
u8 *AllocateHeapBlock(u32 bytes);

inline i32 ReadInt(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline void WriteInt(u8 *bytes, u32 offset, i32 value)
{
    *reinterpret_cast<i32 *>(bytes + offset) = value;
}

inline void WriteUint(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

inline void WriteFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

inline float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline u32 ReadUint(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

} // namespace

// TH10 0x00448db0. Pool-allocates a VM, publishes the +224/+16-offset
// position, binds the script from the effect context (silent whole-record
// wipe when the script is missing), and links it into manager list A; the
// assigned id lands in *(effect context).
void *SpawnStageEffectEdxEbxAbi(void *effect_context,
                                const float position[3], i32 script_index)
{
    void *const vm_memory = AllocatePoolVmEsiAbi(g_MainChainRenderOwner);
    VmRecord &vm = *reinterpret_cast<VmRecord *>(vm_memory);
    vm.render_kind = 0;
    vm.flags |= 0x40000000U;
    vm.delta_pos_x = position[0] + 224.0f;
    vm.delta_pos_y = position[1] + 16.0f;
    vm.delta_pos_z = position[2];
    AssignAnmScriptToVmEcxEaxBbxAbi(effect_context, vm_memory, script_index);
    u32 id = 0;
    LinkEntityAndAssignIdEaxEsiAbi(&id, vm_memory);
    WriteUint(static_cast<u8 *>(effect_context), 0, id);
    return vm_memory;
}

// TH10 0x00405500. Clears the aggregate and seven per-life flag dwords
// (bit 1 only) once the boss frame counter passes 59.
void CleanupStageLifeFlags(void *stage_memory)
{
    u8 *const stage = static_cast<u8 *>(stage_memory);
    if (ReadInt(stage, 0x3738) <= 0x3b)
        return;
    WriteUint(stage, 0x3790, 0);
    static const u32 kFlagOffsets[8] = {
        0x378c, 0xad4, 0xe80, 0x15d8, 0x1984, 0x1d30, 0x20dc, 0x2488};
    for (u32 index = 0; index != 8; ++index)
        WriteUint(stage, kFlagOffsets[index],
                  ReadUint(stage, kFlagOffsets[index]) & ~2U);
}

// TH10 0x00420a90. Copies the path, rewrites the extension to ".wav", and
// pushes the command (opcode 1, track slot = param) into the 31-slot BGM
// queue through the existing semantic body of 0x0043e460.
void StartBgmTrack(const char *path, i32 param)
{
    extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590
    char buffer[256];
    strncpy(buffer, path, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    char *dot = strrchr(buffer, '.');
    if (dot != 0 && dot - buffer < static_cast<int>(sizeof(buffer)) - 3) {
        dot[1] = 'w';
        dot[2] = 'a';
        dot[3] = 'v';
        dot[4] = '\0';
    }
    QueueBgmCommand(&g_TransitionRoot, buffer, 1, param);
}

// TH10 0x00448ac0. List-B variant of the registration: node at vm+4
// ({id, next, prev}), head/tail at manager+0x72dadc/0x72dae0, id counter
// at manager+0x732454 wrapping past zero to 1.
void AttachEffectVmToListB(u32 *out_id, void *vm_memory, void *manager_memory)
{
    LargeRenderOwnerLayout &owner =
        *static_cast<LargeRenderOwnerLayout *>(manager_memory);
    VmRecord &vm = *reinterpret_cast<VmRecord *>(vm_memory);
    vm.link_self = vm_memory;
    vm.link_next = 0;
    vm.link_prev = 0;
    if (owner.first_list_b == 0) {
        owner.first_list_b =
            reinterpret_cast<OwnerLink *>(&vm.link_self);
    } else {
        // Stored list nodes are the records' intrusive {link_self,
        // link_next, link_prev} trio at record+4.
        OwnerLink *const tail = owner.last_list_b;
        VmRecord &last_vm =
            *reinterpret_cast<VmRecord *>(reinterpret_cast<u8 *>(tail) - 0x04);
        u8 *const last_next_node = static_cast<u8 *>(last_vm.link_next);
        if (last_next_node != 0) {
            vm.link_next = last_next_node;
            reinterpret_cast<VmRecord *>(last_next_node - 0x04)
                ->link_prev = &vm.link_self;
        }
        last_vm.link_next = &vm.link_self;
        vm.link_prev = &last_vm.link_self;
    }
    owner.last_list_b = reinterpret_cast<OwnerLink *>(&vm.link_self);
    owner.node_id_counter += 1;
    if (owner.node_id_counter == 0)
        owner.node_id_counter = 1;
    vm.entity_id = static_cast<i32>(owner.node_id_counter);
    *out_id = static_cast<u32>(owner.node_id_counter);
}

// TH10 0x00448b40. List-B front-insertion twin of 0x448ac0: the new node
// is prepended at +0x72dadc and the tail (+0x72dae0) is only touched when
// the list was empty.
void AttachEffectVmToListBFront(u32 *out_id, void *vm, void *manager_memory)
{
    LargeRenderOwnerLayout &owner =
        *static_cast<LargeRenderOwnerLayout *>(manager_memory);
    u8 *const vm_bytes = static_cast<u8 *>(vm);
    VmRecord &vm_rec = *reinterpret_cast<VmRecord *>(vm_bytes);
    vm_rec.link_self = vm_bytes;
    vm_rec.link_next = 0;
    vm_rec.link_prev = 0;
    if (owner.first_list_b == 0) {
        owner.last_list_b =
            reinterpret_cast<OwnerLink *>(&vm_rec.link_self);
    } else {
        vm_rec.link_next = owner.first_list_b;
        OwnerLink *const head = owner.first_list_b;
        reinterpret_cast<VmRecord *>(reinterpret_cast<u8 *>(head) - 0x04)
            ->link_prev = &vm_rec.link_self;
    }
    owner.first_list_b = reinterpret_cast<OwnerLink *>(&vm_rec.link_self);
    owner.node_id_counter += 1;
    if (owner.node_id_counter == 0)
        owner.node_id_counter = 1;
    vm_rec.entity_id = static_cast<i32>(owner.node_id_counter);
    *out_id = static_cast<u32>(owner.node_id_counter);
}

// TH10 0x00424480. Copies rounded parameters from the found entity's
// block (+0x394) into the overlay struct; the busy guard is the only
// failure check (an unresolved id would crash in the original).
i32 CreateGameOverOverlay(void *target_memory, i32 id, i32 p2, i32 p3,
                          i32 p4, i32 p5)
{
    u8 *const target = static_cast<u8 *>(target_memory);
    u8 *const entity =
        FindEntityEdxStackAbi(g_MainChainRenderOwner,
                              static_cast<u32>(id));
    VmRecord &entity_vm = *reinterpret_cast<VmRecord *>(entity);
    const u8 *const params = static_cast<const u8 *>(entity_vm.anim_entry);
    if (ReadInt(target, 4) >= 0)
        return -1;
    WriteInt(target, 4,
             *static_cast<const i32 *>(entity_vm.bound_resource));
    WriteInt(target, 8, p2);
    WriteInt(target, 0x18, static_cast<i32>(
                               ReadFloat(params, 0x30)));
    WriteInt(target, 0xc, p3);
    WriteInt(target, 0x10, p4);
    WriteInt(target, 0x24, static_cast<i32>(
                               ReadFloat(params, 0x30)));
    WriteInt(target, 0x1c, static_cast<i32>(
                               ReadFloat(params, 0xc)));
    WriteInt(target, 0x14, p5);
    WriteInt(target, 0x20, static_cast<i32>(
                               ReadFloat(params, 0x34)));
    WriteInt(target, 0x28, 0);
    return 0;
}

// TH10 0x0041a120. Allocates a 0x88-byte text object, links it at the tail
// of the active layer's list, and applies the defaults the popup helper
// then adjusts.
void *CreateTextEffect(void *list_base, const float position[3],
                       const char *text)
{
    u8 *const object = AllocateHeapBlock(0x88);
    memset(object, 0, 0x88);
    *reinterpret_cast<u32 *>(object + 0xc) = reinterpret_cast<u32>(object);
    WriteInt(object, 0x84, -1);
    WriteInt(object, 0x6c, 300);
    WriteFloat(object, 0x7c, 1.0f);
    WriteInt(object, 0x70, -1);

    u8 *const list_array = static_cast<u8 *>(list_base);
    u32 *const head = reinterpret_cast<u32 *>(
        list_array +
        (static_cast<u32>(g_ActiveTextLayer) * 3 + 0x1e) * 4);
    u32 *cursor = head;
    while (*reinterpret_cast<u32 *const *>(cursor + 1) != 0)
        cursor = *reinterpret_cast<u32 **>(cursor + 1);
    u32 *const next = *reinterpret_cast<u32 **>(cursor + 1);
    if (next != 0)
        next[2] = reinterpret_cast<u32>(object + 0xc);
    *reinterpret_cast<u32 *>(cursor + 1) =
        reinterpret_cast<u32>(object + 0xc);
    *reinterpret_cast<u32 *>(object + 0x14) =
        reinterpret_cast<u32>(cursor);

    WriteFloat(object, 0, position[0]);
    WriteFloat(object, 4, position[1]);
    WriteFloat(object, 8, position[2]);
    strncpy(reinterpret_cast<char *>(object + 0x1c), text, 0x40);
    WriteInt(object, 0x64, 0);
    WriteInt(object, 0x68, static_cast<i32>(g_TextStyleFlag));
    WriteInt(object, 0x60,
             g_TextStyleFlag != 0 ? g_TextStyleAlt : g_TextStyleDefault);
    WriteInt(object, 0x70, 0x1e);
    return object;
}

// TH10 0x0041beb0. The block is just {vel_x, vel_y} — one FSINCOS.
void InitMovementBlock(void *block_memory, float angle, float speed)
{
    u8 *const block = static_cast<u8 *>(block_memory);
    WriteFloat(block, 0, static_cast<float>(
        std::cos(static_cast<double>(angle)) *
        static_cast<double>(speed)));
    WriteFloat(block, 4, static_cast<float>(
        std::sin(static_cast<double>(angle)) *
        static_cast<double>(speed)));
}

} // namespace th10
