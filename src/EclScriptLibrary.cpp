#include "EclScriptLibrary.hpp"

#include "EclScriptVm.hpp"
#include "EntityHelpers.hpp"
#include "GameContext.hpp"
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

// Replaces one bit in the +0x2480 flag dword with an incoming bit.
void ReplaceFlagBit(u8 *record, u32 bit, u32 incoming)
{
    const u32 flags = LoadU32(record, 0x2480U);
    StoreU32(record, 0x2480U, (flags & ~bit) | (incoming ? bit : 0U));
}

// Float/int helpers over the same little-endian byte layout.
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

// ECL script object field offsets are relative to the +0x1044 sub-record
// passed to RunEclScriptSetupStackAbi; this helper dereferences a pointer
// field of that sub-record.
void StoreU16(u8 *record, u32 offset, u16 value)
{
    record[offset] = static_cast<u8>(value);
    record[offset + 1] = static_cast<u8>(value >> 8);
}

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

// TH10 0x004127a0. Native ECX (this) = script manager. Pops the next
// pending script-name request (see the body below); implemented at the end
// of this file.
i32 AcquireScriptNameRequestThisAbi(void *script_manager);

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
    StoreU32(record, 0x1094U, descriptor[0]);
    StoreU32(record, 0x1098U, descriptor[1]);
    StoreU32(record, 0x109cU, descriptor[2]);
    StoreU32(record, 0x23f8U, descriptor[3]);
    StoreU32(record, 0x23fcU, descriptor[5]);
    StoreU32(record, 0x2408U, descriptor[4]);
    ReplaceFlagBit(record, 0x800U, descriptor[6] & 1U);

    const u32 difficulty = ReadGlobalU32(k_difficulty_index);
    record[0x1024U] = static_cast<u8>(1U << difficulty);

    for (u32 i = 0; i < 0x20U; ++i) {
        record[0x1138U + i] =
            reinterpret_cast<const u8 *>(descriptor + 8)[i];
    }

    // Score-anim block: initialize once (flag bit 0 at +0x2468), then
    // unconditionally arm it with mode 2 and the 2.0f rate.
    const u32 anim_state = LoadU32(record, 0x2468U);
    if ((anim_state & 1U) == 0U) {
        StoreU32(record, 0x245cU, 0U);
        StoreU32(record, 0x2458U, static_cast<u32>(-999999));
        StoreU32(record, 0x2460U, 0U);
        StoreU32(record, 0x2464U, 0x476f78U); // &flt_476F78
        StoreU32(record, 0x2468U, anim_state | 1U);
    }
    StoreU32(record, 0x245cU, 2U);
    StoreU32(record, 0x2460U, 0x40000000U);
    StoreU32(record, 0x2458U, 1U);

    ReplaceFlagBit(record, 0x40000U, descriptor[7] & 1U);

    RunEclScriptSetupStackAbi(record + 0x103cU);

    // Bit 0x8000 of the flag dword remaps the +0x2408 kind (1 -> 10,
    // 4 -> 11).
    if ((LoadU32(record, 0x2480U) & 0x8000U) != 0U) {
        u32 kind = LoadU32(record, 0x2408U);
        if (kind == 1U) {
            kind = 10U;
        } else if (kind == 4U) {
            kind = 11U;
        }
        StoreU32(record, 0x2408U, kind);
    }

    // Presentation pair: par-count kind from the owner list parity and a
    // per-file accent value driven by the descriptor's embedded kind pair
    // (only when record+0x1128 == 1).
    u8 *const owner = static_cast<u8 *>(list_owner);
    StoreU32(record, 0x2444U,
             (LoadU32(owner, 0x64U) & 1U) + 2U);
    StoreU32(record, 0x2448U, 359U);
    if (LoadU32(record, 0x1128U) == 1U) {
        switch (LoadU32(record, 0x112cU)) {
        case 0x00U:
        case 0x14U:
        case 0x31U:
            StoreU32(record, 0x2448U, 359U);
            break;
        case 0x05U:
        case 0x19U:
        case 0x32U:
            StoreU32(record, 0x2448U, 356U);
            break;
        case 0x0aU:
        case 0x1eU:
        case 0x33U:
            StoreU32(record, 0x2448U, 362U);
            break;
        case 0x0fU:
        case 0x23U:
            StoreU32(record, 0x2448U, 365U);
            break;
        default:
            break;
        }
    }
    StoreU32(record, 0x244cU, 0U);

    // Append the embedded list node (record+0x116c; next at +0x1170, prev
    // at +0x1174) after the owner's current last node.
    u8 *const node = record + 0x116cU;
    const u32 head = LoadU32(owner, 0x58U);
    if (head != 0U) {
        const u32 last = LoadU32(owner, 0x5cU);
        const u32 last_next = LoadU32(reinterpret_cast<const u8 *>(last), 4U);
        if (last_next != 0U) {
            StoreU32(node, 4U, last_next);
            StoreU32(reinterpret_cast<u8 *>(last_next), 8U,
                     reinterpret_cast<u32>(node));
        }
        StoreU32(reinterpret_cast<u8 *>(last), 4U,
                 reinterpret_cast<u32>(node));
        StoreU32(node, 8U, last);
    } else {
        StoreU32(owner, 0x58U, reinterpret_cast<u32>(node));
    }
    StoreU32(owner, 0x5cU, reinterpret_cast<u32>(node));
    StoreU32(owner, 0x60U, LoadU32(owner, 0x60U) + 1U);
    StoreU32(owner, 0x64U, LoadU32(owner, 0x64U) + 1U);

    return record;
}

