#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdint.h>
#include <string>
#include <cmath>

namespace enjin
{
    std::string floatToString(float value, uint8_t precision = 2);
    std::string formatHz(float value);
    std::string formatMult(float value);
    float fmap(float in, float min, float max, float out_min, float out_max);
}

#endif // UTILS_HPP
