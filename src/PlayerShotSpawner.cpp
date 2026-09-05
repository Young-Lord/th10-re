#include "PlayerShotSpawner.hpp"

#include "EntityHelpers.hpp"
#include "PlayerMotionHelpers.hpp"

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
    u8 *const player = static_cast<u8 *>(player_memory);
    const u8 *const descriptor =
        static_cast<const u8 *>(descriptor_memory);
    const i32 type = static_cast<const i32>(descriptor[0x1d]);
    const i32 slot = static_cast<const i32>(descriptor[0x1c]);

    if (type == 3 &&
        ReadInt(player, 0x42f4 + static_cast<u32>(slot) * 4) != 0)
        return 0;

    u8 *record = 0;
    for (u32 index = 0; index != 128; ++index) {
        u8 *const candidate = player + 0x49c + index * 0x5c;
        if (ReadInt(candidate, 0x40) == 0) {
            record = candidate;
            break;
        }
    }
    if (record == 0)
        return 0;

    const bool was_initialized =
        (*reinterpret_cast<const u8 *>(record + 0x3c) & 1) != 0;
    WriteInt(record, 0x40, 1);
    *reinterpret_cast<const void **>(record + 0x58) = descriptor;
    if (!was_initialized) {
        WriteInt(record, 4, 0);
        WriteInt(record, 0, static_cast<i32>(0xfff0bdc1U));
        WriteInt(record, 8, 0);
        *reinterpret_cast<const float **>(record + 0xc) = &g_FrameTimeScale;
        *reinterpret_cast<u8 *>(record + 0x3c) |= 1;
    }
    WriteInt(record, 4, 0);
    WriteInt(record, 8, 0);
    WriteInt(record, 0, -1);

    if (slot == 0) {
        WriteFloat(record, 0x14, ReadFloat(player, 0x3c0));
        WriteFloat(record, 0x18, ReadFloat(player, 0x3c4));
        WriteFloat(record, 0x1c, ReadFloat(player, 0x3c8));
    } else {
        WriteFloat(record, 0x14,
                   static_cast<float>(
                       ReadInt(player, 0x3244 + static_cast<u32>(slot) *
                                          0x98)) *
                       0.01f);
        WriteFloat(record, 0x18,
                   static_cast<float>(
                       ReadInt(player, 0x3248 + static_cast<u32>(slot) *
                                          0x98)) *
                       0.01f);
        WriteFloat(record, 0x1c, 0.0f);
    }
    if (type == 3)
        WriteInt(player, 0x42f4 + static_cast<u32>(slot) * 4, 1);

    WriteFloat(record, 0x2c, ReadFloat(descriptor, 0x18));
    WriteFloat(record, 0x30,
               WrapAngleToPi(ReadFloat(descriptor, 0x14)));

    if (!was_initialized) {
        PolarToCartesianEdiAbi(record + 0x20, ReadFloat(record, 0x30),
                               ReadFloat(record, 0x2c));
        *reinterpret_cast<u32 *>(record + 0x28) = 0;
    } else {
        WriteFloat(record, 0x34,
                   ReadFloat(record, 0x34) + ReadFloat(record, 0x38));
        WriteFloat(record, 0x30,
                   WrapAngleToPi(ReadFloat(record, 0x2c) +
                                 ReadFloat(record, 0x30)));
    }

    WriteFloat(record, 0x14,
               ReadFloat(record, 0x14) + ReadFloat(descriptor, 4) -
                   ReadFloat(record, 0x20));
    WriteFloat(record, 0x18,
               ReadFloat(record, 0x18) + ReadFloat(descriptor, 8) -
                   ReadFloat(record, 0x24));

    u32 entity_id = 0;
    AllocateAndRegisterEntity(&entity_id,
                              static_cast<i32>(
                                  static_cast<short>(descriptor[0x1e] |
                                                   (descriptor[0x1f] << 8))) +
                                  5);
    WriteInt(record, 0x44, static_cast<i32>(entity_id));
    u8 *const entity =
        FindEntityEdxStackAbi(g_MainChainRenderOwner, entity_id);
    if (entity == 0) {
        WriteInt(record, 0x44, 0);
    } else if ((*reinterpret_cast<const u32 *>(entity + 0x35c) &
                0x08000000U) != 0) {
        WriteFloat(entity, 0x2c, ReadFloat(descriptor, 0x14));
        *reinterpret_cast<u32 *>(entity + 0x35c) |= 4;
    }

    if (type == 3) {
        AllocateAndRegisterEntity(&entity_id, 0x10);
        WriteInt(record, 0x48, static_cast<i32>(entity_id));
    } else {
        WriteInt(record, 0x48, 0);
    }

    DescriptorUpdateFnPtr update =
        reinterpret_cast<DescriptorUpdateFnPtr>(
            *reinterpret_cast<const void *const *>(descriptor + 0x24));
    if (*reinterpret_cast<const void *const *>(descriptor + 0x24) != 0)
        update(player, record, frame);

    const short sound_id = static_cast<short>(
        descriptor[0x22] | (descriptor[0x23] << 8));
    if (sound_id >= 0)
        EnqueueBgmSoundValueFromFloat(&g_TransitionRoot,
            static_cast<u32>(sound_id), ReadFloat(record, 0x14));
    return 0;
}

// TH10 0x00427b50. Fills the first free sub-effect record; the whole
// 0x6c-byte record is zeroed on every spawn, so no state carries over.
void *SpawnPlayerSubEffectEcxDxStackAbi(void *position_memory,
                                        void *player_memory, float vel_x,
                                        float vel_y, i32 count, i32 limit)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    const float *const position =
        static_cast<const float *>(position_memory);
    u8 *const record_base = player + 0x350c;
    for (u32 index = 0; index != 32; ++index) {
        u8 *const record = record_base + index * 0x6c;
        if ((*reinterpret_cast<const u8 *>(record + 0x68) & 1) != 0)
            continue;
        memset(record, 0, 0x6c);
        *reinterpret_cast<u8 *>(record + 0x68) |= 3;
        *reinterpret_cast<u32 *>(record + 0x18) =
            *reinterpret_cast<const u32 *>(position);
        *reinterpret_cast<u32 *>(record + 0x1c) =
            *reinterpret_cast<const u32 *>(position + 1);
        *reinterpret_cast<u32 *>(record + 0x20) =
            *reinterpret_cast<const u32 *>(position + 2);
        WriteFloat(record, 0, vel_x);
        WriteFloat(record, 4, vel_y);
        // Timer block: prev +0x44, count +0x48, accumulator +0x4c, rate
        // pointer +0x50, latch +0x54. The caller pre-sets the rate global
        // (0x476f78) to 1.0f before spawning.
        WriteInt(record, 0x48, count);
        WriteInt(record, 0x44, count - 1);
        WriteFloat(record, 0x4c, static_cast<float>(count));
        *reinterpret_cast<const float **>(record + 0x50) =
            &g_FrameTimeScale;
        *reinterpret_cast<u32 *>(record + 0x54) |= 1;
        WriteInt(record, 0x58, limit);
        WriteInt(record, 0x5c, 0);
        WriteInt(record, 0x60, 999999);
        WriteInt(record, 0x64, 4);
        return record;
    }
    return record_base + 32 * 0x6c;
}

} // namespace th10
