#include "PlayerProjectileManager.hpp"

#include "EntityHelpers.hpp"
#include "PlayerMotionHelpers.hpp"
#include "PlayerRecord.hpp"
#include "PlayerTimerHelpers.hpp"

#include <cmath>

namespace th10 {

// TH10 0x00428280. Updates the 128 player-shot records at player+0x49c
// (stride 0x5c). This pass performs no enemy collision and no damage: it
// advances motion, publishes positions, culls off-field shots, and manages
// the type-3 (option shot) lifetime. Enemy damage is computed by consumers
// of the published entity positions.

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern void *g_AsciiHudOwner; // TH10 DAT_0047770c
extern void *g_AsciiHudConditionalState; // TH10 DAT_00477704
extern float g_FrameTimeScale; // TH10 DAT_00476f78

// TH10 0x4491c0 (EDX = manager): resolve an id to an entity, or null.




// TH10 0x428d70 (ECX = {x, y}, stack = half extents): returns 1 when the
// box is fully outside the fixed playfield rect.
i32 IsOutsidePlayfieldBox(void *position, float half_x, float half_y);

// The shot descriptor's per-frame update callback receives ECX = player.
#if defined(_MSC_VER)
typedef void (__fastcall DescriptorUpdateFnT)(void *player);
typedef DescriptorUpdateFnT *DescriptorUpdateFnPtr;
#else
typedef void __attribute__((fastcall)) DescriptorUpdateFnT(void *player);
typedef DescriptorUpdateFnT *DescriptorUpdateFnPtr;
#endif

const float kRateUnityLow = 0.99f; // TH10 DAT_00470b68
const float kRateUnityHigh = 1.01f; // TH10 DAT_00470b64

inline i32 ReadInt(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void WriteFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

inline void WriteInt(u8 *bytes, u32 offset, i32 value)
{
    *reinterpret_cast<i32 *>(bytes + offset) = value;
}

// Advance the age counter (+0x00 snapshot / +0x04 count / +0x08 accumulator
// / +0x0c rate pointer) with the shared scaled-timer semantics.
void AdvanceAge(PlayerShotRecord *rec)
{
    *reinterpret_cast<i32 *>(&rec->timer_prev) = rec->timer_count;
    const float rate = *rec->timer_rate;
    if (rate > kRateUnityLow && rate < kRateUnityHigh) {
        rec->timer_accum = rec->timer_accum + 1.0f;
        rec->timer_count = rec->timer_count + 1;
    } else {
        const float acc = rec->timer_accum + rate;
        rec->timer_accum = acc;
        rec->timer_count = static_cast<i32>(acc);
    }
}

void ExpireType3Shot(PlayerShotRecord *rec, PlayerRecord &player, i32 slot)
{
    ExpireEntityHandleEaxAbi(&rec->entity_id);
    ExpireEntityHandleEaxAbi(&rec->secondary_entity_id);
    rec->state = 2;
    *reinterpret_cast<float *>(&player.type3_slot_latches[slot]) = 0.0f;
}

} // namespace

// TH10 0x00428280.
i32 UpdatePlayerProjectilesStackAbi(void *player_memory)
{
    u8 *const player_ptr = static_cast<u8 *>(player_memory);
    PlayerRecord &player = *reinterpret_cast<PlayerRecord *>(player_ptr);
    const i32 option_count = player.option_count;
    const bool aborted = (g_AsciiHudOwner != 0 &&
                          *reinterpret_cast<const i32 *>(
                              static_cast<u8 *>(g_AsciiHudOwner) +
                              0x9eb8) != 0) ||
        g_AsciiHudConditionalState == 0;

    for (u32 index = 0; index != 128; ++index) {
        PlayerShotRecord *const record = &player.shots[index];
        const i32 state = static_cast<i32>(record->state);
        if (state == 0)
            continue;
        u8 *const descriptor =
            static_cast<u8 *>(record->descriptor);
        const i32 type = static_cast<const i32>(descriptor[0x1d]);
        const i32 slot = static_cast<const i32>(descriptor[0x1c]);

        if (type == 3 && state == 1 &&
            (player.autocollect_timer.count < 0 ||
             slot - 1 >= option_count)) {
            ExpireType3Shot(record, player, slot);
        }
        if (type == 3 && record->state == 1 && aborted) {
            ExpireType3Shot(record, player, slot);
        }
        if (type == 3 && record->hit_flag == 0 &&
            record->state == 1 && record->magnet_latch == 1) {
            FireEntityHandleEaxAbi(&record->entity_id);
            record->magnet_latch = 0;
        }
        record->hit_flag = 0;

        if (descriptor[0x28] != 0) {
            const DescriptorUpdateFnPtr update =
                reinterpret_cast<DescriptorUpdateFnPtr>(descriptor[0x28]);
            update(player_ptr);
        }

        const bool self_integrating = (record->angle_flags & 1) != 0;
        if (!self_integrating) {
            PolarToCartesianEdiAbi(record->velocity, record->angle,
                                   record->speed);
            record->field_0028 = 0;
        } else {
            record->angle_delta[0] =
                record->angle_delta[0] + record->angle_delta[1];
            record->angle = WrapAngleToPi(record->angle +
                                          record->speed);
        }
        IntegrateSubEffectPositionEsiAbi(record->position);

        u8 *const entity =
            FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                  record->entity_id);
        if (entity == 0) {
            // Deactivate: soft-release the secondary entity if present.
            if (record->secondary_entity_id != 0) {
                ReleaseEntityById(g_MainChainRenderOwner,
                                  record->secondary_entity_id);
            }
            record->secondary_entity_id = 0;
            record->entity_id = 0;
            record->state = 0;
            continue;
        }

        if (type != 3 && record->timer_count >= 10) {
            float position[2] = {record->position[0],
                                 record->position[1]};
            const float half_x =
                *reinterpret_cast<const float *>(
                    *reinterpret_cast<const u32 *>(
                        reinterpret_cast<const u8 *>(entity) + 0x394) +
                    0x30) *
                *reinterpret_cast<const float *>(
                    reinterpret_cast<const u8 *>(entity) + 0x40);
            const float half_y =
                *reinterpret_cast<const float *>(
                    *reinterpret_cast<const u32 *>(
                        reinterpret_cast<const u8 *>(entity) + 0x394) +
                    0x34) *
                *reinterpret_cast<const float *>(
                    reinterpret_cast<const u8 *>(entity) + 0x3c);
            if (IsOutsidePlayfieldBox(position, half_x, half_y) != 0) {
                ReleaseEntityById(g_MainChainRenderOwner,
                                  record->entity_id);
                record->entity_id = 0;
                record->state = 0;
                continue;
            }
        }

        WriteFloat(entity, 0x340, record->position[0] + 224.0f);
        WriteFloat(entity, 0x344, record->position[1] + 16.0f);
        *reinterpret_cast<u32 *>(entity + 0x348) =
            *reinterpret_cast<const u32 *>(&record->position[2]);
        if (record->secondary_entity_id != 0) {
            u8 *const secondary =
                FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                      record->secondary_entity_id);
            if (secondary == 0) {
                record->secondary_entity_id = 0;
            } else {
                WriteFloat(secondary, 0x340,
                           record->position[0] + 224.0f);
                WriteFloat(secondary, 0x344,
                           record->position[1] + 16.0f);
                *reinterpret_cast<u32 *>(secondary + 0x348) =
                    *reinterpret_cast<const u32 *>(&record->position[2]);
            }
        }

        if ((*reinterpret_cast<const u32 *>(entity + 0x35c) &
             0x08000000U) != 0) {
            WriteFloat(entity, 0x2c, record->angle);
            *reinterpret_cast<u32 *>(entity + 0x35c) |= 4;
        }
        AdvanceAge(record);
    }
    return 0;
}

} // namespace th10
