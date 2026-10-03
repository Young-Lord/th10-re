#include "EclScriptLibrary.hpp"

#include "AsciiHudOwner.hpp"
#include "EclScriptObject.hpp"
#include "EclScriptVm.hpp"
#include "EntityHelpers.hpp"
#include "GameContext.hpp"
#include "PlayerRecord.hpp"
#include "TimelineRenderObjectSetup.hpp"
#include "PlayerMotionHelpers.hpp"
#include "PlayerTimerHelpers.hpp"
#include "ResultScreenScript.hpp"
#include "VmLeafHelpers.hpp"

#include <cmath>

namespace th10 {

namespace {

// TH10 0x474c74. Current difficulty index; the ECL object stores
// 1 << index in its +0x1024 difficulty bitmask byte.
const u32 k_difficulty_index = 0x474c74U;

u32 ReadGlobalU32(u32 address)
{
    const u8 *const source = reinterpret_cast<const u8 *>(address);
    return static_cast<u32>(source[0]) | (static_cast<u32>(source[1]) << 8)
         | (static_cast<u32>(source[2]) << 16)
         | (static_cast<u32>(source[3]) << 24);
}

void StoreU32(u8 *record, u32 offset, u32 value)
{
    record[offset + 0] = static_cast<u8>(value);
    record[offset + 1] = static_cast<u8>(value >> 8);
    record[offset + 2] = static_cast<u8>(value >> 16);
    record[offset + 3] = static_cast<u8>(value >> 24);
}

u32 LoadU32(const u8 *record, u32 offset)
{
    return static_cast<u32>(record[offset])
         | (static_cast<u32>(record[offset + 1]) << 8)
         | (static_cast<u32>(record[offset + 2]) << 16)
         | (static_cast<u32>(record[offset + 3]) << 24);
}

// Replaces one bit in the ECL script object's flags_1444 dword (the native
// reads the flag dword at record+0x2480 and rewrites it whole).
void ReplaceWorkFlagBit(EclScriptWork &work, u32 bit, u32 incoming)
{
    const u32 flags = work.flags_1444;
    work.flags_1444 = (flags & ~bit) | (incoming ? bit : 0U);
}

// Float/int helpers over the same little-endian byte layout (kept for the
// byte-level record images that stay RAW: script-manager slots, entity
// records, owner lists).
void StoreF32(u8 *record, u32 offset, float value)
{
    StoreU32(record, offset,
             *reinterpret_cast<const u32 *>(&value));
}

float LoadF32(const u8 *record, u32 offset)
{
    const u32 raw = LoadU32(record, offset);
    return *reinterpret_cast<const float *>(&raw);
}

i32 LoadI32(const u8 *record, u32 offset)
{
    return static_cast<i32>(LoadU32(record, offset));
}

u8 LoadU8(const u8 *record, u32 offset)
{
    return record[offset];
}

void StoreI32(u8 *record, u32 offset, i32 value)
{
    StoreU32(record, offset, static_cast<u32>(value));
}

void StoreU16(u8 *record, u32 offset, u16 value)
{
    record[offset] = static_cast<u8>(value);
    record[offset + 1] = static_cast<u8>(value >> 8);
}

// Dereferences a pointer field stored at `offset` of a raw record image.
u8 *LoadPointer(const u8 *record, u32 offset)
{
    return reinterpret_cast<u8 *>(LoadU32(record, offset));
}

// ---------------------------------------------------------------------------
// Native boundaries consumed by RunEclScriptSetupStackAbi. All of these use
// unusual register ABIs; semantic reconstruction is deferred until their
// remaining callers are covered.
// ---------------------------------------------------------------------------

// TH10 0x00412ac0 is implemented at the end of this file as
// TickVec2AnimInterpolator.
void TickVec2AnimInterpolator(float out_vec2[2], void *block);

// TH10 0x476f78. Global frame-time scale; the interpolator rate resets
// point at it (value 1.0).
extern float g_FrameTimeScale;

// The 0x463b2c conversion used by the interpolator: round half away from
// zero.
inline i32 FloatToI32(float value)
{
    return value >= 0.0f
        ? static_cast<i32>(std::floor(static_cast<double>(value) + 0.5))
        : static_cast<i32>(std::ceil(static_cast<double>(value) - 0.5));
}

// TH10 0x004496d0. Native EAX = pointer to the entity-id slot to rebind,
// stack argument = the new id; looks the current id up through the render
// owner lists and forwards to the 0x43e8b0 bind helper.
void RebindEntitySlotEaxStackAbi(u32 *id_slot, u32 new_id);

// TH10 0x0044fd10 is implemented in EclScriptVm.cpp as
// RunEclContextListEdiStackAbi (declared in EclScriptVm.hpp).

// TH10 0x00428630. Native ECX (this) = null at this call site, stack =
// {item-record base, position pair, size pair}; runs the player/item box
// collision pass and returns the raw damage value.
i32 CollectItemCollisionsStackAbi(void *item_records, const float *position,
                                  const float *size);

// TH10 0x004127a0. Native __thiscall ECX = the ECL script object record
// itself (0x40dc80 loads ECX from record+0x2514, the ctor-planted self
// handle). Pops the next pending script-name request (see the body below);
// implemented at the end of this file.
i32 AcquireScriptNameRequestThisAbi(EclScriptObject &obj);

// TH10 0x0040c6e0 / 0x0040c730. Native EAX = script manager; paired update
// steps around the script bind.
void UpdateScriptManagerAEaxAbi(void *script_manager);
void UpdateScriptManagerBEaxAbi(void *script_manager);

// TH10 0x00450470. Native EAX = the request object returned by
// 0x4127a0 (its +8 holds the table size and +140 the name table); resolves
// the requested script name to table_index + 16, or 0.
i32 ResolveScriptTableIndexEaxAbi(i32 request);

// TH10 0x0040e5f0 is implemented in EnemyDeathEffects.cpp as
// TriggerEnemyDeathSequenceStdcallAbi (declared in EclScriptVm.hpp).

// TH10 0x004266b0. Native usercall: EAX/EDX/ECX carry the half-extent pair,
// the player-state block, and the position pair; checks the timeout region
// and returns 0/1/2.
i32 CheckEnemyTimeoutRegionEaxEdxEcxAbi(const float *half, void *player_block,
                                        const float *position);

// TH10 0x0043dd10. Native EBX = sound id, ESI = sound manager (0x492590),
// stack = float payload; pushes the positional sound request.
void EnqueueSoundEffectEbxStackAbi(u32 sound_id, void *sound_manager,
                                   float value);

// TH10 globals consumed here.
extern void *g_MainChainRenderOwner;   // TH10 DAT_00491c10
extern void *g_PlayerStateBlock;       // TH10 DAT_00477834
extern void *g_StageNode;              // TH10 DAT_004776ec (pointer holder)
extern void *g_SpellBulletBase;        // TH10 DAT_004776f4 (pointer holder)
extern void *g_AsciiHudOwner;          // TH10 DAT_0047770c
extern i32 g_BossDefeatedFlag;         // TH10 DAT_00474c50
extern float g_EclBasePosShift[3];     // TH10 flt_491e6c/70/74

} // namespace

void *CreateEclScriptObjectEaxStackAbi(const u32 *descriptor,
                                       void *list_owner, i32 ctor_arg)
{
    void *const raw = ::operator new(0x2518U);
    u8 *record;
    if (raw != 0) {
        record = static_cast<u8 *>(ConstructEclScriptObjectEsiStackAbi(
            raw, ctor_arg));
    } else {
        record = 0;
    }

    // The native keeps writing through the (possibly null) record.
    EclScriptObject &obj = *reinterpret_cast<EclScriptObject *>(record);
    EclScriptWork &work = obj.work;

    // Creator descriptor dwords 0..2 -> anchor1 position (record+0x1094).
    work.anchor1_pos_0058[0] = *reinterpret_cast<const float *>(&descriptor[0]);
    work.anchor1_pos_0058[1] = *reinterpret_cast<const float *>(&descriptor[1]);
    work.anchor1_pos_0058[2] = *reinterpret_cast<const float *>(&descriptor[2]);
    work.death_score_13bc = static_cast<i32>(descriptor[3]);   // +0x23f8
    work.hp_13c0 = static_cast<i32>(descriptor[5]);            // +0x23fc
    work.kind_13cc = static_cast<i32>(descriptor[4]);          // +0x2408
    ReplaceWorkFlagBit(work, 0x800U, descriptor[6] & 1U);      // +0x2480

    const u32 difficulty = ReadGlobalU32(k_difficulty_index);
    obj.difficulty_mask_1024 = static_cast<u8>(1U << difficulty);

    // 0x20 bytes at descriptor+0x20 -> work.descriptor_vars_00fc[8]
    // (record+0x1138; the ECL variables -9985..-9978).
    for (u32 i = 0; i != 8U; ++i)
        work.descriptor_vars_00fc[i] = descriptor[8U + i];

    // Shift-timer A (= the score-anim block, record+0x2458..0x246c):
    // initialize once (TimerNode flag bit 0), then unconditionally arm it
    // with count 2 and the 2.0f accumulator.
    if ((work.shift_timer_a_141c.flags & 1U) == 0U) {
        work.shift_timer_a_141c.count = 0;
        work.shift_timer_a_141c.prev = static_cast<i32>(-999999);
        work.shift_timer_a_141c.accum = 0;
        work.shift_timer_a_141c.rate =
            reinterpret_cast<const float *>(0x476f78U); // &flt_476F78
        work.shift_timer_a_141c.flags |= 1U;
    }
    work.shift_timer_a_141c.count = 2;
    work.shift_timer_a_141c.accum = static_cast<i32>(0x40000000U); // 2.0f
    work.shift_timer_a_141c.prev = 1;

    ReplaceWorkFlagBit(work, 0x40000U, descriptor[7] & 1U);

    RunEclScriptSetupStackAbi(&work);

    // Bit 0x8000 of flags_1444 remaps the kind (1 -> 10, 4 -> 11).
    if ((work.flags_1444 & 0x8000U) != 0U) {
        u32 kind = static_cast<u32>(work.kind_13cc);
        if (kind == 1U) {
            kind = 10U;
        } else if (kind == 4U) {
            kind = 11U;
        }
        work.kind_13cc = static_cast<i32>(kind);
    }

    // Presentation pair: par-count kind from the owner list parity and a
    // per-file accent value driven by the descriptor's embedded kind pair
    // (only when the mode gate == 1).
    u8 *const owner = static_cast<u8 *>(list_owner);
    work.layer_variant_1408 =
        static_cast<i32>((LoadU32(owner, 0x64U) & 1U) + 2U);
    work.table_value_140c = 359;
    if (work.mode_gate_00ec == 1) {
        switch (static_cast<u32>(work.sub_mode_00f0)) {
        case 0x00U:
        case 0x14U:
        case 0x31U:
            work.table_value_140c = 359;
            break;
        case 0x05U:
        case 0x19U:
        case 0x32U:
            work.table_value_140c = 356;
            break;
        case 0x0aU:
        case 0x1eU:
        case 0x33U:
            work.table_value_140c = 362;
            break;
        case 0x0fU:
        case 0x23U:
            work.table_value_140c = 365;
            break;
        default:
            break;
        }
    }
    work.resource_index_1410 = 0;

    // Append the embedded list node (work.list_self_0130 / list_next_0134 /
    // list_prev_0138; record+0x116c, next at +0x1170, prev at +0x1174) after
    // the owner's current last node.
    void *const node = &work.list_self_0130;
    const u32 head = LoadU32(owner, 0x58U);
    if (head != 0U) {
        const u32 last = LoadU32(owner, 0x5cU);
        // `last` / `last_next` are foreign embedded list nodes (raw next at
        // node+4, prev at node+8), so their fields stay RAW.
        const u32 last_next = LoadU32(reinterpret_cast<const u8 *>(last), 4U);
        if (last_next != 0U) {
            work.list_next_0134 = reinterpret_cast<void *>(last_next);
            StoreU32(reinterpret_cast<u8 *>(last_next), 8U,
                     reinterpret_cast<u32>(node));
        }
        StoreU32(reinterpret_cast<u8 *>(last), 4U,
                 reinterpret_cast<u32>(node));
        work.list_prev_0138 = reinterpret_cast<void *>(last);
    } else {
        StoreU32(owner, 0x58U, reinterpret_cast<u32>(node));
    }
    StoreU32(owner, 0x5cU, reinterpret_cast<u32>(node));
    StoreU32(owner, 0x60U, LoadU32(owner, 0x60U) + 1U);
    StoreU32(owner, 0x64U, LoadU32(owner, 0x64U) + 1U);

    return record;
}

// FUNCTION: TH10 0x0040dc80
// Native ABI: one stack argument (ret 4) = the +0x103c sub-record of an ECL
// script object (both native callers push record+0x103c: 0x40d0b3 and
// 0x40d771), modeled here as the typed th10::EclScriptWork view ("work").
// Motion blocks follow the shared {pos xyz @0, vel xyz @0xc, radius @0x18,
// angle @0x1c, flags @0x28} layout used by IntegrateSubEffectPositionEsiAbi.
i32 RunEclScriptSetupStackAbi(void *sub_record)
{
    u8 *const rec = static_cast<u8 *>(sub_record);
    EclScriptWork &work = *reinterpret_cast<EclScriptWork *>(rec);

    // Run gate: bit 0x400 of flags_1444. A second invocation is a no-op that
    // returns 0; otherwise the bit is armed before anything else runs.
    u32 flags = work.flags_1444;
    if ((flags & 0x400U) != 0U)
        return 0;
    work.flags_1444 = flags | 0x400U;
    flags |= 0x400U;

    // Refresh the working block (working_block_0000, 0x2c bytes) from the
    // base block at base_pos_002c (+0x2c; qmemcpy 0x2c @0x40dcbb) before any
    // animation output is applied.
    u8 *const working_block = reinterpret_cast<u8 *>(work.working_block_0000);
    const u8 *const base_block =
        reinterpret_cast<const u8 *>(work.base_pos_002c);
    for (u32 i = 0; i < 0x2cU; ++i)
        working_block[i] = base_block[i];

    // Four 0x3c-byte vec2 animation blocks, armed by their duration dword:
    //   vec2_a0_01d4 -> anchor1_angle_0074 (wrapped) / anchor1_radius_0070
    //   vec2_a2_024c -> anchor1_radius2_0078 / anchor1_angle2_007c (direct)
    //   vec2_a1_0210 -> anchor2_angle_00a0 (wrapped) / anchor2_radius_009c
    //   vec2_a3_0288 -> anchor2_radius2_00a4 / anchor2_angle2_00a8 (direct)
    float anim_out[2];
    if (work.vec2_a0_01d4.duration != 0) {
        TickVec2AnimInterpolator(anim_out, &work.vec2_a0_01d4);
        work.anchor1_angle_0074 = WrapAngleToPi(anim_out[0]);
        work.anchor1_radius_0070 =
            *reinterpret_cast<const i32 *>(&anim_out[1]);
    }
    if (work.vec2_a2_024c.duration != 0) {
        TickVec2AnimInterpolator(anim_out, &work.vec2_a2_024c);
        work.anchor1_radius2_0078 =
            *reinterpret_cast<const i32 *>(&anim_out[0]);
        work.anchor1_angle2_007c = anim_out[1];
    }
    if (work.vec2_a1_0210.duration != 0) {
        TickVec2AnimInterpolator(anim_out, &work.vec2_a1_0210);
        work.anchor2_angle_00a0 = WrapAngleToPi(anim_out[0]);
        work.anchor2_radius_009c =
            *reinterpret_cast<const i32 *>(&anim_out[1]);
    }
    if (work.vec2_a3_0288.duration != 0) {
        TickVec2AnimInterpolator(anim_out, &work.vec2_a3_0288);
        work.anchor2_radius2_00a4 =
            *reinterpret_cast<const i32 *>(&anim_out[0]);
        work.anchor2_angle2_00a8 = anim_out[1];
    }

    // Motion block 1 (anchor1_pos_0058 / anchor1_delta_0064 /
    // anchor1_radius_0070 / anchor1_angle_0074 / anchor1_flags_0080).
    // Velocity source: vec3_a_013c (armed by its duration) yielding a target
    // position, else the radius/angle pair.
    if (work.vec3_a_013c.duration != 0) {
        float target[3];
        TickVec3Interpolator(&work.vec3_a_013c, target);
        work.anchor1_delta_0064[0] =
            target[0] - work.anchor1_pos_0058[0];
        work.anchor1_delta_0064[1] =
            target[1] - work.anchor1_pos_0058[1];
        work.anchor1_delta_0064[2] =
            target[2] - work.anchor1_pos_0058[2];
    } else if ((work.anchor1_flags_0080 & 1U) != 0U) {
        *reinterpret_cast<float *>(&work.anchor1_radius2_0078) =
            *reinterpret_cast<const float *>(&work.anchor1_radius2_0078)
            + work.anchor1_angle2_007c;
        work.anchor1_angle_0074 = WrapAngleToPi(
            *reinterpret_cast<const float *>(&work.anchor1_radius_0070)
            + work.anchor1_angle_0074);
    } else {
        PolarToCartesianEdiAbi(
            work.anchor1_delta_0064, work.anchor1_angle_0074,
            *reinterpret_cast<const float *>(&work.anchor1_radius_0070));
        work.anchor1_delta_0064[2] = 0.0f;
    }

    // Motion block 2 (anchor2_pos_0084 / anchor2_delta_0090 /
    // anchor2_radius_009c / anchor2_angle_00a0 / anchor2_flags_00ac). Same
    // three modes; the vec3 animation lives at vec3_b_0188.
    if (work.vec3_b_0188.duration != 0) {
        float target[3];
        TickVec3Interpolator(&work.vec3_b_0188, target);
        work.anchor2_delta_0090[0] =
            target[0] - work.anchor2_pos_0084[0];
        work.anchor2_delta_0090[1] =
            target[1] - work.anchor2_pos_0084[1];
        work.anchor2_delta_0090[2] =
            target[2] - work.anchor2_pos_0084[2];
    } else if ((work.anchor2_flags_00ac & 1U) != 0U) {
        *reinterpret_cast<float *>(&work.anchor2_radius2_00a4) =
            *reinterpret_cast<const float *>(&work.anchor2_radius2_00a4)
            + work.anchor2_angle2_00a8;
        work.anchor2_angle_00a0 = WrapAngleToPi(
            *reinterpret_cast<const float *>(&work.anchor2_radius_009c)
            + work.anchor2_angle_00a0);
    } else {
        PolarToCartesianEdiAbi(
            work.anchor2_delta_0090, work.anchor2_angle_00a0,
            *reinterpret_cast<const float *>(&work.anchor2_radius_009c));
        work.anchor2_delta_0090[2] = 0.0f;
    }

    // Integrate block 1; with flag bit 0x40000 the block-2 position is
    // shifted by the global base-position offsets before its integration.
    IntegrateSubEffectPositionEsiAbi(work.anchor1_pos_0058);
    if ((flags & 0x40000U) != 0U) {
        work.anchor2_pos_0084[0] =
            g_EclBasePosShift[0] + work.anchor2_pos_0084[0];
        work.anchor2_pos_0084[1] =
            g_EclBasePosShift[1] + work.anchor2_pos_0084[1];
        work.anchor2_pos_0084[2] =
            g_EclBasePosShift[2] + work.anchor2_pos_0084[2];
    }
    IntegrateSubEffectPositionEsiAbi(work.anchor2_pos_0084);

    // The base block's velocity is the summed position delta, and the base
    // block itself is integrated afterwards.
    work.base_velocity_0038[0] =
        work.anchor2_pos_0084[0] + work.anchor1_pos_0058[0]
        - work.base_pos_002c[0];
    work.base_velocity_0038[1] =
        work.anchor2_pos_0084[1] + work.anchor1_pos_0058[1]
        - work.base_pos_002c[1];
    work.base_velocity_0038[2] =
        work.anchor2_pos_0084[2] + work.anchor1_pos_0058[2]
        - work.base_pos_002c[2];
    IntegrateSubEffectPositionEsiAbi(work.base_pos_002c);

    // Flag bit 0x200: clamp the base position into the rectangle centered
    // at clamp_center_13ac with half extents clamp_half_extent_13b4, then
    // re-derive block-1's position as base - block2.
    if ((flags & 0x200U) != 0U) {
        const float half_x = work.clamp_half_extent_13b4[0] * 0.5f;
        const float lo_x = work.clamp_center_13ac[0] - half_x;
        if (lo_x <= work.base_pos_002c[0]) {
            const float hi_x = half_x + work.clamp_center_13ac[0];
            if (hi_x < work.base_pos_002c[0])
                work.base_pos_002c[0] = hi_x;
        } else {
            work.base_pos_002c[0] = lo_x;
        }
        const float half_y = work.clamp_half_extent_13b4[1] * 0.5f;
        const float lo_y = work.clamp_center_13ac[1] - half_y;
        if (lo_y <= work.base_pos_002c[1]) {
            const float hi_y = half_y + work.clamp_center_13ac[1];
            if (hi_y < work.base_pos_002c[1])
                work.base_pos_002c[1] = hi_y;
        } else {
            work.base_pos_002c[1] = lo_y;
        }
        work.anchor1_pos_0058[0] =
            work.base_pos_002c[0] - work.anchor2_pos_0084[0];
        work.anchor1_pos_0058[1] =
            work.base_pos_002c[1] - work.anchor2_pos_0084[1];
        work.anchor1_pos_0058[2] =
            work.base_pos_002c[2] - work.anchor2_pos_0084[2];
    }

    // Playfield gate: the base position must sit inside x=[-192,192] and
    // y=[0,448] (adjusted by the hitbox_size_13a4 half extents). Leaving the
    // region without the 0x4 stay flag aborts when the 0x100 latch was
    // already set; entering it sets the latch.
    const float half_w = work.hitbox_size_13a4[0] * 0.5f;
    const float half_h = work.hitbox_size_13a4[1] * 0.5f;
    const float pos_x = work.base_pos_002c[0];
    const float pos_y = work.base_pos_002c[1];
    const bool inside = pos_x + half_w >= -192.0f &&
                        pos_x - half_w <= 192.0f &&
                        pos_y + half_h >= 0.0f && pos_y - half_h <= 448.0f;
    if (!inside) {
        if ((flags & 0x100U) != 0U && (flags & 0x4U) == 0U)
            return -1;
    } else {
        flags |= 0x100U;
        work.flags_1444 = flags;
    }

    // Flag bit 0x100000: publish the bind id (primary_bind_id_1448 or, with
    // bit 0x200000, alternate_bind_id_144c) into bind_id_00f4 and rebind the
    // first entity slot. With no active stage node the 0x200000 path clears
    // bits 0x200000|1 instead.
    if ((flags & 0x100000U) != 0U) {
        // g_StageNode holds the 0x48-byte game context (DAT_004776ec); the
        // +0x28 popup-state dword doubles as the stage-active gate.
        GameContext &stage_ctx = *static_cast<GameContext *>(g_StageNode);
        const u32 stage_active = stage_ctx.popup_state;
        if (stage_active == 0U) {
            if ((flags & 0x200000U) != 0U) {
                const u32 id = work.alternate_bind_id_144c;
                work.bind_id_00f4 = static_cast<i32>(id);
                RebindEntitySlotEaxStackAbi(work.published_ids_00c0, id);
                work.flags_1444 = work.flags_1444 & 0xffdffffeU;
            }
        } else if ((flags & 0x200000U) == 0U) {
            const u32 id = work.primary_bind_id_1448;
            work.bind_id_00f4 = static_cast<i32>(id);
            RebindEntitySlotEaxStackAbi(work.published_ids_00c0, id);
            work.flags_1444 = work.flags_1444 | 0x200001U;
        }
    }

    // The record's self handle: the ctor (0x40d89f) stores the record base
    // at record+0x2514 (self_2514, the dword immediately past the 0x14d8
    // -byte work view — the native loads it as [work+0x14d8] at 0x40e242 /
    // 0x40e275 / 0x40e4be / 0x40e4da). The script-manager helpers below
    // (0x44fd10 / 0x4127a0 / 0x40c6e0 / 0x40c730 / 0x40e5f0) all take this
    // record as their manager.
    EclScriptObject &owning_object = *reinterpret_cast<EclScriptObject *>(
        rec - offsetof(EclScriptObject, work));
    EclScriptObject &self =
        *static_cast<EclScriptObject *>(owning_object.self_2514);

    // Entity-list scan over the record; a nonzero result aborts.
    const float scan_value = *work.frame_tail_011c.rate;
    if (RunEclContextListEdiStackAbi(&self, scan_value) != 0)
        return -1;

    // Clear the flicker-arm bit (0x2000; the native reads the low flag byte
    // for the 0x11 mask, which is equivalent), then decide the pass.
    flags = work.flags_1444 & 0xffffdfffU;
    work.flags_1444 = flags;
    if ((flags & 0x11U) != 0U)
        goto post_damage_pass;

    {
        // Damage pass: run the item collision over the 128 player-item
        // records (this = null at the native call site), scale it down on
        // modes 0/2, and drain the boss HP (hp_13c0).
        i32 damage = CollectItemCollisionsStackAbi(
            g_PlayerStateBlock,
            work.base_pos_002c,
            work.hitbox_params_00b0);
        const u32 player_mode = static_cast<u32>(
            reinterpret_cast<PlayerRecord *>(g_PlayerStateBlock)->mode);
        if (player_mode == 2U || player_mode == 0U)
            damage /= 5;
        if (damage == 0) {
            g_BossDefeatedFlag = 1;
            goto post_damage_pass;
        }
        if ((LoadU8(static_cast<const u8 *>(g_SpellBulletBase), 0x378cU) & 1U) != 0U &&
            (flags & 0x8000U) != 0U) {
            damage /= 5;
            if (damage <= 0)
                damage = 1;
        }
        if ((flags & 0x8U) == 0U && work.shift_timer_a_141c.count <= 0)
            work.hp_13c0 -= damage;

        // Pending script-name request handling.
        const i32 request = AcquireScriptNameRequestThisAbi(self);
        if (request != 0) {
            UpdateScriptManagerAEaxAbi(&self);
            UpdateScriptManagerBEaxAbi(&self);
            const i32 resolved = ResolveScriptTableIndexEaxAbi(request);
            // The bind chain goes through bind_node_self_0004 (-> +0x0008);
            // the writes land on bind_node_0008 (+0x8) and
            // script_table_id_000c (+0xc).
            u8 *const bind_buffer = static_cast<u8 *>(self.bind_node_self_0004);
            StoreU32(bind_buffer, 4U, static_cast<u32>(resolved));
            StoreU32(bind_buffer, 0U, 0U);
            if (RunEclContextListEdiStackAbi(&self, scan_value) != 0)
                return -1;
        }

        // Kill/score handling: without flag bit 0x40, a drained HP adds the
        // death score (death_score_13bc; the native passes *(work+0x13bc) at
        // 0x40e23d) and can run the death sequence.
        if ((flags & 0x40U) == 0U && work.hp_13c0 <= 0) {
            AddScoreBlockValueEcxStackAbi(reinterpret_cast<void *>(0x474c40U),
                                          work.death_score_13bc);
            if (TriggerEnemyDeathSequenceStdcallAbi(&self) != 0)
                return 1;
        }
        flags |= 0x2000U;
        work.flags_1444 = flags;
        g_BossDefeatedFlag = 1;
    }

post_damage_pass:
    {
        // Pending script-name request handling (shared tail).
        const i32 request = AcquireScriptNameRequestThisAbi(self);
        if (request != 0) {
            UpdateScriptManagerAEaxAbi(&self);
            UpdateScriptManagerBEaxAbi(&self);
            // See the damage-pass note: writes land on bind_node_0008 /
            // script_table_id_000c through bind_node_self_0004.
            u8 *const bind_buffer = static_cast<u8 *>(self.bind_node_self_0004);
            StoreU32(bind_buffer, 4U,
                     static_cast<u32>(ResolveScriptTableIndexEaxAbi(request)));
            StoreU32(bind_buffer, 0U, 0U);
        }

        // Timeout region check; skipped with flag bits 0x2|0x10 or a
        // positive shift_timer_b_1430 count.
        if ((flags & 0x12U) == 0U && work.shift_timer_b_1430.count <= 0)
            CheckEnemyTimeoutRegionEaxEdxEcxAbi(
                work.base_pos_002c,
                g_PlayerStateBlock,
                work.hitbox_params_00b0);

        // Flag bit 0x1000: direction state machine over the base-block X
        // delta (base_velocity_0038[0]), driving animation-id switches
        // through bind_id_00f4.
        if ((flags & 0x1000U) != 0U) {
            const float dx = work.base_velocity_0038[0];
            const i32 direction =
                dx < -0.1f ? -1 : (dx > 0.1f ? 1 : 0);
            const i32 previous = work.facing_dir_00f8;
            if (previous != direction) {
                i32 step = 0;
                if (previous == -1)
                    step = 3 - (direction != 0 ? 1 : 0);
                else if (previous == 0)
                    step = (direction != -1 ? 1 : 0) + 1;
                else if (previous == 1)
                    step = direction != 0 ? 1 : 4;
                work.facing_dir_00f8 = direction;
                RebindEntitySlotEaxStackAbi(
                    work.published_ids_00c0,
                    static_cast<u32>(work.bind_id_00f4)
                    + static_cast<u32>(step));
            }
        }

        // Publish the base-block position into the eight entity id slots
        // published_ids_00c0[0..7]; flag bit 0x40000 selects the verbatim
        // over the offset path.
        const float position[3] = {work.base_pos_002c[0],
                                   work.base_pos_002c[1],
                                   work.base_pos_002c[2]};
        for (u32 slot = 0; slot != 8U; ++slot) {
            const u32 id = work.published_ids_00c0[slot];
            if ((flags & 0x40000U) != 0U)
                SetEntityPositionDirectEsiAbi(g_MainChainRenderOwner, id,
                                              position);
            else
                SetEntityPositionOffsetEsiAbi(g_MainChainRenderOwner, id,
                                              position);
        }

        // Track the closest enemy in the player-state block (+0x3504 target,
        // +0x3508 latch) by X distance to the player (+0x3c0); skipped with
        // flag bits 0x11 or 0xc0000.
        if ((flags & 0x11U) == 0U && (flags & 0xc0000U) == 0U) {
            PlayerRecord &player =
                *reinterpret_cast<PlayerRecord *>(g_PlayerStateBlock);
            const u8 *const old_target =
                static_cast<const u8 *>(player.homing_target);
            const float player_x = player.position_x;
            const float new_distance =
                work.base_pos_002c[0] - player_x;
            bool farther = old_target == 0;
            if (!farther) {
                // +0x1068 of the foreign target record = its
                // EclScriptWork::base_pos_002c[0]; kept RAW because the
                // target is not guaranteed to be an ECL script object.
                const float old_distance =
                    LoadF32(old_target, 0x1068U) - player_x;
                farther = (new_distance < 0.0f ? -new_distance
                                               : new_distance) >
                          (old_distance < 0.0f ? -old_distance
                                               : old_distance);
            }
            if (farther) {
                if (player.homing_target_latch == 0U)
                    player.homing_target = &self;
                player.homing_target_latch = 1U;
            }
        }

        // Resolve the primary entity slot (published_ids_00c0[0]) and drive
        // the hit-flicker state.
        u8 *entity = FindEntityEdxStackAbi(
            g_MainChainRenderOwner, work.published_ids_00c0[0]);
        if (entity == 0)
            work.published_ids_00c0[0] = 0;
        if (work.hit_flicker_timer_1414 != 0) {
            // Native quirk: the entity pointer is dereferenced even when
            // the lookup failed (unchecked pointer, preserved).
            StoreU32(entity, 0x35cU,
                     LoadU32(entity, 0x35cU) & 0xffff7fffU);
            if ((flags & 0x8000U) != 0U) {
                reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner)
                    ->aux_vm.flags &= 0xffff7fffU;
            }
            --work.hit_flicker_timer_1414;
        } else if ((flags & 0x2000U) != 0U) {
            StoreU32(entity, 0x35cU, LoadU32(entity, 0x35cU) | 0x8000U);
            StoreU32(entity, 0x300U, 0xff0000ffU);
            work.hit_flicker_timer_1414 = 4;
            if ((flags & 0x8000U) != 0U) {
                // Loud hit sound gated by the stage flags word and the
                // deadline delta the request popper publishes at
                // record+0x2404 (= work.unknown_13c8; the native loads the
                // self handle at 0x40e4bc/0x40e4da and reads +0x2404 through
                // it, so this aliases the work field).
                bool loud = false;
                if (self.work.unknown_13c8 < 900) {
                    const u32 stage_flags =
                        LoadU32(static_cast<const u8 *>(g_SpellBulletBase),
                                0x378cU);
                    if ((stage_flags & 1U) == 0U)
                        loud = true;
                    else if ((stage_flags & 8U) == 0U &&
                             self.work.unknown_13c8 < 300)
                        loud = true;
                }
                EnqueueSoundEffectEbxStackAbi(
                    loud ? 0x23U : 0x13U,
                    reinterpret_cast<void *>(0x492590U),
                    work.base_pos_002c[0]);
            } else {
                EnqueueSoundEffectEbxStackAbi(
                    0x13U, reinterpret_cast<void *>(0x492590U),
                    work.base_pos_002c[0]);
            }
        }

        // Two shift timers (shift_timer_a_141c / shift_timer_b_1430) tick
        // down while their counts are positive.
        if (work.shift_timer_a_141c.count > 0)
            ShiftTimerByEsiStackAbi(&work.shift_timer_a_141c, -1.0f);
        if (work.shift_timer_b_1430.count > 0)
            ShiftTimerByEsiStackAbi(&work.shift_timer_b_1430, -1.0f);

        // Frame counter advance over frame_tail_011c (the shared 0x14-byte
        // TimerNode). Outside the 0.99..1.01 rate window the count re-derives
        // from the accumulator; the native conversion truncates through an
        // unsigned 64-bit path.
        const u32 current = static_cast<u32>(work.frame_tail_011c.count);
        work.frame_tail_011c.prev = static_cast<i32>(current);
        const float rate = *work.frame_tail_011c.rate;
        if (rate <= 0.99f || rate >= 1.01f) {
            const float accumulated = rate +
                *reinterpret_cast<const float *>(&work.frame_tail_011c.accum);
            *reinterpret_cast<float *>(&work.frame_tail_011c.accum) =
                accumulated;
            work.frame_tail_011c.count = static_cast<i32>(accumulated);
        } else {
            *reinterpret_cast<float *>(&work.frame_tail_011c.accum) =
                *reinterpret_cast<const float *>(&work.frame_tail_011c.accum)
                + 1.0f;
            work.frame_tail_011c.count = static_cast<i32>(current + 1U);
        }
    }
    return 0;
}

