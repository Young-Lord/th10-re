#include "VmLeafHelpers.hpp"

#include "EclEasedTransforms.hpp"
#include "PlayerMotionHelpers.hpp"
#include "PlayerTimerHelpers.hpp"

#include <cmath>
#include <stdlib.h>

namespace th10 {

// Leaf helpers of the ANM VM interpreter. The interpolator blocks share
// the tail layout {timer prev/cur/accum/rate/flags at +0x30..+0x40,
// duration +0x44, mode +0x48}; mode 7 adds a delta, 8 rides a cubic
// Hermite, 0x11 integrates velocity, everything else eases toward the
// endpoint through the curve selector.

namespace {

extern float g_FrameTimeScale; // TH10 DAT_00476f78

const float kRateUnityLow = 0.99f; // TH10 DAT_00470b68
const float kRateUnityHigh = 1.01f; // TH10 DAT_00470b64

inline float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void WriteFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

// The 0x463b2c conversion: round half away from zero.
i32 FloatToI32(float value)
{
    return value >= 0.0f
        ? static_cast<i32>(std::floor(static_cast<double>(value) + 0.5))
        : static_cast<i32>(std::ceil(static_cast<double>(value) - 0.5));
}

// The catch-up epilogue shared by every interpolator timer: advance by the
// rate (or step one when the rate sits inside the unity window), clamp and
// deactivate on completion. Returns true when the block completed.
bool AdvanceBlockTimer(u8 *block)
{
    const i32 duration =
        *reinterpret_cast<const i32 *>(block + 0x44);
    if (duration > 0) {
        *reinterpret_cast<i32 *>(block + 0x30) =
            *reinterpret_cast<const i32 *>(block + 0x34);
        const float rate =
            **reinterpret_cast<float *const *>(block + 0x3c);
        if (rate > kRateUnityLow && rate < kRateUnityHigh) {
            *reinterpret_cast<float *>(block + 0x38) =
                *reinterpret_cast<float *>(block + 0x38) + 1.0f;
            *reinterpret_cast<i32 *>(block + 0x34) =
                *reinterpret_cast<const i32 *>(block + 0x34) + 1;
        } else {
            const float acc =
                *reinterpret_cast<float *>(block + 0x38) + rate;
            *reinterpret_cast<float *>(block + 0x38) = acc;
            *reinterpret_cast<i32 *>(block + 0x34) = FloatToI32(acc);
        }
        return *reinterpret_cast<const i32 *>(block + 0x34) >= duration;
    }
    if ((*reinterpret_cast<u32 *>(block + 0x40) & 1) == 0) {
        *reinterpret_cast<i32 *>(block + 0x34) = 0;
        *reinterpret_cast<i32 *>(block + 0x30) =
            static_cast<i32>(0xfff0bdc1U);
        *reinterpret_cast<float *>(block + 0x38) = 0.0f;
        *reinterpret_cast<const float **>(block + 0x3c) =
            &g_FrameTimeScale;
        *reinterpret_cast<u32 *>(block + 0x40) |= 1;
    }
    *reinterpret_cast<i32 *>(block + 0x34) = 0;
    *reinterpret_cast<i32 *>(block + 0x30) =
        static_cast<i32>(0xfff0bdc1U);
    *reinterpret_cast<float *>(block + 0x38) = 0.0f;
    *reinterpret_cast<i32 *>(block + 0x44) = 0;
    return true;
}

} // namespace

// TH10 0x00404610. Block layout: cur @0, end @0xc, handle1 @0x18,
// handle2/vel @0x24, timer @0x30, duration @0x44, mode @0x48. Mode 7
// mutates the start position; 0x11 integrates velocity; 8 rides the cubic
// Hermite through both handles; everything else eases with the mode value
// as the curve selector (mode 0xf = 0.0, 0x10 = 1.0 per 0x44c350).
void TickVec3Interpolator(void *block_memory, float out_vec3[3])
{
    u8 *const block = static_cast<u8 *>(block_memory);
    const i32 mode = *reinterpret_cast<const i32 *>(block + 0x48);
    if (AdvanceBlockTimer(block)) {
        const u32 source = mode == 7 ? 0 : 0xc;
        for (u32 component = 0; component != 3; ++component)
            out_vec3[component] =
                ReadFloat(block, source + component * 4);
        return;
    }
    const float t = ReadFloat(block, 0x38) /
                    static_cast<float>(*reinterpret_cast<const i32 *>(
                        block + 0x44));
    for (u32 component = 0; component != 3; ++component) {
        const float pos = ReadFloat(block, component * 4);
        const float end = ReadFloat(block, 0xc + component * 4);
        float value;
        if (mode == 7) {
            value = pos + end;
            WriteFloat(block, component * 4, value);
        } else if (mode == 0x11) {
            value = pos + ReadFloat(block, 0x24 + component * 4);
            WriteFloat(block, component * 4, value);
            WriteFloat(block, 0x24 + component * 4,
                       ReadFloat(block, 0x24 + component * 4) + end);
        } else if (mode == 8) {
            const float h00 = (1.0f + 2.0f * t) * (1.0f - t) * (1.0f - t);
            const float h01 = (3.0f - 2.0f * t) * t * t;
            const float h10 = (1.0f - t) * (1.0f - t) * t;
            const float h11 = (t - 1.0f) * t * t;
            value = h00 * pos + h01 * end +
                    h10 * ReadFloat(block, 0x18 + component * 4) +
                    h11 * ReadFloat(block, 0x24 + component * 4);
        } else {
            const double factor = EasingCurveSelectorEaxStackAbi(
                mode, ReadFloat(block, 0x38),
                static_cast<float>(*reinterpret_cast<const i32 *>(
                    block + 0x44)));
            value = pos + (end - pos) * factor;
        }
        out_vec3[component] = value;
    }
}

// TH10 0x0044C350. The shared easing-curve selector behind every
// interpolator block. Computes t = value / denominator once, then picks
// the curve from EAX (block mode field, raw values 1..16). Constants are
// TH10 flt_470AFC = 1.0, flt_470B04 = 0.0, flt_470B08 = 2.0,
// flt_470B0C = 0.5. The in/out pairs compare 2*t against 1.0 with
// FCOM/FNSTSW: a NaN takes the "out" branch (the unordered case sets the
// parity flag the same as "not below"), preserved by negating the less
// test. Modes 7 and 8 never reach the selector (handled inline by the
// callers), and modes 0/7/8/>16 fall through to the raw ratio.
double EasingCurveSelectorEaxStackAbi(i32 mode, float value,
                                      float denominator)
{
    const double t = static_cast<double>(value) /
                     static_cast<double>(denominator);
    switch (mode) {
    case 1:
        return t * t;
    case 2:
        return t * t * t;
    case 3:
        return t * t * t * t;
    case 4:
        return 1.0 - (1.0 - t) * (1.0 - t);
    case 5:
        return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
    case 6:
        return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t) * (1.0 - t);
    case 9: {
        const double doubled = t + t;
        if (!(doubled < 1.0))
            return (2.0 - (2.0 - doubled) * (2.0 - doubled)) * 0.5;
        return doubled * doubled * 0.5;
    }
    case 10: {
        const double doubled = t + t;
        if (!(doubled < 1.0)) {
            const double mirrored = 2.0 - doubled;
            return (2.0 - mirrored * mirrored * mirrored) * 0.5;
        }
        return doubled * doubled * doubled * 0.5;
    }
    case 11: {
        const double doubled = t + t;
        if (!(doubled < 1.0)) {
            const double mirrored = 2.0 - doubled;
            return (2.0 - mirrored * mirrored * mirrored * mirrored) * 0.5;
        }
        return doubled * doubled * doubled * doubled * 0.5;
    }
    case 12: {
        const double doubled = t + t;
        if (!(doubled < 1.0))
            return (doubled - 1.0) * (doubled - 1.0) * 0.5 + 0.5;
        return 0.5 - (1.0 - doubled) * (1.0 - doubled) * 0.5;
    }
    case 13: {
        const double doubled = t + t;
        if (!(doubled < 1.0))
            return (doubled - 1.0) * (doubled - 1.0) * (doubled - 1.0) *
                       0.5 + 0.5;
        return 0.5 - (1.0 - doubled) * (1.0 - doubled) * (1.0 - doubled) *
                         0.5;
    }
    case 14: {
        const double doubled = t + t;
        if (!(doubled < 1.0))
            return (doubled - 1.0) * (doubled - 1.0) * (doubled - 1.0) *
                       (doubled - 1.0) * 0.5 + 0.5;
        return 0.5 - (1.0 - doubled) * (1.0 - doubled) * (1.0 - doubled) *
                         (1.0 - doubled) * 0.5;
    }
    case 15:
        return 0.0;
    case 16:
        return 1.0;
    default:
        return t;
    }
}

