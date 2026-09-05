#include "ZunMath.hpp"

#include <math.h>

namespace th10 {

// FUNCTION: TH10 0x0041f800
void SetVectorFromAngle(Float2 *output, float angle, float length)
{
    output->x = cosf(angle) * length;
    output->y = sinf(angle) * length;
}

} // namespace th10
