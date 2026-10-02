#include "PlayerShotSpawner.hpp"

#include "EntityHelpers.hpp"
#include "PlayerMotionHelpers.hpp"
#include "PlayerRecord.hpp"

#include <string.h>
#include "BgmRuntime.hpp"

namespace th10 {

// TH10 0x00427e90. Spawns one player shot into the first free record of
// the 128-slot table at player+0x49c (stride 0x5c). Table-full and
// type-3 duplicate rejections are silent. The slot latch (flags bit 0)
// is set only once per slot; the movement-mode branch reads it before the
// latch is armed, so a fresh slot takes the polar path and a re-fired slot
// keeps its recorded mode.

namespace {

extern float g_FrameTimeScale; // TH10 DAT_00476f78
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

// TH10 0x43dd10 (EBX = sound id, ESI = manager 0x492590, stack = float
// the callee ignores): queue the sound on one of the 12 channels.
extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590

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

// The descriptor's update callback receives ECX = player, EDX = record,
// and the frame on the stack (the fastcall convention on x86).
#if defined(_MSC_VER)
typedef void (__fastcall *DescriptorUpdateFnPtr)(void *, void *, i32);
#else
typedef void __attribute__((fastcall)) DescriptorUpdateFnT(void *, void *,
                                                           i32);
typedef DescriptorUpdateFnT *DescriptorUpdateFnPtr;
#endif

void AllocateAndRegisterEntity(u32 *out_id, i32 script_id)
{
    void *const vm = AllocatePoolVmEsiAbi(g_MainChainRenderOwner);
    *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x20) = 0xf;
    *reinterpret_cast<u32 *>(static_cast<u8 *>(vm) + 0x35c) |=
        0x40000000U;
    AssignPoolVmScriptEcxEaxAbi(vm, script_id);
    LinkEntityAndAssignIdEaxEsiAbi(out_id, vm);
}

} // namespace

