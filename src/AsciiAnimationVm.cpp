#include "AsciiAnimationVm.hpp"

#include <string.h>

namespace th10 {

void ResetAsciiAnimationVmRecord(void *record_memory)
{
    u8 *const record = static_cast<u8 *>(record_memory);
    const u32 saved_0020 = *reinterpret_cast<u32 *>(record + 0x20);
    const u32 saved_0340 = *reinterpret_cast<u32 *>(record + 0x340);
    const u32 saved_0344 = *reinterpret_cast<u32 *>(record + 0x344);
    const u32 saved_0348 = *reinterpret_cast<u32 *>(record + 0x348);

    memset(record, 0, 0x3ac);

    *reinterpret_cast<u32 *>(record + 0x20) = saved_0020;
    *reinterpret_cast<u32 *>(record + 0x340) = saved_0340;
    *reinterpret_cast<u32 *>(record + 0x344) = saved_0344;
    *reinterpret_cast<u32 *>(record + 0x348) = saved_0348;
    *reinterpret_cast<u32 *>(record + 0x2fc) = 0xffffffffU;
    *reinterpret_cast<float *>(record + 0x3c) = 1.0f;
    *reinterpret_cast<float *>(record + 0x40) = 1.0f;
    *reinterpret_cast<float *>(record + 0x23c) = 1.0f;
    *reinterpret_cast<float *>(record + 0x250) = 1.0f;
    *reinterpret_cast<float *>(record + 0x264) = 1.0f;
    *reinterpret_cast<float *>(record + 0x278) = 1.0f;
    *reinterpret_cast<u16 *>(record + 0x35c) = 7;
    *reinterpret_cast<u32 *>(record + 0x5c) = 0xfff0bdc1U;
    *reinterpret_cast<void **>(record + 0x4) = record;
    *reinterpret_cast<void **>(record + 0x10) = record;
}

i32 InitializeAsciiAnimationVmEntry(void *vm_memory, u32 entry_index,
                                    void *resource_memory)
{
    u8 *const vm = static_cast<u8 *>(vm_memory);
    u8 *const resource = static_cast<u8 *>(resource_memory);
    if (*reinterpret_cast<void **>(resource + 0x108) == 0 ||
        *reinterpret_cast<u32 *>(resource + 0x124) != 0) {
        return -1;
    }

    u8 *const entry = *reinterpret_cast<u8 **>(resource + 0x118) +
        entry_index * 0x44U;
    *reinterpret_cast<u16 *>(vm + 0x384) = static_cast<u16>(entry_index);
    *reinterpret_cast<void **>(vm + 0x308) = resource;
    *reinterpret_cast<void **>(vm + 0x394) = entry;
    *reinterpret_cast<u32 *>(vm + 0x4c) = *reinterpret_cast<u32 *>(entry + 0x34);
    *reinterpret_cast<u32 *>(vm + 0x50) = *reinterpret_cast<u32 *>(entry + 0x30);

    static const u32 first_zeroes[] = {
        0x240, 0x244, 0x248, 0x24c, 0x254, 0x258, 0x25c, 0x260,
        0x268, 0x26c, 0x270, 0x274
    };
    for (u32 index = 0; index != sizeof(first_zeroes) / sizeof(first_zeroes[0]);
         ++index) {
        *reinterpret_cast<u32 *>(vm + first_zeroes[index]) = 0;
    }
    *reinterpret_cast<float *>(vm + 0x23c) =
        *reinterpret_cast<float *>(vm + 0x4c) / 256.0f;
    *reinterpret_cast<float *>(vm + 0x250) =
        *reinterpret_cast<float *>(vm + 0x50) / 256.0f;
    *reinterpret_cast<float *>(vm + 0x264) = 1.0f;
    *reinterpret_cast<float *>(vm + 0x278) = 1.0f;
    memcpy(vm + 0x27c, vm + 0x23c, 0x40);

    static const u32 second_zeroes[] = {
        0x2c0, 0x2c4, 0x2c8, 0x2cc, 0x2d4, 0x2d8, 0x2dc, 0x2e0,
        0x2e8, 0x2ec, 0x2f0, 0x2f4
    };
    for (u32 index = 0; index != sizeof(second_zeroes) / sizeof(second_zeroes[0]);
         ++index) {
        *reinterpret_cast<u32 *>(vm + second_zeroes[index]) = 0;
    }
    *reinterpret_cast<float *>(vm + 0x2e4) = 1.0f;
    *reinterpret_cast<float *>(vm + 0x2f8) = 1.0f;
    *reinterpret_cast<float *>(vm + 0x2bc) =
        *reinterpret_cast<float *>(entry + 0x38) /
        *reinterpret_cast<float *>(entry + 0x1c) *
        *reinterpret_cast<float *>(vm + 0x4c);
    *reinterpret_cast<float *>(vm + 0x2d0) =
        *reinterpret_cast<float *>(entry + 0x3c) /
        *reinterpret_cast<float *>(entry + 0x18) *
        *reinterpret_cast<float *>(vm + 0x50);
    return 0;
}

} // namespace th10
