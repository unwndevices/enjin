#ifndef POLAR_HPP
#define POLAR_HPP

#include <stdint.h>
#include "Types.hpp"

namespace enjin
{
    Vector2 RadialToCartesian(float phase, uint8_t radius, Vector2 center = Vector2(63, 63));

} // namespace Polar

#endif // !POLAR_HPP