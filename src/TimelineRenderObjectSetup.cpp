#include "TimelineRenderObjectSetup.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "AsciiAnimationVm.hpp"
#include "TimelineRenderObjects.hpp"
#include "Th10Types.hpp"

namespace th10 {
namespace {

const float kPi = 3.1415927f;
const float kTwoPi = 6.2831855f;
const float kNegPi = -3.1415927f;
const float kScaleSentinel = 0.0f;
const float kUnity = 1.0f;
const float kMirror = -1.0f;
const float kRateUnityLow = 0.99f;
const float kRateUnityHigh = 1.01f;
const float kHermiteWeight = 3.0f;
const float kEaseHalf = 0.5f;
const i32 kTimerInactive = static_cast<i32>(0xfff0bdc1U);

float g_TimelineGlobalOffsetX = 0.0f; // TH10 DAT_00491e6c (BSS; not in file image)
float g_TimelineGlobalOffsetY = 0.0f; // TH10 DAT_00491e70
float g_TimelineGlobalOffsetZ = 0.0f; // TH10 DAT_00491e74

float g_VmGlobalFloat10016 = 0.0f; // TH10 DAT_00491d7c
float g_VmGlobalFloat10017 = 0.0f; // TH10 DAT_00491d80
float g_VmGlobalFloat10018 = 1000.0f; // TH10 DAT_00491d84
float g_VmGlobalFloat10019 = 0.0f; // TH10 DAT_00491da0
float g_VmGlobalFloat10020 = 0.0f; // TH10 DAT_00491da4
float g_VmGlobalFloat10021 = 0.0f; // TH10 DAT_00491da8

float g_AnimDefaultVector[3] = {0.0f, 0.0f, 0.0f}; // TH10 DAT_00491c14..1c

u16 g_TimelinePrngStateA[4]; // TH10 DAT_004918a8
u16 g_TimelinePrngStateB[4]; // TH10 DAT_004918b0

extern float g_MainChainStartupScale; // TH10 DAT_00476f78
extern void *g_MainChainRenderOwner;
extern i32 ConvertFloatToI32TowardZeroX87(float value);

inline bool ScaleFieldUsesSentinel(float value)
{
    return (((value) != (value)) || ((kScaleSentinel) != (kScaleSentinel))) ==
           (value == kScaleSentinel);
}

inline u16 &SelectPrngState(u8 *node)
{
    if (*reinterpret_cast<u32 *>(node + 0x35c) & 0x40000000U)
        return g_TimelinePrngStateA[0];
    return g_TimelinePrngStateB[0];
}

u32 PrngModulo(u16 *state, u32 modulus)
{
    if (modulus == 0)
        return 0;
    u32 x = (static_cast<u16>(*state ^ 0x9630u) - 0x6553u);
    i32 hi = static_cast<i32>((x >> 14 & 3u) + x * 4u);
    u16 lo = static_cast<u16>((static_cast<u16>(hi) ^ 0x9630u) + 0x9aadu);
    *state = static_cast<u16>(hi);
    lo = static_cast<u16>((lo >> 14) + lo * 4u);
    *reinterpret_cast<i32 *>(state + 2) += 2;
    *state = lo;
    return (static_cast<u32>(hi) * 65536u + lo) % modulus;
}

float PrngUnitFloat(u16 *state)
{
    u32 x = (static_cast<u16>(*state ^ 0x9630u) - 0x6553u);
    i32 hi = static_cast<i32>((x >> 14 & 3u) + x * 4u);
    u16 lo = static_cast<u16>((static_cast<u16>(hi) ^ 0x9630u) + 0x9aadu);
    *state = static_cast<u16>(hi);
    lo = static_cast<u16>((lo >> 14) + lo * 4u);
    *reinterpret_cast<i32 *>(state + 2) += 2;
    *state = lo;
    u32 combined = static_cast<u32>(hi) * 65536u + lo;
    float value = static_cast<float>(combined);
    if (static_cast<i32>(combined) < 0)
        value += 4294967296.0f;
    return value * (1.0f / 4294967296.0f);
}

// TH10 0x0044bb90: centered draw in [-1,1): combined (+2^32 when
// negative) * 2^-31 - 1 (flt_470BEC / flt_470AFC). Distinct from the
// [0,1) draw 0x0044bb20 above.
float PrngCenteredFloat(u16 *state)
{
    u32 x = (static_cast<u16>(*state ^ 0x9630u) - 0x6553u);
    i32 hi = static_cast<i32>((x >> 14 & 3u) + x * 4u);
    u16 lo = static_cast<u16>((static_cast<u16>(hi) ^ 0x9630u) + 0x9aadu);
    *state = static_cast<u16>(hi);
    lo = static_cast<u16>((lo >> 14) + lo * 4u);
    *reinterpret_cast<i32 *>(state + 2) += 2;
    *state = lo;
    u32 combined = static_cast<u32>(hi) * 65536u + lo;
    float value = static_cast<float>(combined);
    if (static_cast<i32>(combined) < 0)
        value += 4294967296.0f;
    return value * (1.0f / 2147483648.0f) - 1.0f;
}

float ReadVmFloatRegister(u8 *vm, float reg_operand, float fallback)
{
    switch (ConvertFloatToI32TowardZeroX87(reg_operand)) {
    case 10000:
        return static_cast<float>(*reinterpret_cast<i32 *>(vm + 0x30c));
    case 10001:
        return static_cast<float>(*reinterpret_cast<i32 *>(vm + 0x310));
    case 10002:
        return static_cast<float>(*reinterpret_cast<i32 *>(vm + 0x314));
    case 10003:
        return static_cast<float>(*reinterpret_cast<i32 *>(vm + 0x318));
    case 10004:
        return *reinterpret_cast<float *>(vm + 0x31c);
    case 10005:
        return *reinterpret_cast<float *>(vm + 0x320);
    case 10006:
        return *reinterpret_cast<float *>(vm + 0x324);
    case 10007:
        return *reinterpret_cast<float *>(vm + 0x328);
    case 10008:
        return static_cast<float>(*reinterpret_cast<i32 *>(vm + 0x32c));
    case 10009:
        return static_cast<float>(*reinterpret_cast<i32 *>(vm + 0x330));
    case 10010:
        // Native: 0x0044bb90 centered draw * flt_470B18 (3.25).
        return PrngCenteredFloat(&SelectPrngState(vm)) * 3.25f;
    case 10011:
        // Native: 0x0044bb20 unit draw, returned raw.
        return PrngUnitFloat(&SelectPrngState(vm));
    case 10012:
        // Native: 0x0044bb90 centered draw, returned raw.
        return PrngCenteredFloat(&SelectPrngState(vm));
    case 10013:
        return *reinterpret_cast<float *>(vm + 0x334);
    case 10014:
        return *reinterpret_cast<float *>(vm + 0x338);
    case 10015:
        return *reinterpret_cast<float *>(vm + 0x33c);
    case 10016:
        return g_VmGlobalFloat10016;
    case 10017:
        return g_VmGlobalFloat10017;
    case 10018:
        return g_VmGlobalFloat10018;
    case 10019:
        return g_VmGlobalFloat10019;
    case 10020:
        return g_VmGlobalFloat10020;
    case 10021:
        return g_VmGlobalFloat10021;
    default:
        return fallback;
    }
}

i32 ReadVmIntRegister(u8 *vm, i32 reg_id)
{
    switch (reg_id - 10000) {
    case 0:
        return *reinterpret_cast<i32 *>(vm + 0x30c);
    case 1:
        return *reinterpret_cast<i32 *>(vm + 0x310);
    case 2:
        return *reinterpret_cast<i32 *>(vm + 0x314);
    case 3:
        return *reinterpret_cast<i32 *>(vm + 0x318);
    case 4:
        return ConvertFloatToI32TowardZeroX87(
            *reinterpret_cast<float *>(vm + 0x31c));
    case 5:
        return ConvertFloatToI32TowardZeroX87(
            *reinterpret_cast<float *>(vm + 0x320));
    case 6:
        return ConvertFloatToI32TowardZeroX87(
            *reinterpret_cast<float *>(vm + 0x324));
    case 7:
        return ConvertFloatToI32TowardZeroX87(
            *reinterpret_cast<float *>(vm + 0x328));
    case 8:
        return *reinterpret_cast<i32 *>(vm + 0x32c);
    case 9:
        return *reinterpret_cast<i32 *>(vm + 0x330);
    default:
        return reg_id;
    }
}

i32 *ResolveVmIntWriteTarget(i32 *reg_id_ptr, u8 *vm, u8 flags, u8 bit)
{
    if ((flags & static_cast<u8>(1u << bit)) == 0)
        return reg_id_ptr;
    switch (*reg_id_ptr) {
    case 10000:
        return reinterpret_cast<i32 *>(vm + 0x30c);
    case 10001:
        return reinterpret_cast<i32 *>(vm + 0x310);
    case 10002:
        return reinterpret_cast<i32 *>(vm + 0x314);
    case 10003:
        return reinterpret_cast<i32 *>(vm + 0x318);
    case 10008:
        return reinterpret_cast<i32 *>(vm + 0x32c);
    case 10009:
        return reinterpret_cast<i32 *>(vm + 0x330);
    default:
        return reg_id_ptr;
    }
}

float *ResolveVmFloatWriteTarget(float *fallback, u8 *vm, u8 flags, u8 bit,
                                 i32 reg_id)
{
    if ((flags & static_cast<u8>(1u << bit)) == 0)
        return fallback;
    switch (reg_id) {
    case 10004:
        return reinterpret_cast<float *>(vm + 0x31c);
    case 10005:
        return reinterpret_cast<float *>(vm + 0x320);
    case 10006:
        return reinterpret_cast<float *>(vm + 0x324);
    case 10007:
        return reinterpret_cast<float *>(vm + 0x328);
    case 10013:
        return reinterpret_cast<float *>(vm + 0x334);
    case 10014:
        return reinterpret_cast<float *>(vm + 0x338);
    case 10015:
        return reinterpret_cast<float *>(vm + 0x33c);
    default:
        return fallback;
    }
}

float WrapAnglePi(float angle, float delta)
{
    float value = angle + delta;
    i32 iter = 0;
    while (iter < 0x21 && value > kPi) {
        value -= kTwoPi;
        ++iter;
    }
    if (value < kNegPi &&
        !(((value) != (value)) || ((kNegPi) != (kNegPi)))) {
        iter = 0;
        do {
            value += kTwoPi;
            if (iter > 0x20)
                break;
            ++iter;
        } while (value < kNegPi &&
                 !(((value) != (value)) || ((kNegPi) != (kNegPi))));
    }
    return value;
}

void ResetVmTimer(u8 *timer, i32 tick)
{
    u32 &flags = *reinterpret_cast<u32 *>(timer + 0x10);
    if ((flags & 1U) == 0) {
        *reinterpret_cast<i32 *>(timer + 4) = 0;
        *reinterpret_cast<i32 *>(timer) = kTimerInactive;
        *reinterpret_cast<float *>(timer + 8) = 0.0f;
        *reinterpret_cast<const float **>(timer + 12) = &g_MainChainStartupScale;
        flags |= 1U;
    }
    *reinterpret_cast<i32 *>(timer + 4) = tick;
    *reinterpret_cast<i32 *>(timer) = tick - 1;
    *reinterpret_cast<float *>(timer + 8) = static_cast<float>(tick);
}

void AdvanceVmTimer(u8 *timer, float delta)
{
    const float rate = **reinterpret_cast<const float *const *>(timer + 12);
    *reinterpret_cast<i32 *>(timer) = *reinterpret_cast<i32 *>(timer + 4);
    if (rate > kRateUnityLow && rate < kRateUnityHigh)
        *reinterpret_cast<float *>(timer + 8) =
            *reinterpret_cast<float *>(timer + 8) + delta;
    else
        *reinterpret_cast<float *>(timer + 8) =
            *reinterpret_cast<float *>(timer + 8) + delta * rate;
    *reinterpret_cast<i32 *>(timer + 4) = ConvertFloatToI32TowardZeroX87(
        *reinterpret_cast<float *>(timer + 8));
}

float EvaluateCurve(float accum, float duration, i32 mode)
{
    const float t = accum / duration;
    switch (mode) {
    case 1:
        return t * t;
    case 2:
        return t * t * t;
    case 3:
        return t * t * t * t;
    case 4: {
        const float inv = kUnity - t;
        return kUnity - inv * inv;
    }
    case 5: {
        const float inv = kUnity - t;
        return kUnity - inv * inv * inv;
    }
    case 6: {
        const float inv = kUnity - t;
        return kUnity - inv * inv * inv * inv;
    }
    case 9: {
        const float x = t + t;
        if (x < kUnity == (((x) != (x)) || ((kUnity) != (kUnity))))
            return (kEaseHalf - (kEaseHalf - x) * (kEaseHalf - x)) * kHermiteWeight;
        return x * x * kHermiteWeight;
    }
    case 10: {
        const float x = t + t;
        if (x < kUnity != (((x) != (x)) || ((kUnity) != (kUnity))))
            return x * x * x * kHermiteWeight;
        const float inv = kEaseHalf - x;
        return inv * inv * inv * kHermiteWeight;
    }
    case 11: {
        const float x = t + t;
        if (x < kUnity != (((x) != (x)) || ((kUnity) != (kUnity))))
            return x * x * x * x * kHermiteWeight;
        const float inv = kEaseHalf - x;
        return inv * inv * inv * kHermiteWeight;
    }
    case 12: {
        const float x = t + t;
        if (x < kUnity == (((x) != (x)) || ((kUnity) != (kUnity))))
            return (x - kUnity) * (x - kUnity) * kHermiteWeight + kHermiteWeight;
        return kHermiteWeight -
               (kUnity - x) * (kUnity - x) * kHermiteWeight;
    }
    case 13: {
        const float x = t + t;
        if (x < kUnity == (((x) != (x)) || ((kUnity) != (kUnity)))) {
            const float y = x - kUnity;
            return y * y * y * kHermiteWeight + kHermiteWeight;
        }
        const float inv = kUnity - x;
        return kHermiteWeight - inv * inv * inv * kHermiteWeight;
    }
    case 14: {
        const float x = t + t;
        if (x < kUnity == (((x) != (x)) || ((kUnity) != (kUnity)))) {
            const float y = x - kUnity;
            return y * y * y * y * kHermiteWeight + kHermiteWeight;
        }
        const float inv = kUnity - x;
        return kHermiteWeight - inv * inv * inv * inv * kHermiteWeight;
    }
    case 15:
        return kScaleSentinel;
    case 16:
        return kUnity;
    default:
        return t;
    }
}

void AdvanceInterpolatorTimer(u8 *interp_base)
{
    float *const interp = reinterpret_cast<float *>(interp_base);
    const float duration = interp[0x11];
    if (duration <= 0.0f)
        return;
    interp[0xc] = interp[0xd];
    const float rate = **reinterpret_cast<const float *const *>(interp + 0xf);
    if (rate <= kRateUnityHigh &&
        (rate < kRateUnityLow == (((rate) != (rate)) || ((kRateUnityLow) != (kRateUnityLow))))) {
        interp[0xe] = rate + interp[0xe];
        interp[0xd] = static_cast<float>(ConvertFloatToI32TowardZeroX87(interp[0xe]));
    } else {
        interp[0xe] = interp[0xe] + kUnity;
        interp[0xd] = interp[0xd] + 1.0f;
    }
}

void FetchTickVector(u8 *interp_base, float out[3])
{
    float *const interp = reinterpret_cast<float *>(interp_base);
    const float duration = interp[0x11];
    if (duration > 0.0f) {
        AdvanceInterpolatorTimer(interp_base);
        if (static_cast<i32>(interp[0xd]) >= static_cast<i32>(duration)) {
            u32 &flags = *reinterpret_cast<u32 *>(interp + 0x10);
            if ((flags & 1U) == 0) {
                interp[0xd] = 0.0f;
                interp[0xc] = static_cast<float>(kTimerInactive);
                interp[0xe] = 0.0f;
                *reinterpret_cast<const float **>(interp + 0xf) =
                    &g_MainChainStartupScale;
                flags |= 1U;
            }
            interp[0xd] = duration;
            interp[0xc] = duration - 1.0f;
            interp[0xe] = static_cast<float>(static_cast<i32>(duration));
            interp[0x11] = 0.0f;
            if (static_cast<i32>(interp[0x12]) != 7) {
                out[0] = interp[3];
                out[1] = interp[4];
                out[2] = interp[5];
                return;
            }
            out[0] = interp[0];
            out[1] = interp[1];
            out[2] = interp[2];
            return;
        }
    }
    const i32 mode = static_cast<i32>(interp[0x12]);
    if (mode == 7) {
        const float x = interp[0];
        const float y = interp[1];
        const float z = interp[2];
        interp[0] = x + interp[3];
        interp[1] = y + interp[4];
        interp[2] = z + interp[5];
        out[0] = interp[3] + x;
        out[1] = interp[4] + y;
        out[2] = interp[5] + z;
        return;
    }
    if (mode == 0x11) {
        const float x = interp[0];
        const float y = interp[1];
        const float z = interp[2];
        const float vx = interp[9];
        const float vy = interp[10];
        const float vz = interp[11];
        interp[0] = x + vx;
        interp[1] = y + vy;
        interp[2] = z + vz;
        interp[9] = vx + interp[3];
        interp[10] = interp[4] + vy;
        interp[11] = interp[5] + vz;
        out[0] = vx + x;
        out[1] = vy + y;
        out[2] = vz + z;
        return;
    }
    if (mode == 8) {
        const float t = interp[0xe] / duration;
        const float t2 = t * t;
        const float inv = kUnity - t;
        const float w0 = (kHermiteWeight - (t + t)) * t2;
        const float w1 = (t + t + kUnity) * inv * inv;
        const float w2 = inv * inv * t;
        const float w3 = inv * t2;
        out[0] = w0 * interp[0] + w1 * interp[3] + w2 * interp[6] + w3 * interp[9];
        out[1] = w0 * interp[1] + w1 * interp[4] + w2 * interp[7] + w3 * interp[10];
        out[2] = w0 * interp[2] + w1 * interp[5] + w2 * interp[8] + w3 * interp[11];
        return;
    }
    const float curve = EvaluateCurve(interp[0xe], duration, mode);
    out[0] = interp[0] + (interp[3] - interp[0]) * curve;
    out[1] = interp[1] + (interp[4] - interp[1]) * curve;
    out[2] = interp[2] + (interp[5] - interp[2]) * curve;
}

void SampleVec3Anim(u8 *interp_base, u8 *out_bytes)
{
    float sample[3];
    FetchTickVector(interp_base, sample);
    out_bytes[0] = static_cast<u8>(static_cast<i32>(sample[0]));
    out_bytes[1] = static_cast<u8>(static_cast<i32>(sample[1]));
    out_bytes[2] = static_cast<u8>(static_cast<i32>(sample[2]));
}

u8 SampleScalarAnim(u8 *interp_base)
{
    float *const interp = reinterpret_cast<float *>(interp_base);
    const float duration = interp[9];
    if (duration > 0.0f) {
        interp[4] = interp[5];
        const float rate = **reinterpret_cast<const float *const *>(interp + 7);
        if (rate <= kRateUnityHigh &&
            (rate < kRateUnityLow ==
             (((rate) != (rate)) || ((kRateUnityLow) != (kRateUnityLow))))) {
            interp[6] = rate + interp[6];
            interp[5] = static_cast<float>(ConvertFloatToI32TowardZeroX87(interp[6]));
        } else {
            interp[6] = interp[6] + kUnity;
            interp[5] = interp[5] + 1.0f;
        }
        if (interp[5] >= duration) {
            u32 &flags = *reinterpret_cast<u32 *>(interp + 8);
            if ((flags & 1U) == 0) {
                interp[5] = 0.0f;
                interp[4] = static_cast<float>(kTimerInactive);
                interp[6] = 0.0f;
                *reinterpret_cast<const float **>(interp + 7) =
                    &g_MainChainStartupScale;
                flags |= 1U;
            }
            interp[5] = duration;
            interp[4] = duration - 1.0f;
            interp[6] = duration;
            interp[9] = 0.0f;
            if (static_cast<i32>(interp[10]) != 7)
                return static_cast<u8>(static_cast<i32>(interp[1]));
            return static_cast<u8>(static_cast<i32>(interp[0]));
        }
    }
    const i32 mode = static_cast<i32>(interp[10]);
    if (mode == 7) {
        interp[0] = interp[0] + interp[1];
        return static_cast<u8>(static_cast<i32>(interp[0]));
    }
    if (mode == 0x11) {
        interp[0] = interp[0] + interp[3];
        interp[3] = interp[1] + interp[3];
        return static_cast<u8>(static_cast<i32>(interp[0]));
    }
    if (mode == 8)
        return static_cast<u8>(static_cast<i32>(interp[0]));
    const float curve = EvaluateCurve(interp[6], duration, mode);
    const float value = interp[0] + (interp[1] - interp[0]) * curve;
    return static_cast<u8>(static_cast<i32>(value));
}

void SampleFloat2Anim(u8 *interp_base, float out[2])
{
    float *const interp = reinterpret_cast<float *>(interp_base);
    const float duration = interp[0xd];
    if (duration > 0.0f) {
        interp[8] = interp[9];
        const float rate = **reinterpret_cast<const float *const *>(interp + 0xb);
        if (rate <= kRateUnityHigh &&
            (rate < kRateUnityLow ==
             (((rate) != (rate)) || ((kRateUnityLow) != (kRateUnityLow))))) {
            interp[10] = rate + interp[10];
            interp[9] = static_cast<float>(ConvertFloatToI32TowardZeroX87(interp[10]));
        } else {
            interp[10] = interp[10] + kUnity;
            interp[9] = interp[9] + 1.0f;
        }
        if (interp[9] >= duration) {
            u32 &flags = *reinterpret_cast<u32 *>(interp + 0xc);
            if ((flags & 1U) == 0) {
                interp[9] = 0.0f;
                interp[8] = static_cast<float>(kTimerInactive);
                interp[10] = 0.0f;
                *reinterpret_cast<const float **>(interp + 0xb) =
                    &g_MainChainStartupScale;
                flags |= 1U;
            }
            interp[9] = duration;
            interp[8] = duration - 1.0f;
            interp[10] = duration;
            interp[0xd] = 0.0f;
            if (static_cast<i32>(interp[0xe]) != 7) {
                out[0] = interp[2];
                out[1] = interp[3];
                return;
            }
            out[0] = interp[0];
            out[1] = interp[1];
            return;
        }
    }
    const i32 mode = static_cast<i32>(interp[0xe]);
    if (mode == 7) {
        const float x = interp[0];
        const float y = interp[1];
        interp[0] = x + interp[2];
        interp[1] = y + interp[3];
        out[0] = x + interp[2];
        out[1] = y + interp[3];
        return;
    }
    if (mode == 0x11) {
        const float x = interp[0];
        const float y = interp[1];
        const float vx = interp[6];
        const float vy = interp[7];
        interp[0] = x + vx;
        interp[1] = y + vy;
        interp[6] = vx + interp[2];
        interp[7] = interp[3] + vy;
        out[0] = x + vx;
        out[1] = y + vy;
        return;
    }
    if (mode == 8) {
        const float t = interp[10] / duration;
        const float t2 = t * t;
        const float inv = kUnity - t;
        const float w0 = (kHermiteWeight - (t + t)) * t2;
        const float w1 = (kHermiteWeight - (t + t)) * t2;
        const float w2 = inv * inv * t;
        const float w3 = inv * t2;
        out[0] = w0 * interp[0] + w1 * interp[2] + w2 * interp[4] + w3 * interp[6];
        out[1] = w0 * interp[1] + w1 * interp[3] + w2 * interp[5] + w3 * interp[7];
        return;
    }
    const float curve = EvaluateCurve(interp[10], duration, mode);
    out[0] = interp[0] + (interp[2] - interp[0]) * curve;
    out[1] = interp[1] + (interp[3] - interp[1]) * curve;
}

void PolarToCartesian(float *out, float angle, float radius)
{
    out[0] = static_cast<float>(std::cos(static_cast<double>(angle)) *
                              static_cast<double>(radius));
    out[1] = static_cast<float>(std::sin(static_cast<double>(angle)) *
                              static_cast<double>(radius));
}

void TriggerVec3Render(u8 *block)
{
    u32 &flags = *reinterpret_cast<u32 *>(block + 0x40);
    if ((flags & 1U) == 0) {
        *reinterpret_cast<i32 *>(block + 0x34) = 0;
        *reinterpret_cast<i32 *>(block + 0x30) = kTimerInactive;
        *reinterpret_cast<i32 *>(block + 0x38) = 0;
        *reinterpret_cast<const float **>(block + 0x3c) = &g_MainChainStartupScale;
        flags |= 1U;
    }
    *reinterpret_cast<i32 *>(block + 0x34) = 0;
    *reinterpret_cast<i32 *>(block + 0x38) = 0;
    *reinterpret_cast<i32 *>(block + 0x30) = -1;
}

void SetupPolyline(u8 *node)
{
    (void)node;
}

void QueueTimelineAudio(u8 *node, i32 audio_id)
{
    AdvanceVmTimer(node + 0x5c, static_cast<float>(-audio_id));
}

void StartVec3Anim(u8 *node, u8 *pc, u32 block_offset, u32 copy_offset,
                   u32 duration_offset)
{
    u8 flags = pc[3];
    i32 anim_id = *reinterpret_cast<i32 *>(pc + 4);
    if (flags & 1U)
        anim_id = ReadVmIntRegister(node, anim_id);
    *reinterpret_cast<i32 *>(node + block_offset) = anim_id;
    std::memcpy(node + block_offset + 4, g_AnimDefaultVector, sizeof(g_AnimDefaultVector));
    std::memcpy(node + block_offset + 16, g_AnimDefaultVector, sizeof(g_AnimDefaultVector));
    std::memcpy(node + block_offset + 28, g_AnimDefaultVector, sizeof(g_AnimDefaultVector));
    if ((*reinterpret_cast<u32 *>(node + 0x35c) & 0x100U) == 0) {
        std::memcpy(node + copy_offset, node + 0x334, 12);
    } else {
        std::memcpy(node + copy_offset, node + 0x34c, 12);
    }
    i32 duration = *reinterpret_cast<i32 *>(pc + duration_offset);
    if (flags & (1u << ((duration_offset - 4) / 2)))
        duration = ReadVmIntRegister(node, duration);
    *reinterpret_cast<i32 *>(node + block_offset + 44) = duration;
    TriggerVec3Render(node + copy_offset);
}

void StartFloat2Anim(u8 *node, u8 *pc)
{
    (void)pc;
    *reinterpret_cast<u32 *>(node + 0x1b4) = 1U;
    TriggerVec3Render(node + 0x188);
    *reinterpret_cast<u32 *>(node + 0x35c) |= 8U;
}

void StartScalarAnim(u8 *node, u8 *pc)
{
    (void)pc;
    *reinterpret_cast<u32 *>(node + 0x22c) = 1U;
}

void StartColorAnim(u8 *node, u8 *pc)
{
    (void)pc;
    *reinterpret_cast<u32 *>(node + 300) = 1U;
}

void BindSubEntry(u8 *node, u8 *pc)
{
    i32 entry = *reinterpret_cast<i32 *>(pc + 8);
    if (pc[3] & 1U)
        entry = ReadVmIntRegister(node, entry);
    InitializeAsciiAnimationVmEntry(node, static_cast<u32>(entry),
                                    *reinterpret_cast<void **>(node + 0x308));
    *reinterpret_cast<i32 *>(node + 0x380) = *reinterpret_cast<i32 *>(node + 0x60);
}

void LinkChildNode(u8 *parent_link, u8 *child)
{
    u8 *const next = *reinterpret_cast<u8 **>(parent_link + 4);
    if (next != 0) {
        *reinterpret_cast<u8 **>(child + 4) = next;
        *reinterpret_cast<u8 **>(next + 8) = child;
    }
    *reinterpret_cast<u8 **>(parent_link + 4) = child;
    *reinterpret_cast<u8 **>(child + 8) = parent_link;
}

void CopyCreatedObjectVectors(u8 *node, u8 *created, bool copy_all_three)
{
    // The binary copies all three dwords of both vectors unconditionally;
    // the copy_all_three parameter is retained for call-site compatibility.
    (void)copy_all_three;
    *reinterpret_cast<i32 *>(created + 0x34c) =
        *reinterpret_cast<i32 *>(node + 0x334);
    *reinterpret_cast<i32 *>(created + 0x350) =
        *reinterpret_cast<i32 *>(node + 0x338);
    *reinterpret_cast<i32 *>(created + 0x354) =
        *reinterpret_cast<i32 *>(node + 0x33c);
    *reinterpret_cast<i32 *>(created + 0x340) =
        *reinterpret_cast<i32 *>(node + 0x340);
    *reinterpret_cast<i32 *>(created + 0x344) =
        *reinterpret_cast<i32 *>(node + 0x344);
    *reinterpret_cast<i32 *>(created + 0x348) =
        *reinterpret_cast<i32 *>(node + 0x348);
}

i32 ResolveIntOperand(u8 *node, u8 *pc, u8 bit, i32 offset)
{
    i32 value = *reinterpret_cast<i32 *>(pc + offset);
    if (pc[3] & static_cast<u8>(1u << bit))
        value = ReadVmIntRegister(node, value);
    return value;
}

float ResolveFloatOperand(u8 *node, u8 *pc, u8 bit, i32 offset)
{
    float value = *reinterpret_cast<float *>(pc + offset);
    if (pc[3] & static_cast<u8>(1u << bit))
        value = ReadVmFloatRegister(node, value, value);
    return value;
}

void JumpConditionalFalse(u8 *node, u8 *pc)
{
    ResetVmTimer(node + 0x5c, *reinterpret_cast<i32 *>(pc + 20));
    *reinterpret_cast<u8 **>(node + 0x390) =
        *reinterpret_cast<u8 **>(node + 0x38c) +
        *reinterpret_cast<i32 *>(pc + 16);
}

void RestartKindScript(u8 *node)
{
    const short kind = *reinterpret_cast<short *>(node + 0x304);
    u8 *fallback = 0;
    u8 *scan = *reinterpret_cast<u8 **>(node + 0x38c);
    u16 opcode;
    for (;;) {
        opcode = *reinterpret_cast<u16 *>(scan);
        if (opcode == 0x40 &&
            (kind == *reinterpret_cast<short *>(scan + 8) ||
             *reinterpret_cast<short *>(scan + 8) == -1)) {
            if (*reinterpret_cast<short *>(scan + 8) == -1)
                fallback = scan;
            if (kind == *reinterpret_cast<short *>(scan + 8))
                break;
        }
        if (opcode == 0xffff)
            break;
        scan = scan + *reinterpret_cast<u16 *>(scan + 2);
    }
    if (opcode != 0x40) {
        if (fallback != 0)
            scan = fallback;
        else {
            AdvanceVmTimer(node + 0x5c, -1.0f);
            return;
        }
    }
    *reinterpret_cast<short *>(node + 0x304) = 0;
    *reinterpret_cast<u32 *>(node + 0x35c) &= ~0x1000U;
    *reinterpret_cast<i32 *>(node + 0x368) = *reinterpret_cast<i32 *>(node + 0x5c);
    *reinterpret_cast<i32 *>(node + 0x36c) = *reinterpret_cast<i32 *>(node + 0x60);
    *reinterpret_cast<i32 *>(node + 0x370) = *reinterpret_cast<i32 *>(node + 0x64);
    *reinterpret_cast<u32 *>(node + 0x374) =
        *reinterpret_cast<u32 *>(node + 0x68);
    *reinterpret_cast<u32 *>(node + 0x378) =
        *reinterpret_cast<u32 *>(node + 0x6c);
    *reinterpret_cast<u8 **>(node + 0x37c) =
        *reinterpret_cast<u8 **>(node + 0x390);
    ResetVmTimer(node + 0x5c, *reinterpret_cast<i32 *>(scan + 4));
    *reinterpret_cast<u8 **>(node + 0x390) =
        scan + *reinterpret_cast<u16 *>(scan + 2);
    *reinterpret_cast<u32 *>(node + 0x35c) |= 1U;
}

void RunPolylineEpilogue(u8 *node)
{
    i32 count = *reinterpret_cast<i32 *>(node + 0x30c) - 1;
    float *cursor = *reinterpret_cast<float **>(node + 0x358);
    float angle = *reinterpret_cast<float *>(node + 0x2c);
    const float step = kTwoPi / static_cast<float>(count);
    float phase = 0.0f;
    const float radial_step =
        static_cast<float>(*reinterpret_cast<i32 *>(node + 0x310)) /
        static_cast<float>(count);
    float *const anchor = cursor;
    if (count > 0) {
        do {
            cursor[3] = kUnity;
            cursor[4] = *reinterpret_cast<float *>(node + 0x2fc);
            cursor[5] = *reinterpret_cast<float *>(
                            *reinterpret_cast<u8 **>(node + 0x394) + 0x20) +
                        *reinterpret_cast<float *>(node + 0x54);
            cursor[6] = phase + *reinterpret_cast<float *>(node + 0x58);
            PolarToCartesian(
                cursor, angle,
                *reinterpret_cast<float *>(node + 0x3c) * kEaseHalf +
                    *reinterpret_cast<float *>(node + 0x40));
            cursor[2] = 0.0f;
            cursor[0] = *reinterpret_cast<float *>(node + 0x340) +
                        *reinterpret_cast<float *>(node + 0x334) + cursor[0];
            cursor[1] = *reinterpret_cast<float *>(node + 0x344) +
                        *reinterpret_cast<float *>(node + 0x338) + cursor[1];
            cursor[2] = *reinterpret_cast<float *>(node + 0x348) +
                        *reinterpret_cast<float *>(node + 0x33c) + cursor[2];
            cursor[10] = kUnity;
            cursor[11] = *reinterpret_cast<float *>(node + 0x2fc);
            cursor[12] = *reinterpret_cast<float *>(
                             *reinterpret_cast<u8 **>(node + 0x394) + 0x28) +
                         *reinterpret_cast<float *>(node + 0x54);
            cursor[13] = phase + *reinterpret_cast<float *>(node + 0x58);
            PolarToCartesian(
                cursor + 7, phase,
                *reinterpret_cast<float *>(node + 0x40) -
                    *reinterpret_cast<float *>(node + 0x3c) * kEaseHalf);
            cursor[9] = 0.0f;
            cursor[7] = *reinterpret_cast<float *>(node + 0x340) +
                       *reinterpret_cast<float *>(node + 0x334) + cursor[7];
            cursor[8] = *reinterpret_cast<float *>(node + 0x344) +
                       *reinterpret_cast<float *>(node + 0x338) + cursor[8];
            cursor[9] = *reinterpret_cast<float *>(node + 0x348) +
                       *reinterpret_cast<float *>(node + 0x33c) + cursor[9];
            phase += radial_step;
            angle = WrapAnglePi(angle, step);
            count -= 1;
            cursor += 14;
        } while (count != 0);
    }
    float *src = *reinterpret_cast<float **>(node + 0x358);
    float *dst = anchor;
    for (i32 i = 0; i != 7; ++i)
        dst[i] = src[i];
    dst[6] = phase + *reinterpret_cast<float *>(node + 0x58);
    src = *reinterpret_cast<float **>(node + 0x358) + 7;
    dst = anchor + 7;
    for (i32 i = 0; i != 7; ++i)
        dst[i] = src[i];
    dst[6] = phase + *reinterpret_cast<float *>(node + 0x58);
}

i32 RunSetupEpilogue(u8 *node)
{
    if (ScaleFieldUsesSentinel(*reinterpret_cast<float *>(node + 0x30))) {
        *reinterpret_cast<float *>(node + 0x24) = WrapAnglePi(
            *reinterpret_cast<float *>(node + 0x24),
            g_MainChainStartupScale * *reinterpret_cast<float *>(node + 0x30));
        *reinterpret_cast<u32 *>(node + 0x35c) |= 4U;
    }
    if (ScaleFieldUsesSentinel(*reinterpret_cast<float *>(node + 0x34))) {
        *reinterpret_cast<float *>(node + 0x28) = WrapAnglePi(
            *reinterpret_cast<float *>(node + 0x28),
            g_MainChainStartupScale * *reinterpret_cast<float *>(node + 0x34));
        *reinterpret_cast<u32 *>(node + 0x35c) |= 4U;
    }
    if (ScaleFieldUsesSentinel(*reinterpret_cast<float *>(node + 0x38))) {
        *reinterpret_cast<float *>(node + 0x2c) = WrapAnglePi(
            *reinterpret_cast<float *>(node + 0x2c),
            g_MainChainStartupScale * *reinterpret_cast<float *>(node + 0x38));
        *reinterpret_cast<u32 *>(node + 0x35c) |= 4U;
    }
    if (ScaleFieldUsesSentinel(*reinterpret_cast<float *>(node + 0x48))) {
        *reinterpret_cast<float *>(node + 0x40) =
            g_MainChainStartupScale * *reinterpret_cast<float *>(node + 0x48) +
            *reinterpret_cast<float *>(node + 0x40);
        *reinterpret_cast<u32 *>(node + 0x35c) |= 8U;
    }
    if (ScaleFieldUsesSentinel(*reinterpret_cast<float *>(node + 0x44))) {
        *reinterpret_cast<float *>(node + 0x3c) =
            g_MainChainStartupScale * *reinterpret_cast<float *>(node + 0x44) +
            *reinterpret_cast<float *>(node + 0x3c);
        *reinterpret_cast<u32 *>(node + 0x35c) |= 0xcU;
    }
    float angle = g_MainChainStartupScale *
                      *reinterpret_cast<float *>(node + 0x234) +
                  *reinterpret_cast<float *>(node + 0x54);
    if (angle < kUnity) {
        if (angle < kScaleSentinel !=
            (((angle) != (angle)) || ((kScaleSentinel) != (kScaleSentinel))))
            angle += kUnity;
    } else {
        angle -= kUnity;
    }
    *reinterpret_cast<float *>(node + 0x54) = angle;
    angle = g_MainChainStartupScale *
                *reinterpret_cast<float *>(node + 0x238) +
            *reinterpret_cast<float *>(node + 0x58);
    if (angle < kUnity) {
        if (angle < kScaleSentinel !=
            (((angle) != (angle)) || ((kScaleSentinel) != (kScaleSentinel))))
            angle += kUnity;
    } else {
        angle -= kUnity;
    }
    *reinterpret_cast<float *>(node + 0x58) = angle;
    if (*reinterpret_cast<u32 *>(node + 0x35c) & 0x2000U) {
        *reinterpret_cast<float *>(node + 0x340) += g_TimelineGlobalOffsetX;
        *reinterpret_cast<float *>(node + 0x344) += g_TimelineGlobalOffsetY;
        *reinterpret_cast<float *>(node + 0x348) += g_TimelineGlobalOffsetZ;
    }
    if (*reinterpret_cast<i32 *>(node + 0xb4) != 0) {
        float sample[3];
        if ((*reinterpret_cast<u32 *>(node + 0x35c) & 0x100U) == 0)
            FetchTickVector(node + 0x70, sample);
        else
            FetchTickVector(node + 0x70, sample);
        if ((*reinterpret_cast<u32 *>(node + 0x35c) & 0x100U) == 0) {
            *reinterpret_cast<float *>(node + 0x334) = sample[0];
            *reinterpret_cast<float *>(node + 0x338) = sample[1];
            *reinterpret_cast<float *>(node + 0x33c) = sample[2];
        } else {
            *reinterpret_cast<float *>(node + 0x34c) = sample[0];
            *reinterpret_cast<float *>(node + 0x350) = sample[1];
            *reinterpret_cast<float *>(node + 0x354) = sample[2];
        }
    }
    if (*reinterpret_cast<i32 *>(node + 0x100) != 0)
        SampleVec3Anim(node + 0xbc, node + 0x2fc);
    if (*reinterpret_cast<i32 *>(node + 300) != 0)
        *reinterpret_cast<u8 *>(node + 0x2ff) = SampleScalarAnim(node + 0x108);
    if (*reinterpret_cast<i32 *>(node + 0x1b4) != 0) {
        float sample[2];
        SampleFloat2Anim(node + 0x180, sample);
        *reinterpret_cast<float *>(node + 0x3c) = sample[0];
        *reinterpret_cast<float *>(node + 0x40) = sample[1];
        *reinterpret_cast<u32 *>(node + 0x35c) |= 8U;
    }
    if (*reinterpret_cast<i32 *>(node + 0x178) != 0) {
        float sample[3];
        FetchTickVector(node + 0x134, sample);
        *reinterpret_cast<float *>(node + 0x24) = sample[0];
        *reinterpret_cast<float *>(node + 0x28) = sample[1];
        *reinterpret_cast<float *>(node + 0x2c) = sample[2];
        *reinterpret_cast<u32 *>(node + 0x35c) |= 4U;
    }
    if (*reinterpret_cast<i32 *>(node + 0x200) != 0)
        SampleVec3Anim(node + 0x1bc, node + 0x300);
    if (*reinterpret_cast<i32 *>(node + 0x22c) != 0)
        *reinterpret_cast<u8 *>(node + 0x303) = SampleScalarAnim(node + 0x208);
    if ((*reinterpret_cast<u32 *>(node + 0x35c) & 0x3c00000U) == 0x2400000U)
        RunPolylineEpilogue(node);
    const float *rate =
        *reinterpret_cast<const float *const *>(node + 0x68);
    *reinterpret_cast<i32 *>(node + 0x5c) =
        *reinterpret_cast<i32 *>(node + 0x60);
    if ((rate[0] <= kRateUnityHigh) &&
        (rate[0] < kRateUnityLow ==
         (((rate[0]) != (rate[0])) || ((kRateUnityLow) != (kRateUnityLow))))) {
        *reinterpret_cast<float *>(node + 0x64) =
            rate[0] + *reinterpret_cast<float *>(node + 0x64);
        *reinterpret_cast<i32 *>(node + 0x60) =
            ConvertFloatToI32TowardZeroX87(*reinterpret_cast<float *>(node + 0x64));
    } else {
        *reinterpret_cast<i32 *>(node + 0x60) =
            *reinterpret_cast<i32 *>(node + 0x60) + 1;
        *reinterpret_cast<float *>(node + 0x64) =
            *reinterpret_cast<float *>(node + 0x64) + kUnity;
    }
    return 0;
}

bool DispatchSetupOpcode(u8 *node, u8 *pc)
{
    const u16 opcode = *reinterpret_cast<u16 *>(pc);
    const u16 step = *reinterpret_cast<u16 *>(pc + 2);
    u8 flags = pc[3];
    u32 &node_flags = *reinterpret_cast<u32 *>(node + 0x35c);

    switch (opcode) {
    case 0xffff:
    case 1:
        node_flags &= ~1U;
        // fallthrough
    case 2:
        *reinterpret_cast<u8 **>(node + 0x390) = 0;
        return false;
    case 3:
        node_flags |= 1U;
            BindSubEntry(node, pc);
        return true;
    case 4:
        ResetVmTimer(node + 0x5c, *reinterpret_cast<i32 *>(pc + 12));
        *reinterpret_cast<u8 **>(node + 0x390) =
            *reinterpret_cast<u8 **>(node + 0x38c) +
            *reinterpret_cast<i32 *>(pc + 8);
        return true;
    case 5: {
        i32 count = *reinterpret_cast<i32 *>(pc + 8);
        if (flags & 1U)
            count = ReadVmIntRegister(node, count);
        i32 *target = ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0);
        *target = *target - 1;
        if (count < 1) {
            JumpConditionalFalse(node, pc);
            return true;
        }
        ResetVmTimer(node + 0x5c, *reinterpret_cast<i32 *>(pc + 16));
        *reinterpret_cast<u8 **>(node + 0x390) =
            *reinterpret_cast<u8 **>(node + 0x38c) +
            *reinterpret_cast<i32 *>(pc + 12);
        return true;
    }
    case 6: {
        i32 value = ResolveIntOperand(node, pc, 1, 12);
        i32 *target = ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0);
        *target = value;
        break;
    }
    case 7: {
        float value = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = value;
        break;
    }
    case 8: {
        i32 delta = ResolveIntOperand(node, pc, 1, 12);
        i32 *target = ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0);
        *target = *target + delta;
        break;
    }
    case 9: {
        float delta = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = *target + delta;
        break;
    }
    case 10: {
        i32 delta = ResolveIntOperand(node, pc, 1, 12);
        i32 *target = ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0);
        *target = *target - delta;
        break;
    }
    case 11: {
        float delta = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = *target - delta;
        break;
    }
    case 12: {
        i32 factor = ResolveIntOperand(node, pc, 1, 12);
        i32 *target = ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0);
        *target = static_cast<i32>(static_cast<float>(*target) *
                                     static_cast<float>(factor));
        break;
    }
    case 13: {
        float factor = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = *target * factor;
        break;
    }
    case 14: {
        i32 divisor = ResolveIntOperand(node, pc, 1, 12);
        i32 *target = ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0);
        *target = *target / divisor;
        break;
    }
    case 15: {
        float divisor = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = *target / divisor;
        break;
    }
    case 16: {
        i32 divisor = ResolveIntOperand(node, pc, 1, 12);
        i32 *target = ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0);
        *target = *target % divisor;
        break;
    }
    case 17: {
        float divisor = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = std::fmod(*target, divisor);
        break;
    }
    case 18: {
        i32 rhs = ResolveIntOperand(node, pc, 2, 16);
        i32 lhs = ResolveIntOperand(node, pc, 1, 12);
        i32 *target = ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0);
        *target = lhs / rhs;
        break;
    }
    case 19: {
        float rhs = ResolveFloatOperand(node, pc, 2, 16);
        float lhs = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = lhs / rhs;
        break;
    }
    case 20: {
        i32 rhs = ResolveIntOperand(node, pc, 2, 16);
        i32 lhs = ResolveIntOperand(node, pc, 1, 12);
        i32 *target = ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0);
        *target = lhs - rhs;
        break;
    }
    case 21: {
        float rhs = ResolveFloatOperand(node, pc, 2, 16);
        float lhs = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = lhs - rhs;
        break;
    }
    case 22: {
        i32 rhs = ResolveIntOperand(node, pc, 2, 16);
        i32 lhs = ResolveIntOperand(node, pc, 1, 12);
        i32 *target = ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0);
        *target = rhs * lhs;
        break;
    }
    case 23: {
        float rhs = ResolveFloatOperand(node, pc, 2, 16);
        float lhs = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = rhs * lhs;
        break;
    }
    case 24: {
        i32 rhs = ResolveIntOperand(node, pc, 2, 16);
        i32 lhs = ResolveIntOperand(node, pc, 1, 12);
        i32 *target = ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0);
        *target = lhs % rhs;
        break;
    }
    case 25: {
        float rhs = ResolveFloatOperand(node, pc, 2, 16);
        float lhs = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = std::fmod(lhs, rhs);
        break;
    }
    case 26: {
        float max_value = ResolveFloatOperand(node, pc, 1, 12);
        float value = ResolveFloatOperand(node, pc, 0, 8);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = value < max_value ? value : max_value;
        break;
    }
    case 27: {
        i32 lhs = ResolveIntOperand(node, pc, 0, 8);
        i32 rhs = ResolveIntOperand(node, pc, 1, 12);
        if (lhs != rhs)
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 28: {
        float lhs = ResolveFloatOperand(node, pc, 0, 8);
        float rhs = ResolveFloatOperand(node, pc, 1, 12);
        if ((((lhs) != (lhs)) || ((rhs) != (rhs))) == (lhs == rhs))
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 29: {
        i32 lhs = ResolveIntOperand(node, pc, 0, 8);
        i32 rhs = ResolveIntOperand(node, pc, 1, 12);
        if (lhs == rhs)
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 30: {
        float lhs = ResolveFloatOperand(node, pc, 0, 8);
        float rhs = ResolveFloatOperand(node, pc, 1, 12);
        if ((((lhs) != (lhs)) || ((rhs) != (rhs))) != (lhs == rhs))
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 31: {
        i32 lhs = ResolveIntOperand(node, pc, 0, 8);
        i32 rhs = ResolveIntOperand(node, pc, 1, 12);
        if (rhs <= lhs)
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 32: {
        float lhs = ResolveFloatOperand(node, pc, 0, 8);
        float rhs = ResolveFloatOperand(node, pc, 1, 12);
        if (lhs < rhs == (((lhs) != (lhs)) || ((rhs) != (rhs))))
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 33: {
        i32 lhs = ResolveIntOperand(node, pc, 0, 8);
        i32 rhs = ResolveIntOperand(node, pc, 1, 12);
        if (rhs < lhs)
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 34: {
        float lhs = ResolveFloatOperand(node, pc, 0, 8);
        float rhs = ResolveFloatOperand(node, pc, 1, 12);
        if (lhs < rhs == (lhs == rhs))
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 35: {
        i32 lhs = ResolveIntOperand(node, pc, 0, 8);
        i32 rhs = ResolveIntOperand(node, pc, 1, 12);
        if (lhs <= rhs)
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 36: {
        float lhs = ResolveFloatOperand(node, pc, 0, 8);
        float rhs = ResolveFloatOperand(node, pc, 1, 12);
        if (lhs <= rhs)
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 37: {
        i32 lhs = ResolveIntOperand(node, pc, 0, 8);
        i32 rhs = ResolveIntOperand(node, pc, 1, 12);
        if (lhs < rhs)
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 38: {
        float lhs = ResolveFloatOperand(node, pc, 0, 8);
        float rhs = ResolveFloatOperand(node, pc, 1, 12);
        if (rhs <= lhs)
            break;
        JumpConditionalFalse(node, pc);
        return true;
    }
    case 0x28: {
        if (flags & 2U)
            (void)ReadVmIntRegister(node, *reinterpret_cast<i32 *>(pc + 12));
        u32 *target = reinterpret_cast<u32 *>(ResolveVmIntWriteTarget(
            reinterpret_cast<i32 *>(pc + 6), node, flags, 0));
        *target = PrngModulo(&SelectPrngState(node), *target);
        break;
    }
    case 0x29: {
        if (flags & 2U)
            (void)ReadVmFloatRegister(node, *reinterpret_cast<float *>(pc + 12),
                                      0.0f);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = PrngUnitFloat(&SelectPrngState(node));
        break;
    }
    case 0x2a: {
        float angle = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = static_cast<float>(std::sin(static_cast<double>(angle)));
        break;
    }
    case 0x2b: {
        float angle = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = static_cast<float>(std::cos(static_cast<double>(angle)));
        break;
    }
    case 0x2c: {
        float angle = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = static_cast<float>(std::tan(static_cast<double>(angle)));
        break;
    }
    case 0x2d: {
        float value = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = static_cast<float>(std::sqrt(static_cast<double>(value)));
        break;
    }
    case 0x2e: {
        float value = ResolveFloatOperand(node, pc, 1, 12);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = static_cast<float>(
            std::atan(static_cast<double>(value)));
        break;
    }
    case 0x2f: {
        float base = ResolveFloatOperand(node, pc, 0, 8);
        i32 reg = *reinterpret_cast<i32 *>(pc + 6);
        float *target = ResolveVmFloatWriteTarget(
            reinterpret_cast<float *>(pc + 12), node, flags, 0, reg);
        *target = WrapAnglePi(base, 0.0f);
        break;
    }
    case 0x30: {
        float z = ResolveFloatOperand(node, pc, 2, 16);
        float y = ResolveFloatOperand(node, pc, 1, 12);
        float x = ResolveFloatOperand(node, pc, 0, 8);
        float *target = 0;
        if ((node_flags & 0x100U) == 0)
            target = reinterpret_cast<float *>(node + 0x334);
        else
            target = reinterpret_cast<float *>(node + 0x34c);
        target[0] = x;
        target[1] = y;
        target[2] = z;
        break;
    }
    case 0x31:
        *reinterpret_cast<float *>(node + 0x24) =
            ResolveFloatOperand(node, pc, 0, 8);
        *reinterpret_cast<float *>(node + 0x28) =
            ResolveFloatOperand(node, pc, 1, 12);
        *reinterpret_cast<float *>(node + 0x2c) =
            ResolveFloatOperand(node, pc, 2, 16);
        node_flags |= 4U;
        break;
    case 0x32:
        *reinterpret_cast<float *>(node + 0x3c) =
            ResolveFloatOperand(node, pc, 0, 8);
        *reinterpret_cast<float *>(node + 0x40) =
            ResolveFloatOperand(node, pc, 1, 12);
        node_flags |= 8U;
        break;
    case 0x33: {
        u8 value = static_cast<u8>(ResolveIntOperand(node, pc, 0, 8));
        *reinterpret_cast<u8 *>(node + 0x2ff) = value;
        break;
    }
    case 0x34: {
        *reinterpret_cast<u8 *>(node + 0x2fe) =
            static_cast<u8>(ResolveIntOperand(node, pc, 0, 8));
        *reinterpret_cast<u8 *>(node + 0x2fd) =
            static_cast<u8>(ResolveIntOperand(node, pc, 1, 12));
        *reinterpret_cast<u8 *>(node + 0x2fc) =
            static_cast<u8>(ResolveIntOperand(node, pc, 2, 16));
        break;
    }
    case 0x35:
        *reinterpret_cast<float *>(node + 0x30) =
            ResolveFloatOperand(node, pc, 0, 8);
        *reinterpret_cast<float *>(node + 0x34) =
            ResolveFloatOperand(node, pc, 1, 12);
        *reinterpret_cast<float *>(node + 0x38) =
            ResolveFloatOperand(node, pc, 2, 16);
        node_flags |= 4U;
        break;
    case 0x36:
        *reinterpret_cast<float *>(node + 0x44) =
            ResolveFloatOperand(node, pc, 0, 8);
        if (flags & 2U)
            *reinterpret_cast<float *>(node + 0x48) =
                ReadVmFloatRegister(node, *reinterpret_cast<float *>(pc + 12),
                                    *reinterpret_cast<float *>(pc + 12));
        else
            *reinterpret_cast<float *>(node + 0x48) =
                *reinterpret_cast<float *>(pc + 12);
        break;
    case 0x37:
        StartScalarAnim(node, pc);
        break;
    case 0x38:
        StartColorAnim(node, pc);
        break;
    case 0x39:
        StartVec3Anim(node, pc, 0xb4, 0x70, 12);
        break;
    case 0x3a:
        StartFloat2Anim(node, pc);
        break;
    case 0x3b:
        StartVec3Anim(node, pc, 0x178, 0x134, 12);
        break;
    case 0x3c:
        StartFloat2Anim(node, pc);
        break;
    case 0x3d:
        *reinterpret_cast<float *>(node + 0x3c) =
            *reinterpret_cast<float *>(node + 0x3c) * kMirror;
        node_flags = (node_flags ^ 0x200U) | 8U;
        break;
    case 0x3e:
        *reinterpret_cast<float *>(node + 0x40) =
            *reinterpret_cast<float *>(node + 0x40) * kMirror;
        node_flags = (node_flags ^ 0x400U) | 8U;
        break;
    case 0x3f:
        RestartKindScript(node);
        return true;
    case 0x41: {
        u32 value = node_flags;
        value ^= ((static_cast<u32>(pc[8]) << 0x12) ^ value) & 0xc0000U;
        value ^= ((static_cast<u32>(pc[10]) << 0x14) ^ value) & 0x300000U;
        node_flags = value;
        break;
    }
    case 0x42:
        node_flags = (node_flags ^ (pc[8] << 4)) & 0x30U ^ node_flags;
        break;
    case 0x43: {
        node_flags = (node_flags ^ (pc[8] << 0x16)) & 0x3c00000U ^ node_flags;
        if ((node_flags & 0x3c00000U) == 0x2800000U)
            SetupPolyline(node);
        break;
    }
    case 0x44:
        *reinterpret_cast<u32 *>(node + 0x20) = pc[8];
        break;
    case 0x45:
        node_flags &= ~1U;
        RestartKindScript(node);
        return true;
    case 0x46:
        if (flags & 1U)
            *reinterpret_cast<float *>(node + 0x234) =
                ReadVmFloatRegister(node, *reinterpret_cast<float *>(pc + 8),
                                    *reinterpret_cast<float *>(pc + 8));
        else
            *reinterpret_cast<i32 *>(node + 0x234) =
                *reinterpret_cast<i32 *>(pc + 8);
        break;
    case 0x47:
        if (flags & 1U)
            *reinterpret_cast<float *>(node + 0x238) =
                ReadVmFloatRegister(node, *reinterpret_cast<float *>(pc + 8),
                                    *reinterpret_cast<float *>(pc + 8));
        else
            *reinterpret_cast<i32 *>(node + 0x238) =
                *reinterpret_cast<i32 *>(pc + 8);
        break;
    case 0x48:
        node_flags ^= (static_cast<u32>(pc[8]) ^ node_flags) & 1U;
        break;
    case 0x49:
        node_flags ^= (pc[8] << 0xb) & 0x800U;
        break;
    case 0x4a:
        node_flags ^= (pc[8] << 0xd) & 0x2000U;
        break;
    case 0x4b:
        if (flags & 1U)
            (void)ReadVmIntRegister(node, *reinterpret_cast<i32 *>(pc + 8));
        QueueTimelineAudio(node, *reinterpret_cast<i32 *>(pc + 8));
        break;
    case 0x4c:
        *reinterpret_cast<u8 *>(node + 0x302) =
            static_cast<u8>(ResolveIntOperand(node, pc, 0, 8));
        *reinterpret_cast<u8 *>(node + 0x301) =
            static_cast<u8>(ResolveIntOperand(node, pc, 1, 12));
        *reinterpret_cast<u8 *>(node + 0x300) =
            static_cast<u8>(ResolveIntOperand(node, pc, 2, 16));
        break;
    case 0x4d:
        *reinterpret_cast<u8 *>(node + 0x303) =
            static_cast<u8>(ResolveIntOperand(node, pc, 0, 8));
        break;
    case 0x4e:
        StartColorAnim(node, pc);
        break;
    case 0x4f:
        StartScalarAnim(node, pc);
        break;
    case 0x50:
        node_flags ^= (pc[8] << 0xf) & 0x8000U;
        break;
    case 0x51:
        *reinterpret_cast<i32 *>(node + 0x5c) =
            *reinterpret_cast<i32 *>(node + 0x368);
        *reinterpret_cast<i32 *>(node + 0x60) =
            *reinterpret_cast<i32 *>(node + 0x36c);
        *reinterpret_cast<i32 *>(node + 0x64) =
            *reinterpret_cast<i32 *>(node + 0x370);
        *reinterpret_cast<u32 *>(node + 0x68) =
            *reinterpret_cast<u32 *>(node + 0x374);
        *reinterpret_cast<u32 *>(node + 0x6c) =
            *reinterpret_cast<u32 *>(node + 0x378);
        *reinterpret_cast<u8 **>(node + 0x390) =
            *reinterpret_cast<u8 **>(node + 0x37c);
        break;
    case 0x52:
        node_flags ^= (pc[8] << 0x1b) & 0x8000000U;
        break;
    case 0x53:
        std::memcpy(node + 0x334, node + 0x340, 12);
        *reinterpret_cast<i32 *>(node + 0x340) = 0;
        *reinterpret_cast<i32 *>(node + 0x344) = 0;
        *reinterpret_cast<i32 *>(node + 0x348) = 0;
        break;
    case 0x54: {
        i32 count = ResolveIntOperand(node, pc, 0, 8);
        node_flags = node_flags & 0xfe7fffffU | 0x2400000U;
        *reinterpret_cast<void **>(node + 0x358) =
            std::malloc(static_cast<size_t>(count) * 0x38U);
        break;
    }
    case 0x55:
        node_flags ^= (pc[8] << 0x1c) & 0x10000000U;
        break;
    case 0x56: {
        i32 bit = ResolveIntOperand(node, pc, 0, 8);
        node_flags = (node_flags ^ (bit << 0x1d)) & 0x20000000U ^ node_flags;
        break;
    }
    case 0x57:
        node_flags ^= (pc[8] << 0x1e) & 0x40000000U;
        break;
    case 0x58: {
        i32 entry = ResolveIntOperand(node, pc, 0, 8);
        i32 *created = SpawnSetupEffectVmListABack(
            entry, *reinterpret_cast<u32 *>(node + 0x20));
        u8 *child = static_cast<u8 *>(
            RefreshTimelineTextHandle(reinterpret_cast<void **>(created)));
        LinkChildNode(node + 0x10, child);
        CopyCreatedObjectVectors(node, child, true);
        break;
    }
    case 0x59:
        node_flags = (node_flags & 0x7fffffffU) |
                     (static_cast<u32>(pc[8]) << 0x1f);
        break;
    case 0x5a: {
        i32 entry = ResolveIntOperand(node, pc, 0, 8);
        i32 *created = SpawnSetupEffectVmListBBack(
            entry, *reinterpret_cast<u32 *>(node + 0x20));
        u8 *child = static_cast<u8 *>(
            RefreshTimelineTextHandle(reinterpret_cast<void **>(created)));
        LinkChildNode(node + 0x10, child);
        CopyCreatedObjectVectors(node, child, true);
        break;
    }
    case 0x5b: {
        i32 entry = ResolveIntOperand(node, pc, 0, 8);
        i32 *created = SpawnSetupEffectVmListAFront(
            entry, *reinterpret_cast<u32 *>(node + 0x20));
        u8 *child = static_cast<u8 *>(
            RefreshTimelineTextHandle(reinterpret_cast<void **>(created)));
        LinkChildNode(node + 0x10, child);
        CopyCreatedObjectVectors(node, child, false);
        break;
    }
    case 0x5c: {
        i32 entry = ResolveIntOperand(node, pc, 0, 8);
        i32 *created = SpawnSetupEffectVmListBFront(
            entry, *reinterpret_cast<u32 *>(node + 0x20));
        u8 *child = static_cast<u8 *>(
            RefreshTimelineTextHandle(reinterpret_cast<void **>(created)));
        LinkChildNode(node + 0x10, child);
        CopyCreatedObjectVectors(node, child, false);
        break;
    }
    default:
        JumpConditionalFalse(node, pc);
        return true;
    }
    *reinterpret_cast<u8 **>(node + 0x390) = pc + step;
    return true;
}

} // namespace

i32 FinalizeTimelineRenderObjectSetup(void *node_memory)
{
    u8 *const node = static_cast<u8 *>(node_memory);
    if (*reinterpret_cast<u8 **>(node + 0x390) == 0)
        return 1;
    if (*reinterpret_cast<u32 *>(node + 0x35c) & 0x20000U)
        return 0;

    const float saved_scale = g_MainChainStartupScale;
    if (*reinterpret_cast<u32 *>(node + 0x35c) & 0x20000000U)
        g_MainChainStartupScale = kUnity;

    if (*reinterpret_cast<short *>(node + 0x304) != 0)
        RestartKindScript(node);

    for (;;) {
        u8 *const pc = *reinterpret_cast<u8 **>(node + 0x390);
        const i32 limit = static_cast<i32>(*reinterpret_cast<const short *>(pc + 4));
        if (*reinterpret_cast<i32 *>(node + 0x60) >= limit)
            break;
        if (!DispatchSetupOpcode(node, pc)) {
            g_MainChainStartupScale = saved_scale;
            return 1;
        }
    }

    const i32 result = RunSetupEpilogue(node);
    g_MainChainStartupScale = saved_scale;
    return result;
}

} // namespace th10
