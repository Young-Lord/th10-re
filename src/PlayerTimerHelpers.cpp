#include "PlayerTimerHelpers.hpp"

#include <cmath>

namespace th10 {

namespace {

extern float g_FrameTimeScale; // TH10 DAT_00476f78

const float kRateUnityLow = 0.99f; // TH10 DAT_00470b68
const float kRateUnityHigh = 1.01f; // TH10 DAT_00470b64

// TH10 0x00463b2c: x87 conversion, round half away from zero.
i32 FloatToI32(float value)
{
    return value >= 0.0f
        ? static_cast<i32>(std::floor(static_cast<double>(value) + 0.5))
        : static_cast<i32>(std::ceil(static_cast<double>(value) - 0.5));
}

inline void WriteInt(u8 *bytes, u32 offset, i32 value)
{
    *reinterpret_cast<i32 *>(bytes + offset) = value;
}

inline void WriteFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

} // namespace

// TH10 0x00405410. Lazily initializes the block (the -NaN sentinel at prev
// is overwritten below; only the flag and rate pointer survive), then arms
// it: count = duration, prev = duration - 1, accumulator = duration.
void TickPlayerTimerEaxStackAbi(void *timer_memory, i32 duration)
{
    u8 *const timer = static_cast<u8 *>(timer_memory);
    if ((*reinterpret_cast<u8 *>(timer + 0x10) & 1) == 0) {
        WriteInt(timer, 4, 0);
        WriteInt(timer, 0, static_cast<i32>(0xfff0bdc1U));
        WriteInt(timer, 8, 0);
        *reinterpret_cast<const float **>(timer + 0xc) = &g_FrameTimeScale;
        *reinterpret_cast<u8 *>(timer + 0x10) |= 1;
    }
    WriteInt(timer, 4, duration);
    WriteInt(timer, 0, duration - 1);
    WriteFloat(timer, 8, static_cast<float>(duration));
}

// TH10 0x00404ed0. prev = count; inside the (0.99, 1.01) window the count
// steps by one and the accumulator by 1.0f; outside it (or for NaN) the
// accumulator takes the rate and the count is re-derived.
void TickTimerForwardEsiAbi(void *timer_memory)
{
    u8 *const timer = static_cast<u8 *>(timer_memory);
    *reinterpret_cast<i32 *>(timer) =
        *reinterpret_cast<const i32 *>(timer + 4);
    const float rate = **reinterpret_cast<float *const *>(timer + 0xc);
    if (rate > kRateUnityLow && rate < kRateUnityHigh) {
        WriteInt(timer, 4, *reinterpret_cast<const i32 *>(timer + 4) + 1);
        WriteFloat(timer, 8,
                   *reinterpret_cast<const float *>(timer + 8) + 1.0f);
    } else {
        const float acc =
            *reinterpret_cast<const float *>(timer + 8) + rate;
        WriteFloat(timer, 8, acc);
        WriteInt(timer, 4, FloatToI32(acc));
    }
}

// TH10 0044bf40. prev = count; the accumulator shifts by delta frames
// (rate-scaled outside the window) and the count is re-derived in both
// branches. The snapshot at +0 is left stale by design.
void ShiftTimerByEsiStackAbi(void *timer_memory, float delta)
{
    u8 *const timer = static_cast<u8 *>(timer_memory);
    *reinterpret_cast<i32 *>(timer) =
        *reinterpret_cast<const i32 *>(timer + 4);
    const float rate = **reinterpret_cast<float *const *>(timer + 0xc);
    float acc;
    if (rate > kRateUnityLow && rate < kRateUnityHigh) {
        acc = *reinterpret_cast<const float *>(timer + 8) + delta;
    } else {
        acc = *reinterpret_cast<const float *>(timer + 8) +
              delta * rate;
    }
    WriteFloat(timer, 8, acc);
    WriteInt(timer, 4, FloatToI32(acc));
}

} // namespace th10
