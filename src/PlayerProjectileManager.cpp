#include "PlayerProjectileManager.hpp"

#include "EntityHelpers.hpp"
#include "PlayerMotionHelpers.hpp"
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
void AdvanceAge(u8 *record)
{
    *reinterpret_cast<i32 *>(record) =
        *reinterpret_cast<const i32 *>(record + 4);
    const float rate =
        **reinterpret_cast<float *const *>(record + 0xc);
    if (rate > kRateUnityLow && rate < kRateUnityHigh) {
        *reinterpret_cast<float *>(record + 8) =
            *reinterpret_cast<float *>(record + 8) + 1.0f;
        *reinterpret_cast<i32 *>(record + 4) =
            *reinterpret_cast<const i32 *>(record + 4) + 1;
    } else {
        const float acc = *reinterpret_cast<float *>(record + 8) + rate;
        *reinterpret_cast<float *>(record + 8) = acc;
        *reinterpret_cast<i32 *>(record + 4) = static_cast<i32>(acc);
    }
}

void ExpireType3Shot(u8 *record, u8 *player, i32 slot)
{
    ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(record + 0x44));
    ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(record + 0x48));
    *reinterpret_cast<i32 *>(record + 0x40) = 2;
    WriteFloat(player, 0x42f4 + slot * 4, 0.0f);
}

} // namespace

// TH10 0x00428280.
i32 UpdatePlayerProjectilesStackAbi(void *player_memory)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    const i32 option_count = ReadInt(player, 0x3500);
    const bool aborted = (g_AsciiHudOwner != 0 &&
                          *reinterpret_cast<const i32 *>(
                              static_cast<u8 *>(g_AsciiHudOwner) +
                              0x9eb8) != 0) ||
        g_AsciiHudConditionalState == 0;

    for (u32 index = 0; index != 128; ++index) {
        u8 *const record = player + 0x49c + index * 0x5c;
        const i32 state = ReadInt(record, 0x40);
        if (state == 0)
            continue;
        u8 *const descriptor =
            *reinterpret_cast<u8 *const *>(record + 0x58);
        const i32 type = static_cast<const i32>(descriptor[0x1d]);
        const i32 slot = static_cast<const i32>(descriptor[0x1c]);

        if (type == 3 && state == 1 &&
            (ReadInt(player, 0x464) < 0 || slot - 1 >= option_count)) {
            ExpireType3Shot(record, player, slot);
        }
        if (type == 3 && ReadInt(record, 0x40) == 1 && aborted) {
            ExpireType3Shot(record, player, slot);
        }
        if (type == 3 && ReadInt(record, 0x50) == 0 &&
            ReadInt(record, 0x40) == 1 && ReadInt(record, 0x54) == 1) {
            FireEntityHandleEaxAbi(reinterpret_cast<u32 *>(record + 0x44));
            WriteInt(record, 0x54, 0);
        }
        WriteInt(record, 0x50, 0);

        if (descriptor[0x28] != 0) {
            const DescriptorUpdateFnPtr update =
                reinterpret_cast<DescriptorUpdateFnPtr>(descriptor[0x28]);
            update(player);
        }

        const bool self_integrating =
            (*reinterpret_cast<const u8 *>(record + 0x3c) & 1) != 0;
        if (!self_integrating) {
            PolarToCartesianEdiAbi(record + 0x20,
                ReadFloat(record, 0x30), ReadFloat(record, 0x2c));
            *reinterpret_cast<u32 *>(record + 0x28) = 0;
        } else {
            WriteFloat(record, 0x34,
                       ReadFloat(record, 0x34) + ReadFloat(record, 0x38));
            WriteFloat(record, 0x30,
                       WrapAngleToPi(ReadFloat(record, 0x30) +
                                     ReadFloat(record, 0x2c)));
        }
        IntegrateSubEffectPositionEsiAbi(record + 0x14);

        u8 *const entity =
            FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                  static_cast<u32>(ReadInt(record, 0x44)));
        if (entity == 0) {
            // Deactivate: soft-release the secondary entity if present.
            if (ReadInt(record, 0x48) != 0) {
                ReleaseEntityById(g_MainChainRenderOwner,
                                  static_cast<u32>(ReadInt(record, 0x48)));
            }
            WriteInt(record, 0x48, 0);
            WriteInt(record, 0x44, 0);
            *reinterpret_cast<i32 *>(record + 0x40) = 0;
            continue;
        }

        if (type != 3 && ReadInt(record, 4) >= 10) {
            float position[2] = {ReadFloat(record, 0x14),
                                 ReadFloat(record, 0x18)};
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
                                  static_cast<u32>(ReadInt(record, 0x44)));
                WriteInt(record, 0x44, 0);
                *reinterpret_cast<i32 *>(record + 0x40) = 0;
                continue;
            }
        }

        WriteFloat(entity, 0x340, ReadFloat(record, 0x14) + 224.0f);
        WriteFloat(entity, 0x344, ReadFloat(record, 0x18) + 16.0f);
        *reinterpret_cast<u32 *>(entity + 0x348) =
            *reinterpret_cast<const u32 *>(record + 0x1c);
        if (ReadInt(record, 0x48) != 0) {
            u8 *const secondary =
                FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                      static_cast<u32>(ReadInt(record,
                                                               0x48)));
            if (secondary == 0) {
                WriteInt(record, 0x48, 0);
            } else {
                WriteFloat(secondary, 0x340,
                           ReadFloat(record, 0x14) + 224.0f);
                WriteFloat(secondary, 0x344,
                           ReadFloat(record, 0x18) + 16.0f);
                *reinterpret_cast<u32 *>(secondary + 0x348) =
                    *reinterpret_cast<const u32 *>(record + 0x1c);
            }
        }

        if ((*reinterpret_cast<const u32 *>(entity + 0x35c) &
             0x08000000U) != 0) {
            WriteFloat(entity, 0x2c, ReadFloat(record, 0x30));
            *reinterpret_cast<u32 *>(entity + 0x35c) |= 4;
        }
        AdvanceAge(record);
    }
    return 0;
}

} // namespace th10
