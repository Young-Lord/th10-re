#include "AsciiAnimationVm.hpp"

#include "VmRecord.hpp"

#include <string.h>

namespace th10 {

void ResetAsciiAnimationVmRecord(void *record_memory)
{
    u8 *const record = static_cast<u8 *>(record_memory);
    VmRecord &vm = *reinterpret_cast<VmRecord *>(record);
    const u32 saved_0020 = vm.render_kind;
    const float saved_0340 = vm.delta_pos_x;
    const float saved_0344 = vm.delta_pos_y;
    const float saved_0348 = vm.delta_pos_z;

    memset(record, 0, 0x3ac);

    vm.render_kind = saved_0020;
    vm.delta_pos_x = saved_0340;
    vm.delta_pos_y = saved_0344;
    vm.delta_pos_z = saved_0348;
    vm.primary_color = 0xffffffffU;
    vm.scale_x = 1.0f;
    vm.scale_y = 1.0f;
    vm.base_matrix[0] = 1.0f;
    vm.base_matrix[5] = 1.0f;
    vm.base_matrix[10] = 1.0f;
    vm.base_matrix[15] = 1.0f;
    // u16-width store into the low half of the +0x35c flags dword.
    *reinterpret_cast<u16 *>(record + 0x35c) = 7;
    vm.timer_prev = static_cast<i32>(0xfff0bdc1U);
    vm.link_self = record;
    vm.child_head = record;
}

i32 InitializeAsciiAnimationVmEntry(void *vm_memory, u32 entry_index,
                                    void *resource_memory)
{
    u8 *const vm = static_cast<u8 *>(vm_memory);
    VmRecord &rec = *reinterpret_cast<VmRecord *>(vm);
    u8 *const resource = static_cast<u8 *>(resource_memory);
    if (*reinterpret_cast<void **>(resource + 0x108) == 0 ||
        *reinterpret_cast<u32 *>(resource + 0x124) != 0) {
        return -1;
    }

    u8 *const entry = *reinterpret_cast<u8 **>(resource + 0x118) +
        entry_index * 0x44U;
    rec.sprite_entry_id = static_cast<u16>(entry_index);
    rec.bound_resource = resource;
    rec.anim_entry = entry;
    rec.width = *reinterpret_cast<u32 *>(entry + 0x34);
    rec.height = *reinterpret_cast<u32 *>(entry + 0x30);

    static const u32 first_zeroes[] = {
        0x240, 0x244, 0x248, 0x24c, 0x254, 0x258, 0x25c, 0x260,
        0x268, 0x26c, 0x270, 0x274
    };
    for (u32 index = 0; index != sizeof(first_zeroes) / sizeof(first_zeroes[0]);
         ++index) {
        *reinterpret_cast<u32 *>(vm + first_zeroes[index]) = 0;
    }
    rec.base_matrix[0] =
        *reinterpret_cast<const float *>(vm + 0x4c) / 256.0f;
    rec.base_matrix[5] =
        *reinterpret_cast<const float *>(vm + 0x50) / 256.0f;
    rec.base_matrix[10] = 1.0f;
    rec.base_matrix[15] = 1.0f;
    memcpy(rec.world_matrix, rec.base_matrix, 0x40);

    static const u32 second_zeroes[] = {
        0x2c0, 0x2c4, 0x2c8, 0x2cc, 0x2d4, 0x2d8, 0x2dc, 0x2e0,
        0x2e8, 0x2ec, 0x2f0, 0x2f4
    };
    for (u32 index = 0; index != sizeof(second_zeroes) / sizeof(second_zeroes[0]);
         ++index) {
        *reinterpret_cast<u32 *>(vm + second_zeroes[index]) = 0;
    }
    rec.texture_matrix[10] = 1.0f;
    rec.texture_matrix[15] = 1.0f;
    rec.texture_matrix[0] =
        *reinterpret_cast<const float *>(entry + 0x38) /
        *reinterpret_cast<const float *>(entry + 0x1c) *
        *reinterpret_cast<const float *>(vm + 0x4c);
    rec.texture_matrix[5] =
        *reinterpret_cast<const float *>(entry + 0x3c) /
        *reinterpret_cast<const float *>(entry + 0x18) *
        *reinterpret_cast<const float *>(vm + 0x50);
    return 0;
}

} // namespace th10
