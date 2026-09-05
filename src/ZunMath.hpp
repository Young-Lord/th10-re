#pragma once

namespace th10 {

struct Float2 {
    float x;
    float y;
};

// FUNCTION: TH10 0x0041f800
void SetVectorFromAngle(Float2 *output, float angle, float length);

} // namespace th10
