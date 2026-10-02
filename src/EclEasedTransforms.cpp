#include "EclEasedTransforms.hpp"

#include "ConditionalStateObject.hpp"
#include "EclScriptLibrary.hpp"
#include "VmLeafHelpers.hpp"

#include <cmath>

namespace th10 {

// Eased-transform cluster of the ECL/entity animation library: the per-frame
// tickers for the RGB color, alpha and vec2 scale interpolation blocks that
// 0x442220/0x442050/0x442300/0x441f50/0x41ab70 arm, the ECL script object
// constructor boundary, and the ribbon ring-buffer frame callback.

namespace {

extern float g_FrameTimeScale; // TH10 DAT_00476f78 (rate pointer default)
extern void *g_AsciiHudConditionalState; // TH10 DAT_00477704

// TH10 0x00450470. Native EAX = script-name request object (its +8 holds
// the table size and +140 the name table); resolves the requested name to
// table_index + 16, or 0.
i32 ResolveScriptTableIndexEaxAbi(i32 request);

const float kRateUnityLow = 0.99f; // TH10 DAT_00470b68
const float kRateUnityHigh = 1.01f; // TH10 DAT_00470b64
const float kAngleStep = 0.2026834f; // TH10 DAT_00470b6c (2*pi/31)
const float kHermiteThree = 3.0f; // TH10 DAT_00470bd4

const i32 kTimerPoison = static_cast<i32>(0xfff0bdc1U); // -999999

inline float LoadF32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void StoreF32(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

inline i32 LoadI32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline void StoreI32(u8 *bytes, u32 offset, i32 value)
{
    *reinterpret_cast<i32 *>(bytes + offset) = value;
}

inline u32 LoadU32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline void StoreU32(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

// TH10 0x463b2c (_ftol2): round half away from zero.
i32 FloatToI32(double value)
{
    return value >= 0.0
        ? static_cast<i32>(std::floor(value + 0.5))
        : static_cast<i32>(std::ceil(value - 0.5));
}

// Weight layout of the mode-8 cubic Hermite, exactly as the native x87
// sequences build them: h01 pairs with the endpoint, h00 with the start,
// h10 with handle1 and h11 with handle2 (the mode 0x11 velocity slot).
inline void HermiteWeights(float t, float &h00, float &h01, float &h10,
                           float &h11)
{
    h01 = (kHermiteThree - 2.0f * t) * t * t;
    h00 = (2.0f * t + 1.0f) * (t - 1.0f) * (t - 1.0f);
    h10 = (1.0f - t) * (1.0f - t) * t;
    h11 = (t - 1.0f) * t * t;
}

// Tail field offsets of one interpolation block; every ticker shares the
// timer machinery, only the head (component count / component type) and
// the offsets differ.
struct EasedTail {
    u32 prev;     // previous timer value (0xfff0bdc1 poison when stopped)
    u32 timer;    // integer frame count
    u32 accum;    // float accumulator advanced by the rate
    u32 rate;     // float* rate source (defaults to DAT_00476f78 = 1.0f)
    u32 flags;    // bit 0: rate pointer / accumulator initialized
    u32 duration; // armed frame count (0 disarms the block)
    u32 mode;     // 7 add delta, 8 Hermite, 0x11 velocity, else easing
};

// Shared timer advance. Returns true when the block completed this tick
// (timer reached the duration); false means the caller must run the mode
// dispatch. With a non-positive duration the native skips straight to the
// dispatch without touching prev/timer. The caller keeps the duration in a
// stack scratch slot that the native aliases over the argument area; the
// scratch value is what mode 8 and the default easing convert with FILD.
bool AdvanceEasedTimer(u8 *block, const EasedTail &tail, i32 &duration)
{
    duration = LoadI32(block, tail.duration);
    if (duration <= 0)
        return false;
    StoreU32(block, tail.prev, LoadU32(block, tail.timer));
    const float rate =
        **reinterpret_cast<float *const *>(block + tail.rate);
    if (rate > kRateUnityLow && rate < kRateUnityHigh) {
        // Inside the unity window the accumulator steps a whole frame and
        // the timer simply counts (no FILD/FTOL round trip).
        StoreF32(block, tail.accum, LoadF32(block, tail.accum) + 1.0f);
        StoreI32(block, tail.timer, LoadI32(block, tail.timer) + 1);
    } else {
        const float accumulated = LoadF32(block, tail.accum) + rate;
        StoreF32(block, tail.accum, accumulated);
        StoreI32(block, tail.timer, FloatToI32(accumulated));
    }
    return LoadI32(block, tail.timer) >= duration;
}

// Completion epilogue: one-time rearm of the rate pointer behind flag bit 0
// (the timer/accumulator zeroing it performs is immediately overwritten),
// then the observable state: timer = duration, prev = duration - 1,
// accumulator = (float)duration, duration = 0 (disarmed).
void CompleteEasedTimer(u8 *block, const EasedTail &tail, i32 duration)
{
    u32 flags = LoadU32(block, tail.flags);
    if ((flags & 1U) == 0U) {
        StoreI32(block, tail.timer, 0);
        StoreI32(block, tail.prev, kTimerPoison);
        StoreF32(block, tail.accum, 0.0f);
        *reinterpret_cast<const float **>(block + tail.rate) =
            &g_FrameTimeScale;
        StoreU32(block, tail.flags, flags | 1U);
    }
    StoreF32(block, tail.accum, static_cast<float>(duration)); // FILD
    StoreI32(block, tail.timer, duration);
    StoreI32(block, tail.prev, duration - 1);
    StoreI32(block, tail.duration, 0);
}

// TH10 0x441e50. Native EDI = three-dword source vector (components read
// as integers), ESI = destination, stack (ret 4) = float weight; each
// destination component is FTOL(component * weight).
i32 ScaleIntComponent(i32 component, float weight)
{
    return FloatToI32(static_cast<double>(component) *
                      static_cast<double>(weight));
}

// TH10 0x0040cc70. Native EDX = the +0x103c sub-record of an ECL script
// object; clears the pending-flag bit 0 of the seven per-tick dwords,
// zeroes the eight 0x210-byte command-slot blocks (each keeping a -1 at
// +0x204) and clears bit 0 of the two trailing state dwords.
void ResetEclSubObjectFlags(void *sub_memory)
{
    u8 *sub = static_cast<u8 *>(sub_memory);
    const u32 flag_offsets[7] = {
        0x12cU, 0x17cU, 0x1c8U, 0x204U, 0x240U, 0x27cU, 0x2b8U
    };
    for (u32 index = 0; index != 7U; ++index) {
        const u32 offset = flag_offsets[index];
        StoreU32(sub, offset, LoadU32(sub, offset) & 0xfffffffeU);
    }
    for (u32 block = 0; block != 8U; ++block) {
        u8 *slot = sub + 0x2c4U + block * 0x210U;
        for (u32 index = 0; index != 0x210U; ++index)
            slot[index] = 0;
        StoreU32(slot, 0x204U, 0xffffffffU);
    }
    StoreU32(sub, 0x142cU, LoadU32(sub, 0x142cU) & 0xfffffffeU);
    StoreU32(sub, 0x1440U, LoadU32(sub, 0x1440U) & 0xfffffffeU);
}

// Ribbon texture-coordinate scroll: the triggered accumulator takes the
// jitter step, and while it stays negative the whole 33-entry coordinate
// progression (one float per vertex at a 0x1c stride) advances by one so
// the texture wraps forward. Unordered (NaN) skips the sweep.
void ScrollRibbonCoordinate(u8 *buffer, u32 field_offset, float jitter,
                            float *trigger)
{
    *trigger += jitter;
    if (*trigger < 0.0f) {
        for (u32 index = 0; index != 33U; ++index)
            *reinterpret_cast<float *>(buffer + field_offset +
                                       index * 0x1cU) += 1.0f;
    }
}

} // namespace

// FUNCTION: TH10 0x00441600
void TickRgbColorInterpolationEaxStackAbi(void *block_memory, u32 out_rgb[3])
{
    u8 *const block = static_cast<u8 *>(block_memory);
    const EasedTail tail = { 0x30U, 0x34U, 0x38U, 0x3cU, 0x40U, 0x44U,
                             0x48U };
    i32 duration;
    if (!AdvanceEasedTimer(block, tail, duration)) {
        const i32 mode = LoadI32(block, 0x48);
        if (mode == 7) {
            for (u32 component = 0; component != 3U; ++component) {
                const i32 value = LoadI32(block, component * 4U) +
                                  LoadI32(block, 0xcU + component * 4U);
                StoreI32(block, component * 4U, value);
                out_rgb[component] = static_cast<u32>(value);
            }
        } else if (mode == 0x11) {
            // pos += vel, then vel += end (integer channels); the outputs
            // are the new positions.
            for (u32 component = 0; component != 3U; ++component) {
                const i32 velocity =
                    LoadI32(block, 0x24U + component * 4U);
                const i32 position = LoadI32(block, component * 4U) +
                                     velocity;
                StoreI32(block, component * 4U, position);
                StoreI32(block, 0x24U + component * 4U,
                         velocity + LoadI32(block, 0xcU + component * 4U));
                out_rgb[component] = static_cast<u32>(position);
            }
        } else if (mode == 8) {
            const float t = LoadF32(block, 0x38) /
                            static_cast<float>(duration);
            float h00, h01, h10, h11;
            HermiteWeights(t, h00, h01, h10, h11);
            for (u32 component = 0; component != 3U; ++component) {
                const u32 offset = component * 4U;
                out_rgb[component] =
                    static_cast<u32>(ScaleIntComponent(
                        LoadI32(block, 0xcU + offset), h01) +
                                     ScaleIntComponent(
                        LoadI32(block, offset), h00) +
                                     ScaleIntComponent(
                        LoadI32(block, 0x18U + offset), h10) +
                                     ScaleIntComponent(
                        LoadI32(block, 0x24U + offset), h11));
            }
        } else {
            const double factor = EasingCurveSelectorEaxStackAbi(
                mode, LoadF32(block, 0x38),
                static_cast<float>(duration));
            for (u32 component = 0; component != 3U; ++component) {
                const i32 start = LoadI32(block, component * 4U);
                const i32 delta = LoadI32(block, 0xcU + component * 4U) -
                                  start;
                out_rgb[component] = static_cast<u32>(
                    start + FloatToI32(factor * static_cast<double>(delta)));
            }
        }
        return;
    }
    CompleteEasedTimer(block, tail, duration);
    const u32 source = LoadI32(block, 0x48) == 7 ? 0U : 0xcU;
    for (u32 component = 0; component != 3U; ++component)
        out_rgb[component] = LoadU32(block, source + component * 4U);
}

// FUNCTION: TH10 0x00441950
i32 TickAlphaInterpolationEsiAbi(void *block_memory)
{
    u8 *const block = static_cast<u8 *>(block_memory);
    const EasedTail tail = { 0x10U, 0x14U, 0x18U, 0x1cU, 0x20U, 0x24U,
                             0x28U };
    i32 duration;
    if (!AdvanceEasedTimer(block, tail, duration)) {
        const i32 mode = LoadI32(block, 0x28);
        if (mode == 7) {
            const i32 value = LoadI32(block, 0U) + LoadI32(block, 4U);
            StoreI32(block, 0U, value);
            return value;
        }
        if (mode == 0x11) {
            const i32 velocity = LoadI32(block, 0xcU);
            const i32 value = LoadI32(block, 0U) + velocity;
            StoreI32(block, 0U, value);
            StoreI32(block, 0xcU, LoadI32(block, 4U) + velocity);
            return value;
        }
        if (mode == 8) {
            const float t = LoadF32(block, 0x18) /
                            static_cast<float>(duration);
            float h00, h01, h10, h11;
            HermiteWeights(t, h00, h01, h10, h11);
            // The native scales each channel with FIMUL (integer multiply
            // against the x87 register) and truncates the sum once.
            return FloatToI32(
                static_cast<double>(h01) *
                    static_cast<double>(LoadI32(block, 4U)) +
                static_cast<double>(h11) *
                    static_cast<double>(LoadI32(block, 0xcU)) +
                static_cast<double>(h10) *
                    static_cast<double>(LoadI32(block, 8U)) +
                static_cast<double>(h00) *
                    static_cast<double>(LoadI32(block, 0U)));
        }
        const double factor = EasingCurveSelectorEaxStackAbi(
            mode, LoadF32(block, 0x18), static_cast<float>(duration));
        const i32 start = LoadI32(block, 0U);
        const i32 delta = LoadI32(block, 4U) - start;
        // FIMUL by the delta, FIADD of the start, then the FTOL tail call.
        return FloatToI32(factor * static_cast<double>(delta) +
                          static_cast<double>(start));
    }
    CompleteEasedTimer(block, tail, duration);
    return LoadI32(block, LoadI32(block, 0x28) == 7 ? 0U : 4U);
}

// FUNCTION: TH10 0x00441ad0
void *TickScaleInterpolationEsiEdiAbi(void *block_memory, float out_xy[2])
{
    u8 *const block = static_cast<u8 *>(block_memory);
    const EasedTail tail = { 0x20U, 0x24U, 0x28U, 0x2cU, 0x30U, 0x34U,
                             0x38U };
    i32 duration;
    if (!AdvanceEasedTimer(block, tail, duration)) {
        const i32 mode = LoadI32(block, 0x38);
        if (mode == 7) {
            for (u32 component = 0; component != 2U; ++component) {
                const float value = LoadF32(block, component * 4U) +
                                    LoadF32(block, 8U + component * 4U);
                StoreF32(block, component * 4U, value);
                out_xy[component] = value;
            }
        } else if (mode == 0x11) {
            // pos += vel, vel += end in float arithmetic.
            for (u32 component = 0; component != 2U; ++component) {
                const float velocity =
                    LoadF32(block, 0x18U + component * 4U);
                const float position = LoadF32(block, component * 4U) +
                                       velocity;
                StoreF32(block, component * 4U, position);
                StoreF32(block, 0x18U + component * 4U,
                         velocity + LoadF32(block, 8U + component * 4U));
                out_xy[component] = position;
            }
        } else if (mode == 8) {
            const float t = LoadF32(block, 0x28) /
                            static_cast<float>(duration);
            float h00, h01, h10, h11;
            HermiteWeights(t, h00, h01, h10, h11);
            // Each weighted product is stored as a float before the
            // component sums are formed.
            for (u32 component = 0; component != 2U; ++component) {
                const u32 offset = component * 4U;
                out_xy[component] =
                    h00 * LoadF32(block, offset) +
                    h01 * LoadF32(block, 8U + offset) +
                    h10 * LoadF32(block, 0x10U + offset) +
                    h11 * LoadF32(block, 0x18U + offset);
            }
        } else {
            const double factor = EasingCurveSelectorEaxStackAbi(
                mode, LoadF32(block, 0x28),
                static_cast<float>(duration));
            for (u32 component = 0; component != 2U; ++component) {
                const float start = LoadF32(block, component * 4U);
                const float delta = LoadF32(block, 8U + component * 4U) -
                                    start;
                // The scaled delta is rounded through a float store before
                // the start offset is added back.
                const float scaled =
                    static_cast<float>(factor * static_cast<double>(delta));
                out_xy[component] = static_cast<float>(
                    static_cast<double>(start) +
                    static_cast<double>(scaled));
            }
        }
        return out_xy;
    }
    CompleteEasedTimer(block, tail, duration);
    const u32 source = LoadI32(block, 0x38) == 7 ? 0U : 8U;
    for (u32 component = 0; component != 2U; ++component)
        out_xy[component] = LoadF32(block, source + component * 4U);
    return out_xy;
}

// FUNCTION: TH10 0x0040d830
// Native userpurge: ESI = the freshly allocated 0x2518-byte record, one
// stack argument (ret 4) = the ctor argument forwarded to 0x450470 to
// resolve the ECL table id. Installs the 0x46d0c0 vtable and initializes
// the record; returns the record in EAX.
void *ConstructEclScriptObjectEsiStackAbi(void *record_memory, i32 ctor_arg)
{
    u8 *const record = static_cast<u8 *>(record_memory);

    StoreU32(record, 0U, 0x46d0c0U); // vtable
    StoreU32(record, 0x1010U, 0U);
    StoreU32(record, 0x1014U, 0U);
    ResetEclSubObjectFlags(record + 0x103cU);
    // Zero the whole tail (0x537 dwords from +0x103c to the 0x2518 end);
    // this runs after the sub-record reset and overwrites its -1 markers.
    for (u32 index = 0; index != 0x537U; ++index)
        StoreU32(record, 0x103cU + index * 4U, 0U);

    StoreU32(record, 8U, 0U);
    StoreU32(record, 0xcU, 0U);
    StoreU32(record, 0x101cU, reinterpret_cast<u32>(record));
    StoreU32(record, 0x1028U, LoadU32(record, 0x1028U) & 0xfffffffeU);
    StoreU32(record, 0x1020U, 0U);
    StoreU32(record, 4U, reinterpret_cast<u32>(record + 8U));
    StoreU32(record, 0x1018U, static_cast<u32>(-1));
    StoreU32(record, 0x1030U, reinterpret_cast<u32>(record + 8U));
    StoreU32(record, 0x1034U, 0U);
    StoreU32(record, 0x1038U, 0U);
    StoreU32(record, 0x2514U, reinterpret_cast<u32>(record));
    StoreU32(record, 0x11bcU, 0U);
    StoreU32(record, 0x1208U, 0U);
    StoreU32(record, 0x1244U, 0U);
    StoreU32(record, 0x1280U, 0U);
    StoreU32(record, 0x12bcU, 0U);
    StoreU32(record, 0x12f8U, 0U);
    for (u32 index = 0; index != 11U; ++index) {
        StoreU32(record, 0x10c0U + index * 4U, 0U);
        StoreU32(record, 0x1094U + index * 4U, 0U);
        StoreU32(record, 0x1068U + index * 4U, 0U);
    }
    for (u32 index = 0; index != 4U; ++index)
        StoreF32(record, 0x10ecU + index * 4U, 24.0f); // 0x41c00000
    StoreU32(record, 0x248cU, static_cast<u32>(-1));
    // Embedded list node at +0x116c: next points at the record itself,
    // prev is null.
    StoreU32(record, 0x116cU, reinterpret_cast<u32>(record));
    StoreU32(record, 0x1170U, 0U);
    StoreU32(record, 0x1174U, 0U);
    for (u32 index = 0; index != 15U; ++index)
        StoreU32(record, 0x2408U + index * 4U, 0U);
    StoreF32(record, 0x243cU, 32.0f); // 0x42000000
    StoreF32(record, 0x2440U, 32.0f);

    // Three animation tails {prev, timer, accumulator, rate, flags} at
    // +0x1158, +0x2458 and +0x246c. Each takes the one-time init behind
    // flag bit 0 (poison prev, zero timer/accumulator, rate = 1.0f) and
    // then the unconditional stopped-state arm with prev = -1.
    const u32 tail_bases[3] = { 0x1158U, 0x2458U, 0x246cU };
    for (u32 index = 0; index != 3U; ++index) {
        const u32 base = tail_bases[index];
        const u32 flags_offset = base + 0x10U;
        u32 flags = LoadU32(record, flags_offset);
        if ((flags & 1U) == 0U) {
            flags |= 1U;
            StoreU32(record, base + 4U, 0U); // timer
            StoreU32(record, base, static_cast<u32>(kTimerPoison));
            StoreU32(record, base + 8U, 0U); // accumulator
            *reinterpret_cast<const float **>(record + base + 0xcU) =
                &g_FrameTimeScale; // &flt_476F78
            StoreU32(record, flags_offset, flags);
        }
        StoreU32(record, base + 4U, 0U);
        StoreU32(record, base + 8U, 0U);
        StoreU32(record, base, static_cast<u32>(-1));
    }

    // Presentation slot from the HUD conditional state: the native reads
    // DAT_00477704 once and takes state+0x54 (the name-registry pointer)
    // from it, then resolves the ctor argument into the +8 node (id at +4,
    // null at +0).
    ConditionalState *const conditional_state =
        static_cast<ConditionalState *>(g_AsciiHudConditionalState);
    StoreU32(record, 0x102cU,
             reinterpret_cast<u32>(conditional_state->name_registry_0054));
    const i32 table_id = ResolveScriptTableIndexEaxAbi(ctor_arg);
    StoreU32(record, 0xcU, static_cast<u32>(table_id));
    StoreU32(record, 8U, 0U);
    StoreU32(record, 0x23fcU, 0U);
    StoreU32(record, 0x2400U, 0U);
    StoreU32(record, 0x2404U, 0U);

    // Eight {-1, -1, 0} triples from +0x2494 at a 0x10 stride.
    for (u32 index = 0; index != 8U; ++index) {
        const u32 base = 0x2494U + index * 0x10U;
        StoreU32(record, base, static_cast<u32>(-1));
        StoreU32(record, base + 4U, static_cast<u32>(-1));
        StoreU32(record, base + 8U, 0U);
    }
    return record;
}

// FUNCTION: TH10 0x00445620
void RibbonFrameUpdateCallback(void *entity_memory)
{
    u8 *const entity = static_cast<u8 *>(entity_memory);
    u8 *const buffer = *reinterpret_cast<u8 *const *>(entity + 0x358);
    // The native dereferences the buffer without a null check.
    const float jitter = LoadF32(buffer, 0x4a4);

    float center[3];
    for (u32 component = 0; component != 3U; ++component)
        center[component] =
            LoadF32(entity, 0x334U + component * 4U) +
            LoadF32(entity, 0x340U + component * 4U);
    StoreF32(buffer, 0U, center[0]);
    StoreF32(buffer, 4U, center[1]);
    StoreF32(buffer, 8U, center[2]);

    ScrollRibbonCoordinate(buffer, 0x14U, jitter,
                           reinterpret_cast<float *>(buffer + 0x14U));
    ScrollRibbonCoordinate(buffer, 0x18U, jitter,
                           reinterpret_cast<float *>(buffer + 0x18U));

    const u32 color = LoadU32(entity, 0x2fcU);
    StoreU32(buffer, 0x10U, color);
    // Native quirk: the packed color dword is reinterpreted as the polar
    // angle for every vertex, while the accumulated theta is the radius.
    u32 angle_bits_raw = color;
    const float angle_bits =
        *reinterpret_cast<const float *>(&angle_bits_raw);
    // The rotation counter starts at -pi and advances 2*pi/31 per vertex
    // but is never read (native quirk, preserved).
    float rotation = -3.1415927f;

    u8 *vertex = buffer + 0x1cU;
    for (u32 index = 0; index != 31U; ++index) {
        ScrollRibbonCoordinate(buffer, 0x14U, jitter,
                               reinterpret_cast<float *>(vertex + 0x14U));
        ScrollRibbonCoordinate(buffer, 0x18U, jitter,
                               reinterpret_cast<float *>(vertex + 0x18U));
        StoreU32(vertex, 0x10U, color);
        vertex[0x13U] = 0;

        float *const theta_slot =
            reinterpret_cast<float *>(buffer + 0x3a0U + index * 4U);
        const float radius =
            *theta_slot +
            *reinterpret_cast<const float *>(buffer + 0x424U + index * 4U);
        *theta_slot = radius;
        StoreF32(vertex, 0U,
                 std::cos(static_cast<double>(angle_bits)) *
                     static_cast<double>(radius));
        StoreF32(vertex, 4U,
                 std::sin(static_cast<double>(angle_bits)) *
                     static_cast<double>(radius));
        for (u32 component = 0; component != 3U; ++component)
            StoreF32(vertex, component * 4U,
                     LoadF32(vertex, component * 4U) + center[component]);
        rotation += kAngleStep;
        vertex += 0x1cU;
    }
    // Wrap-around: slot 32 (past the 32 stored vertices) duplicates the
    // seven dwords of vertex 1.
    for (u32 index = 0; index != 7U; ++index)
        StoreU32(vertex, index * 4U, LoadU32(buffer + 0x1cU, index * 4U));
}

} // namespace th10
