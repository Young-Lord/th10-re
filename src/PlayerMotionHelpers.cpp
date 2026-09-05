#include "PlayerMotionHelpers.hpp"

#include <cmath>

namespace th10 {

namespace {

const float kPi = 3.1415927f; // TH10 DAT_00470b18
const float kNegPi = -3.1415927f; // TH10 DAT_00470b10
const float kTwoPi = 6.2831855f; // TH10 DAT_00470b14
const float kHundred = 100.0f; // TH10 DAT_00470b44
const float kHundredth = 0.01f; // TH10 DAT_00470b00

inline float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void WriteFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

} // namespace

// TH10 0x0044bc70. Subtracts 2*pi while the value stays <= pi, then adds
// 2*pi while it stays < -pi, sharing one 32-iteration budget per direction;
// an unconverged value is returned as-is.
float WrapAngleToPi(float value)
{
    i32 iterations = 0;
    while (!(value > kPi)) {
        value -= kTwoPi;
        if (iterations++ > 32)
            return value;
    }
    while (value < kNegPi) {
        value += kTwoPi;
        if (iterations++ > 32)
            return value;
    }
    return value;
}

// TH10 0x0044c5d0. out = {cos(angle) * radius, sin(angle) * radius}.
void PolarToCartesianEdiAbi(void *out_memory, float angle, float radius)
{
    u8 *const out = static_cast<u8 *>(out_memory);
    WriteFloat(out, 0, static_cast<float>(
        std::cos(static_cast<double>(angle)) *
        static_cast<double>(radius)));
    WriteFloat(out, 4, static_cast<float>(
        std::sin(static_cast<double>(angle)) *
        static_cast<double>(radius)));
}

// TH10 0x0044c2a0. Motion block layout: pos {x@0, y@4, z@8}, vel
// {x@0xc, y@0x10, z@0x14}, angle @0x1c, radius @0x20, flags @0x28.
// Flag bit 0 selects the polar path: pos += polar(angle, radius) + vel
// with z = vel.z; otherwise pos += vel componentwise. x/y are then
// quantized with floor(x * 100) * 0.01; z is left alone.
void IntegrateSubEffectPositionEsiAbi(void *motion_memory)
{
    u8 *const motion = static_cast<u8 *>(motion_memory);
    if ((motion[0x28] & 1) == 0) {
        WriteFloat(motion, 0,
                   ReadFloat(motion, 0) + ReadFloat(motion, 0xc));
        WriteFloat(motion, 4,
                   ReadFloat(motion, 4) + ReadFloat(motion, 0x10));
        WriteFloat(motion, 8,
                   ReadFloat(motion, 8) + ReadFloat(motion, 0x14));
    } else {
        float polar[2];
        PolarToCartesianEdiAbi(polar, ReadFloat(motion, 0x1c),
                               ReadFloat(motion, 0x20));
        WriteFloat(motion, 0, polar[0] + ReadFloat(motion, 0xc));
        WriteFloat(motion, 4, polar[1] + ReadFloat(motion, 0x10));
        WriteFloat(motion, 8, ReadFloat(motion, 0x14));
    }
    for (u32 component = 0; component != 2; ++component) {
        const float value = ReadFloat(motion, component * 4);
        WriteFloat(motion, component * 4,
                   std::floor(static_cast<double>(value) * kHundred) *
                       kHundredth);
    }
}

} // namespace th10
