/**
 * @file noise.hpp
 * @brief Noise generation utilities for procedural content
 *
 * Provides Perlin noise, value noise, FBM noise, and cellular noise
 * for textures, terrain generation, and visual effects.
 */
#pragma once

#include <cstdint>
#include <cmath>

namespace enjin2 {

/**
 * @brief Noise generation utilities
 * 
 * Provides Perlin noise and various noise functions for
 * procedural generation and visual effects.
 * Based on original Enjin noise implementation.
 */
namespace Noise {

/** @brief Fast floor function for noise calculations */
#define FASTFLOOR(x) (((x) > 0) ? ((int)x) : ((int)x - 1))

/** @brief Linear interpolation between two values */
#define LERP(t, a, b) ((a) + (t) * ((b) - (a)))

/** @brief Fade function for smooth Perlin noise interpolation */
#define FADE(t) (t * t * t * (t * (t * 6 - 15) + 10))

/**
 * @brief Perlin noise permutation table
 * 
 * Standard permutation table repeated to avoid modulo operations
 */
extern const unsigned char perm[512];

/**
 * @brief Gradient function for Perlin noise
 * @param hash Hash value
 * @param x X coordinate
 * @param y Y coordinate
 * @return Gradient value
 */
float grad(int hash, float x, float y);

/**
 * @brief 2D Perlin noise function
 * @param x X coordinate
 * @param y Y coordinate
 * @param px Period in X direction (for tiling)
 * @param py Period in Y direction (for tiling)
 * @return Noise value in range [-1, 1]
 */
float pnoise(float x, float y, int px, int py);

/**
 * @brief Warped Perlin noise for more organic patterns
 * @param nx X coordinate
 * @param ny Y coordinate
 * @param px Period in X direction
 * @param py Period in Y direction
 * @param seed Random seed
 * @param warp_strength Warping strength (0.0 - 1.0)
 * @return Noise value in range [0, 255]
 */
uint8_t warped_pnoise(float nx, float ny, int px, int py, int seed, float warp_strength = 0.5f);

/**
 * @brief Simple value noise
 * @param x X coordinate
 * @param y Y coordinate
 * @param seed Random seed
 * @return Noise value in range [0, 255]
 */
uint8_t value_noise(float x, float y, uint32_t seed);

/**
 * @brief Fractal Brownian Motion (FBM) noise
 * @param x X coordinate
 * @param y Y coordinate
 * @param octaves Number of octaves
 * @param persistence Amplitude multiplier per octave
 * @param lacunarity Frequency multiplier per octave
 * @return Noise value in range [0, 255]
 */
uint8_t fbm_noise(float x, float y, int octaves = 4, float persistence = 0.5f, float lacunarity = 2.0f);

/**
 * @brief Cellular/Worley noise
 * @param x X coordinate
 * @param y Y coordinate
 * @param scale Grid scale
 * @return Distance to nearest feature point [0, 255]
 */
uint8_t cellular_noise(float x, float y, float scale = 10.0f);

/**
 * @brief Convert noise to 4-bit range
 * @param noise_value 8-bit noise value
 * @return 4-bit noise value (0-15)
 */
inline uint8_t to_4bit(uint8_t noise_value) {
    return noise_value >> 4;
}

/**
 * @brief Generate noise texture into buffer
 * @param buffer Output buffer
 * @param width Texture width
 * @param height Texture height
 * @param scale Noise scale
 * @param offset_x X offset for animation
 * @param offset_y Y offset for animation
 * @param noise_type 0=perlin, 1=value, 2=fbm, 3=cellular
 */
void generate_noise_texture(uint8_t* buffer, int width, int height, 
                          float scale, float offset_x, float offset_y,
                          int noise_type = 0);

} // namespace Noise

} // namespace enjin2