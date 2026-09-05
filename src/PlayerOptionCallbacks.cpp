#include "PlayerOptionCallbacks.hpp"

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

inline i32 ReadInt(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline void WriteInt(u8 *bytes, u32 offset, i32 value)
{
    *reinterpret_cast<i32 *>(bytes + offset) = value;
}

} // namespace

// TH10 0x00427950 (inner 0x00427960).
i32 TH10_FASTCALL UpdateHomingOptionRecord(void *record_memory)
{
    u8 *const record = static_cast<u8 *>(record_memory);
    u8 *const player = g_OptionPositionBase;
    const i32 mode = ReadInt(player, 0x4474);
    const u32 index = static_cast<u32>(ReadInt(record, 0x88));
    u8 *const history = player + 0x436c;

    WriteInt(record, 0x34,
             ReadInt(history + index * 0x40, 0));
    WriteInt(record, 0x38,
             ReadInt(history + index * 0x40, 4));

    if (mode == 0) {
        // Unfocused: freeze the spread offset relative to the player.
        WriteInt(record, 0x44,
                 ReadInt(record, 0x34) - ReadInt(player, 0x3cc));
        WriteInt(record, 0x48,
                 ReadInt(record, 0x38) - ReadInt(player, 0x3d0));
    } else {
        // Focused: pin the segment head to the player plus the frozen
        // offset, then fill the eight trail slots at 1/8 steps toward the
        // entry one segment ahead (0x4778ac family read). The x87 constant
        // 0.125 lives at 0x00470b90.
        WriteInt(history + index * 0x40, 0,
                 ReadInt(player, 0x3cc) + ReadInt(record, 0x44));
        WriteInt(history + index * 0x40, 4,
                 ReadInt(player, 0x3d0) + ReadInt(record, 0x48));
        const i32 head_x = ReadInt(history + index * 0x40, 0);
        const i32 head_y = ReadInt(history + index * 0x40, 4);
        const i32 tail_x =
            ReadInt(history + index * 0x40 + 0x40, 0);
        const i32 tail_y =
            ReadInt(history + index * 0x40 + 0x40, 4);
        const i32 dx = tail_x - head_x;
        const i32 dy = tail_y - head_y;
        for (i32 k = 1; k <= 8; ++k) {
            WriteInt(history + index * 0x40 + k * 8, 0,
                     FloatToI32(static_cast<float>(dx * k) * 0.125f) +
                         head_x);
            WriteInt(history + index * 0x40 + k * 8, 4,
                     FloatToI32(static_cast<float>(dy * k) * 0.125f) +
                         head_y);
        }
    }

    WriteInt(record, 0x34,
             ReadInt(record, 0x44) + ReadInt(player, 0x3cc));
    WriteInt(record, 0x38,
             ReadInt(record, 0x48) + ReadInt(player, 0x3d0));
    WriteInt(record, 0x84, mode);
    return 0;
}

// TH10 0x00427ad0 (inner 0x00427ae0).
i32 TH10_FASTCALL UpdateAngularOptionRecord(void *record_memory)
{
    u8 *const record = static_cast<u8 *>(record_memory);
    u8 *const player = g_OptionPositionBase;
    const i32 mode = ReadInt(player, 0x4474);
    const i32 previous_mode = ReadInt(record, 0x84);

    if (mode == 0) {
        if (previous_mode != 0)
            SetEntityStateWordEaxEsiAbi(
                reinterpret_cast<u32 *>(record + 0x68), 6);
        WriteInt(record, 0x4c, ReadInt(record, 0x3c));
        WriteInt(record, 0x50, ReadInt(record, 0x40));
    } else {
        if (previous_mode == 0)
            SetEntityStateWordEaxEsiAbi(
                reinterpret_cast<u32 *>(record + 0x68), 3);
        WriteInt(record, 0x34, ReadInt(record, 0x4c));
        WriteInt(record, 0x38, ReadInt(record, 0x50));
    }
    WriteInt(record, 0x84, mode);
    return 0;
}

} // namespace th10