// FUNCTION: TH10 0x0040dc80
// Native ABI: one stack argument (ret 4) = the +0x1044 sub-record of an ECL
// script object. All offsets below are relative to that sub-record ("rec").
// Motion blocks follow the shared {pos xyz @0, vel xyz @0xc, radius @0x18,
// angle @0x1c, flags @0x28} layout used by IntegrateSubEffectPositionEsiAbi.
i32 RunEclScriptSetupStackAbi(void *sub_record)
{
    u8 *const rec = static_cast<u8 *>(sub_record);

    // Run gate: bit 0x400 of +0x1444. A second invocation is a no-op that
    // returns 0; otherwise the bit is armed before anything else runs.
    u32 flags = LoadU32(rec, 0x1444U);
    if ((flags & 0x400U) != 0U)
        return 0;
    StoreU32(rec, 0x1444U, flags | 0x400U);
    flags |= 0x400U;

    // Refresh the working block (rec+0x00, 0x2c bytes) from the base block
    // at +0x2c before any animation output is applied.
    for (u32 i = 0; i < 0x2cU; ++i)
        rec[i] = rec[0x2cU + i];

    // Four 0x3c-byte vec2 animation blocks, armed by the duration dword at
    // +0x34 of each (stride 0x3c):
    //   A0 +0x1d4, A1 +0x210, A2 +0x24c, A3 +0x288. Outputs:
    //   A0 -> angle at +0x74 (wrapped), slope at +0x70
    //   A1 -> angle at +0xa0 (wrapped), slope at +0x9c
    //   A2 -> radius slope at +0x78 / +0x7c (direct pair)
    //   A3 -> radius slope at +0xa4 / +0xa8 (direct pair)
    float anim_out[2];
    if (LoadU32(rec, 0x208U) != 0U) {
        TickVec2AnimInterpolator(anim_out, rec + 0x1d4U);
        StoreF32(rec, 0x74U, WrapAngleToPi(anim_out[0]));
        StoreF32(rec, 0x70U, anim_out[1]);
    }
    if (LoadU32(rec, 0x280U) != 0U) {
        TickVec2AnimInterpolator(anim_out, rec + 0x24cU);
        StoreF32(rec, 0x78U, anim_out[0]);
        StoreF32(rec, 0x7cU, anim_out[1]);
    }
    if (LoadU32(rec, 0x244U) != 0U) {
        TickVec2AnimInterpolator(anim_out, rec + 0x210U);
        StoreF32(rec, 0xa0U, WrapAngleToPi(anim_out[0]));
        StoreF32(rec, 0x9cU, anim_out[1]);
    }
    if (LoadU32(rec, 0x2bcU) != 0U) {
        TickVec2AnimInterpolator(anim_out, rec + 0x288U);
        StoreF32(rec, 0xa4U, anim_out[0]);
        StoreF32(rec, 0xa8U, anim_out[1]);
    }

    // Motion block 1 (pos +0x58, vel +0x64, radius +0x70, angle +0x74,
    // flags +0x80). Velocity source: a vec3 animation at +0x13c (armed by
    // +0x180) yielding a target position, else the radius/angle pair.
    if (LoadU32(rec, 0x180U) != 0U) {
        float target[3];
        TickVec3Interpolator(rec + 0x13cU, target);
        StoreF32(rec, 0x64U, target[0] - LoadF32(rec, 0x58U));
        StoreF32(rec, 0x68U, target[1] - LoadF32(rec, 0x5cU));
        StoreF32(rec, 0x6cU, target[2] - LoadF32(rec, 0x60U));
    } else if ((LoadU32(rec, 0x80U) & 1U) != 0U) {
        StoreF32(rec, 0x78U, LoadF32(rec, 0x78U) + LoadF32(rec, 0x7cU));
        StoreF32(rec, 0x74U, WrapAngleToPi(LoadF32(rec, 0x70U) +
                                           LoadF32(rec, 0x74U)));
    } else {
        PolarToCartesianEdiAbi(rec + 0x64U, LoadF32(rec, 0x74U),
                               LoadF32(rec, 0x70U));
        StoreU32(rec, 0x6cU, 0U);
    }

    // Motion block 2 (pos +0x84, vel +0x90, radius +0x9c, angle +0xa0,
    // flags +0xac). Same three modes; the vec3 animation lives at +0x188
    // (armed by +0x1cc).
    if (LoadU32(rec, 0x1ccU) != 0U) {
        float target[3];
        TickVec3Interpolator(rec + 0x188U, target);
        StoreF32(rec, 0x90U, target[0] - LoadF32(rec, 0x84U));
        StoreF32(rec, 0x94U, target[1] - LoadF32(rec, 0x88U));
        StoreF32(rec, 0x98U, target[2] - LoadF32(rec, 0x8cU));
    } else if ((LoadU32(rec, 0xacU) & 1U) != 0U) {
        StoreF32(rec, 0xa4U, LoadF32(rec, 0xa4U) + LoadF32(rec, 0xa8U));
        StoreF32(rec, 0xa0U, WrapAngleToPi(LoadF32(rec, 0x9cU) +
                                           LoadF32(rec, 0xa0U)));
    } else {
        PolarToCartesianEdiAbi(rec + 0x90U, LoadF32(rec, 0xa0U),
                               LoadF32(rec, 0x9cU));
        StoreU32(rec, 0x98U, 0U);
    }

    // Integrate block 1; with flag bit 0x40000 the block-2 position is
    // shifted by the global base-position offsets before its integration.
    IntegrateSubEffectPositionEsiAbi(rec + 0x58U);
    if ((flags & 0x40000U) != 0U) {
        StoreF32(rec, 0x84U, g_EclBasePosShift[0] + LoadF32(rec, 0x84U));
        StoreF32(rec, 0x88U, g_EclBasePosShift[1] + LoadF32(rec, 0x88U));
        StoreF32(rec, 0x8cU, g_EclBasePosShift[2] + LoadF32(rec, 0x8cU));
    }
    IntegrateSubEffectPositionEsiAbi(rec + 0x84U);

    // The base block's velocity is the summed position delta, and the base
    // block itself is integrated afterwards.
    StoreF32(rec, 0x38U,
             LoadF32(rec, 0x84U) + LoadF32(rec, 0x58U) - LoadF32(rec, 0x2cU));
    StoreF32(rec, 0x3cU,
             LoadF32(rec, 0x88U) + LoadF32(rec, 0x5cU) - LoadF32(rec, 0x30U));
    StoreF32(rec, 0x40U,
             LoadF32(rec, 0x8cU) + LoadF32(rec, 0x60U) - LoadF32(rec, 0x34U));
    IntegrateSubEffectPositionEsiAbi(rec + 0x2cU);

    // Flag bit 0x200: clamp the base position into the rectangle centered
    // at (+0x13ac, +0x13b0) with half extents (+0x13b4, +0x13b8), then
    // re-derive block-1's position as base - block2.
    if ((flags & 0x200U) != 0U) {
        const float half_x = LoadF32(rec, 0x13b4U) * 0.5f;
        const float lo_x = LoadF32(rec, 0x13acU) - half_x;
        if (lo_x <= LoadF32(rec, 0x2cU)) {
            const float hi_x = half_x + LoadF32(rec, 0x13acU);
            if (hi_x < LoadF32(rec, 0x2cU))
                StoreF32(rec, 0x2cU, hi_x);
        } else {
            StoreF32(rec, 0x2cU, lo_x);
        }
        const float half_y = LoadF32(rec, 0x13b8U) * 0.5f;
        const float lo_y = LoadF32(rec, 0x13b0U) - half_y;
        if (lo_y <= LoadF32(rec, 0x30U)) {
            const float hi_y = half_y + LoadF32(rec, 0x13b0U);
            if (hi_y < LoadF32(rec, 0x30U))
                StoreF32(rec, 0x30U, hi_y);
        } else {
            StoreF32(rec, 0x30U, lo_y);
        }
        StoreF32(rec, 0x58U, LoadF32(rec, 0x2cU) - LoadF32(rec, 0x84U));
        StoreF32(rec, 0x5cU, LoadF32(rec, 0x30U) - LoadF32(rec, 0x88U));
        StoreF32(rec, 0x60U, LoadF32(rec, 0x34U) - LoadF32(rec, 0x8cU));
    }

    // Playfield gate: the base position must sit inside x=[-192,192] and
    // y=[0,448] (adjusted by the +0x13a4/+0x13a8 half extents). Leaving the
    // region without the 0x4 stay flag aborts when the 0x100 latch was
    // already set; entering it sets the latch.
    const float half_w = LoadF32(rec, 0x13a4U) * 0.5f;
    const float half_h = LoadF32(rec, 0x13a8U) * 0.5f;
    const float pos_x = LoadF32(rec, 0x2cU);
    const float pos_y = LoadF32(rec, 0x30U);
    const bool inside = pos_x + half_w >= -192.0f &&
                        pos_x - half_w <= 192.0f &&
                        pos_y + half_h >= 0.0f && pos_y - half_h <= 448.0f;
    if (!inside) {
        if ((flags & 0x100U) != 0U && (flags & 0x4U) == 0U)
            return -1;
    } else {
        flags |= 0x100U;
        StoreU32(rec, 0x1444U, flags);
    }

    // Flag bit 0x100000: publish the bind id (+0x1448 or, with bit 0x200000,
    // +0x144c) into +0xf4 and rebind the first entity slot. With no active
    // stage node the 0x200000 path clears bits 0x200000|1 instead.
    if ((flags & 0x100000U) != 0U) {
        // g_StageNode holds the 0x48-byte game context (DAT_004776ec); the
        // +0x28 popup-state dword doubles as the stage-active gate.
        GameContext &stage_ctx = *static_cast<GameContext *>(g_StageNode);
        const u32 stage_active = stage_ctx.popup_state;
        if (stage_active == 0U) {
            if ((flags & 0x200000U) != 0U) {
                const u32 id = LoadU32(rec, 0x144cU);
                StoreU32(rec, 0xf4U, id);
                RebindEntitySlotEaxStackAbi(reinterpret_cast<u32 *>(rec + 0xc0U),
                                            id);
                StoreU32(rec, 0x1444U, LoadU32(rec, 0x1444U) & 0xffdffffeU);
            }
        } else if ((flags & 0x200000U) == 0U) {
            const u32 id = LoadU32(rec, 0x1448U);
            StoreU32(rec, 0xf4U, id);
            RebindEntitySlotEaxStackAbi(reinterpret_cast<u32 *>(rec + 0xc0U),
                                        id);
            StoreU32(rec, 0x1444U, LoadU32(rec, 0x1444U) | 0x200001U);
        }
    }

    // Entity-list scan over the script manager; a nonzero result aborts.
    void *const script_manager = LoadPointer(rec, 0x14d8U);
    const float scan_value = LoadF32(LoadPointer(rec, 0x128U), 0U);
    if (RunEclContextListEdiStackAbi(script_manager, scan_value) != 0)
        return -1;

    // Clear the flicker-arm bit, then decide the pass.
    flags = LoadU32(rec, 0x1444U) & 0xffffdfffU;
    StoreU32(rec, 0x1444U, flags);
    if ((flags & 0x11U) != 0U)
        goto post_damage_pass;

    {
        // Damage pass: run the item collision over the 128 player-item
        // records (this = null at the native call site), scale it down on
        // modes 0/2, and drain the boss HP at +0x13c0.
        i32 damage = CollectItemCollisionsStackAbi(
            g_PlayerStateBlock,
            reinterpret_cast<const float *>(rec + 0x2cU),
            reinterpret_cast<const float *>(rec + 0xb0U));
        const u32 player_mode = LoadU32(
            reinterpret_cast<const u8 *>(g_PlayerStateBlock), 0x458U);
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
        if ((flags & 0x8U) == 0U && LoadI32(rec, 0x1420U) <= 0)
            StoreI32(rec, 0x13c0U, LoadI32(rec, 0x13c0U) - damage);

        // Pending script-name request handling.
        const i32 request = AcquireScriptNameRequestThisAbi(script_manager);
        if (request != 0) {
            UpdateScriptManagerAEaxAbi(script_manager);
            UpdateScriptManagerBEaxAbi(script_manager);
            const i32 resolved = ResolveScriptTableIndexEaxAbi(request);
            u8 *const bind_buffer = LoadPointer(static_cast<const u8 *>(script_manager), 4U);
            StoreU32(bind_buffer, 4U, static_cast<u32>(resolved));
            StoreU32(bind_buffer, 0U, 0U);
            if (RunEclContextListEdiStackAbi(script_manager,
                                                 scan_value) != 0)
                return -1;
        }

        // Kill/score handling: without flag bit 0x40, a drained HP adds the
        // +0x13c8 score value and can run the death sequence.
        if ((flags & 0x40U) == 0U && LoadI32(rec, 0x13c0U) <= 0) {
            AddScoreBlockValueEcxStackAbi(reinterpret_cast<void *>(0x474c40U),
                                          LoadI32(rec, 0x13c8U));
            if (TriggerEnemyDeathSequenceStdcallAbi(script_manager) != 0)
                return 1;
        }
        flags |= 0x2000U;
        StoreU32(rec, 0x1444U, flags);
        g_BossDefeatedFlag = 1;
    }

post_damage_pass:
    {
        // Pending script-name request handling (shared tail).
        const i32 request = AcquireScriptNameRequestThisAbi(script_manager);
        if (request != 0) {
            UpdateScriptManagerAEaxAbi(script_manager);
            UpdateScriptManagerBEaxAbi(script_manager);
            u8 *const bind_buffer = LoadPointer(static_cast<const u8 *>(script_manager), 4U);
            StoreU32(bind_buffer, 4U,
                     static_cast<u32>(ResolveScriptTableIndexEaxAbi(request)));
            StoreU32(bind_buffer, 0U, 0U);
        }

        // Timeout region check; skipped with flag bits 0x2|0x10 or a
        // positive +0x1434 timer count.
        if ((flags & 0x12U) == 0U && LoadI32(rec, 0x1434U) <= 0)
            CheckEnemyTimeoutRegionEaxEdxEcxAbi(
                reinterpret_cast<const float *>(rec + 0x2cU),
                g_PlayerStateBlock,
                reinterpret_cast<const float *>(rec + 0xb0U));

        // Flag bit 0x1000: direction state machine over the base-block X
        // delta, driving animation-id switches through +0xf4.
        if ((flags & 0x1000U) != 0U) {
            const float dx = LoadF32(rec, 0x38U);
            const i32 direction =
                dx < -0.1f ? -1 : (dx > 0.1f ? 1 : 0);
            const i32 previous = LoadI32(rec, 0xf8U);
            if (previous != direction) {
                i32 step = 0;
                if (previous == -1)
                    step = 3 - (direction != 0 ? 1 : 0);
                else if (previous == 0)
                    step = (direction != -1 ? 1 : 0) + 1;
                else if (previous == 1)
                    step = direction != 0 ? 1 : 4;
                StoreI32(rec, 0xf8U, direction);
                RebindEntitySlotEaxStackAbi(
                    reinterpret_cast<u32 *>(rec + 0xc0U),
                    LoadU32(rec, 0xf4U) + static_cast<u32>(step));
            }
        }

        // Publish the base-block position into the eight entity id slots at
        // +0xc0; flag bit 0x40000 selects the verbatim over the offset path.
        const float position[3] = {LoadF32(rec, 0x2cU), LoadF32(rec, 0x30U),
                                   LoadF32(rec, 0x34U)};
        for (u32 slot = 0; slot != 8U; ++slot) {
            const u32 id = LoadU32(rec, 0xc0U + slot * 4U);
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
            u8 *const player = static_cast<u8 *>(g_PlayerStateBlock);
            const u8 *const old_target = LoadPointer(player, 0x3504U);
            const float player_x = LoadF32(player, 0x3c0U);
            const float new_distance = LoadF32(rec, 0x2cU) - player_x;
            bool farther = old_target == 0;
            if (!farther) {
                const float old_distance =
                    LoadF32(old_target, 0x1068U) - player_x;
                farther = (new_distance < 0.0f ? -new_distance
                                               : new_distance) >
                          (old_distance < 0.0f ? -old_distance
                                               : old_distance);
            }
            if (farther) {
                if (player[0x3508U] == 0U)
                    StoreU32(player, 0x3504U,
                             reinterpret_cast<u32>(script_manager));
                player[0x3508U] = 1U;
            }
        }

        // Resolve the primary entity slot and drive the hit-flicker state.
        u8 *entity = FindEntityEdxStackAbi(
            g_MainChainRenderOwner, LoadU32(rec, 0xc0U));
        if (entity == 0)
            StoreU32(rec, 0xc0U, 0U);
        if (LoadI32(rec, 0x1414U) != 0) {
            // Native quirk: the entity pointer is dereferenced even when
            // the lookup failed (unchecked pointer, preserved).
            StoreU32(entity, 0x35cU,
                     LoadU32(entity, 0x35cU) & 0xffff7fffU);
            if ((flags & 0x8000U) != 0U) {
                u8 *const hud = static_cast<u8 *>(g_AsciiHudOwner);
                StoreU32(hud, 0x9da4U,
                         LoadU32(hud, 0x9da4U) & 0xffff7fffU);
            }
            StoreI32(rec, 0x1414U, LoadI32(rec, 0x1414U) - 1);
        } else if ((flags & 0x2000U) != 0U) {
            StoreU32(entity, 0x35cU, LoadU32(entity, 0x35cU) | 0x8000U);
            StoreU32(entity, 0x300U, 0xff0000ffU);
            StoreI32(rec, 0x1414U, 4);
            if ((flags & 0x8000U) != 0U) {
                // Loud hit sound gated by the stage flags word and the
                // manager timer at +0x2404; otherwise the quiet variant.
                bool loud = false;
                if (LoadI32(static_cast<const u8 *>(script_manager), 0x2404U) < 900) {
                    const u32 stage_flags =
                        LoadU32(static_cast<const u8 *>(g_SpellBulletBase),
                                0x378cU);
                    if ((stage_flags & 1U) == 0U)
                        loud = true;
                    else if ((stage_flags & 8U) == 0U &&
                             LoadI32(static_cast<const u8 *>(script_manager), 0x2404U) < 300)
                        loud = true;
                }
                EnqueueSoundEffectEbxStackAbi(
                    loud ? 0x23U : 0x13U,
                    reinterpret_cast<void *>(0x492590U),
                    LoadF32(rec, 0x2cU));
            } else {
                EnqueueSoundEffectEbxStackAbi(
                    0x13U, reinterpret_cast<void *>(0x492590U),
                    LoadF32(rec, 0x2cU));
            }
        }

        // Two shift timers (blocks at +0x141c and +0x1430) tick down while
        // their counts at +0x1420/+0x1434 are positive.
        if (LoadI32(rec, 0x1420U) > 0)
            ShiftTimerByEsiStackAbi(rec + 0x141cU, -1.0f);
        if (LoadI32(rec, 0x1434U) > 0)
            ShiftTimerByEsiStackAbi(rec + 0x1430U, -1.0f);

        // Frame counter advance over the {prev +0x11c, count +0x120,
        // accumulator +0x124, rate pointer +0x128} block. Outside the
        // 0.99..1.01 rate window the count re-derives from the accumulator;
        // the native conversion truncates through an unsigned 64-bit path.
        const u32 current = LoadU32(rec, 0x120U);
        StoreU32(rec, 0x11cU, current);
        const float rate = LoadF32(LoadPointer(rec, 0x128U), 0U);
        if (rate <= 0.99f || rate >= 1.01f) {
            const float accumulated = rate + LoadF32(rec, 0x124U);
            StoreF32(rec, 0x124U, accumulated);
            StoreU32(rec, 0x120U, static_cast<u32>(static_cast<i32>(
                                      accumulated)));
        } else {
            StoreF32(rec, 0x124U, LoadF32(rec, 0x124U) + 1.0f);
            StoreU32(rec, 0x120U, current + 1U);
        }
    }
    return 0;
}

