#include "PlayerMovement.hpp"

#include "EntityHelpers.hpp"
#include "PlayerOptionCallbacks.hpp"
#include "PlayerRecord.hpp"

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

} // namespace

i32 UpdatePlayerMovementEdiAbi(void *player_memory)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
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
    player_rec.input_state = state;

    if (g_AsciiHudConditionalState == 0 ||
        *reinterpret_cast<const i32 *>(
            static_cast<u8 *>(g_AsciiHudConditionalState) + 0x60) == 0 ||
        player_rec.move_gate_timer.count < 4) {
        player_rec.focus_flag = 0;
        player_rec.deathbomb_lerp_percent = 30;
    } else {
        player_rec.focus_flag = (input >> 2) & 1;
        if (static_cast<i32>(g_PlayerShotType + g_PlayerCharacter * 3) == 5) {
            if ((input & 4) != 0)
                player_rec.deathbomb_lerp_percent = 0;
            else if (player_rec.deathbomb_lerp_percent < 30)
                player_rec.deathbomb_lerp_percent =
                    player_rec.deathbomb_lerp_percent + 1;
        }
    }

    const bool focused = player_rec.focus_flag != 0;
    i32 dx = 0;
    i32 dy = 0;
    if (!focused) {
        const u32 tracked = player_rec.focus_glide_entity_id;
        if (tracked != 0) {
            if (FindEntityEdxStackAbi(g_MainChainRenderOwner, tracked) != 0)
                StopEntityById(g_MainChainRenderOwner, tracked);
            player_rec.focus_glide_entity_id = 0;
        }
        switch (state) {
        case 1: dx = -player_rec.speed_unfocused; break;
        case 2: dx = player_rec.speed_unfocused; break;
        case 3: dy = -player_rec.speed_unfocused; break;
        case 4: dy = player_rec.speed_unfocused; break;
        case 5: dx = -player_rec.speed_unfocused_diagonal; dy = dx; break;
        case 6: dx = player_rec.speed_unfocused_diagonal; dy = dx; break;
        case 7: dx = -player_rec.speed_unfocused_diagonal; dy = -dx; break;
        case 8: dx = player_rec.speed_unfocused_diagonal; dy = dx; break;
        default: break;
        }
        const i32 previous_dx = player_rec.direction_x;
        if (dx < 0 && previous_dx >= 0)
            SpawnDirectionAnimEcxEaxBbxAbi(player_rec.anm_manager_work,
                &player_rec.anim_vm, 1);
        if (dx > 0 && previous_dx <= 0)
            SpawnDirectionAnimEcxEaxBbxAbi(player_rec.anm_manager_work,
                &player_rec.anim_vm, 3);
        if (dx == 0) {
            if (previous_dx < 0)
                SpawnDirectionAnimEcxEaxBbxAbi(player_rec.anm_manager_work,
                    &player_rec.anim_vm, 2);
            if (previous_dx > 0)
                SpawnDirectionAnimEcxEaxBbxAbi(player_rec.anm_manager_work,
                    &player_rec.anim_vm, 4);
        }
    } else {
        if (player_rec.focus_glide_entity_id == 0) {
            void *const vm = AllocatePoolVmEsiAbi(g_MainChainRenderOwner);
            *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x20) = 9;
            *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x35c) |=
                0x40000000U;
            AssignPoolVmScriptEcxEaxAbi(vm, 0x160);
            u32 id = 0;
            LinkEntityAndAssignIdEaxEsiAbi(&id, vm);
            player_rec.focus_glide_entity_id = id;
        }
        switch (state) {
        case 1: dx = -player_rec.speed_focused; break;
        case 2: dx = player_rec.speed_focused; break;
        case 3: dy = -player_rec.speed_focused; break;
        case 4: dy = player_rec.speed_focused; break;
        case 5: dx = -player_rec.speed_focused_diagonal; dy = dx; break;
        case 6: dx = player_rec.speed_focused_diagonal; dy = dx; break;
        case 7: dx = -player_rec.speed_focused_diagonal; dy = -dx; break;
        case 8: dx = player_rec.speed_focused_diagonal; dy = dx; break;
        default: break;
        }
    }

    player_rec.direction_x = dx;
    player_rec.direction_y = dy;
    const i32 step_x = FloatToI32(static_cast<float>(dx) * g_FrameTimeScale);
    const i32 step_y = FloatToI32(static_cast<float>(dy) * g_FrameTimeScale);
    player_rec.step_x = step_x;
    player_rec.step_y = step_y;
    player_rec.position_x_fixed = player_rec.position_x_fixed + step_x;
    player_rec.position_y_fixed = player_rec.position_y_fixed + step_y;

    if (player_rec.position_x_fixed < -0x47e0)
        player_rec.position_x_fixed = -0x47e0;
    else if (player_rec.position_x_fixed > 0x47e0)
        player_rec.position_x_fixed = 0x47e0;
    if (player_rec.position_y_fixed < 0xc80)
        player_rec.position_y_fixed = 0xc80;
    else if (player_rec.position_y_fixed > 0xa8c0)
        player_rec.position_y_fixed = 0xa8c0;

    const float position_x =
        static_cast<float>(player_rec.position_x_fixed) * 0.01f;
    const float position_y =
        static_cast<float>(player_rec.position_y_fixed) * 0.01f;
    player_rec.position_x = position_x;
    player_rec.position_y = position_y;

    const u32 tracked = player_rec.focus_glide_entity_id;
    if (tracked != 0) {
        if (FindEntityEdxStackAbi(g_MainChainRenderOwner, tracked) != 0) {
            const float target[3] = {position_x + 224.0f,
                                     position_y + 16.0f,
                                     player_rec.position_z};
            SetEntityPositionDirectEsiAbi(g_MainChainRenderOwner, tracked,
                                          target);
        } else {
            player_rec.focus_glide_entity_id = 0;
        }
    }

    // History ring: 32 dword pairs; the valid window is optionCount*8 pairs
    // and the tail is saturated to its oldest entry. Only shifts while
    // unfocused and moving; the head write runs every frame.
    const i32 count = player_rec.option_count;
    u32 *const history = player_rec.trail_history;
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
    history[0] = static_cast<u32>(player_rec.position_x_fixed);
    history[1] = static_cast<u32>(player_rec.position_y_fixed);

    const i32 lerp_percent = player_rec.deathbomb_lerp_percent;
    for (u32 index = 0; index != 4; ++index) {
        PlayerOptionRecord &rec = player_rec.options[index];
        if (rec.state == 0)
            continue;
        // The option position fields carry x100 fixed-point dwords; the
        // dword view is kept on every access.
        const float *const offset_source =
            focused ? rec.offset_source_b : rec.offset_source_a;
        *reinterpret_cast<i32 *>(&rec.unfocused_position[0]) =
            player_rec.position_x_fixed +
            *reinterpret_cast<const i32 *>(&offset_source[0]);
        *reinterpret_cast<i32 *>(&rec.unfocused_position[1]) =
            player_rec.position_y_fixed +
            *reinterpret_cast<const i32 *>(&offset_source[1]);

        OptionRecordFn *const callback =
            reinterpret_cast<OptionRecordFn *>(rec.update_callback);
        if (rec.update_callback != 0)
            (void)(*callback)(&rec);

        if (rec.tier_latch != 0) {
            rec.tier_latch = 0;
            *reinterpret_cast<i32 *>(&rec.render_position[0]) =
                *reinterpret_cast<const i32 *>(&rec.unfocused_position[0]);
            *reinterpret_cast<i32 *>(&rec.render_position[1]) =
                *reinterpret_cast<const i32 *>(&rec.unfocused_position[1]);
        } else if (lerp_percent > 29) {
            const i32 delta_x =
                (*reinterpret_cast<const i32 *>(&rec.unfocused_position[0]) -
                 *reinterpret_cast<const i32 *>(&rec.render_position[0])) *
                lerp_percent / 100;
            const i32 delta_y =
                (*reinterpret_cast<const i32 *>(&rec.unfocused_position[1]) -
                 *reinterpret_cast<const i32 *>(&rec.render_position[1])) *
                lerp_percent / 100;
            if (delta_x != 0 || delta_y != 0) {
                *reinterpret_cast<i32 *>(&rec.render_position[0]) =
                    *reinterpret_cast<const i32 *>(
                        &rec.render_position[0]) + delta_x;
                *reinterpret_cast<i32 *>(&rec.render_position[1]) =
                    *reinterpret_cast<const i32 *>(
                        &rec.render_position[1]) + delta_y;
            } else {
                *reinterpret_cast<i32 *>(&rec.render_position[0]) =
                    *reinterpret_cast<const i32 *>(
                        &rec.unfocused_position[0]);
                *reinterpret_cast<i32 *>(&rec.render_position[1]) =
                    *reinterpret_cast<const i32 *>(
                        &rec.unfocused_position[1]);
            }
        }

        const float entity_position[3] = {
            static_cast<float>(*reinterpret_cast<const i32 *>(
                &rec.render_position[0])) * 0.01f,
            static_cast<float>(*reinterpret_cast<const i32 *>(
                &rec.render_position[1])) * 0.01f, 0.0f};
        SetEntityPositionOffsetEsiAbi(g_MainChainRenderOwner,
                                      rec.sprite_entity_id,
                                      entity_position);
        SetEntityPositionOffsetEsiAbi(g_MainChainRenderOwner,
                                      rec.power_effect_entity_id,
                                      entity_position);
    }
    return 0;
}

} // namespace th10
