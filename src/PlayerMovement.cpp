#include "PlayerMovement.hpp"

#include "EntityHelpers.hpp"
#include "PlayerOptionCallbacks.hpp"

#include <cmath>

namespace th10 {

// TH10 0x004250b0. Input decode, focus gating, fixed-point integration with
// clamping, the option-position history ring, and the four option record
// updates. Native input is EDI = player; the direction-animation helper and
// entity-position helpers keep their native register ABIs as boundaries.

namespace {

extern u32 g_PlayerCharacter; // TH10 DAT_00474c68
extern u32 g_PlayerShotType; // TH10 DAT_00474c6c
extern u8 g_InputMask; // TH10 DAT_00474e5c (b2 focus, b4 L, b5 R, b6 U, b7 D)
extern float g_FrameTimeScale; // TH10 DAT_00476f78
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern void *g_AsciiHudConditionalState; // TH10 DAT_00477704

// TH10 0x0043e710: native ECX = owner (player+0x10), EAX = sub-VM
// (player+0x14), EBX = animation id.
void SpawnDirectionAnimEcxEaxBbxAbi(void *owner, void *vm, i32 anim_id);

// The callbacks are thiscalls with ECX = record; the fastcall function-type
// typedef models that contract for both supported compilers.
#if defined(_MSC_VER)
typedef i32 (__fastcall OptionRecordFn)(void *record);
#else
typedef i32 __attribute__((fastcall)) OptionRecordFnT(void *record);
typedef OptionRecordFnT *OptionRecordFn;
#endif

// TH10 0x00463b2c: round half away from zero.
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

i32 UpdatePlayerMovementEdiAbi(void *player_memory)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    const u8 input = g_InputMask;

    // Diagonal masks require both bits and win over the cardinals.
    i32 state = 0;
    if ((input & 0x50) == 0x50)
        state = 5;
    else if ((input & 0x60) == 0x60)
        state = 7;
    else if ((input & 0x90) == 0x90)
        state = 6;
    else if ((input & 0xa0) == 0xa0)
        state = 8;
    else if ((input & 0x20) != 0)
        state = 2;
    else if ((input & 0x10) != 0)
        state = 1;
    else if ((input & 0x40) != 0)
        state = 3;
    else if ((input & 0x80) != 0)
        state = 4;
    WriteInt(player, 0x454, state);

    if (g_AsciiHudConditionalState == 0 ||
        *reinterpret_cast<const i32 *>(
            static_cast<u8 *>(g_AsciiHudConditionalState) + 0x60) == 0 ||
        ReadInt(player, 0x48c) < 4) {
        WriteInt(player, 0x4474, 0);
        WriteInt(player, 0x4308, 30);
    } else {
        WriteInt(player, 0x4474, (input >> 2) & 1);
        if (static_cast<i32>(g_PlayerShotType + g_PlayerCharacter * 3) == 5) {
            if ((input & 4) != 0)
                WriteInt(player, 0x4308, 0);
            else if (ReadInt(player, 0x4308) < 30)
                WriteInt(player, 0x4308, ReadInt(player, 0x4308) + 1);
        }
    }

