// Stage-trigger object feature helpers (0x4074b0..0x407ef0): the bodies
// dispatched by UpdateSceneTriggerObjectStackAbi on the accumulated +0x43c
// flag bits, plus the two EBX-ABI region-wrap twins (0x407da0/0x407e40) and
// the shared angle/region leaves 0x44bc10 / 0x408660 / 0x406160 / 0x4061d0.
#include "SceneTriggerFeatures.hpp"

#include <cmath>

#include "GameManagerState.hpp"
#include "PlayerRecord.hpp"
#include "PlayerTimerHelpers.hpp"
#include "SceneTriggerObject.hpp"

namespace th10 {

namespace {

extern void *g_ScreenTargetBlock; // TH10 DAT_00477834 (+0x3c0 anchor pos)
extern void *g_SoundGateContext;  // TH10 DAT_00492590
extern float g_FrameTimeScale;    // TH10 DAT_00476f78 (shared rate float)

// Constant pool -----------------------------------------------------------

const float kPi = 3.1415927f;            // TH10 flt_470b18 / -flt_470b10
const float kTwoPi = 6.2831855f;         // TH10 flt_470b14
const float kHalf = 0.5f;                // TH10 flt_470b0c
const float kPiHalfImmediate = 1.5707964f; // TH10 0x3fc90fdb immediate (pi/2)
const float kTinyAngle = 9.9999997e-05f; // TH10 flt_470c58
const float kLaunchBase = 5.0f;          // TH10 flt_470c68
const float kLaunchDecay = 0.3125f;      // TH10 flt_470c6c
const float kFieldLeft = -192.0f;        // TH10 flt_470b40
const float kFieldRight = 192.0f;        // TH10 flt_470b3c
const float kFieldBottom = 448.0f;       // TH10 flt_470b38
const float kFieldWidth = 384.0f;        // TH10 flt_470b54 / -flt_470b58
const float kSpeedSentinel = -990.0f;    // TH10 flt_470b50

inline u32 LoadU32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline void StoreU32(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

inline i32 LoadI32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline float LoadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void StoreFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

inline void PolarVelocity(u8 *object, float speed)
{
    SetPolarVelocityThisAbi(reinterpret_cast<float *>(object + 0x3c0U),
                            LoadFloat(object, 0x3e4U), speed);
}

// Shared aim-at-screen-target angle (native inline at 0x407a9f / 0x407f0e):
// atan2(dy, dx) against the DAT_00477834 anchor; the exact (0, 0) delta
// answers the 0x3fc90fdb pi/2 immediate instead of calling FPATAN. NaN
// deltas fail both zero comparisons and reach the atan2 path.
float AimAngleAtScreenTarget(const u8 *object)
{
    const PlayerRecord &player =
        *reinterpret_cast<const PlayerRecord *>(g_ScreenTargetBlock);
    const float dx = player.position_x - LoadFloat(object, 0x3b4U);
    const float dy = player.position_y - LoadFloat(object, 0x3b8U);
    if (dy == 0.0f && dx == 0.0f)
        return kPiHalfImmediate;
    return static_cast<float>(
        atan2(static_cast<double>(dy), static_cast<double>(dx)));
}

// Sound reserve shared by the fire/bounce bodies: native ECX = 0x492590,
// EDI = the record's +0x458 state word, stack = 0.
void ReserveTriggerSound(const u8 *object)
{
    if (LoadI32(object, 0x458U) >= 0)
        ReserveContextChannel(g_SoundGateContext,
                              LoadU32(object, 0x458U), 0U);
}

// Fire step shared by the three slow-to-stop variants: sound reserve, the
// +0x6e0 fire counter with the +0x6dc budget (the bit clear only stops
// future dispatch — the fire itself always completes), the per-variant
// angle update, the speed reset to +0x6c4 and the +0x6b0 timer re-arm.
void SlowStopFire(u8 *object, u32 clear_and_mask, const float *angle_value,
                  bool angle_accumulate)
{
    ReserveTriggerSound(object);
    const u32 fires = LoadU32(object, 0x6e0U) + 1U;
    StoreU32(object, 0x6e0U, fires);
    if (fires >= LoadU32(object, 0x6dcU))
        StoreU32(object, 0x43cU, LoadU32(object, 0x43cU) & clear_and_mask);
    if (angle_value != 0) {
        const float current = angle_accumulate
            ? LoadFloat(object, 0x3e4U) : 0.0f;
        StoreFloat(object, 0x3e4U, current + *angle_value);
    }
    StoreFloat(object, 0x3d8U, LoadFloat(object, 0x6c4U));
    TickPlayerTimerEaxStackAbi(object + 0x6b0U, 0);
}

// Interpolation step: speed decays linearly to zero over the +0x6d8 budget
// using the float accumulator and the *integer* fidiv divisor.
float SlowStopDecaySpeed(const u8 *object)
{
    const float speed = LoadFloat(object, 0x3d8U);
    return speed
        - speed * LoadFloat(object, 0x6b8U)
              / static_cast<float>(LoadI32(object, 0x6d8U));
}

// Common epilogue: polar velocity from the (possibly updated) angle and
// speed, then the +0x6b0 forward tick.
void SlowStopTail(u8 *object, float speed)
{
    PolarVelocity(object, speed);
    TickTimerForwardEsiAbi(object + 0x6b0U);
}

} // namespace

// TH10 0x44bc10.
float WrapAngleSumStackAbi(float a, float b)
{
    float value = a + b;
    i32 count = 0;
    while (value > kPi) { // NaN exits here (fcom parity chain)
        value -= kTwoPi;
        const i32 previous = count++;
        if (previous > 0x20)
            break;
    }
    count = 0;
    while (value < -kPi) {
        value += kTwoPi;
        const i32 previous = count++;
        if (previous > 0x20)
            break;
    }
    return value;
}

// TH10 0x408660.
float AngleDifferenceWrappedStackAbi(float a, float b)
{
    const float delta = a - b;
    if (delta > kPi)
        return a - (b + kTwoPi);
    const float inverse = b - a;
    if (inverse > kPi)
        return a - (b - kTwoPi);
    return delta; // NaN and the in-range case return the raw a - b
}

namespace {

// Native body of 0x406160/0x4061d0: every comparison continues on NaN and
// the shared fall-through makes the NaN path answer 0 (still inside).
i32 RegionExited(const float *position, float half_x, float half_y,
                 float y_min)
{
    const float x = position[0];
    const float y = position[1];
    const float ax = half_x * kHalf;
    const float ay = half_y * kHalf;
    float value = ax + x;
    if (value == value && value <= kFieldLeft)
        return 1;
    value = x - ax;
    if (value == value && value >= kFieldRight)
        return 1;
    value = ay + y;
    if (value == value && value <= y_min)
        return 1;
    value = y - ay;
    if (value == value && value >= kFieldBottom)
        return 1;
    return 0;
}

} // namespace

// TH10 0x406160.
i32 CheckRegionExitedPlayfield64EcxStackAbi(const float *position,
                                            float half_x, float half_y)
{
    return RegionExited(position, half_x, half_y, -64.0f);
}

// TH10 0x4061d0.
i32 CheckRegionExitedPlayfield0EcxStackAbi(const float *position,
                                           float half_x, float half_y)
{
    return RegionExited(position, half_x, half_y, 0.0f);
}

// TH10 0x4074b0 (flag bit 1).
void UpdateSceneTriggerLaunchSlowdownEsiAbi(void *object)
{
    u8 *const obj = static_cast<u8 *>(object);
    if (LoadI32(obj, 0x618U) > 0x10) {
        // Budget spent: native XOR clears the dispatching bit.
        StoreU32(obj, 0x43cU, LoadU32(obj, 0x43cU) ^ 1U);
    } else {
        const float speed = kLaunchBase
            - LoadFloat(obj, 0x61cU) * kLaunchDecay
            + LoadFloat(obj, 0x3d8U);
        PolarVelocity(obj, speed);
    }
    TickTimerForwardEsiAbi(obj + 0x614U);
}

// TH10 0x407560 (flag bit 0x10).
void UpdateSceneTriggerAccelerationEsiAbi(void *object)
{
    u8 *const obj = static_cast<u8 *>(object);
    if (LoadI32(obj, 0x64cU) >= LoadU32(obj, 0x670U)) {
        StoreU32(obj, 0x43cU, LoadU32(obj, 0x43cU) & 0xffffffefU);
    } else {
        StoreFloat(obj, 0x3d8U,
                   LoadFloat(obj, 0x3d8U)
                       + g_FrameTimeScale * LoadFloat(obj, 0x65cU));
        StoreFloat(obj, 0x3c0U,
                   LoadFloat(obj, 0x3c0U)
                       + g_FrameTimeScale * LoadFloat(obj, 0x664U));
        StoreFloat(obj, 0x3c4U,
                   LoadFloat(obj, 0x3c4U)
                       + g_FrameTimeScale * LoadFloat(obj, 0x668U));
        StoreFloat(obj, 0x3c8U,
                   LoadFloat(obj, 0x3c8U)
                       + g_FrameTimeScale * LoadFloat(obj, 0x66cU));
        // Angle recompute when either screen axis outruns the 1e-4
        // threshold; a NaN x reaches the atan2, a NaN y (small x) skips it.
        const float ax = LoadFloat(obj, 0x3c0U) < 0.0f
            ? -LoadFloat(obj, 0x3c0U) : LoadFloat(obj, 0x3c0U);
        const float ay = LoadFloat(obj, 0x3c4U) < 0.0f
            ? -LoadFloat(obj, 0x3c4U) : LoadFloat(obj, 0x3c4U);
        bool recompute;
        if (ax > kTinyAngle || ax != ax)
            recompute = true;
        else
            recompute = ay > kTinyAngle;
        if (recompute)
            StoreFloat(obj, 0x3e4U,
                       static_cast<float>(atan2(
                           static_cast<double>(LoadFloat(obj, 0x3c4U)),
                           static_cast<double>(LoadFloat(obj, 0x3c0U)))));
    }
    TickTimerForwardEsiAbi(obj + 0x648U);
}

// TH10 0x4076a0 (flag bit 0x20).
void UpdateSceneTriggerAngleTurnEsiAbi(void *object)
{
    u8 *const obj = static_cast<u8 *>(object);
    if (LoadI32(obj, 0x680U) >= LoadU32(obj, 0x6a4U)) {
        StoreU32(obj, 0x43cU, LoadU32(obj, 0x43cU) & 0xffffffdfU);
    } else {
        StoreFloat(obj, 0x3e4U,
                   WrapAngleSumStackAbi(LoadFloat(obj, 0x3e4U),
                                        g_FrameTimeScale
                                            * LoadFloat(obj, 0x694U)));
        const float speed = LoadFloat(obj, 0x3d8U)
            + g_FrameTimeScale * LoadFloat(obj, 0x690U);
        StoreFloat(obj, 0x3d8U, speed);
        PolarVelocity(obj, speed);
    }
    TickTimerForwardEsiAbi(obj + 0x67cU);
}

// TH10 0x407780 (flag bit 0x40).
void UpdateSceneTriggerSlowStopRelaunchEsiAbi(void *object)
{
    u8 *const obj = static_cast<u8 *>(object);
    if (LoadI32(obj, 0x6b4U) >= LoadI32(obj, 0x6d8U)) {
        const float bias = LoadFloat(obj, 0x6c8U);
        SlowStopFire(obj, 0xffffffbfU, &bias, true);
        SlowStopTail(obj, LoadFloat(obj, 0x6c4U));
    } else {
        SlowStopTail(obj, SlowStopDecaySpeed(obj));
    }
}

// TH10 0x4078e0 (flag bit 0x100).
void UpdateSceneTriggerSlowStopFixedAngleEsiAbi(void *object)
{
    u8 *const obj = static_cast<u8 *>(object);
    if (LoadI32(obj, 0x6b4U) >= LoadI32(obj, 0x6d8U)) {
        const float bias = LoadFloat(obj, 0x6c8U);
        SlowStopFire(obj, 0xfffffeffU, &bias, false);
        SlowStopTail(obj, LoadFloat(obj, 0x6c4U));
    } else {
        SlowStopTail(obj, SlowStopDecaySpeed(obj));
    }
}

// TH10 0x407a30 (flag bit 0x80).
void UpdateSceneTriggerSlowStopAimPlayerEsiAbi(void *object)
{
    u8 *const obj = static_cast<u8 *>(object);
    if (LoadI32(obj, 0x6b4U) >= LoadI32(obj, 0x6d8U)) {
        SlowStopFire(obj, 0xffffff7fU, 0, false);
        StoreFloat(obj, 0x3e4U,
                   WrapAngleSumStackAbi(AimAngleAtScreenTarget(obj),
                                        LoadFloat(obj, 0x6c8U)));
        SlowStopTail(obj, LoadFloat(obj, 0x6c4U));
    } else {
        SlowStopTail(obj, SlowStopDecaySpeed(obj));
    }
}

// TH10 0x407be0 (flag bits 0x400 | 0x800 | 0x8000000).
void UpdateSceneTriggerWallBounceEsiAbi(void *object)
{
    u8 *const obj = static_cast<u8 *>(object);
    const float x = LoadFloat(obj, 0x3b4U);
    const float y = LoadFloat(obj, 0x3b8U);
    // The entry gate's NaN directions: NaN x continues into the y checks
    // (which then answer no-bounce), so the plain comparisons match.
    if (!(x <= kFieldLeft || x >= kFieldRight || y <= 0.0f
          || y >= kFieldBottom))
        return;

    ReserveTriggerSound(obj);
    bool bounced = false;
    if (x < kFieldLeft || x >= kFieldRight) {
        // Angle mirror across the wall normal: -angle - pi wrapped.
        const float flipped = -LoadFloat(obj, 0x3e4U) - kPi;
        StoreFloat(obj, 0x3e4U, WrapAngleSumStackAbi(flipped, 0.0f));
        bounced = true;
        // Position mirror: -384 - x below the left wall, 384 - x past the
        // right one (the >= boundary and NaN take the 384 branch).
        const float mirror = x < kFieldLeft ? -kFieldWidth : kFieldWidth;
        StoreFloat(obj, 0x3b4U, mirror - x);
    }
    if ((LoadU32(obj, 0x43cU) & 0x8000000U) == 0U) {
        // Ceiling (y < 0) reflects unconditionally; the floor (y >= 448)
        // reflection additionally requires flag bit 0x400. The exact-zero
        // y and NaN re-checks fall to the 896 - y branch, matching the
        // fcomp parity chain.
        bool reflect_y = false;
        if (y < 0.0f)
            reflect_y = true;
        else if (y >= kFieldBottom && (LoadU32(obj, 0x43cU) & 0x400U) != 0U)
            reflect_y = true;
        if (reflect_y) {
            StoreFloat(obj, 0x3e4U, -LoadFloat(obj, 0x3e4U));
            if (y < 0.0f)
                StoreFloat(obj, 0x3b8U, -y);
            else
                StoreFloat(obj, 0x3b8U, 2.0f * kFieldBottom - y);
            bounced = true;
        }
    }
    // Speed override: the +0x6f8 dword (the raw opcode bits 0x400/0x800/
    // 0x8000000 from the queue handler) is consumed as a float; those bits
    // are positive denormals, so the effect is a near-zero speed. The
    // sentinel -990.0 itself and lower values are skipped.
    const float override_speed = LoadFloat(obj, 0x6f8U);
    if (override_speed > kSpeedSentinel || override_speed != override_speed)
        StoreFloat(obj, 0x3d8U, override_speed);
    PolarVelocity(obj, LoadFloat(obj, 0x3d8U));
    if (bounced)
        StoreU32(obj, 0x70cU, LoadU32(obj, 0x70cU) + 1U);
    if (LoadI32(obj, 0x70cU) >= LoadU32(obj, 0x710U))
        StoreU32(obj, 0x43cU, LoadU32(obj, 0x43cU) & 0xf7fff3ffU);
}

// TH10 0x407ef0 (flag bit 0x4000000).
void UpdateSceneTriggerHomingTurnEsiAbi(void *object)
{
    u8 *const obj = static_cast<u8 *>(object);
    if (LoadI32(obj, 0x7b8U) >= LoadU32(obj, 0x7dcU)) {
        StoreU32(obj, 0x43cU, LoadU32(obj, 0x43cU) & 0xfbffffffU);
    } else {
        const float target = AimAngleAtScreenTarget(obj);
        const float wrapped = WrapAngleSumStackAbi(
            LoadFloat(obj, 0x7ccU), target);
        float delta = AngleDifferenceWrappedStackAbi(
            wrapped, LoadFloat(obj, 0x3e4U));
        delta *= LoadFloat(obj, 0x7c8U);
        delta *= g_FrameTimeScale;
        const float angle = WrapAngleSumStackAbi(
            LoadFloat(obj, 0x3e4U), delta);
        StoreFloat(obj, 0x3e4U, angle);
        PolarVelocity(obj, LoadFloat(obj, 0x3d8U));
    }
    TickTimerForwardEsiAbi(obj + 0x7b4U);
}

namespace {

// Shared region-wrap body (0x407da0 / 0x407e40): teleport the wrapped axis
// to the other side of the play field by the field span plus the linked
// region half extent, shift the paired timer by -1.0, reserve the sound and
// drop the dispatch bit once the paired counter reached zero. Both twins
// exit-test through the 0x4061d0 variant (y_min = 0).
void WrapRegionAxis(u8 *object, u32 position_offset, u32 half_offset,
                    float low_bound, float high_bound, float span,
                    u32 timer_offset, u32 counter_offset, u32 flag_bit)
{
    const u8 *const region =
        reinterpret_cast<const u8 *>(LoadU32(object, 0x39cU));
    const float half = LoadFloat(region, half_offset);
    if (CheckRegionExitedPlayfield0EcxStackAbi(
            reinterpret_cast<const float *>(object + 0x3b4U),
            LoadFloat(region, 0x34U), LoadFloat(region, 0x30U)) == 0)
        return;

    float position = LoadFloat(object, position_offset);
    bool moved = false;
    if (position < low_bound) {
        // Fully left / above: shift forward by span + half.
        position += half + span;
        moved = true;
    } else if (position > high_bound || position != position) {
        // Fully right / below (and NaN): shift backward by span + half.
        position -= half + span;
        moved = true;
    }
    if (moved) {
        StoreFloat(object, position_offset, position);
        ShiftTimerByEsiStackAbi(object + timer_offset, -1.0f);
        ReserveTriggerSound(object);
    }
    if (LoadI32(object, counter_offset) <= 0)
        StoreU32(object, 0x43cU, LoadU32(object, 0x43cU) ^ flag_bit);
}

} // namespace

// TH10 0x407da0 (flag bit 0x100000).
void WrapSceneTriggerRegionXEbxAbi(void *object)
{
    WrapRegionAxis(static_cast<u8 *>(object), 0x3b4U, 0x34U,
                   kFieldLeft, kFieldRight, kFieldWidth,
                   0x74cU, 0x750U, 0x100000U);
}

// TH10 0x407e40 (flag bit 0x200000).
void WrapSceneTriggerRegionYEbxAbi(void *object)
{
    WrapRegionAxis(static_cast<u8 *>(object), 0x3b8U, 0x30U,
                   0.0f, kFieldBottom, kFieldBottom,
                   0x780U, 0x784U, 0x200000U);
}

} // namespace th10