// TH10 0x00412ac0. Native EDI = out vec2, ESI = the 0x3c-byte vec2
// animation block {cur[2]@0x00, end[2]@0x08, handle1[2]@0x10,
// velocity/handle2[2]@0x18, timer {prev@0x20, cur@0x24, accum@0x28,
// rate ptr@0x2c, flags@0x30}, duration@0x34, mode@0x38}. Same timer
// behavior as the vec3 interpolator (unity rate window, 0xfff0bdc1 poison,
// rate reset to DAT_00476f78), except the completion additionally zeroes
// the duration and the vec2 Hermite gives handle2 the proper (t-1)*t^2
// basis. Duration <= 0 skips the timer and interpolates with
// t = accum / duration, so a zero duration yields inf/NaN. Mode 7 adds the
// end pair into cur, 0x11 integrates velocity, 8 rides the cubic Hermite,
// and every other mode eases through the 0x44c350 curve selector.
void TickVec2AnimInterpolator(float out_vec2[2], void *block_memory)
{
    u8 *const block = static_cast<u8 *>(block_memory);
    const i32 duration =
        *reinterpret_cast<const i32 *>(block + 0x34);
    if (duration > 0) {
        *reinterpret_cast<i32 *>(block + 0x20) =
            *reinterpret_cast<const i32 *>(block + 0x24);
        const float rate = **reinterpret_cast<float *const *>(block + 0x2c);
        if (rate > 0.99f && rate < 1.01f) {
            *reinterpret_cast<float *>(block + 0x28) =
                *reinterpret_cast<float *>(block + 0x28) + 1.0f;
            *reinterpret_cast<i32 *>(block + 0x24) =
                *reinterpret_cast<const i32 *>(block + 0x24) + 1;
        } else {
            const float accum = *reinterpret_cast<float *>(block + 0x28) +
                rate;
            *reinterpret_cast<float *>(block + 0x28) = accum;
            *reinterpret_cast<i32 *>(block + 0x24) = FloatToI32(accum);
        }
        if (*reinterpret_cast<const i32 *>(block + 0x24) >= duration) {
            if ((*reinterpret_cast<u32 *>(block + 0x30) & 1U) == 0U) {
                *reinterpret_cast<i32 *>(block + 0x24) = 0;
                *reinterpret_cast<i32 *>(block + 0x20) =
                    static_cast<i32>(0xfff0bdc1U);
                *reinterpret_cast<float *>(block + 0x28) = 0.0f;
                *reinterpret_cast<float **>(block + 0x2c) =
                    &g_FrameTimeScale;
                *reinterpret_cast<u32 *>(block + 0x30) |= 1U;
            }
            *reinterpret_cast<i32 *>(block + 0x24) = duration;
            *reinterpret_cast<i32 *>(block + 0x20) = duration - 1;
            *reinterpret_cast<float *>(block + 0x28) =
                static_cast<float>(duration);
            *reinterpret_cast<i32 *>(block + 0x34) = 0;
            const u32 source =
                *reinterpret_cast<const i32 *>(block + 0x38) == 7 ? 0 : 8;
            out_vec2[0] = *reinterpret_cast<const float *>(block + source);
            out_vec2[1] = *reinterpret_cast<const float *>(block + source + 4);
            return;
        }
    }

    const i32 mode = *reinterpret_cast<const i32 *>(block + 0x38);
    const float t = *reinterpret_cast<float *>(block + 0x28) /
                    static_cast<float>(duration);
    for (u32 component = 0; component != 2U; ++component) {
        const float cur = *reinterpret_cast<const float *>(block +
            component * 4);
        const float end = *reinterpret_cast<const float *>(block + 8 +
            component * 4);
        float value;
        if (mode == 7) {
            value = cur + end;
            *reinterpret_cast<float *>(block + component * 4) = value;
        } else if (mode == 0x11) {
            value = cur + *reinterpret_cast<const float *>(block + 0x18 +
                component * 4);
            *reinterpret_cast<float *>(block + component * 4) = value;
            *reinterpret_cast<float *>(block + 0x18 + component * 4) =
                *reinterpret_cast<const float *>(block + 0x18 +
                    component * 4) + end;
        } else if (mode == 8) {
            const float w_start = (1.0f + 2.0f * t) * (t - 1.0f) * (t - 1.0f);
            const float w_end = (3.0f - 2.0f * t) * t * t;
            const float w_handle1 = (1.0f - t) * (1.0f - t) * t;
            const float w_handle2 = (t - 1.0f) * t * t;
            value = w_start * cur + w_end * end +
                w_handle1 * *reinterpret_cast<const float *>(block + 0x10 +
                    component * 4) +
                w_handle2 * *reinterpret_cast<const float *>(block + 0x18 +
                    component * 4);
        } else {
            const double factor = EasingCurveSelectorEaxStackAbi(
                mode, *reinterpret_cast<float *>(block + 0x28),
                static_cast<float>(duration));
            value = cur + (end - cur) * static_cast<float>(factor);
        }
        out_vec2[component] = value;
    }
}

