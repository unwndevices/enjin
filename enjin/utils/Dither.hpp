// Create enjin/Utils/Dither.hpp
#ifndef DITHER_HPP
#define DITHER_HPP

#include <cstdint>
#include <vector>
#include <cmath>
#include <numeric>

namespace enjin::Utils
{

    // --- Bayer Matrices ---
    // Using vectors for easier handling, could use raw arrays for performance if needed.

    // 2x2 Bayer Matrix (Thresholds: 0/4, 1/4, 2/4, 3/4)
    const std::vector<std::vector<uint8_t>> bayer2x2 = {
        {0, 2},
        {3, 1}};

    // 4x4 Bayer Matrix (Thresholds: 0/16 to 15/16)
    const std::vector<std::vector<uint8_t>> bayer4x4 = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5}};

    // 8x8 Bayer Matrix (Thresholds: 0/64 to 63/64)
    const std::vector<std::vector<uint8_t>> bayer8x8 = {
        {0, 32, 8, 40, 2, 34, 10, 42},
        {48, 16, 56, 24, 50, 18, 58, 26},
        {12, 44, 4, 36, 14, 46, 6, 38},
        {60, 28, 52, 20, 62, 30, 54, 22},
        {3, 35, 11, 43, 1, 33, 9, 41},
        {51, 19, 59, 27, 49, 17, 57, 25},
        {15, 47, 7, 39, 13, 45, 5, 37},
        {63, 31, 55, 23, 61, 29, 53, 21}};

    enum class BayerPatternType
    {
        BAYER_2x2,
        BAYER_4x4,
        BAYER_8x8
        // Add more if needed
    };

    /**
     * @brief Generates a dithered gradient using a specified Bayer pattern.
     *
     * Creates a vertical gradient (top to bottom) and applies dithering.
     * Assumes a 1-bit (black/white) output for simplicity.
     *
     * @param outputBuffer Pointer to the buffer where the dithered gradient will be stored. Must be pre-allocated with size width * height.
     * @param width Width of the gradient image.
     * @param height Height of the gradient image.
     * @param patternType The Bayer pattern to use for dithering.
     * @param startColor The grayscale value (0-255) at the top of the gradient.
     * @param endColor The grayscale value (0-255) at the bottom of the gradient.
     * @param outputMaxValue The value representing 'white' or the higher color in the dithered output (e.g., 255 or 1).
     * @param outputMinValue The value representing 'black' or the lower color in the dithered output (e.g., 0).
     */
    inline void generateDitheredGradient(
        uint8_t *outputBuffer,
        int width,
        int height,
        BayerPatternType patternType,
        uint8_t startColor = 0,
        uint8_t endColor = 255,
        uint8_t outputMaxValue = 255,
        uint8_t outputMinValue = 0)
    {
        const std::vector<std::vector<uint8_t>> *matrix;
        int matrixSize;
        float thresholdMultiplier;

        switch (patternType)
        {
        case BayerPatternType::BAYER_4x4:
            matrix = &bayer4x4;
            matrixSize = 4;
            thresholdMultiplier = 256.0f / 16.0f; // 4*4 = 16 levels
            break;
        case BayerPatternType::BAYER_8x8:
            matrix = &bayer8x8;
            matrixSize = 8;
            thresholdMultiplier = 256.0f / 64.0f; // 8*8 = 64 levels
            break;
        case BayerPatternType::BAYER_2x2:
        default: // Default to 2x2
            matrix = &bayer2x2;
            matrixSize = 2;
            thresholdMultiplier = 256.0f / 4.0f; // 2*2 = 4 levels
            break;
        }

        for (int y = 0; y < height; ++y)
        {
            // Calculate the ideal gradient value for this row (0-255)
            float idealValue = static_cast<float>(startColor) +
                               (static_cast<float>(endColor - startColor) * y / (height > 1 ? (height - 1) : 1));
            idealValue = std::max(0.0f, std::min(255.0f, idealValue)); // Clamp

            for (int x = 0; x < width; ++x)
            {
                // Get the Bayer threshold for this pixel position
                int matrixY = y % matrixSize;
                int matrixX = x % matrixSize;
                float threshold = static_cast<float>((*matrix)[matrixY][matrixX]);

                // Scale the threshold to the 0-255 range
                float scaledThreshold = threshold * thresholdMultiplier;

                // Compare and set the output pixel
                outputBuffer[y * width + x] = (idealValue >= scaledThreshold) ? outputMaxValue : outputMinValue;
            }
        }
    }

} // namespace enjin::Utils

#endif // DITHER_HPP
