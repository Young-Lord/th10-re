#include "ZunMath.hpp"

#include <math.h>

namespace th10 {

// FUNCTION: TH10 0x0041f800
void __fastcall SetVectorFromAngle(Float2 *output, float angle, float length)
{
    output->x = (float)(cos(angle) * length);
    output->y = (float)(sin(angle) * length);
}

} // namespace th10
