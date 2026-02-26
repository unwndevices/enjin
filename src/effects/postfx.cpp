#include "../../include/enjin2/effects/postfx.hpp"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>

namespace enjin2
{

    PostFx::PostFx()
        : elapsed_time(0.0f), noisePeriodAccum(0.0f), scanline_offset(0),
          noise_seed(static_cast<uint16_t>(std::time(nullptr) & 0xFFFF))
    {
        // Initialize random seed for effects
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
    }

    void PostFx::update(float dt)
    {
        elapsed_time += dt;

        // Update scanline animation (matches original Enjin timing: 150ms = 0.15s)
        if (elapsed_time >= 0.15f)
        {
            scanline_offset++;
            if (scanline_offset > 127)
            {
                scanline_offset = 0;
            }
        }

        // Update noise seed periodically using sub-accumulator (replaces integer modulo)
        noisePeriodAccum += dt;
        if (noisePeriodAccum >= 0.1f)
        {
            noisePeriodAccum -= 0.1f;
            noise_seed++;
        }
    }

    void PostFx::applyCrtScanlines(ICanvas<uint8_t> &canvas, const PostFxParams &params)
    {
        if (!params.enabled || params.intensity <= 0.0f)
            return;

        // Apply scanlines (matches original Enjin C_CrtSim exactly)
        for (int y = 0; y < 128; y++)
        { // Assume max canvas height
            if (y % 2 == 0)
            {
                // Draw scanline
                for (int x = 0; x < 128; x++)
                { // Assume max canvas width
                    uint8_t current = canvas.getPixel(x, y);
                    uint8_t darkened = static_cast<uint8_t>(current * (1.0f - params.intensity * 0.5f));
                    canvas.setPixel(x, y, clamp4bit(darkened));
                }
            }
        }
    }

    void PostFx::applyMovingScanlines(ICanvas<uint8_t> &canvas, const PostFxParams &params, uint32_t time)
    {
        if (!params.enabled || params.intensity <= 0.0f)
            return;

        // Calculate moving scanline position based on time
        float speed = params.speed * 0.1f; // Adjust speed multiplier
        int scanline_pos = static_cast<int>((time * speed) / 100.0f) % 128;

        // Draw multiple moving scanlines
        for (int line = 0; line < 3; line++)
        {
            int y = (scanline_pos + line * 42) % 128; // Space scanlines apart

            // Draw bright scanline
            for (int x = 0; x < 128; x++)
            {
                uint8_t current = canvas.getPixel(x, y);
                uint8_t brightened = clamp4bit(static_cast<int>(current) + static_cast<int>(params.intensity * 4));
                canvas.setPixel(x, y, brightened);
            }

            // Add subtle glow above and below
            if (y > 0)
            {
                for (int x = 0; x < 128; x++)
                {
                    uint8_t current = canvas.getPixel(x, y - 1);
                    uint8_t glow = clamp4bit(static_cast<int>(current) + static_cast<int>(params.intensity * 2));
                    canvas.setPixel(x, y - 1, glow);
                }
            }
            if (y < 127)
            {
                for (int x = 0; x < 128; x++)
                {
                    uint8_t current = canvas.getPixel(x, y + 1);
                    uint8_t glow = clamp4bit(static_cast<int>(current) + static_cast<int>(params.intensity * 2));
                    canvas.setPixel(x, y + 1, glow);
                }
            }
        }
    }