// TH10 0x004050d0. Re-arms the block timer to "frame 0 just started".
void ResetVec3InterpolatorTimer(void *block_memory)
{
    u8 *const block = static_cast<u8 *>(block_memory);
    if ((*reinterpret_cast<u32 *>(block + 0x40) & 1) == 0) {
        *reinterpret_cast<i32 *>(block + 0x34) = 0;
        *reinterpret_cast<i32 *>(block + 0x30) =
            static_cast<i32>(0xfff0bdc1U);
        *reinterpret_cast<float *>(block + 0x38) = 0.0f;
        *reinterpret_cast<const float **>(block + 0x3c) =
            &g_FrameTimeScale;
        *reinterpret_cast<u32 *>(block + 0x40) |= 1;
    }
    *reinterpret_cast<i32 *>(block + 0x34) = 0;
    *reinterpret_cast<float *>(block + 0x38) = 0.0f;
    *reinterpret_cast<i32 *>(block + 0x30) = -1;
}

// TH10 0x00413200. Node fields: next at +4, prev at +8. When the list is
// empty the new node's next field is deliberately left untouched.
void LinkChildListNode(void *node_memory, void *list_owner_memory)
{
    u32 *const node = static_cast<u32 *>(node_memory);
    u32 *const head = static_cast<u32 *>(list_owner_memory);
    u32 *const old_next = reinterpret_cast<u32 *>(head[1]);
    if (old_next != 0) {
        node[1] = reinterpret_cast<u32>(old_next);
        old_next[2] = reinterpret_cast<u32>(node);
    }
    head[1] = reinterpret_cast<u32>(node);
    node[2] = reinterpret_cast<u32>(head);
}

