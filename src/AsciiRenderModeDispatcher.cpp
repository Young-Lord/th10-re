#include "AsciiRenderModeDispatcher.hpp"

#include "AsciiGlyphRenderer.hpp"
#include "AsciiMode8Renderer.hpp"
#include "AsciiMode9Renderer.hpp"
#include "AsciiProjectedRenderer.hpp"
#include "VmRecord.hpp"

namespace th10 {

i32 DispatchAsciiAnimationVmRenderMode(void *vm_memory, void *owner)
{
    u8 *const vm = static_cast<u8 *>(vm_memory);
    const VmRecord &vm_record = *reinterpret_cast<const VmRecord *>(vm);
    const u32 flags = vm_record.flags;
    if ((flags & 3U) != 3U || *(vm + 0x2ff) == 0)
        return -1;

    switch ((flags >> 22) & 0xfU) {
    case 0:
        DrawAsciiAnimationVmUnscaledToOwner(vm, owner);
        return 0;
    case 1:
        DrawAsciiAnimationVmRotatedMode1(vm, owner);
        return 0;
    case 2:
        DrawAsciiAnimationVmScaledToOwner(vm, owner);
        return 0;
    case 3:
        DrawAsciiAnimationVmRotatedMode3(vm, owner);
        return 0;
    case 4:
        return DrawAsciiAnimationVmProjectedMode4(vm, owner);
    case 5:
        return static_cast<i32>(DrawAsciiAnimationVmProjectedMode5(vm, owner));
    case 6:
        return DrawAsciiAnimationVmPerspectiveFadedMode6(vm, owner);
    case 7:
        return static_cast<i32>(DrawAsciiAnimationVmProjectedFoggedMode7(owner, vm));
    case 8:
        return DrawAsciiAnimationVmMode8(vm, owner);
    case 9: {
        const u32 vertex_count =
            static_cast<u32>(vm_record.reg_10000) * 2U;
        return DrawAsciiAnimationVmMode9(vm, owner,
            vm_record.vertex_buffer, vertex_count);
    }
    default:
        return 0;
    }
}

} // namespace th10
