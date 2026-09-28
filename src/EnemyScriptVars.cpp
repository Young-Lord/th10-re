// TH10 enemy-script variable accessors used by the ECL instruction layer
// (0x0043ec70 read / 0x0043eda0 write-slot resolution).
//
// Variable ids 10000-10009 map onto the enemy record:
//   10000-10003 ints   at +0x30c/+0x310/+0x314/+0x318
//   10004-10007 floats at +0x31c/+0x320/+0x324/+0x328 (truncated to int via
//              the shared _ftol2 conversion 0x00463b2c)
//   10008/10009 dwords at +0x32c/+0x330
// The writer resolves the slot address for the same id family (0x2710..13
// and 0x2718/0x2719), gated by a slot write mask.
#include "Th10Types.hpp"
#include "Th10Platform.hpp"

namespace th10 {

// TH10 0x0043ec70. Native EDX:EAX = id (low dword used), ECX = enemy.
// Returns the value (64-bit through EDX:EAX; the int slots only define the
// low dword and the native leaves EDX untouched in those paths).
long long ReadEnemyScriptVarEdxEcxAbi(u32 var_id, void *enemy)
{
    const u8 *record = static_cast<const u8 *>(enemy);
    const u32 index = var_id - 10000U;
    switch (index) {
    case 0:
        return *reinterpret_cast<const i32 *>(record + 0x30cU);
    case 1:
        return *reinterpret_cast<const i32 *>(record + 0x310U);
    case 2:
        return *reinterpret_cast<const i32 *>(record + 0x314U);
    case 3:
        return *reinterpret_cast<const i32 *>(record + 0x318U);
    case 4: {
        // fld dword [+0x31c]; jmp _ftol2 (0x00463b2c, registered boundary).
        const float value =
            *reinterpret_cast<const float *>(record + 0x31cU);
        return static_cast<long long>(value);
    }
    case 5: {
        const float value =
            *reinterpret_cast<const float *>(record + 0x320U);
        return static_cast<long long>(value);
    }
    case 6: {
        const float value =
            *reinterpret_cast<const float *>(record + 0x324U);
        return static_cast<long long>(value);
    }
    case 7: {
        const float value =
            *reinterpret_cast<const float *>(record + 0x328U);
        return static_cast<long long>(value);
    }
    case 8:
        return *reinterpret_cast<const u32 *>(record + 0x32cU);
    case 9:
        return *reinterpret_cast<const u32 *>(record + 0x330U);
    default:
        return static_cast<long long>(var_id);
    }
}

// TH10 0x0043eda0. Native EAX = pointer to the current id value, EDX =
// enemy, CL = slot index, stack = 16-bit slot mask. When the slot bit is
// set, resolves the target address for the id family 10000-10003 and
// 10008-10009 (0x2718/0x2719); otherwise returns the id pointer unchanged.
u32 *ResolveEnemyScriptVarSlotEaxAbi(u32 *id_slot, void *enemy, u8 slot,
                                     u16 slot_mask)
{
    if (((1U << slot) & slot_mask) == 0U)
        return id_slot;

    const u8 *record = static_cast<const u8 *>(enemy);
    switch (*id_slot) {
    case 0x2710U:
        return reinterpret_cast<u32 *>(
            const_cast<u8 *>(record) + 4U * 195U);
    case 0x2711U:
        return reinterpret_cast<u32 *>(
            const_cast<u8 *>(record) + 4U * 196U);
    case 0x2712U:
        return reinterpret_cast<u32 *>(
            const_cast<u8 *>(record) + 4U * 197U);
    case 0x2713U:
        return reinterpret_cast<u32 *>(
            const_cast<u8 *>(record) + 4U * 198U);
    case 0x2718U:
        return reinterpret_cast<u32 *>(
            const_cast<u8 *>(record) + 4U * 203U);
    case 0x2719U:
        return reinterpret_cast<u32 *>(
            const_cast<u8 *>(record) + 4U * 204U);
    default:
        return id_slot;
    }
}

} // namespace th10
