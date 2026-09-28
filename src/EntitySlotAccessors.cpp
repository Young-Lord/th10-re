#include "EntitySlotAccessors.hpp"

#include "PlayerMotionHelpers.hpp"

#include <cmath>
#include <string.h>

namespace th10 {

namespace {

extern float g_FrameTimeScale; // TH10 DAT_00476f78 (rate pointer default)

} // namespace

// TH10 0x00412a10. Slot resolver used by the ECL script library callers
// around 0x4100c0/0x411xxx. The work record hangs off context+0x14d8;
// its +4 slot table publishes a 16-bit presence word at +8 and one
// entry dword per slot at +16. Negative entries mean "stored in the
// fallback resolver" (a thiscall on the object at table+0x1014, vtable
// slot 2); non-negative entries are offsets relative to table+8 plus
// the relocation base at table+0x100c.
void *ResolveEntitySlotPointerEaxEcxAbi(const void *context, i32 slot)
{
    const u8 *base = static_cast<const u8 *>(context);
    u8 *work = *reinterpret_cast<u8 *const *>(base + 0x14d8);
    u8 *table = *reinterpret_cast<u8 *const *>(work + 4);
    const u16 presence = *reinterpret_cast<const u16 *>(table + 8);
    if ((presence & (1U << slot)) == 0)
        return 0;

    const i32 entry = *reinterpret_cast<const i32 *>(
        table + 0x10 + static_cast<u32>(slot) * 4U);
    if (entry < 0) {
        u8 *resolver = *reinterpret_cast<u8 *const *>(table + 0x1014);
        void **vtable = *reinterpret_cast<void ***>(resolver);
        typedef void *(*ResolveThunk)(void *, i32);
        ResolveThunk thunk = reinterpret_cast<ResolveThunk>(vtable[2]);
        return thunk(resolver, entry);
    }
    return table + 8 + static_cast<u32>(entry) +
           *reinterpret_cast<const u32 *>(table + 0x100c);
}

namespace {

// TH10 0x00412d60 family: dst +0x0 / +0x8 / +0x10 / +0x18.
void CopyVec2PairToSlotOffset(const void *src, void *dst, u32 offset)
{
    const u32 *source = static_cast<const u32 *>(src);
    u32 *target = reinterpret_cast<u32 *>(
        static_cast<u8 *>(dst) + offset);
    target[0] = source[0];
    target[1] = source[1];
}

} // namespace

void CopyVec2PairToSlotBaseEaxEcxAbi(const void *src, void *dst)
{
    CopyVec2PairToSlotOffset(src, dst, 0);
}

void CopyVec2PairToSlot8EaxEcxAbi(const void *src, void *dst)
{
    CopyVec2PairToSlotOffset(src, dst, 8);
}

void CopyVec2PairToSlot10EaxEcxAbi(const void *src, void *dst)
{
    CopyVec2PairToSlotOffset(src, dst, 0x10);
}

void CopyVec2PairToSlot18EaxEcxAbi(const void *src, void *dst)
{
    CopyVec2PairToSlotOffset(src, dst, 0x18);
}

// TH10 0x00412db0. The lazy timer seed mirrors the interpolator tail:
// bit 0 of +0x30 gates the poison reset of +0x20..+0x2c (the rate slot
// is seeded with the frame-time scale pointer), then cur = 0, accum =
// 0, prev = -1. The rate slot itself is left untouched by the tail.
void RearmAnimTimerAt20EaxAbi(void *block)
{
    u8 *bytes = static_cast<u8 *>(block);
    u32 flags = *reinterpret_cast<u32 *>(bytes + 0x30);
    if ((flags & 1U) == 0) {
        flags |= 1U;
        *reinterpret_cast<u32 *>(bytes + 0x24) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x20) = 0xfff0bdc1U;
        *reinterpret_cast<u32 *>(bytes + 0x28) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x2c) =
            reinterpret_cast<u32>(&g_FrameTimeScale);
        *reinterpret_cast<u32 *>(bytes + 0x30) = flags;
    }
    *reinterpret_cast<u32 *>(bytes + 0x24) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x28) = 0;
    *reinterpret_cast<i32 *>(bytes + 0x20) = -1;
}

void SetAnimDurationInvalidateCacheEaxEcxAbi(void *block, i32 duration)
{
    u8 *bytes = static_cast<u8 *>(block);
    if (*reinterpret_cast<i32 *>(bytes + 0x44) != duration)
        *reinterpret_cast<u32 *>(bytes + 0x4c) = 0;
    *reinterpret_cast<i32 *>(bytes + 0x44) = duration;
}

void SetSlotPairFromStackEaxStackAbi(void *dst, u32 value_a, u32 value_b)
{
    u32 *target = static_cast<u32 *>(dst);
    target[0] = value_a;
    target[1] = value_b;
}

void CopyVec3ToSlotCEaxEcxAbi(const void *src, void *dst)
{
    const u32 *source = static_cast<const u32 *>(src);
    u32 *target = reinterpret_cast<u32 *>(
        static_cast<u8 *>(dst) + 0xc);
    target[0] = source[0];
    target[1] = source[1];
    target[2] = source[2];
}

void WrapAngleIntoSlot1CEcxStackAbi(void *dst, float angle)
{
    *reinterpret_cast<float *>(static_cast<u8 *>(dst) + 0x1c) =
        WrapAngleToPi(angle);
}

i32 ReadEntityFlagBit3EaxAbi(const void *record)
{
    const u32 flags = *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(record) + 0x60);
    return static_cast<i32>((flags >> 3) & 1U);
}

void CopyVec3EaxEcxAbi(const void *src, void *dst)
{
    const u32 *source = static_cast<const u32 *>(src);
    u32 *target = static_cast<u32 *>(dst);
    target[0] = source[0];
    target[1] = source[1];
    target[2] = source[2];
}

// TH10 0x00413270. Native fsincos of the raw angle; the sine branch is
// scaled by the first stack float into out[0] and the cosine branch by
// the second into out[1]. The caller (SpawnEnemyDeathScatterEsiStackAbi
// at 0x40ca21) uses this for the death-scatter velocity seeds.
void SetPolarVec2SinCosScaledEaxStackAbi(float *out, float angle,
                                         float sin_scale, float cos_scale)
{
    const float sine = static_cast<float>(std::sin(angle));
    const float cosine = static_cast<float>(std::cos(angle));
    out[0] = sine * sin_scale;
    out[1] = cosine * cos_scale;
}

} // namespace th10