// TH10 0x0041ab70. Snapshot/current at entity+0x180, target at +0x188,
// duration +0x1b4, mode +0x1b8, timer at +0x1a0..+0x1b0.
void SetupScaleInterpolation(void *entity_memory, const float target[2],
                             i32 duration, i32 mode)
{
    u8 *const entity = static_cast<u8 *>(entity_memory);
    *reinterpret_cast<i32 *>(entity + 0x1b4) = duration;
    *reinterpret_cast<u8 *>(entity + 0x1b8) =
        static_cast<u8>(mode);
    *reinterpret_cast<u32 *>(entity + 0x180) =
        *reinterpret_cast<const u32 *>(entity + 0x3c);
    *reinterpret_cast<u32 *>(entity + 0x184) =
        *reinterpret_cast<const u32 *>(entity + 0x40);
    *reinterpret_cast<u32 *>(entity + 0x188) =
        *reinterpret_cast<const u32 *>(target);
    *reinterpret_cast<u32 *>(entity + 0x18c) =
        *reinterpret_cast<const u32 *>(target + 1);
    u8 *const timer = entity + 0x1a0;
    if ((*reinterpret_cast<u32 *>(timer + 0x10) & 1) == 0) {
        *reinterpret_cast<i32 *>(timer + 4) = 0;
        *reinterpret_cast<i32 *>(timer) = static_cast<i32>(0xfff0bdc1U);
        *reinterpret_cast<float *>(timer + 8) = 0.0f;
        *reinterpret_cast<const float **>(timer + 0xc) = &g_FrameTimeScale;
        *reinterpret_cast<u32 *>(timer + 0x10) |= 1;
    }
    *reinterpret_cast<i32 *>(timer + 4) = 0;
    *reinterpret_cast<float *>(timer + 8) = 0.0f;
    *reinterpret_cast<i32 *>(timer) = -1;
}

