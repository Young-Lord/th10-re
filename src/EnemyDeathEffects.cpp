#include "EnemyDeathEffects.hpp"

#include <math.h>

#include "PlayerFrameworkHelpers.hpp"
#include "PlayerMotionHelpers.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

// TH10 globals (addresses used directly, matching sibling modules).
const u32 k_explosion_manager_slot = 0x477818U; // DAT_004777818 (pointer slot)
const u32 k_sound_manager = 0x492590U;          // sound manager object
const u32 k_death_effect_table = 0x477704U;     // DAT_004777704 pointer holder
const u32 k_score_popup_block = 0x474c40U;      // score popup state block

// TH10 0x0043dd10. Native ESI = sound manager (0x492590), stack = float x;
// pushes the positional enemy-death sound request.
void EnqueueDeathSoundEsiStackAbi(void *sound_manager, float x);

// TH10 0x00448db0. Native EDI = position pair {x, y}, stack = {effect table
// entry (0x477704[idx]+0x30), effect id, script manager}; allocates a render
// VM record, seeds the position (+1/256, -0.5, z) with flag 0x40000000,
// binds the effect script and registers it.
void SpawnDeathEffectVmEdiStackAbi(const float *position, void *table_entry,
                                   u32 effect_id, void *script_manager);

// TH10 0x00412ff0. Native EDI = score popup block (0x474c40), stack = i32
// amount; ticks the popup counter and re-arms the 130-step animation.
void TickScorePopupEdiStackAbi(u32 *score_block, i32 amount);

// TH10 0x00413270 (sub_413270). thiscall ECX = out pair, stack = {angle,
// radius_x, radius_y}: out = {cos(angle) * radius_x, sin(angle) * radius_y}
// via the x87 fsincos sequence.
void ComputeScatterOffset(float out[2], float angle, float radius_x,
                          float radius_y)
{
    out[0] = cos(angle) * radius_x;
    out[1] = sin(angle) * radius_y;
}

const float k_death_angle = -1.5707964f; // 0xbfc90fdb
const float k_death_speed = 2.2f;        // 0x400ccccd

// Raw 32-bit draw of the 0x4918b0 LCG pair (inline sequences inside
// 0x40c9d0; see EclScriptVm.cpp for the half-step description). Native
// oddity preserved: the combined value duplicates the second half.
u32 LcgDrawRaw32Duplicated()
{
    extern u16 g_TimelinePrngStateB[4]; // TH10 DAT_004918b0, +0x4918b4 counter
    u32 x = (static_cast<u32>(*g_TimelinePrngStateB) ^ 0x9630U) - 0x6553U;
    const u32 h1 = ((x >> 14) & 0xFFFFU) + x * 4U;
    *g_TimelinePrngStateB = static_cast<u16>(h1);
    x = (static_cast<u32>(*g_TimelinePrngStateB) ^ 0x9630U) - 0x6553U;
    const u32 h2 = ((x >> 14) & 0xFFFFU) + x * 4U;
    *g_TimelinePrngStateB = static_cast<u16>(h2);
    const u32 counter = static_cast<u32>(g_TimelinePrngStateB[2]) |
                        (static_cast<u32>(g_TimelinePrngStateB[3]) << 16);
    const u32 next = counter + 2U;
    g_TimelinePrngStateB[2] = static_cast<u16>(next);
    g_TimelinePrngStateB[3] = static_cast<u16>(next >> 16);
    return (h2 << 16) | h2;
}

double LcgDrawAsDouble()
{
    const u32 raw = LcgDrawRaw32Duplicated();
    double value = static_cast<double>(static_cast<i32>(raw));
    if (static_cast<i32>(raw) < 0)
        value += 4294967296.0;
    return value;
}

} // namespace