// ---------------------------------------------------------------------------
// TH10 0x004127a0 - pending script-name request popper.
//
// The script manager holds eight request slots at +0x2494 (0x10 stride):
//   +0x00 scheduled battle-frame deadline (-1 = empty)
//   +0x04 secondary countdown deadline (-1 = disarmed), relative to the
//         +0x115c animation-tail timer
//   +0x08 the request id returned to the caller (resolved through
//         0x450470 ResolveScriptTableIndexEaxAbi by the call sites)
// Related manager state: the battle timer at +0x23fc, the deadline delta
// published at +0x2404, the +0x1158 animation tail (flags at +0x1168), and
// the +0x2480 flag dword (bit 0x10000 = secondary countdown expiry).
// ---------------------------------------------------------------------------

namespace {

// The one-time init + unconditional stopped-state arm of the +0x1158
// animation tail, shared by both consume paths (same shape as the tail
// resets documented in EclEasedTransforms.cpp).
void ResetScriptManagerRequestTail(u8 *mgr)
{
    u32 flags = LoadU32(mgr, 0x1168U);
    if ((flags & 1U) == 0U) {
        flags |= 1U;
        StoreU32(mgr, 0x115cU, 0U);
        StoreU32(mgr, 0x1158U, 0xFFF0BDC1U); // NaN poison
        StoreU32(mgr, 0x1160U, 0U);
        StoreU32(mgr, 0x1164U, 0x476F78U); // &flt_476f78 rate pointer
        StoreU32(mgr, 0x1168U, flags);
    }
    StoreU32(mgr, 0x115cU, 0U);
    StoreU32(mgr, 0x1160U, 0U);
    StoreU32(mgr, 0x1158U, 0xFFFFFFFFU); // prev = -1
}

} // namespace

