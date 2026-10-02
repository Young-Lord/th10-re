// Player shot homing callbacks (TH10 0x428ad0/0x428b10) plus the wrapped
// angle-delta helper 0x00428ce0. Analysis was done offline with objdump;
// see docs/evidence/player-shot-homing.md.
//
// The 0x34-byte shot descriptors bind these routines at load time
// (0x004265b0 chain binder): field +0x24 indexes table 0x47476c (entry 1
// = the 0x428ad0 target acquire, run once from the spawner with the frame
// number) and field +0x28 indexes table 0x474778 (entry 1 = the 0x428b10
// per-frame movement; entry 2 = the option-locked 0x428c20 variant, still
// a boundary). The projectile update 0x00428280 invokes the +0x28
// callback with ECX = player and EDX = record.
#include <cmath>

#include "PlayerMotionHelpers.hpp"
#include "PlayerRecord.hpp"
#include "PlayerShotHoming.hpp"

namespace th10 {

namespace {

inline u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

inline float LoadF32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const float *>(
        static_cast<const u8 *>(base) + offset);
}

const float k_pi = 3.14159274f;        // 0x470b18
const float k_two_pi = 6.28318548f;    // 0x470b14
const float k_target_limit_x = 224.0f; // 0x470b4c
const float k_speed_ramp = 0.1f;       // 0x470c18
const float k_speed_ramp_fast = 0.2f;  // 0x470c38
const float k_speed_cap = 16.0f;       // 0x470b48
const float k_slow_speed = 4.0f;       // 0x470c40
const float k_turn_rate = 0.2f;        // 0x470c38 again
const float k_straight_angle = 0.2617994f; // 0x470c3c (pi/12, bits 0x3E860A92)
const float k_quarter_turn = 0.7853982f;  // 0x470c48 (pi/4)
const u32 k_homing_age_gate = 0x78U;   // 120 frames

const u32 k_target_flags_offset = 0x2480U;
const u32 k_target_x_offset = 0x1068U;
const u32 k_target_y_offset = 0x106cU;

bool IsFloatUnordered(float left, float right)
{
    return !(left < right) && !(left >= right);
}

} // namespace

// TH10 0x00428ce0.
float WrapAngleDeltaStackAbi(float a, float b)
{
    const float diff = a - b;
    if (diff > k_pi)
        return diff - k_two_pi;
    if (diff < -k_pi)
        return diff + k_two_pi;
    return diff;
}

// TH10 0x00428ad0.
void AcquireHomingTargetEcxEdxStackAbi(void *player, void *shot_record)
{
    PlayerShotRecord &shot = *reinterpret_cast<PlayerShotRecord *>(shot_record);
    PlayerRecord &player_rec = *reinterpret_cast<PlayerRecord *>(player);
    shot.homing_target = 0;

    void *const target = player_rec.homing_target;
    if (target == 0)
        return;

    shot.homing_target = target;
    const float target_x = LoadF32At(target, k_target_x_offset);
    // |x| > 224 ordered drops the target; a NaN comparison keeps it
    // (the native jne is taken on the unordered flags).
    if (target_x > k_target_limit_x || target_x < -k_target_limit_x)
        shot.homing_target = 0;
}

// TH10 0x00428b10.
i32 TickHomingShotMovementEdxAbi(void *shot_record)
{
    PlayerShotRecord &shot = *reinterpret_cast<PlayerShotRecord *>(shot_record);

    if (shot.state == 2U)
        return 0;

    // Drop the target once its flag dword shows bits 0x1/0x10 or
    // 0xc0000 (dead / untouchable states).
    void *target = shot.homing_target;
    if (target != 0U) {
        const u32 flags = LoadU32At(target, k_target_flags_offset);
        if ((flags & 0x11U) != 0U || (flags & 0xc0000U) != 0U)
            shot.homing_target = 0;
    }

    float speed = shot.speed;
    target = shot.homing_target;
    if (target == 0U) {
        // No target: ramp the speed while it stays within the cap.
        const float ramped = speed + k_speed_ramp;
        speed = (ramped > k_speed_cap) ? k_speed_cap : ramped;
        shot.speed = speed;
        return 0;
    }

    const u8 *const target_bytes = reinterpret_cast<const u8 *>(target);
    const float direction = static_cast<float>(std::atan2(
        static_cast<double>(LoadF32At(target_bytes, k_target_y_offset)
                            - shot.position[1]),
        static_cast<double>(LoadF32At(target_bytes, k_target_x_offset)
                            - shot.position[0])));
    const float delta = WrapAngleDeltaStackAbi(direction, shot.angle);

    if (static_cast<u32>(shot.timer_count) >= k_homing_age_gate) {
        // Post-lock acceleration, uncapped, no steering.
        shot.speed = speed + k_speed_ramp_fast;
        return 0;
    }

    const float magnitude = delta < 0.0f ? -delta : delta;
    if (!(magnitude >= k_quarter_turn)) {
        // magnitude < pi/4 (a NaN delta takes the same branch via the
        // unordered C0 flag). The 0.2617994f (pi/12) comparison cannot change the
        // outcome for ordered values — both sides run the identical
        // capped ramp — so only the unordered case skips it.
        if (!IsFloatUnordered(magnitude, k_straight_angle)) {
            const float ramped = speed + k_speed_ramp;
            speed = (ramped > k_speed_cap) ? k_speed_cap : ramped;
        }
    } else {
        // Target behind: the native comparison routes *every ordered*
        // result to the 4.0f constant — only a NaN speed survives.
        const float decelerated = speed - 0.3f;
        speed = IsFloatUnordered(decelerated, k_slow_speed)
            ? decelerated
            : k_slow_speed;
    }

    shot.angle = WrapAngleToPi(shot.angle + k_turn_rate * delta);
    shot.speed = speed;
    return 0;
}

} // namespace th10