    void PostFx::applyBarrelDistortion(ICanvas<uint8_t> &canvas, const PostFxParams &params)
    {
        if (!params.enabled || params.intensity <= 0.0f)
            return;

        // Create temporary buffer for distortion
        uint8_t temp_buffer[128 * 128];

        // Copy original canvas to temp buffer
        for (int y = 0; y < 128; y++)
        {
            for (int x = 0; x < 128; x++)
            {
                temp_buffer[y * 128 + x] = canvas.getPixel(x, y);
            }
        }

        // Apply barrel distortion
        float centerX = 64.0f;
        float centerY = 64.0f;
        float strength = params.intensity * 0.5f; // Distortion strength
        float scale = strength * 2.2f;

        for (int y = 0; y < 128; y++)
        {
            for (int x = 0; x < 128; x++)
            {
                // Calculate distance from center
                float dx = x - centerX;
                float dy = y - centerY;
                float distance = std::sqrt(dx * dx + dy * dy);
                float maxDistance = std::sqrt(centerX * centerX + centerY * centerY);

                // Normalize distance
                float normalizedDistance = distance / maxDistance;

                // Apply barrel distortion formula (invert for true barrel effect)
                float distortionFactor = 1.0f - strength * normalizedDistance * normalizedDistance;

                // Calculate source coordinates (reverse mapping)
                float sourceX = centerX + (dx * scale) / distortionFactor;
                float sourceY = centerY + (dy * scale) / distortionFactor;

                // Bounds check and sample with nearest-neighbor
                int nearestX = static_cast<int>(sourceX + 0.5f); // Round to nearest
                int nearestY = static_cast<int>(sourceY + 0.5f); // Round to nearest

                if (nearestX >= 0 && nearestX < 128 && nearestY >= 0 && nearestY < 128)
                {
                    // Nearest-neighbor sampling (no interpolation)
                    uint8_t pixel = temp_buffer[nearestY * 128 + nearestX];
                    canvas.setPixel(x, y, pixel);
                }
                else
                {
                    // Out of bounds - set to black
                    canvas.setPixel(x, y, 0);
                }
            }
        }
    }

    void PostFx::applyNoise(ICanvas<uint8_t> &canvas, const PostFxParams &params)
    {
        if (!params.enabled || params.intensity <= 0.0f)
            return;

        // Add random noise overlay (matches original Enjin pattern)
        int noise_count = static_cast<int>(400 * params.intensity);

        for (int i = 0; i < noise_count; i++)
        {
            int x = random() % 128;
            int y = random() % 128;
            uint8_t noise_value = (random() % 3) + 1;

            uint8_t current = canvas.getPixel(x, y);
            uint8_t noisy = clamp4bit(static_cast<int>(current) - noise_value);
            canvas.setPixel(x, y, noisy);
        }
    }

    void PostFx::applyBlur(ICanvas<uint8_t> &canvas, const PostFxParams &params)
    {
        if (!params.enabled || params.intensity <= 0.0f)
            return;

        // Simple box blur implementation
        const int radius = std::max(1, static_cast<int>(params.intensity * 3));

        // Create temporary buffer for blur operation
        uint8_t temp_buffer[128 * 128];

        for (int y = 0; y < 128; y++)
        {
            for (int x = 0; x < 128; x++)
            {
                int sum = 0;
                int count = 0;

                // Sample neighborhood
                for (int dy = -radius; dy <= radius; dy++)
                {
                    for (int dx = -radius; dx <= radius; dx++)
                    {
                        int nx = x + dx;
                        int ny = y + dy;

                        if (nx >= 0 && nx < 128 && ny >= 0 && ny < 128)
                        {
                            sum += canvas.getPixel(nx, ny);
                            count++;
                        }
                    }
                }

                temp_buffer[y * 128 + x] = (count > 0) ? (sum / count) : 0;
            }
        }

        // Copy back to canvas
        for (int y = 0; y < 128; y++)
        {
            for (int x = 0; x < 128; x++)
            {
                canvas.setPixel(x, y, temp_buffer[y * 128 + x]);
            }
        }
    }

