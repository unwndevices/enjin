#include "Utils.hpp"

namespace enjin
{
    std::string floatToString(float value, uint8_t precision)
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(precision) << value;
        return ss.str();
    }

    std::string formatHz(float value)
    {
        float abs_value = fabsf(value);
        std::stringstream ss;
        if (abs_value < 1.0f && abs_value > 0.0f)
        {
            // Show "0.xxx" format for sub-1 values (4 total characters: 0 + . + 3 digits)
            ss << "0." << std::setw(3) << std::setfill('0') << static_cast<int>(abs_value * 1000.0f);
        }
        else if (abs_value < 9.995f)
        {
            // Show "x.xx" format for values 1-9.99 (4 total characters: 1 + . + 2 digits)
            ss << std::fixed << std::setprecision(2) << abs_value;
        }
        else if (abs_value < 100.0f)
        {
            // Show "xx.x" format for values 10-99.9 (4 total characters: 2 + . + 1 digit)
            ss << std::fixed << std::setprecision(1) << abs_value;
        }
        else
        {
            // Show "xxxx" format for values >= 100 (4 total characters, no decimal)
            ss << std::fixed << std::setprecision(0) << abs_value;
        }
        return ss.str();
    }

    std::string formatMult(float value)
    {
        float abs_value = fabsf(value);
        std::stringstream ss;
        if (abs_value < 1.0f && abs_value > 0.0f)
        {
            // Show "0.xxx" format for sub-1 values (4 total characters: 0 + . + 3 digits)
            ss << "0." << std::setw(3) << std::setfill('0') << static_cast<int>(abs_value * 1000.0f);
        }
        else if (abs_value < 9.995f)
        {
            // Show "x.xx" format for values 1-9.99 (4 total characters: 1 + . + 2 digits)
            ss << std::fixed << std::setprecision(2) << abs_value;
        }
        else if (abs_value < 100.0f)
        {
            // Show "xx.x" format for values 10-99.9 (4 total characters: 2 + . + 1 digit)
            ss << std::fixed << std::setprecision(1) << abs_value;
        }
        else
        {
            // Show "xxxx" format for values >= 100 (4 total characters, no decimal)
            ss << std::fixed << std::setprecision(0) << abs_value;
        }
        return ss.str();
    }
    float fmap(float in, float min, float max, float out_min, float out_max)
    {
        return out_min + (in - min) * (out_max - out_min) / (max - min);
    }
}