// TH10 0x00442220 / 0x00442050 (and the scalar twins 0x00442300 /
// 0x00441f50). Block: start @0, target @+0xc, ctrl/vel zeroed @+0x18,
// timer @+0x30, duration @+0x44, mode @+0x48.
static void SetupColorInterpolationBlock(u8 *block, const u8 start[3],
                                         const u8 target[3], i32 duration,
                                         i32 mode)
{
    *reinterpret_cast<i32 *>(block + 0x44) = duration;
    for (u32 component = 0; component != 3; ++component) {
        *reinterpret_cast<u32 *>(block + 0x18 + component * 4) = 0;
        *reinterpret_cast<u32 *>(block + 0x24 + component * 4) = 0;
    }
    *reinterpret_cast<i32 *>(block + 0x48) = mode;
    for (u32 component = 0; component != 3; ++component) {
        *reinterpret_cast<u8 *>(block + component) = start[component];
        *reinterpret_cast<u8 *>(block + 0xc + component) =
            target[component];
    }
    u8 *const timer = block + 0x30;
    if ((*reinterpret_cast<u32 *>(timer + 0x10) & 1) == 0) {
        *reinterpret_cast<i32 *>(timer + 4) = 0;
        *reinterpret_cast<i32 *>(timer) = static_cast<i32>(0xfff0bdc1U);
        *reinterpret_cast<float *>(timer + 8) = 0.0f;
        *reinterpret_cast<const float **>(timer + 0xc) = &g_FrameTimeScale;
        *reinterpret_cast<u32 *>(timer + 0x10) |= 1;
    }
    *reinterpret_cast<i32 *>(timer + 4) = 0;
    *reinterpret_cast<float *>(timer + 8) = 0.0f;
    *reinterpret_cast<i32 *>(timer) = -1;
}

void SetupRgbInterpolation(void *vm_memory, u32 block_base,
                           const u8 start[3], const u8 target[3],
                           i32 duration, i32 mode)
{
    SetupColorInterpolationBlock(static_cast<u8 *>(vm_memory) + block_base,
                                 start, target, duration, mode);
}

// The scalar twins share the same tail layout with the color stored as two
// dwords at +0/+4 and the deltas at +8/+0xc.
void SetupAlphaInterpolation(void *vm_memory, u32 block_base, i32 start,
                             i32 end, i32 duration, i32 mode)
{
    u8 *const block = static_cast<u8 *>(vm_memory) + block_base;
    *reinterpret_cast<i32 *>(block + 0x44) = duration;
    *reinterpret_cast<i32 *>(block + 0x48) = mode;
    *reinterpret_cast<i32 *>(block) = start;
    *reinterpret_cast<i32 *>(block + 4) = end;
    *reinterpret_cast<u32 *>(block + 8) = 0;
    *reinterpret_cast<u32 *>(block + 0xc) = 0;
    u8 *const timer = block + 0x30;
    if ((*reinterpret_cast<u32 *>(timer + 0x10) & 1) == 0) {
        *reinterpret_cast<i32 *>(timer + 4) = 0;
        *reinterpret_cast<i32 *>(timer) = static_cast<i32>(0xfff0bdc1U);
        *reinterpret_cast<float *>(timer + 8) = 0.0f;
        *reinterpret_cast<const float **>(timer + 0xc) = &g_FrameTimeScale;
        *reinterpret_cast<u32 *>(timer + 0x10) |= 1;
    }
    *reinterpret_cast<i32 *>(timer + 4) = 0;
    *reinterpret_cast<float *>(timer + 8) = 0.0f;
    *reinterpret_cast<i32 *>(timer) = -1;
}

// TH10 0x00428dd0. Negates the operand and shifts the vm script timer
// (base +0x5c) through the shared delta-shift helper.
void ShiftVmTimerBack(void *vm_memory, i32 frames)
{
    ShiftTimerByEsiStackAbi(static_cast<u8 *>(vm_memory) + 0x5c,
                            static_cast<float>(-frames));
}

namespace {

extern u16 g_TimelinePrngStateB[4]; // TH10 DAT_004918b0 (LCG state)
u32 g_TimelinePrngCounterB; // TH10 DAT_004918b4 (+2 per draw)

// TH10 0x0044bb90: centered float in [-1,1) from the 16-bit LCG pair.
// Native: fild(combined), +2^32 when negative, *2^-31, -1.0
// (flt_470BEC / flt_470AFC). Distinct from the [0,1) draw 0x0044bb20.
float PrngCenteredFloat()
{
    u32 x = (static_cast<u16>(*g_TimelinePrngStateB ^ 0x9630U) - 0x6553U);
    const i32 hi = static_cast<i32>((x >> 14 & 3U) + x * 4U);
    u16 lo = static_cast<u16>(
        (static_cast<u16>(hi) ^ 0x9630U) + 0x9aadU);
    *g_TimelinePrngStateB = static_cast<u16>(hi);
    lo = static_cast<u16>((lo >> 14) + lo * 4U);
    *reinterpret_cast<i32 *>(g_TimelinePrngStateB + 2) += 2;
    *g_TimelinePrngStateB = lo;
    const u32 combined = static_cast<u32>(hi) * 65536U + lo;
    float value = static_cast<float>(combined);
    if (static_cast<i32>(combined) < 0)
        value += 4294967296.0f;
    return value * (1.0f / 2147483648.0f) - 1.0f;
}

// TH10 0x00445880: the per-frame render callback installed into the
// record (boundary; it runs from the entity ticker). The frame-update
// callback 0x00445620 is implemented in EclEasedTransforms.cpp and bound
// through EclEasedTransforms.hpp, which declares both callbacks.

// TH10 0x452493 / 0x452422: operator new / delete.
u8 *AllocateHeapBlock(u32 bytes);
void FreeHeapBlock(void *pointer);

} // namespace