// TH10 0x00412ac0. Native EDI = out vec2, ESI = the 0x3c-byte vec2
// animation block, modeled here as the typed th10::EclVec2AnimBlock view.
// Same timer behavior as the vec3 interpolator (unity rate window,
// 0xfff0bdc1 poison, rate reset to DAT_00476f78), except the completion
// additionally zeroes the duration and the vec2 Hermite gives handle2 the
// proper (t-1)*t^2 basis. Duration <= 0 skips the timer and interpolates
// with t = accum / duration, so a zero duration yields inf/NaN. Mode 7 adds
// the end pair into cur, 0x11 integrates velocity, 8 rides the cubic
// Hermite, and every other mode eases through the 0x44c350 curve selector.
void TickVec2AnimInterpolator(float out_vec2[2], void *block_memory)
{
    EclVec2AnimBlock &a = *static_cast<EclVec2AnimBlock *>(block_memory);
    const i32 duration = a.duration;
    if (duration > 0) {
        a.timer_prev = static_cast<i32>(a.timer_count);
        const float rate = *a.timer_rate;
        if (rate > 0.99f && rate < 1.01f) {
            a.timer_accum = a.timer_accum + 1.0f;
            a.timer_count = a.timer_count + 1U;
        } else {
            const float accum = a.timer_accum + rate;
            a.timer_accum = accum;
            a.timer_count = static_cast<u32>(FloatToI32(accum));
        }
        if (static_cast<i32>(a.timer_count) >= duration) {
            if ((a.flags & 1U) == 0U) {
                a.timer_count = 0;
                a.timer_prev = static_cast<i32>(0xfff0bdc1U);
                a.timer_accum = 0.0f;
                a.timer_rate = &g_FrameTimeScale;
                a.flags |= 1U;
            }
            a.timer_count = static_cast<u32>(duration);
            a.timer_prev = duration - 1;
            a.timer_accum = static_cast<float>(duration);
            a.duration = 0;
            const float *const source = a.mode == 7 ? a.cur : a.dst;
            out_vec2[0] = source[0];
            out_vec2[1] = source[1];
            return;
        }
    }

    const i32 mode = a.mode;
    const float t = a.timer_accum / static_cast<float>(duration);
    for (u32 component = 0; component != 2U; ++component) {
        const float cur = a.cur[component];
        const float end = a.dst[component];
        float value;
        if (mode == 7) {
            value = cur + end;
            a.cur[component] = value;
        } else if (mode == 0x11) {
            value = cur + a.handle2_vel[component];
            a.cur[component] = value;
            a.handle2_vel[component] = a.handle2_vel[component] + end;
        } else if (mode == 8) {
            const float w_start = (1.0f + 2.0f * t) * (t - 1.0f) * (t - 1.0f);
            const float w_end = (3.0f - 2.0f * t) * t * t;
            const float w_handle1 = (1.0f - t) * (1.0f - t) * t;
            const float w_handle2 = (t - 1.0f) * t * t;
            value = w_start * cur + w_end * end +
                w_handle1 * a.handle1[component] +
                w_handle2 * a.handle2_vel[component];
        } else {
            const double factor = EasingCurveSelectorEaxStackAbi(
                mode, a.timer_accum, static_cast<float>(duration));
            value = cur + (end - cur) * static_cast<float>(factor);
        }
        out_vec2[component] = value;
    }
}

