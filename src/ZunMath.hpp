#pragma once

namespace th10 {

struct Float2 {
    float x;
    float y;
};

// FUNCTION: TH10 0x0041f800
// Original ABI: output pointer in ECX, angle/length on the stack, `ret 8` —
// MSVC __fastcall with a leading pointer and trailing float arguments.
void __fastcall SetVectorFromAngle(Float2 *output, float angle, float length);

} // namespace th10