    const bool focused = ReadInt(player, 0x4474) != 0;
    i32 dx = 0;
    i32 dy = 0;
    if (!focused) {
        const u32 tracked = ReadInt(player, 0x329c);
        if (tracked != 0) {
            if (FindEntityEdxStackAbi(g_MainChainRenderOwner, tracked) != 0)
                StopEntityById(g_MainChainRenderOwner, tracked);
            WriteInt(player, 0x329c, 0);
        }
        switch (state) {
        case 1: dx = -ReadInt(player, 0x3d4); break;
        case 2: dx = ReadInt(player, 0x3d4); break;
        case 3: dy = -ReadInt(player, 0x3d4); break;
        case 4: dy = ReadInt(player, 0x3d4); break;
        case 5: dx = -ReadInt(player, 0x3dc); dy = dx; break;
        case 6: dx = ReadInt(player, 0x3dc); dy = dx; break;
        case 7: dx = -ReadInt(player, 0x3dc); dy = -dx; break;
        case 8: dx = ReadInt(player, 0x3dc); dy = dx; break;
        default: break;
        }
        const i32 previous_dx = ReadInt(player, 0x44c);
        if (dx < 0 && previous_dx >= 0)
            SpawnDirectionAnimEcxEaxBbxAbi(
                *reinterpret_cast<void *const *>(player + 0x10),
                player + 0x14, 1);
        if (dx > 0 && previous_dx <= 0)
            SpawnDirectionAnimEcxEaxBbxAbi(
                *reinterpret_cast<void *const *>(player + 0x10),
                player + 0x14, 3);
        if (dx == 0) {
            if (previous_dx < 0)
                SpawnDirectionAnimEcxEaxBbxAbi(
                    *reinterpret_cast<void *const *>(player + 0x10),
                    player + 0x14, 2);
            if (previous_dx > 0)
                SpawnDirectionAnimEcxEaxBbxAbi(
                    *reinterpret_cast<void *const *>(player + 0x10),
                    player + 0x14, 4);
        }
    } else {
        if (ReadInt(player, 0x329c) == 0) {
            void *const vm = AllocatePoolVmEsiAbi(g_MainChainRenderOwner);
            *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x20) = 9;
            *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x35c) |=
                0x40000000U;
            AssignPoolVmScriptEcxEaxAbi(vm, 0x160);
            u32 id = 0;
            LinkEntityAndAssignIdEaxEsiAbi(&id, vm);
            WriteInt(player, 0x329c, static_cast<i32>(id));
        }
        switch (state) {
        case 1: dx = -ReadInt(player, 0x3d8); break;
        case 2: dx = ReadInt(player, 0x3d8); break;
        case 3: dy = -ReadInt(player, 0x3d8); break;
        case 4: dy = ReadInt(player, 0x3d8); break;
        case 5: dx = -ReadInt(player, 0x3e0); dy = dx; break;
        case 6: dx = ReadInt(player, 0x3e0); dy = dx; break;
        case 7: dx = -ReadInt(player, 0x3e0); dy = -dx; break;
        case 8: dx = ReadInt(player, 0x3e0); dy = dx; break;
        default: break;
        }
    }

    WriteInt(player, 0x44c, dx);
    WriteInt(player, 0x450, dy);
    const i32 step_x = FloatToI32(static_cast<float>(dx) * g_FrameTimeScale);
    const i32 step_y = FloatToI32(static_cast<float>(dy) * g_FrameTimeScale);
    WriteInt(player, 0x3f0, step_x);
    WriteInt(player, 0x3f4, step_y);
    WriteInt(player, 0x3cc, ReadInt(player, 0x3cc) + step_x);
    WriteInt(player, 0x3d0, ReadInt(player, 0x3d0) + step_y);

    if (ReadInt(player, 0x3cc) < -0x47e0)
        WriteInt(player, 0x3cc, -0x47e0);
    else if (ReadInt(player, 0x3cc) > 0x47e0)
        WriteInt(player, 0x3cc, 0x47e0);
    if (ReadInt(player, 0x3d0) < 0xc80)
        WriteInt(player, 0x3d0, 0xc80);
    else if (ReadInt(player, 0x3d0) > 0xa8c0)
        WriteInt(player, 0x3d0, 0xa8c0);

    const float position_x =
        static_cast<float>(ReadInt(player, 0x3cc)) * 0.01f;
    const float position_y =
        static_cast<float>(ReadInt(player, 0x3d0)) * 0.01f;
    *reinterpret_cast<float *>(player + 0x3c0) = position_x;
    *reinterpret_cast<float *>(player + 0x3c4) = position_y;

    const u32 tracked = ReadInt(player, 0x329c);
    if (tracked != 0) {
        if (FindEntityEdxStackAbi(g_MainChainRenderOwner, tracked) != 0) {
            const float target[3] = {position_x + 224.0f,
                                     position_y + 16.0f,
                                     *reinterpret_cast<const float *>(
                                         player + 0x3c8)};
            SetEntityPositionDirectEsiAbi(g_MainChainRenderOwner, tracked,
                                          target);
        } else {
            WriteInt(player, 0x329c, 0);
        }
    }

    // History ring: 32 dword pairs; the valid window is optionCount*8 pairs
    // and the tail is saturated to its oldest entry. Only shifts while
    // unfocused and moving; the head write runs every frame.
    const i32 count = ReadInt(player, 0x3500);
    u32 *const history = reinterpret_cast<u32 *>(player + 0x436c);
    if (!focused && (dx != 0 || dy != 0)) {
        const i32 valid = count * 8;
        if (valid < 32) {
            for (i32 index = 32; index > valid; --index) {
                history[index * 2 - 2] = history[valid * 2];
                history[index * 2 - 1] = history[valid * 2 + 1];
            }
            for (i32 index = valid; index > 1; --index) {
                history[index * 2 - 2] = history[index * 2 - 4];
                history[index * 2 - 1] = history[index * 2 - 3];
            }
        } else {
            for (i32 index = 32; index > 1; --index) {
                history[index * 2 - 2] = history[index * 2 - 4];
                history[index * 2 - 1] = history[index * 2 - 3];
            }
        }
    }
    history[0] = *reinterpret_cast<const u32 *>(player + 0x3cc);
    history[1] = *reinterpret_cast<const u32 *>(player + 0x3d0);

    const i32 lerp_percent = ReadInt(player, 0x4308);
    for (u32 index = 0; index != 4; ++index) {
        u8 *const record = player + 0x32a0 + index * 0x98;
        if (ReadInt(record, 0) == 0)
            continue;
        const u32 offset_base = focused ? 0x4c : 0x44;
        WriteInt(record, 0x34,
                 ReadInt(player, 0x3cc) + ReadInt(record, offset_base));
        WriteInt(record, 0x38,
                 ReadInt(player, 0x3d0) + ReadInt(record, offset_base + 4));

        OptionRecordFn *const callback =
            *reinterpret_cast<OptionRecordFn *const *>(record + 0x90);
        if (*reinterpret_cast<void *const *>(record + 0x90) != 0)
            (void)(*callback)(record);

        if (ReadInt(record, 0x8c) != 0) {
            WriteInt(record, 0x8c, 0);
            WriteInt(record, 0x3c, ReadInt(record, 0x34));
            WriteInt(record, 0x40, ReadInt(record, 0x38));
        } else if (lerp_percent > 29) {
            const i32 delta_x = (ReadInt(record, 0x34) -
                                 ReadInt(record, 0x3c)) * lerp_percent / 100;
            const i32 delta_y = (ReadInt(record, 0x38) -
                                 ReadInt(record, 0x40)) * lerp_percent / 100;
            if (delta_x != 0 || delta_y != 0) {
                WriteInt(record, 0x3c, ReadInt(record, 0x3c) + delta_x);
                WriteInt(record, 0x40, ReadInt(record, 0x40) + delta_y);
            } else {
                WriteInt(record, 0x3c, ReadInt(record, 0x34));
                WriteInt(record, 0x40, ReadInt(record, 0x38));
            }
        }

        const float entity_position[3] = {
            static_cast<float>(ReadInt(record, 0x3c)) * 0.01f,
            static_cast<float>(ReadInt(record, 0x40)) * 0.01f, 0.0f};
        SetEntityPositionOffsetEsiAbi(g_MainChainRenderOwner,
                                      ReadInt(record, 0x68),
                                      entity_position);
        SetEntityPositionOffsetEsiAbi(g_MainChainRenderOwner,
                                      ReadInt(record, 0x6c),
                                      entity_position);
    }
    return 0;
}

} // namespace th10