i32 AcquireScriptNameRequestThisAbi(void *script_manager)
{
    u8 *const mgr = static_cast<u8 *>(script_manager);
    const i32 battle_timer = LoadI32(mgr, 0x23fcU);

    // First pass: the first slot whose primary deadline is non-negative.
    for (u32 index = 0; index != 8U; ++index) {
        const u32 slot = 0x2494U + index * 0x10U;
        if (LoadI32(mgr, slot) < 0)
            continue;

        // Publish the manager-frame delta regardless of the branch taken.
        StoreI32(mgr, 0x2404U, battle_timer - LoadI32(mgr, slot));
        if (battle_timer > LoadI32(mgr, slot))
            break; // deadline not reached; fall into the secondary pass

        // Consume: the deadline becomes the new battle timer.
        StoreI32(mgr, 0x23fcU, LoadI32(mgr, slot));
        StoreI32(mgr, slot, -1);
        ResetScriptManagerRequestTail(mgr);
        StoreU32(mgr, 0x2480U, LoadU32(mgr, 0x2480U) & ~0x10000U);
        return LoadI32(mgr, slot + 8U);
    }

    // Second pass: slots armed with a secondary countdown (deadline > 0).
    for (u32 index = 0; index != 8U; ++index) {
        const u32 slot = 0x2494U + index * 0x10U;
        if (LoadI32(mgr, slot) < 0 || LoadI32(mgr, slot + 4U) <= 0)
            continue;

        // HUD countdown in seconds until the secondary deadline, measured
        // against the +0x115c tail timer and clamped at 99 (native signed
        // divide-by-60 magic 0x88888889).
        const i32 countdown
            = (LoadI32(mgr, slot + 4U) - LoadI32(mgr, 0x115cU) + 59) / 60;
        StoreU32(static_cast<u8 *>(g_AsciiHudOwner), 0x9ec0U,
                 static_cast<u32>(countdown > 99 ? 99 : countdown));

        if (LoadI32(mgr, 0x115cU) < LoadI32(mgr, slot + 4U))
            return 0; // countdown still running (but the HUD value updated)

        // Consume with the full expiry side effects.
        StoreI32(mgr, 0x23fcU, LoadI32(mgr, slot));
        StoreI32(mgr, slot, -1);
        ResetScriptManagerRequestTail(mgr);
        StoreU32(mgr, 0x2480U, LoadU32(mgr, 0x2480U) | 0x10000U);

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

        return LoadI32(mgr, slot + 8U);
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