// TH10 0x0044452f0. Frees the previous vertex buffer, allocates 0x4b0
// bytes, installs the callbacks, and seeds 32 vertices of 7 floats
// {x, y, z, 1, 0, u, v} around the entity center: UV from the unit
// direction at half radius, the radial offset from a clamped random walk
// (draw/30 + previous, clamped to +-1/15), angles advancing 2*pi/31 from
// -pi. The two jitter fields at 0x4a4/0x4a8 take unit draws scaled by
// 1/120 (scaled reading of the native "rand*1/120").
i32 RebuildRibbonRingBuffer(void *entity_memory)
{
    u8 *const entity = static_cast<u8 *>(entity_memory);
    u8 *const previous =
        *reinterpret_cast<u8 *const *>(entity + 0x358);
    if (previous != 0)
        FreeHeapBlock(previous);
    u8 *const buffer = AllocateHeapBlock(0x4b0);
    *reinterpret_cast<u8 **>(entity + 0x358) = buffer;
    *reinterpret_cast<void **>(entity + 0x398) =
        reinterpret_cast<void *>(&RibbonFrameUpdateCallback);
    *reinterpret_cast<void **>(entity + 0x39c) =
        reinterpret_cast<void *>(&RibbonRenderCallback);
    WriteFloat(buffer, 0x4a4, PrngCenteredFloat() / 120.0f);
    WriteFloat(buffer, 0x4a8, PrngCenteredFloat() / 120.0f);

    float center[3];
    for (u32 component = 0; component != 3; ++component)
        center[component] =
            ReadFloat(entity, 0x334 + component * 4) +
            ReadFloat(entity, 0x340 + component * 4);
    WriteFloat(buffer, 0, center[0]);
    WriteFloat(buffer, 4, center[1]);
    WriteFloat(buffer, 8, center[2]);
    WriteFloat(buffer, 0xc, 1.0f);
    WriteFloat(buffer, 0x14, 0.5f);
    WriteFloat(buffer, 0x18, 0.5f);

    float angle = -3.1415927f;
    // The radial walk starts at centered * 1/15 and steps centered * 1/30,
    // clamped into [-1/15, +1/15] (unordered draws keep the walk value -
    // native quirk). Vertex radius is the absolute theta slot; the walk
    // value is only the per-frame delta the frame callback (0x00445620)
    // accumulates into it.
    float walk = PrngCenteredFloat() * (1.0f / 15.0f);
    for (u32 index = 0; index != 31; ++index) {
        u8 *const vertex = buffer + (index + 1) * 0x1c;
        float direction[2];
        PolarToCartesianEdiAbi(direction, angle, 0.5f);
        WriteFloat(vertex, 0x14, direction[0] + 0.5f);
        WriteFloat(vertex, 0x18, direction[1] + 0.5f);
        WriteFloat(vertex, 8, 0.0f);
        WriteFloat(vertex, 0xc, 1.0f);
        // Per-frame delta slot first (pre-update walk value), then the
        // absolute theta slot: centered * 8 + 80
        // (flt_470BD0 / flt_470C28).
        WriteFloat(buffer, 0x424 + index * 4, walk);
        const float theta = PrngCenteredFloat() * 8.0f + 80.0f;
        WriteFloat(buffer, 0x3a0 + index * 4, theta);
        walk += PrngCenteredFloat() * (1.0f / 30.0f);
        if (walk <= -1.0f / 15.0f) {
            walk = -1.0f / 15.0f;
        } else if (walk > 1.0f / 15.0f) {
            walk = 1.0f / 15.0f;
        }
        float offset[2];
        PolarToCartesianEdiAbi(offset, angle, theta);
        WriteFloat(vertex, 0, center[0] + offset[0]);
        WriteFloat(vertex, 4, center[1] + offset[1]);
        WriteFloat(vertex, 8, center[2] + offset[2]);
        angle += 6.2831855f / 31.0f;
        if (angle >= 3.25f)
            angle -= 6.2831855f;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Color track (TH10 0x004049a0) and its 0x1c-byte record helpers
// 0x404d40 / 0x404da0 / 0x404e10.
// ---------------------------------------------------------------------------

namespace {

// The 28-byte color record consumed by the track: six floats followed by
// four bytes that always mirror floats 2..5 through the 0x463b2c rounding
// conversion (the four back-to-back calls each pop one x87 value).
struct ColorTrackRecord {
    float fields[6];
    u8 bytes[4];
};

// TH10 0x404d40. Writes `to - from` into out. The native receives one stack
// argument (the eased factor pushed by 0x4049a0) and never reads it; the
// argument stays on the stack and chains into 0x404da0's factor parameter.
void ColorTrackRecordDifference(const ColorTrackRecord &from,
                                const ColorTrackRecord &to,
                                ColorTrackRecord &out)
{
    for (u32 i = 0; i != 6U; ++i)
        out.fields[i] = to.fields[i] - from.fields[i];
    for (u32 i = 0; i != 4U; ++i)
        out.bytes[i] = static_cast<u8>(FloatToI32(out.fields[i + 2]));
}

// TH10 0x404da0 (ret 4): out = factor * record.
void ColorTrackRecordScaled(const ColorTrackRecord &record, float factor,
                            ColorTrackRecord &out)
{
    for (u32 i = 0; i != 6U; ++i)
        out.fields[i] = factor * record.fields[i];
    for (u32 i = 0; i != 4U; ++i)
        out.bytes[i] = static_cast<u8>(FloatToI32(out.fields[i + 2]));
}

// TH10 0x404e10: out = a + b.
void ColorTrackRecordSum(const ColorTrackRecord &a, const ColorTrackRecord &b,
                         ColorTrackRecord &out)
{
    for (u32 i = 0; i != 6U; ++i)
        out.fields[i] = a.fields[i] + b.fields[i];
    for (u32 i = 0; i != 4U; ++i)
        out.bytes[i] = static_cast<u8>(FloatToI32(out.fields[i + 2]));
}

inline i32 ColorTrackLoadI32(const u8 *block, u32 offset)
{
    return *reinterpret_cast<const i32 *>(block + offset);
}

inline void ColorTrackCopyOut(const ColorTrackRecord &record,
                              u32 out_colors[7])
{
    out_colors[0] = *reinterpret_cast<const u32 *>(record.fields);
    out_colors[1] = *reinterpret_cast<const u32 *>(record.fields + 1);
    out_colors[2] = *reinterpret_cast<const u32 *>(record.fields + 2);
    out_colors[3] = *reinterpret_cast<const u32 *>(record.fields + 3);
    out_colors[4] = *reinterpret_cast<const u32 *>(record.fields + 4);
    out_colors[5] = *reinterpret_cast<const u32 *>(record.fields + 5);
    out_colors[6] = *reinterpret_cast<const u32 *>(record.bytes);
}

} // namespace

// TH10 0x004049a0. Native EBX = track block, stack = 7-dword scratch out
// (the native returns the scratch pointer and cleans one argument). Block:
// four 0x1c color records at +0x00/+0x1c/+0x38/+0x54, then the timer
// {prev@0x70, cur@0x74, accum@0x78, rate ptr@0x7c, flags@0x80}, duration
// @0x84 and mode@0x88. The rate window and the 0xfff0bdc1 poison match the
// vec3 interpolator, but the completion does NOT stop the track (the
// duration stays armed and every later tick re-snaps cur/prev/accum to the
// end). Mode 7 accumulates record 1 into record 0, 0x11 integrates record
// 3 as velocity against the record-1 delta, 8 rides a cubic Hermite (the
// native reuses the handle-1 weight (1-t)^2*t for record 3 as well), and
// everything else eases record 0 toward record 1 through the 0x44c350
// curve selector. Duration <= 0 skips the timer entirely and interpolates
// with t = accum / duration, so a zero duration produces inf/NaN weights.
u32 *TickColorTrack(void *block_memory, u32 out_colors[7])
{
    u8 *const block = static_cast<u8 *>(block_memory);
    ColorTrackRecord *const records =
        reinterpret_cast<ColorTrackRecord *>(block);

    i32 duration = ColorTrackLoadI32(block, 0x84);
    if (duration > 0) {
        *reinterpret_cast<i32 *>(block + 0x70) =
            *reinterpret_cast<const i32 *>(block + 0x74);
        const float rate = **reinterpret_cast<float *const *>(block + 0x7c);
        if (rate > kRateUnityLow && rate < kRateUnityHigh) {
            WriteFloat(block, 0x78, ReadFloat(block, 0x78) + 1.0f);
            *reinterpret_cast<i32 *>(block + 0x74) =
                *reinterpret_cast<const i32 *>(block + 0x74) + 1;
        } else {
            const float accum = ReadFloat(block, 0x78) + rate;
            WriteFloat(block, 0x78, accum);
            *reinterpret_cast<i32 *>(block + 0x74) = FloatToI32(accum);
        }
        duration = ColorTrackLoadI32(block, 0x84);
        if (*reinterpret_cast<const i32 *>(block + 0x74) >= duration) {
            if ((ColorTrackLoadI32(block, 0x80) & 1) == 0) {
                *reinterpret_cast<i32 *>(block + 0x74) = 0;
                *reinterpret_cast<i32 *>(block + 0x70) =
                    static_cast<i32>(0xfff0bdc1U);
                WriteFloat(block, 0x78, 0.0f);
                *reinterpret_cast<float **>(block + 0x7c) = &g_FrameTimeScale;
                *reinterpret_cast<u32 *>(block + 0x80) |= 1U;
            }
            // Re-snap to the end every tick past the duration.
            *reinterpret_cast<i32 *>(block + 0x74) = duration;
            *reinterpret_cast<i32 *>(block + 0x70) = duration - 1;
            WriteFloat(block, 0x78, static_cast<float>(duration));
            ColorTrackCopyOut(ColorTrackLoadI32(block, 0x88) == 7
                                  ? records[0] : records[1],
                              out_colors);
            return out_colors;
        }
    }

    const i32 mode = ColorTrackLoadI32(block, 0x88);
    ColorTrackRecord out_record;
    if (mode == 7) {
        ColorTrackRecord sum;
        ColorTrackRecordSum(records[0], records[1], sum);
        records[0] = sum;
        out_record = records[0];
    } else if (mode == 0x11) {
        const ColorTrackRecord old_velocity = records[3];
        ColorTrackRecord sum;
        ColorTrackRecordSum(old_velocity, records[0], sum);
        records[0] = sum;
        ColorTrackRecordSum(records[1], old_velocity, sum);
        records[3] = sum;
        out_record = records[0];
    } else if (mode == 8) {
        const float t = ReadFloat(block, 0x78) /
                        static_cast<float>(duration);
        const float w_start = (1.0f + 2.0f * t) * (t - 1.0f) * (t - 1.0f);
        const float w_end = (3.0f - 2.0f * t) * t * t;
        const float w_handle1 = (1.0f - t) * (1.0f - t) * t;
        const float w_handle2 = (t - 1.0f) * (t - 1.0f) * t;
        ColorTrackRecord scaled_start, scaled_end, scaled_handle1;
        ColorTrackRecord scaled_handle2, sum;
        ColorTrackRecordScaled(records[0], w_start, scaled_start);
        ColorTrackRecordScaled(records[1], w_end, scaled_end);
        ColorTrackRecordScaled(records[2], w_handle1, scaled_handle1);
        ColorTrackRecordScaled(records[3], w_handle2, scaled_handle2);
        ColorTrackRecordSum(scaled_start, scaled_end, sum);
        ColorTrackRecordSum(sum, scaled_handle1, sum);
        ColorTrackRecordSum(sum, scaled_handle2, sum);
        out_record = sum;
    } else {
        const float factor = static_cast<float>(EasingCurveSelectorEaxStackAbi(
            mode, ReadFloat(block, 0x78), static_cast<float>(duration)));
        ColorTrackRecord difference, scaled, total;
        ColorTrackRecordDifference(records[0], records[1], difference);
        ColorTrackRecordScaled(difference, factor, scaled);
        ColorTrackRecordSum(records[0], scaled, total);
        out_record = total;
    }
    ColorTrackCopyOut(out_record, out_colors);
    return out_colors;
}

} // namespace th10
