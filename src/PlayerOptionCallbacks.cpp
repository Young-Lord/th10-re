#include "PlayerOptionCallbacks.hpp"

#include "PlayerRecord.hpp"

#include <cmath>

namespace th10 {

// The two Reimu option-record update callbacks. Both are thiscalls on the
// record (ECX) invoked from the movement function 0x4250b0; the player
// pointer comes from the global at 0x477834, not from the record. Neither
// fires attacks: the homing callback maintains the focused trail
// interpolation and the unfocused spread offset, the angular callback
// latches the orbit anchor and switches the option sprite kind.

namespace {

extern u8 *g_OptionPositionBase; // TH10 DAT_00477834 (player pointer)
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

// TH10 0x449470 (EAX = &idSlot, ESI = value): resolve the entity id and set
// the u16 state word at ent+0x304 (and on all children when ent+0x18 == 0).
void SetEntityStateWordEaxEsiAbi(u32 *id_slot, i32 value);

// TH10 0x00463b2c: x87 conversion, round half away from zero.
i32 FloatToI32(float value)
{
    return value >= 0.0f
        ? static_cast<i32>(std::floor(static_cast<double>(value) + 0.5))
        : static_cast<i32>(std::ceil(static_cast<double>(value) - 0.5));
}

} // namespace

// TH10 0x00427950 (inner 0x00427960).
i32 TH10_FASTCALL UpdateHomingOptionRecord(void *record_memory)
{
    PlayerOptionRecord &record =
        *reinterpret_cast<PlayerOptionRecord *>(record_memory);
    PlayerRecord &player_rec =
        *reinterpret_cast<PlayerRecord *>(g_OptionPositionBase);
    // The option position fields carry x100 fixed-point dwords; the dword
    // view is kept on every access.
    const i32 mode = static_cast<i32>(player_rec.focus_flag);
    const u32 index = record.option_index;

    *reinterpret_cast<i32 *>(&record.unfocused_position[0]) =
        static_cast<i32>(player_rec.trail_history[index * 16]);
    *reinterpret_cast<i32 *>(&record.unfocused_position[1]) =
        static_cast<i32>(player_rec.trail_history[index * 16 + 1]);

    if (mode == 0) {
        // Unfocused: freeze the spread offset relative to the player.
        *reinterpret_cast<i32 *>(&record.offset_source_a[0]) =
            *reinterpret_cast<const i32 *>(&record.unfocused_position[0]) -
            player_rec.position_x_fixed;
        *reinterpret_cast<i32 *>(&record.offset_source_a[1]) =
            *reinterpret_cast<const i32 *>(&record.unfocused_position[1]) -
            player_rec.position_y_fixed;
    } else {
        // Focused: pin the segment head to the player plus the frozen
        // offset, then fill the eight trail slots at 1/8 steps toward the
        // entry one segment ahead (0x4778ac family read). The x87 constant
        // 0.125 lives at 0x00470b90.
        player_rec.trail_history[index * 16] = static_cast<u32>(
            player_rec.position_x_fixed +
            *reinterpret_cast<const i32 *>(&record.offset_source_a[0]));
        player_rec.trail_history[index * 16 + 1] = static_cast<u32>(
            player_rec.position_y_fixed +
            *reinterpret_cast<const i32 *>(&record.offset_source_a[1]));
        const i32 head_x =
            static_cast<i32>(player_rec.trail_history[index * 16]);
        const i32 head_y =
            static_cast<i32>(player_rec.trail_history[index * 16 + 1]);
        const i32 tail_x =
            static_cast<i32>(player_rec.trail_history[(index + 1) * 16]);
        const i32 tail_y =
            static_cast<i32>(player_rec.trail_history[(index + 1) * 16 + 1]);
        const i32 dx = tail_x - head_x;
        const i32 dy = tail_y - head_y;
        for (i32 k = 1; k <= 8; ++k) {
            player_rec.trail_history[index * 16 + k * 2] = static_cast<u32>(
                FloatToI32(static_cast<float>(dx * k) * 0.125f) + head_x);
            player_rec.trail_history[index * 16 + k * 2 + 1] =
                static_cast<u32>(
                    FloatToI32(static_cast<float>(dy * k) * 0.125f) +
                    head_y);
        }
    }

    *reinterpret_cast<i32 *>(&record.unfocused_position[0]) =
        *reinterpret_cast<const i32 *>(&record.offset_source_a[0]) +
        player_rec.position_x_fixed;
    *reinterpret_cast<i32 *>(&record.unfocused_position[1]) =
        *reinterpret_cast<const i32 *>(&record.offset_source_a[1]) +
        player_rec.position_y_fixed;
    record.focus_latch = static_cast<u32>(mode);
    return 0;
}

// TH10 0x00427ad0 (inner 0x00427ae0).
i32 TH10_FASTCALL UpdateAngularOptionRecord(void *record_memory)
{
    PlayerOptionRecord &record =
        *reinterpret_cast<PlayerOptionRecord *>(record_memory);
    PlayerRecord &player_rec =
        *reinterpret_cast<PlayerRecord *>(g_OptionPositionBase);
    // The option position fields carry x100 fixed-point dwords; the dword
    // view is kept on every access.
    const i32 mode = static_cast<i32>(player_rec.focus_flag);
    const i32 previous_mode = static_cast<i32>(record.focus_latch);

    if (mode == 0) {
        if (previous_mode != 0)
            SetEntityStateWordEaxEsiAbi(&record.sprite_entity_id, 6);
        *reinterpret_cast<i32 *>(&record.offset_source_b[0]) =
            *reinterpret_cast<const i32 *>(&record.render_position[0]);
        *reinterpret_cast<i32 *>(&record.offset_source_b[1]) =
            *reinterpret_cast<const i32 *>(&record.render_position[1]);
    } else {
        if (previous_mode == 0)
            SetEntityStateWordEaxEsiAbi(&record.sprite_entity_id, 3);
        *reinterpret_cast<i32 *>(&record.unfocused_position[0]) =
            *reinterpret_cast<const i32 *>(&record.offset_source_b[0]);
        *reinterpret_cast<i32 *>(&record.unfocused_position[1]) =
            *reinterpret_cast<const i32 *>(&record.offset_source_b[1]);
    }
    record.focus_latch = static_cast<u32>(mode);
    return 0;
}

} // namespace th10
