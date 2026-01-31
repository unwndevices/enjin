#pragma once

#include "../core/types.hpp"
#include "../graphics/canvas.hpp"
#include <functional>
#include <cstdint>
#include <vector>

namespace enjin2 {

/**
 * @brief Post-processing effect types
 */
enum class EffectType {
    None,
    CrtScanlines,    ///< CRT-style scanlines
    MovingScanlines, ///< Moving scanline effect
    BarrelDistortion,///< Barrel distortion effect
    Noise,           ///< Random noise overlay
    Blur,            ///< Simple blur effect
    Glow,            ///< Glow/bloom effect
    Dither,          ///< Dithering pattern
    Contrast,        ///< Contrast adjustment
    Brightness       ///< Brightness adjustment
};

/**
 * @brief PostFx parameter structure
 */
struct PostFxParams {
    float intensity = 1.0f;      ///< Effect intensity (0.0-1.0)
    float speed = 1.0f;          ///< Animation speed multiplier
    uint8_t threshold = 8;       ///< Threshold for effects like glow
    uint8_t color = 2;           ///< Effect color
    bool enabled = true;         ///< Effect enabled flag
    
    PostFxParams() = default;
    PostFxParams(float i, float s = 1.0f, uint8_t t = 8, uint8_t c = 2) 
        : intensity(i), speed(s), threshold(t), color(c) {}
};

/**
 * @brief Post-processing effects system
 * 
 * Provides various visual effects that can be applied to canvases
 * including CRT simulation, noise, blur, glow, etc.
 * Based on original Enjin PostFx with expanded functionality.
 */
class PostFx {
public:
    /**
     * @brief Constructor
     */
    PostFx();
    
    /**
     * @brief Update effect animations
     * @param deltaTime Time since last update in milliseconds
     */
    void update(uint16_t deltaTime);
    
    /**
     * @brief Apply CRT scanlines effect
     * @param canvas Target canvas to modify
     * @param params Effect parameters
     */
    static void applyCrtScanlines(ICanvas<uint8_t>& canvas, const PostFxParams& params = PostFxParams(0.5f));
    
    /**
     * @brief Apply moving scanlines effect
     * @param canvas Target canvas to modify
     * @param params Effect parameters
     * @param time Current time for animation
     */
    static void applyMovingScanlines(ICanvas<uint8_t>& canvas, const PostFxParams& params, uint32_t time);
    
    /**
     * @brief Apply barrel distortion effect
     * @param canvas Target canvas to modify
     * @param params Effect parameters (intensity controls distortion strength)
     */
    static void applyBarrelDistortion(ICanvas<uint8_t>& canvas, const PostFxParams& params = PostFxParams(0.3f));
    
    /**
     * @brief Apply noise overlay
     * @param canvas Target canvas to modify
     * @param params Effect parameters
     */
    static void applyNoise(ICanvas<uint8_t>& canvas, const PostFxParams& params = PostFxParams(0.3f));
    
    /**
     * @brief Apply simple blur effect
     * @param canvas Target canvas to modify
     * @param params Effect parameters
     */
    static void applyBlur(ICanvas<uint8_t>& canvas, const PostFxParams& params = PostFxParams(0.5f));
    
    /**
     * @brief Apply glow/bloom effect
     * @param canvas Target canvas to modify
     * @param params Effect parameters
     */
    static void applyGlow(ICanvas<uint8_t>& canvas, const PostFxParams& params = PostFxParams(0.4f));
    
    /**
     * @brief Apply dithering pattern
     * @param canvas Target canvas to modify
     * @param params Effect parameters
     */
    static void applyDither(ICanvas<uint8_t>& canvas, const PostFxParams& params = PostFxParams(0.5f));
    
    /**
     * @brief Apply contrast adjustment
     * @param canvas Target canvas to modify
     * @param params Effect parameters (intensity = contrast multiplier)
     */
    static void applyContrast(ICanvas<uint8_t>& canvas, const PostFxParams& params = PostFxParams(1.2f));
    
    /**
     * @brief Apply brightness adjustment
     * @param canvas Target canvas to modify
     * @param params Effect parameters (intensity = brightness offset)
     */
    static void applyBrightness(ICanvas<uint8_t>& canvas, const PostFxParams& params = PostFxParams(2.0f));
    
    /**
     * @brief Apply multiple effects in sequence
     * @param canvas Target canvas to modify
     * @param effects Vector of effect type and parameter pairs
     * @param time Current time for animated effects
     */
    static void applyEffectChain(ICanvas<uint8_t>& canvas, 
                                const std::vector<std::pair<EffectType, PostFxParams>>& effects,
                                uint32_t time = 0);
    
    /**
     * @brief Get current animation time for time-based effects
     * @return Current time in milliseconds
     */
    uint32_t getTime() const { return elapsed_time; }
    
private:
    uint32_t elapsed_time;          ///< Elapsed time for animations
    uint8_t scanline_offset;        ///< Current scanline offset
    uint16_t noise_seed;            ///< Noise generation seed
    
    /**
     * @brief Simple random number generator for effects
     * @return Random value 0-255
     */
    static uint8_t random();
    
    /**
     * @brief Clamp value to 4-bit range (0-15)
     * @param value Input value
     * @return Clamped value
     */
    static uint8_t clamp4bit(int value);
    
    /**
     * @brief Get dither pattern value for coordinates
     * @param x X coordinate
     * @param y Y coordinate
     * @return Dither pattern value
     */
    static uint8_t getDitherPattern(uint8_t x, uint8_t y);
};

} // namespace enjin2