// ---------------------------------------------------------------------------
// 0x0040c9d0
//
// Scatter table layout (base = the stack argument):
//   +0x00 u32  pending particle kind (cleared by the callers)
//   +0x04 u32  counts[11] (rows 0..10)
//   +0x30 u32  extra dword cleared together with the counts
//   +0x34 f32  x radius (arg to cos)
//   +0x38 f32  y radius (arg to sin)
//
// Per spawned particle:
//   angle' = (draw * 2^-31 - 1) * 0.8 + angle + 1.60773 (0x3fce147b),
//            wrapped into (-pi, pi] through 0x44bc70
//   scale  = draw * 2^-33 + 0.5            (in [0.5, 1.0))
//   pos    = position + {cos(angle) * rx, sin(angle) * ry} * scale
//   spawn  = kind row+1, color 0xffffffff, angle -pi/2, speed 2.2
// The initial angle is PrngUnitFloat-style draw * 3.25 (flt_470b18).
// ---------------------------------------------------------------------------
i32 SpawnEnemyDeathScatterEsiStackAbi(const float position[3],
                                      void *scatter_table)
{
    u8 *table = static_cast<u8 *>(scatter_table);
    u32 *counts = reinterpret_cast<u32 *>(table + 4U);
    const float radius_x = *reinterpret_cast<const float *>(table + 0x34U);
    const float radius_y = *reinterpret_cast<const float *>(table + 0x38U);

    // Initial angle: 0x44bb90 draw (raw combined * 2^-31 - 1) * 3.25.
    float angle = static_cast<float>(LcgDrawAsDouble() * 0.0000000004656612873077392578125 -
                                     1.0) *
                  3.25f;

    for (u32 row = 0; row < 11U; ++row) {
        const i32 count = static_cast<i32>(counts[row]);
        for (i32 i = 0; i < count; ++i) {
            float offset[2];
            ComputeScatterOffset(offset, angle, radius_x, radius_y);
            // Scale draw: raw * 2^-33 + 0.5.
            const double scale =
                LcgDrawAsDouble() * 0.000000000116415321826934814453125 +
                0.5;
            offset[0] = offset[0] * static_cast<float>(scale) + position[0];
            offset[1] = offset[1] * static_cast<float>(scale) + position[1];
            float particle_position[3] = {offset[0], offset[1],
                                          position[2]};
            SpawnExplosionParticleEaxEcxEfxAbi(
                const_cast<void *>(*reinterpret_cast<void * const *>(
                    static_cast<u32>(k_explosion_manager_slot))),
                particle_position,
                static_cast<i32>(row) + 1, 0xFFFFFFFFU, k_death_angle,
                k_death_speed);
            // Angle advance draw: (raw * 2^-31 - 1) * 0.8 + angle + 1.60773,
            // then wrapped through 0x44bc70.
            const double advance = (LcgDrawAsDouble() *
                                        0.0000000004656612873077392578125 -
                                    1.0) *
                                       0.8 +
                                   angle + 1.60773f;
            angle = WrapAngleToPi(static_cast<float>(advance));
        }
    }

    // Clear counts[0..10] plus the extra dword at table+0x30.
    for (u32 i = 0; i < 12U; ++i)
        counts[i] = 0U;
    return 0;
}

// ---------------------------------------------------------------------------
// 0x0040c9a0
// ---------------------------------------------------------------------------
void FlushEnemyDeathDropEaxEdiAbi(const float position[3], u32 *kind_slot)
{
    if (static_cast<i32>(*kind_slot) > 0) {
        SpawnExplosionParticleEaxEcxEfxAbi(
            const_cast<void *>(*reinterpret_cast<void * const *>(static_cast<u32>(k_explosion_manager_slot))),
            const_cast<float *>(position),
            static_cast<i32>(*kind_slot), 0xFFFFFFFFU, k_death_angle,
            k_death_speed);
    }
    SpawnEnemyDeathScatterEsiStackAbi(position, kind_slot);
    *kind_slot = 0U;
}

// ---------------------------------------------------------------------------
// 0x0040e5f0 (stdcall, ret 4; argument = enemy script manager)
//
// Enemy script record fields used here:
//   +0x1068 {x, y, z} position pair/floats
//   +0x2408 pending big-explosion kind (cleared after the burst)
//   +0x240c..+0x2438 scatter table (11 counts + extra dword)
//   +0x243c/+0x2440 scatter radii (not cleared)
//   +0x2444 death sound slot (i32; negative disables)
//   +0x2448 death effect id (i32; negative disables)
//   +0x244c effect table index
// ---------------------------------------------------------------------------
i32 TriggerEnemyDeathSequenceStdcallAbi(void *script_manager)
{
    u8 *mgr = static_cast<u8 *>(script_manager);
    const float *position = reinterpret_cast<const float *>(mgr + 0x1068U);

    if (*reinterpret_cast<const i32 *>(mgr + 0x2444U) >= 0)
        EnqueueDeathSoundEsiStackAbi(reinterpret_cast<void *>(k_sound_manager),
                                     position[0]);

    if (*reinterpret_cast<const i32 *>(mgr + 0x2448U) >= 0) {
        const u32 effect_id =
            *reinterpret_cast<const u32 *>(mgr + 0x2448U);
        const u32 table_index =
            *reinterpret_cast<const u32 *>(mgr + 0x244cU);
        void *table_entry = *reinterpret_cast<void **>(
            *reinterpret_cast<u8 * const>(static_cast<u32>(k_death_effect_table)) + 0x30U +
            table_index * 4U);
        SpawnDeathEffectVmEdiStackAbi(position, table_entry, effect_id,
                                      script_manager);
    }

    const u32 kind = *reinterpret_cast<const u32 *>(mgr + 0x2408U);
    if (static_cast<i32>(kind) > 0) {
        SpawnExplosionParticleEaxEcxEfxAbi(
            const_cast<void *>(*reinterpret_cast<void * const *>(static_cast<u32>(k_explosion_manager_slot))),
            const_cast<float *>(position), static_cast<i32>(kind),
            0xFFFFFFFFU, k_death_angle, k_death_speed);
    }

    SpawnEnemyDeathScatterEsiStackAbi(position, mgr + 0x2408U);
    *reinterpret_cast<u32 *>(mgr + 0x2408U) = 0U;

    TickScorePopupEdiStackAbi(reinterpret_cast<u32 *>(k_score_popup_block),
                              10);
    return 1;
}

} // namespace th10