    void PostFx::applyGlow(ICanvas<uint8_t> &canvas, const PostFxParams &params)
    {
        if (!params.enabled || params.intensity <= 0.0f)
            return;

        // Create glow by expanding bright pixels
        uint8_t temp_buffer[128 * 128];

        // Copy original
        for (int y = 0; y < 128; y++)
        {
            for (int x = 0; x < 128; x++)
            {
                temp_buffer[y * 128 + x] = canvas.getPixel(x, y);
            }
        }

        // Add glow around bright pixels
        for (int y = 0; y < 128; y++)
        {
            for (int x = 0; x < 128; x++)
            {
                uint8_t current = temp_buffer[y * 128 + x];

                if (current >= params.threshold)
                {
                    // Add glow in surrounding area
                    int glow_radius = static_cast<int>(params.intensity * 2 + 1);

                    for (int dy = -glow_radius; dy <= glow_radius; dy++)
                    {
                        for (int dx = -glow_radius; dx <= glow_radius; dx++)
                        {
                            int nx = x + dx;
                            int ny = y + dy;

                            if (nx >= 0 && nx < 128 && ny >= 0 && ny < 128)
                            {
                                float distance = std::sqrt(dx * dx + dy * dy);
                                float glow_strength = params.intensity * (1.0f - distance / glow_radius);

                                if (glow_strength > 0)
                                {
                                    uint8_t existing = canvas.getPixel(nx, ny);
                                    uint8_t glow_add = static_cast<uint8_t>(current * glow_strength * 0.3f);
                                    canvas.setPixel(nx, ny, clamp4bit(existing + glow_add));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void PostFx::applyDither(ICanvas<uint8_t> &canvas, const PostFxParams &params)
    {
        if (!params.enabled || params.intensity <= 0.0f)
            return;

        // Apply Bayer dithering pattern
        for (int y = 0; y < 128; y++)
        {
            for (int x = 0; x < 128; x++)
            {
                uint8_t current = canvas.getPixel(x, y);
                uint8_t dither_value = getDitherPattern(x, y);

                float dither_effect = params.intensity * (dither_value / 15.0f - 0.5f);
                int dithered = static_cast<int>(current + dither_effect * 4);

                canvas.setPixel(x, y, clamp4bit(dithered));
            }
        }
    }

    void PostFx::applyContrast(ICanvas<uint8_t> &canvas, const PostFxParams &params)
    {
        if (!params.enabled)
            return;

        // Apply contrast adjustment
        float contrast = params.intensity;
        const float midpoint = 7.5f; // Middle of 0-15 range

        for (int y = 0; y < 128; y++)
        {
            for (int x = 0; x < 128; x++)
            {
                uint8_t current = canvas.getPixel(x, y);
                float adjusted = (current - midpoint) * contrast + midpoint;
                canvas.setPixel(x, y, clamp4bit(static_cast<int>(adjusted)));
            }
        }
    }

    void PostFx::applyBrightness(ICanvas<uint8_t> &canvas, const PostFxParams &params)
    {
        if (!params.enabled)
            return;

        // Apply brightness adjustment
        int brightness_offset = static_cast<int>(params.intensity);

        for (int y = 0; y < 128; y++)
        {
            for (int x = 0; x < 128; x++)
            {
                uint8_t current = canvas.getPixel(x, y);
                int brightened = static_cast<int>(current) + brightness_offset;
                canvas.setPixel(x, y, clamp4bit(brightened));
            }
        }
    }

    void PostFx::applyEffectChain(ICanvas<uint8_t> &canvas,
                                  const std::vector<std::pair<EffectType, PostFxParams>> &effects,
                                  uint32_t time)
    {
        // Apply effects in order
        for (const auto &effect_pair : effects)
        {
            switch (effect_pair.first)
            {
            case EffectType::CrtScanlines:
                applyCrtScanlines(canvas, effect_pair.second);
                break;
            case EffectType::MovingScanlines:
                applyMovingScanlines(canvas, effect_pair.second, time);
                break;
            case EffectType::BarrelDistortion:
                applyBarrelDistortion(canvas, effect_pair.second);
                break;
            case EffectType::Noise:
                applyNoise(canvas, effect_pair.second);
                break;
            case EffectType::Blur:
                applyBlur(canvas, effect_pair.second);
                break;
            case EffectType::Glow:
                applyGlow(canvas, effect_pair.second);
                break;
            case EffectType::Dither:
                applyDither(canvas, effect_pair.second);
                break;
            case EffectType::Contrast:
                applyContrast(canvas, effect_pair.second);
                break;
            case EffectType::Brightness:
                applyBrightness(canvas, effect_pair.second);
                break;
            case EffectType::None:
            default:
                break;
            }
        }
    }

    uint8_t PostFx::random()
    {
        return static_cast<uint8_t>(std::rand() & 0xFF);
    }

    uint8_t PostFx::clamp4bit(int value)
    {
        return static_cast<uint8_t>(std::max(0, std::min(15, value)));
    }

    uint8_t PostFx::getDitherPattern(uint8_t x, uint8_t y)
    {
        // 4x4 Bayer dithering matrix
        static const uint8_t bayer_matrix[4][4] = {
            {0, 8, 2, 10},
            {12, 4, 14, 6},
            {3, 11, 1, 9},
            {15, 7, 13, 5}};

        return bayer_matrix[y % 4][x % 4];
    }

} // namespace enjin2