// ---------------------------------------------------------------------------
// TH10 0x004127a0 - pending script-name request popper.
//
// Native __thiscall ECX = the ECL script object record itself (0x40dc80
// loads ECX from record+0x2514, the ctor-planted self handle), so the old
// raw "mgr" offsets map onto the typed record as follows:
//   +0x23fc -> work.hp_13c0  (the boss HP dword doubles as the request
//             timebase natively; a consumed deadline overwrites it),
//   +0x2404 -> work.unknown_13c8 (the deadline delta published for the
//             hit-sound gate; read back at 0x40e4bc..0x40e4e4),
//   +0x2480 -> work.flags_1444 (bit 0x10000 = secondary countdown expiry),
//   +0x2494 -> work.request_slots_1458[8][16] (0x10-byte slots),
//   +0x1158 -> work.frame_tail_011c (the 0x14-byte TimerNode; its flags
//             dword at +0x10 is the animation-tail arm latch).
//
// The eight request slots at work.request_slots_1458 (0x10 stride, kept RAW
// inside each slot):
//   +0x00 scheduled battle-frame deadline (-1 = empty)
//   +0x04 secondary countdown deadline (-1 = disarmed), relative to the
//         frame_tail_011c count
//   +0x08 the request id returned to the caller (resolved through
//         0x450470 ResolveScriptTableIndexEaxAbi by the call sites)
// ---------------------------------------------------------------------------

