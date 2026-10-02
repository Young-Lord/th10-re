#include "PlayerRecordHelpers.hpp"

#include "PlayerRecord.hpp"

#include <string.h>

namespace th10 {

namespace {

// TH10 0x00449470: SetEntityStateWordByHandleSlot (EAX = slot
// pointer, ESI = mode). Splits the player motion across the focused /
// unfocused states.
void SetEntityStateWordByHandleSlotNative(void *slot, i32 mode);

extern void *g_PlayerRecord;   // TH10 DAT_00477834

const float kHomingScale = 0.01f; // TH10 DAT_00470b00

} // namespace

i32 WritePlayerQuadSlotValueEaxEcxAbi(i32 value, void *player)
{
    u8 *bytes = static_cast<u8 *>(player);
    *reinterpret_cast<i32 *>(bytes + 0x334c) = value;
    *reinterpret_cast<i32 *>(bytes + 0x33d4) = value;
    *reinterpret_cast<i32 *>(bytes + 0x345c) = value;
    *reinterpret_cast<i32 *>(bytes + 0x34e4) = value;
    return value;
}

void *ResetPlayerRecordEsiAbi(void *player)
{
    u8 *bytes = static_cast<u8 *>(player);
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(bytes);
    // Player-relative VmRecord anim-block flag table; kept as a raw offset
    // walk on purpose (the offsets straddle the embedded anim_vm fields).
    static const u32 kFlagOffsets[] = {
        0x80U, 0xc4U, 0x110U, 0x13cU, 0x188U, 0x1c4U, 0x210U, 0x23cU,
        0x38cU
    };
    for (u32 index = 0; index != 9U; ++index)
        *reinterpret_cast<u32 *>(bytes + kFlagOffsets[index]) &= ~1U;
    memset(&player_rec.anim_vm, 0, 0x3ac);
    *reinterpret_cast<u16 *>(&player_rec.anim_vm.sprite_entry_id) = 0xffffU;
    player_rec.autocollect_timer.flags &= ~1U;
    player_rec.frame_timer.flags &= ~1U;
    player_rec.move_gate_timer.flags &= ~1U;
    for (i32 i = 0; i < 128; ++i)
        player_rec.shots[i].timer_latch &= ~1U;
    player_rec.options[0].flag_0080 &= ~1U;
    player_rec.options[1].flag_0080 &= ~1U;
    player_rec.options[2].flag_0080 &= ~1U;
    player_rec.options[3].flag_0080 &= ~1U;
    // 33 entries: the native walk runs one record past the named array,
    // into the padding at player+0x42e0.
    for (i32 i = 0; i < 33; ++i)
        player_rec.sub_effects[i].timer_latch &= ~1U;
    player_rec.deathbomb_timer.flags &= ~1U;
    memset(bytes, 0, 0x4478);
    g_PlayerRecord = bytes;
    return bytes;
}

void UpdatePlayerFocusMotionEdiAbi(void *entity)
{
    PlayerOptionRecord &record =
        *reinterpret_cast<PlayerOptionRecord *>(entity);
    PlayerRecord &player_rec =
        *reinterpret_cast<PlayerRecord *>(g_PlayerRecord);
    // The option position fields carry x100 fixed-point dwords; the dword
    // view is kept on every access.
    const u32 focused = player_rec.focus_flag;
    const u32 previous = record.focus_latch;
    if (focused != 0U) {
        if (previous == 0U)
            SetEntityStateWordByHandleSlotNative(&record.sprite_entity_id, 6);
        *reinterpret_cast<u32 *>(&record.offset_source_b[0]) =
            *reinterpret_cast<const u32 *>(&record.render_position[0]);
        *reinterpret_cast<u32 *>(&record.offset_source_b[1]) =
            *reinterpret_cast<const u32 *>(&record.render_position[1]);
    } else {
        if (previous == 0U)
            SetEntityStateWordByHandleSlotNative(&record.sprite_entity_id, 3);
        *reinterpret_cast<u32 *>(&record.unfocused_position[0]) =
            *reinterpret_cast<const u32 *>(&record.offset_source_b[0]);
        *reinterpret_cast<u32 *>(&record.unfocused_position[1]) =
            *reinterpret_cast<const u32 *>(&record.offset_source_b[1]);
    }
    record.focus_latch = focused;
}

double FloorfToDoubleStackAbi(float value)
{
    // The native widens the float into a stack qword and calls the
    // CRT floor; the widening itself is exact, so the direct
    // conversion matches the binary's observable result.
    return static_cast<double>(value);
}

void SubtractVec2EaxEcxDxAbi(float out[2], const float a[2],
                             const float b[2])
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
}

void AddVec2EaxDxEsiAbi(float out[2], const float a[2], const float b[2])
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
}

void CopyVec2ToDualOutputsEaxEcxDxAbi(float out1[2], float out2[2],
                                      const float src[2])
{
    out2[0] = src[0];
    out2[1] = src[1];
    out1[0] = src[0];
    out1[1] = out2[1];
}

i32 SetHomingSlotAnchorEaxEcxDxStackAbi(i32 unused, void *table,
                                        void *context, i32 dead_argument)
{
    (void)unused;
    (void)dead_argument; // Native never reads the stack argument.
    u8 *context_bytes = static_cast<u8 *>(context);
    u8 *entity = *reinterpret_cast<u8 **>(context_bytes + 0x58);
    const i32 slot = static_cast<i32>(
        static_cast<signed char>(*reinterpret_cast<const signed char *>(
            entity + 0x1c)));
    u8 *entry = static_cast<u8 *>(table) +
                static_cast<u32>(slot) * 0x98U + 0x3244U;
    const float scaled_a =
        static_cast<float>(*reinterpret_cast<const i32 *>(entry)) *
        kHomingScale;
    const float scaled_b =
        static_cast<float>(*reinterpret_cast<const i32 *>(entry + 4)) *
        kHomingScale;
    *reinterpret_cast<float *>(context_bytes + 0x14) = scaled_a;
    *reinterpret_cast<float *>(context_bytes + 0x18) = scaled_b;
    *reinterpret_cast<u32 *>(context_bytes + 0x1c) = 0;
    // Native quirk: the raw int at entry+0 is re-read as a float
    // after the earlier 0.01-scaled read.
    *reinterpret_cast<float *>(entry) =
        *reinterpret_cast<const float *>(entity + 4) -
        *reinterpret_cast<const float *>(context_bytes + 0x20) +
        static_cast<float>(*reinterpret_cast<const i32 *>(entry));
    *reinterpret_cast<float *>(context_bytes + 0x18) =
        *reinterpret_cast<const float *>(entity + 8) -
        *reinterpret_cast<const float *>(context_bytes + 0x24) +
        *reinterpret_cast<const float *>(context_bytes + 0x18);
    return 0;
}

} // namespace th10
