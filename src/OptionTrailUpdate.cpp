// Option trail history update (0x00427960): the per-option slot refresh
// that feeds the homing option record (0x00427950 reads +0x34/+0x38).
#include "OptionTrailUpdate.hpp"

#include "Th10Types.hpp"

namespace th10 {

namespace {

// TH10 DAT_00477864: the player option root holding the slot table
// (+0x43cc per-slot position, +0x4370 per-slot trail history) and the
// trailing-mode live flag at +0x4474.
u8 *g_PlayerOptionRoot;

// TH10 0x463b2c (_ftol2): round half away from zero.
i32 FloatToI32(double value)
{
    return value >= 0.0 ? static_cast<i32>(value + 0.5)
                        : static_cast<i32>(value - 0.5);
}

} // namespace

// FUNCTION: TH10 0x00427960
void UpdateOptionTrailHistoryEsiAbi(void *option_memory)
{
    u32 *const option = static_cast<u32 *>(option_memory);
    u8 *const root = g_PlayerOptionRoot;

    const u32 trailing_flag = *reinterpret_cast<u32 *>(root + 17524);
    const u32 slot_offset = option[34] * 64U; // (option+0x88) << 6
    u32 *const slot_pair = reinterpret_cast<u32 *>(
        root + 17324 + slot_offset);

    option[13] = slot_pair[0];
    option[14] = slot_pair[1];

    if (trailing_flag != 0) {
        // Leading mode: publish the current delta into the slot head, then
        // zero-fill the seven following 8-byte trail entries. The native
        // computes each store through _ftol2 of the x87 register file;
        // on every reachable path those registers hold 0.0 here, so the
        // observable result is a zero fill.
        slot_pair[0] = option[17] + *reinterpret_cast<u32 *>(root + 972);
        slot_pair[1] = option[18] + *reinterpret_cast<u32 *>(root + 976);
        u32 *trail = reinterpret_cast<u32 *>(root + 17264 +
                                             8 * (option[34] * 8U + 1));
        for (u32 index = 0; index != 7U; ++index) {
            trail[0] = static_cast<u32>(FloatToI32(0.0));
            trail[1] = static_cast<u32>(FloatToI32(0.0));
            trail += 2;
        }
    } else {
        // Trailing mode: the option trails the table position, so the
        // stored delta is table - current.
        option[17] = slot_pair[0] - *reinterpret_cast<u32 *>(root + 972);
        option[18] = slot_pair[1] - *reinterpret_cast<u32 *>(root + 976);
    }

    option[13] = *reinterpret_cast<u32 *>(root + 972) + option[17];
    option[14] = *reinterpret_cast<u32 *>(root + 976) + option[18];
    option[33] = trailing_flag;
}

} // namespace th10