namespace {

// The one-time init + unconditional stopped-state arm of the
// frame_tail_011c animation tail, shared by both consume paths (same shape
// as the tail resets documented in EclEasedTransforms.cpp).
void ResetScriptManagerRequestTail(EclScriptWork &work)
{
    u32 flags = work.frame_tail_011c.flags;
    if ((flags & 1U) == 0U) {
        flags |= 1U;
        work.frame_tail_011c.count = 0;
        work.frame_tail_011c.prev = static_cast<i32>(0xFFF0BDC1U); // NaN poison
        work.frame_tail_011c.accum = 0;
        work.frame_tail_011c.rate =
            reinterpret_cast<const float *>(0x476F78U); // &flt_476f78 rate ptr
        work.frame_tail_011c.flags = flags;
    }
    work.frame_tail_011c.count = 0;
    work.frame_tail_011c.accum = 0;
    work.frame_tail_011c.prev = -1; // prev = -1
}

} // namespace

i32 AcquireScriptNameRequestThisAbi(EclScriptObject &obj)
{
    EclScriptWork &work = obj.work;
    // +0x23fc: the boss HP dword doubles as the request timebase natively
    // (AcquireScriptNameRequestThisAbi runs on the ECL script object).
    const i32 battle_timer = work.hp_13c0;

    // First pass: the first slot whose primary deadline is non-negative.
    for (u32 index = 0; index != 8U; ++index) {
        u8 *const slot = work.request_slots_1458[index];
        if (LoadI32(slot, 0) < 0)
            continue;

        // Publish the manager-frame delta regardless of the branch taken
        // (-> work.unknown_13c8, the hit-sound gate value).
        work.unknown_13c8 = battle_timer - LoadI32(slot, 0);
        if (battle_timer > LoadI32(slot, 0))
            break; // deadline not reached; fall into the secondary pass

        // Consume: the deadline becomes the new battle timer (overwriting
        // the hp_13c0 dword; native behavior).
        work.hp_13c0 = LoadI32(slot, 0);
        StoreI32(slot, 0, -1);
        ResetScriptManagerRequestTail(work);
        work.flags_1444 = work.flags_1444 & ~0x10000U;
        return LoadI32(slot, 8);
    }

    // Second pass: slots armed with a secondary countdown (deadline > 0).
    for (u32 index = 0; index != 8U; ++index) {
        u8 *const slot = work.request_slots_1458[index];
        if (LoadI32(slot, 0) < 0 || LoadI32(slot, 4) <= 0)
            continue;

        // HUD countdown in seconds until the secondary deadline, measured
        // against the frame_tail_011c count and clamped at 99 (native
        // signed divide-by-60 magic 0x88888889).
        const i32 countdown
            = (LoadI32(slot, 4) - work.frame_tail_011c.count + 59) / 60;
        reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner)
            ->spell_countdown = countdown > 99 ? 99 : countdown;

        if (work.frame_tail_011c.count < LoadI32(slot, 4))
            return 0; // countdown still running (but the HUD value updated)

        // Consume with the full expiry side effects.
        work.hp_13c0 = LoadI32(slot, 0);
        StoreI32(slot, 0, -1);
        ResetScriptManagerRequestTail(work);
        work.flags_1444 = work.flags_1444 | 0x10000U;

        // Score block +0xc (0x474c4c): subtract 3000, clamp at 5000.
        {
            u8 *const bank = reinterpret_cast<u8 *>(0x474C4CU);
            const i32 decremented = static_cast<i32>(LoadU32(bank, 0U)) - 3000;
            StoreU32(bank, 0U, static_cast<u32>(decremented));
            if (decremented < 5000)
                StoreU32(bank, 0U, 5000U);
        }

        // Spell-bullet-base housekeeping: only when +0x378c bit 0x8 is
        // clear and the +0x3738 counter reached 60, clear bit 1 of the
        // flag dword and of the eight per-VM flag dwords (stride 0x3ac).
        {
            u8 *const base = static_cast<u8 *>(g_SpellBulletBase);
            if ((LoadU32(base, 0x378cU) & 8U) == 0U
                && LoadI32(base, 0x3738U) >= 60) {
                StoreU32(base, 0x3790U, 0U);
                const u32 cleared
                    = LoadU32(base, 0x378cU) & 0xfffffffdU;
                StoreU32(base, 0x378cU, cleared);
                const u32 vm_offsets[7]
                    = { 0xad4U, 0xe80U, 0x15d8U, 0x1984U, 0x1d30U,
                        0x20dcU, 0x2488U };
                for (u32 vm = 0; vm != 7U; ++vm)
                    StoreU32(base, vm_offsets[vm],
                             LoadU32(base, vm_offsets[vm]) & 0xfffffffdU);
            }
        }

        return LoadI32(slot, 8);
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Entity (re)bind helpers
// ---------------------------------------------------------------------------

// TH10 0x0043e8b0 real body. Native usercall: EAX = entity record,
// EDX = the owner list (the pointer stored at entity+0x308), ECX = the
// new VM-table id. Looks the new id up in the owner list's entity/VM
// table at +0x11c, rewires the entity onto that VM record and re-arms
// its timers. The +0x124 non-zero gate suppresses the whole bind.
void BindEntityToScriptSlotEaxEdxEcxAbi(void *entity_memory,
                                        void *owner_list_memory, u32 new_id)
{
    u8 *const entity = static_cast<u8 *>(entity_memory);
    const u8 *const owner = static_cast<const u8 *>(owner_list_memory);

    const u32 *const vm_table =
        *reinterpret_cast<u32 *const *>(LoadPointer(owner, 0x11cU));
    const u32 vm = vm_table[new_id];
    if (vm == 0U)
        return;
    if (LoadU32(owner, 0x124U) != 0U)
        return;

    StoreU16(entity, 0x38aU, static_cast<u16>(new_id));

    u32 flags = LoadU32(entity, 0x35cU);
    if ((flags & 0x200U) != 0U) {
        // Flip the sign of the +0x3c float and move the 0x200 flag bit
        // onto bit 0x8 before the low word gets overwritten below.
        StoreF32(entity, 0x3cU, LoadF32(entity, 0x3cU) * -1.0f);
        flags = (flags | 8U) ^ 0x200U;
        StoreU32(entity, 0x35cU, flags);
    }
    StoreU16(entity, 0x35cU, 7U);

    StoreU32(entity, 0x2fcU, 0xffffffffU);
    StoreU32(entity, 0x60U, 0U);
    StoreU32(entity, 0x64U, 0U);
    StoreU32(entity, 0x5cU, 0xfff0bdc1U); // NaN sentinel of the timer pair
    StoreU32(entity, 0xb4U, 0U);
    StoreU32(entity, 0x100U, 0U);
    StoreU32(entity, 0x12cU, 0U);
    StoreU32(entity, 0x178U, 0U);
    StoreU32(entity, 0x1b4U, 0U);
    StoreU32(entity, 0x200U, 0U);
    StoreU32(entity, 0x22cU, 0U);

    StoreU16(entity, 0x386U,
             static_cast<u16>(LoadU32(owner, 0U) & 0xffffU));

    flags = LoadU32(entity, 0x35cU) & 0xfffff9ffU;
    StoreU32(entity, 0x308U, reinterpret_cast<u32>(owner_list_memory));
    StoreU32(entity, 0x35cU, flags);

    StoreU32(entity, 0x38cU, vm);
    StoreU32(entity, 0x390U, vm);

    // Lazy arm of the +0x5c/+0x60/+0x64 timer triple against the global
    // frame-time scale, then the unconditional reset that overwrites the
    // sentinel with -1.
    if ((LoadU32(entity, 0x6cU) & 1U) == 0U) {
        StoreU32(entity, 0x60U, 0U);
        StoreU32(entity, 0x5cU, 0xfff0bdc1U);
        StoreU32(entity, 0x64U, 0U);
        StoreU32(entity, 0x68U,
                 reinterpret_cast<u32>(&g_FrameTimeScale));
        StoreU32(entity, 0x6cU, LoadU32(entity, 0x6cU) | 1U);
    }
    StoreU32(entity, 0x60U, 0U);
    StoreU32(entity, 0x64U, 0U);
    StoreU32(entity, 0x5cU, 0xffffffffU);

    flags = LoadU32(entity, 0x35cU) & 0xfffffffeU;
    StoreU32(entity, 0x35cU, flags);

    // Run the freshly bound VM body once, then count the bind on the
    // render owner (+0x4c).
    (void)FinalizeTimelineRenderObjectSetup(entity);
    StoreU32(reinterpret_cast<u8 *>(g_MainChainRenderOwner), 0x4cU,
             LoadU32(reinterpret_cast<const u8 *>(g_MainChainRenderOwner),
                     0x4cU) + 1U);
}

// TH10 0x004496d0 real body. Native EAX = id slot, stack =
// new id (`ret 4`). Resolves the entity currently carrying the slot's id
// and rebinds it to the new VM-table entry; a failed lookup is silent.
void RebindEntitySlotEaxStackAbi(u32 *id_slot, u32 new_id)
{
    u8 *const entity = FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                             *id_slot);
    if (entity == 0)
        return;
    BindEntityToScriptSlotEaxEdxEcxAbi(
        entity, reinterpret_cast<void *>(LoadU32(entity, 0x308U)), new_id);
}

} // namespace th10