i32 SpawnPlayerShotStackAbi(void *player_memory, const void *descriptor_memory,
                            i32 frame)
{
    u8 *const player_ptr = static_cast<u8 *>(player_memory);
    PlayerRecord &player = *reinterpret_cast<PlayerRecord *>(player_ptr);
    const u8 *const descriptor =
        static_cast<const u8 *>(descriptor_memory);
    const i32 type = static_cast<const i32>(descriptor[0x1d]);
    const i32 slot = static_cast<const i32>(descriptor[0x1c]);

    if (type == 3 && player.type3_slot_latches[slot] != 0)
        return 0;

    PlayerShotRecord *record = 0;
    for (u32 index = 0; index != 128; ++index) {
        PlayerShotRecord *const candidate = &player.shots[index];
        if (candidate->state == 0) {
            record = candidate;
            break;
        }
    }
    if (record == 0)
        return 0;

    const bool was_initialized = (record->angle_flags & 1) != 0;
    record->state = 1;
    record->descriptor = const_cast<void *>(
        static_cast<const void *>(descriptor));
    if (!was_initialized) {
        record->timer_count = 0;
        *reinterpret_cast<u32 *>(&record->timer_prev) =
            static_cast<u32>(0xfff0bdc1U);
        *reinterpret_cast<u32 *>(&record->timer_accum) = 0;
        record->timer_rate = &g_FrameTimeScale;
        record->angle_flags |= 1;
    }
    record->timer_count = 0;
    *reinterpret_cast<u32 *>(&record->timer_accum) = 0;
    *reinterpret_cast<u32 *>(&record->timer_prev) = static_cast<u32>(-1);

    if (slot == 0) {
        record->position[0] = player.position_x;
        record->position[1] = player.position_y;
        record->position[2] = player.position_z;
    } else {
        record->position[0] =
            static_cast<float>(*reinterpret_cast<const i32 *>(
                &player.options[slot - 1].render_position[0])) *
            0.01f;
        record->position[1] =
            static_cast<float>(*reinterpret_cast<const i32 *>(
                &player.options[slot - 1].render_position[1])) *
            0.01f;
        record->position[2] = 0.0f;
    }
    if (type == 3)
        player.type3_slot_latches[slot] = 1;

    record->speed = ReadFloat(descriptor, 0x18);
    record->angle = WrapAngleToPi(ReadFloat(descriptor, 0x14));

    if (!was_initialized) {
        PolarToCartesianEdiAbi(record->velocity, record->angle,
                               record->speed);
        record->field_0028 = 0;
    } else {
        record->angle_delta[0] =
            record->angle_delta[0] + record->angle_delta[1];
        record->angle = WrapAngleToPi(record->speed + record->angle);
    }

    record->position[0] = record->position[0] + ReadFloat(descriptor, 4) -
                          record->velocity[0];
    record->position[1] = record->position[1] + ReadFloat(descriptor, 8) -
                          record->velocity[1];

    u32 entity_id = 0;
    AllocateAndRegisterEntity(&entity_id,
                              static_cast<i32>(
                                  static_cast<short>(descriptor[0x1e] |
                                                   (descriptor[0x1f] << 8))) +
                                  5);
    record->entity_id = entity_id;
    u8 *const entity =
        FindEntityEdxStackAbi(g_MainChainRenderOwner, entity_id);
    if (entity == 0) {
        record->entity_id = 0;
    } else if ((*reinterpret_cast<const u32 *>(entity + 0x35c) &
                0x08000000U) != 0) {
        WriteFloat(entity, 0x2c, ReadFloat(descriptor, 0x14));
        *reinterpret_cast<u32 *>(entity + 0x35c) |= 4;
    }

    if (type == 3) {
        AllocateAndRegisterEntity(&entity_id, 0x10);
        record->secondary_entity_id = entity_id;
    } else {
        record->secondary_entity_id = 0;
    }

    DescriptorUpdateFnPtr update =
        reinterpret_cast<DescriptorUpdateFnPtr>(
            *reinterpret_cast<const void *const *>(descriptor + 0x24));
    if (*reinterpret_cast<const void *const *>(descriptor + 0x24) != 0)
        update(player_ptr, record, frame);

    const short sound_id = static_cast<short>(
        descriptor[0x22] | (descriptor[0x23] << 8));
    if (sound_id >= 0)
        EnqueueBgmSoundValueFromFloat(&g_TransitionRoot,
            static_cast<u32>(sound_id), record->position[0]);
    return 0;
}

// TH10 0x00427b50. Fills the first free sub-effect record; the whole
// 0x6c-byte record is zeroed on every spawn, so no state carries over.
void *SpawnPlayerSubEffectEcxDxStackAbi(void *position_memory,
                                        void *player_memory, float vel_x,
                                        float vel_y, i32 count, i32 limit)
{
    PlayerRecord &player = *reinterpret_cast<PlayerRecord *>(player_memory);
    const float *const position =
        static_cast<const float *>(position_memory);
    PlayerSubEffectRecord *record = player.sub_effects;
    for (u32 index = 0; index != 32; ++index) {
        if ((record->flags & 1) != 0) {
            ++record;
            continue;
        }
        memset(record, 0, 0x6c);
        record->flags |= 3;
        record->position[0] = position[0];
        record->position[1] = position[1];
        record->position[2] = position[2];
        record->velocity[0] = vel_x;
        record->velocity[1] = vel_y;
        // Timer block: prev +0x44, count +0x48, accumulator +0x4c, rate
        // pointer +0x50, latch +0x54. The caller pre-sets the rate global
        // (0x476f78) to 1.0f before spawning.
        record->timer_count = count;
        record->timer_prev = count - 1;
        record->timer_accum = static_cast<float>(count);
        record->timer_rate = &g_FrameTimeScale;
        record->timer_latch |= 1;
        record->value = limit;
        record->accumulated = 0;
        record->limit = 999999;
        record->period = 4;
        return record;
    }
    return player.sub_effects + 32;
}

} // namespace th10
