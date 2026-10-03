#include "EclEasedTransforms.hpp"

#include "ConditionalStateObject.hpp"
#include "EclScriptLibrary.hpp"
#include "EclScriptObject.hpp"
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
    EclScriptWork &work = *static_cast<EclScriptWork *>(sub_memory);
    // The seven per-tick flag dwords: the frame-tail timer first, then the
    // six eased-anim blocks (vec3 pair, then the four vec2 blocks in the
    // A0/A1/A2/A3 order).
    work.frame_tail_011c.flags &= 0xfffffffeU; // sub+0x12c
    work.vec3_a_013c.flags &= 0xfffffffeU;     // sub+0x17c
    work.vec3_b_0188.flags &= 0xfffffffeU;     // sub+0x1c8
    work.vec2_a0_01d4.flags &= 0xfffffffeU;    // sub+0x204
    work.vec2_a1_0210.flags &= 0xfffffffeU;    // sub+0x240
    work.vec2_a2_024c.flags &= 0xfffffffeU;    // sub+0x27c
    work.vec2_a3_0288.flags &= 0xfffffffeU;    // sub+0x2b8
    for (u32 block = 0; block != 8U; ++block) {
        u8 *slot = work.command_slots_02c4[block];
        for (u32 index = 0; index != 0x210U; ++index)
            slot[index] = 0;
        StoreU32(slot, 0x204U, 0xffffffffU); // per-slot pending marker
    }
    work.shift_timer_a_141c.flags &= 0xfffffffeU; // sub+0x142c
    work.shift_timer_b_1430.flags &= 0xfffffffeU; // sub+0x1440
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
    EclScriptObject &obj = *static_cast<EclScriptObject *>(record_memory);
    EclScriptWork &work = obj.work;

    obj.vtable_0000 = reinterpret_cast<void *>(0x46d0c0U); // vtable
    obj.field_1010 = 0;
    obj.field_1014 = 0;
    ResetEclSubObjectFlags(&work);
    // Zero the whole tail (0x537 dwords from +0x103c to the 0x2518 end,
    // i.e. the working sub-record plus the +0x2514 self slot); this runs
    // after the sub-record reset and overwrites its -1 markers.
    u32 *const tail_zero = reinterpret_cast<u32 *>(&work);
    for (u32 index = 0; index != 0x537U; ++index)
        tail_zero[index] = 0;

    obj.bind_node_0008 = 0;
    obj.script_table_id_000c = 0;
    obj.self_101c = &obj;
    obj.flags_1028 &= 0xfffffffeU;
    obj.field_1020 = 0;
    obj.bind_node_self_0004 = &obj.bind_node_0008;
    obj.sentinel_1018 = -1;
    obj.bind_node_ptr_1030 = &obj.bind_node_0008;
    obj.alloc_list_1034 = 0;
    obj.field_1038 = 0;
    obj.self_2514 = &obj;

    // Disarm the six eased-anim blocks (each block's arm dword is its
    // duration: work+0x180/0x1cc/0x208/0x244/0x280/0x2bc).
    work.vec3_a_013c.duration = 0;
    work.vec3_b_0188.duration = 0;
    work.vec2_a0_01d4.duration = 0;
    work.vec2_a1_0210.duration = 0;
    work.vec2_a2_024c.duration = 0;
    work.vec2_a3_0288.duration = 0;

    // The native zeroes three 11-dword runs covering the base block
    // (sub+0x2c), the anchor1 block (sub+0x58) and the anchor2 block
    // (sub+0x84); each run ends with the polar-mode flags byte plus its
    // 3-byte gap dword.
    for (u32 index = 0; index != 3U; ++index) {
        work.base_pos_002c[index] = 0.0f;
        work.base_velocity_0038[index] = 0.0f;
    }
    u32 *const base_gap = reinterpret_cast<u32 *>(work.unknown_0044);
    for (u32 index = 0; index != 5U; ++index)
        base_gap[index] = 0;
    for (u32 index = 0; index != 3U; ++index) {
        work.anchor1_pos_0058[index] = 0.0f;
        work.anchor1_delta_0064[index] = 0.0f;
    }
    work.anchor1_radius_0070 = 0;
    work.anchor1_angle_0074 = 0.0f;
    work.anchor1_radius2_0078 = 0;
    work.anchor1_angle2_007c = 0.0f;
    *reinterpret_cast<u32 *>(&work.anchor1_flags_0080) = 0;
    for (u32 index = 0; index != 3U; ++index) {
        work.anchor2_pos_0084[index] = 0.0f;
        work.anchor2_delta_0090[index] = 0.0f;
    }
    work.anchor2_radius_009c = 0;
    work.anchor2_angle_00a0 = 0.0f;
    work.anchor2_radius2_00a4 = 0;
    work.anchor2_angle2_00a8 = 0.0f;
    *reinterpret_cast<u32 *>(&work.anchor2_flags_00ac) = 0;
    for (u32 index = 0; index != 4U; ++index)
        work.hitbox_params_00b0[index] = 24.0f; // 0x41c00000
    work.published_id_index_1450 = -1;
    // Embedded list node (work+0x130 = record+0x116c): list_self_0130 is
    // the back-pointer to the record itself; list_next_0134 and
    // list_prev_0138 are null.
    work.list_self_0130 = &obj;
    work.list_next_0134 = 0;
    work.list_prev_0138 = 0;
    work.kind_13cc = 0;
    u32 *const kind_gap = reinterpret_cast<u32 *>(work.unknown_13d0);
    for (u32 index = 0; index != 12U; ++index)
        kind_gap[index] = 0;
    work.sprite_size_1400[0] = 32.0f; // 0x42000000
    work.sprite_size_1400[1] = 32.0f;

    // Three timer tails: frame_tail_011c (record+0x1158), shift_timer_a
    // (+0x2458) and shift_timer_b (+0x246c). Each takes the one-time init
    // behind flag bit 0 (poison prev, zero timer/accumulator, rate =
    // 1.0f) and then the unconditional stopped-state arm with prev = -1.
    TimerNode *const tails[3] = {
        &work.frame_tail_011c,
        &work.shift_timer_a_141c,
        &work.shift_timer_b_1430,
    };
    for (u32 index = 0; index != 3U; ++index) {
        TimerNode &tail_timer = *tails[index];
        if ((tail_timer.flags & 1U) == 0U) {
            tail_timer.flags |= 1U;
            tail_timer.count = 0;
            tail_timer.prev = kTimerPoison;
            tail_timer.rate = &g_FrameTimeScale; // &flt_476F78
        }
        tail_timer.count = 0;
        tail_timer.accum = 0;
        tail_timer.prev = -1;
    }

    // Presentation slot from the HUD conditional state: the native reads
    // DAT_00477704 once and takes state+0x54 (the name-registry pointer)
    // from it, then resolves the ctor argument into the +8 node (id at +4,
    // null at +0).
    ConditionalState *const conditional_state =
        static_cast<ConditionalState *>(g_AsciiHudConditionalState);
    obj.name_registry_102c = conditional_state->name_registry_0054;
    const i32 table_id = ResolveScriptTableIndexEaxAbi(ctor_arg);
    obj.script_table_id_000c = table_id;
    obj.bind_node_0008 = 0;
    work.hp_13c0 = 0;
    work.unknown_13c4 = 0;
    work.unknown_13c8 = 0;

    // Eight {-1, -1, 0} triples from +0x2494 at a 0x10 stride (the request
    // slots; the trailing 4 bytes of each 16-byte slot stay untouched).
    for (u32 index = 0; index != 8U; ++index) {
        u8 *const slot = work.request_slots_1458[index];
        StoreU32(slot, 0U, static_cast<u32>(-1));
        StoreU32(slot, 4U, static_cast<u32>(-1));
        StoreU32(slot, 8U, 0U);
    }
    return &obj;
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
