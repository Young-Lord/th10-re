#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0044bc70. Native input is one stack float, result in ST0; the
// register ABI remains a thunk boundary. Wraps into (-pi, pi] with at most
// 32 iterations per direction; a value that does not converge (or NaN)
// comes back unnormalized.
float WrapAngleToPi(float value);

// TH10 0x0044c5d0. Native inputs ECX = out {x, y}, stack = angle, radius.
void PolarToCartesianEdiAbi(void *out, float angle, float radius);

// TH10 0x0044c2a0. Native input ESI = motion block
// {pos xyz, vel xyz, angle, radius, flags@0x28}; integrates the position
// and quantizes x/y onto the 1/100 grid.
void IntegrateSubEffectPositionEsiAbi(void *motion);

} // namespace th10
