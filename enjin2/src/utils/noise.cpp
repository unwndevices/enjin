#include "../../include/enjin2/utils/noise.hpp"
#include <algorithm>
#include <cstdlib>
#include <ctime>

namespace enjin2 {
namespace Noise {

// Perlin noise permutation table (matches original Enjin exactly)
const unsigned char perm[512] = {
    151, 160, 137, 91, 90, 15,
    131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
    190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
    88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
    77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
    102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
    135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
    5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
    223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
    129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
    251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
    49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
    138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180,
    // Repeated for optimization
    151, 160, 137, 91, 90, 15,
    131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
    190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
    88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
    77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
    102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
    135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
    5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
    223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
    129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
    251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
    49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
    138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
};

float grad(int hash, float x, float y) {
    int h = hash & 7;        // Convert low 3 bits of hash code
    float u = h < 4 ? x : y; // into 8 simple gradient directions,
    float v = h < 4 ? y : x; // and compute the dot product with (x,y).
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

float pnoise(float x, float y, int px, int py) {
    int ix0, iy0, ix1, iy1;
    float fx0, fy0, fx1, fy1;
    float s, t, nx0, nx1, n0, n1;

    ix0 = FASTFLOOR(x); // Integer part of x
    iy0 = FASTFLOOR(y); // Integer part of y
    fx0 = x - ix0;      // Fractional part of x
    fy0 = y - iy0;      // Fractional part of y
    fx1 = fx0 - 1.0f;
    fy1 = fy0 - 1.0f;
    ix1 = ((ix0 + 1) % px) & 0xff; // Wrap to 0..px-1 and wrap to 0..255
    iy1 = ((iy0 + 1) % py) & 0xff; // Wrap to 0..py-1 and wrap to 0..255
    ix0 = (ix0 % px) & 0xff;
    iy0 = (iy0 % py) & 0xff;

    t = FADE(fy0);
    s = FADE(fx0);

    nx0 = grad(perm[ix0 + perm[iy0]], fx0, fy0);
    nx1 = grad(perm[ix0 + perm[iy1]], fx0, fy1);
    n0 = LERP(t, nx0, nx1);

    nx0 = grad(perm[ix1 + perm[iy0]], fx1, fy0);
    nx1 = grad(perm[ix1 + perm[iy1]], fx1, fy1);
    n1 = LERP(t, nx0, nx1);

    return 0.507f * (LERP(s, n0, n1));
}

uint8_t warped_pnoise(float nx, float ny, int px, int py, int seed, float warp_strength) {
    // Generate coordinate offsets using secondary noise layers (matches original Enjin exactly)
    float dx = warp_strength * pnoise(nx + 5.0f, ny + seed, px, py); // Different X offset seed
    float dy = warp_strength * pnoise(nx + seed, ny + 5.0f, px, py); // Different Y offset seed

    // Sample noise at warped coordinates
    float warped = pnoise(nx + dx + seed, ny + dy + seed, px, py);

    // Proper unsigned 8-bit conversion with corrected range mapping
    return static_cast<uint8_t>((warped * 0.5f + 0.5f) * 255.0f); // Maps [-1,1] → [0,255]
}

uint8_t value_noise(float x, float y, uint32_t seed) {
    // Simple hash-based value noise
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    
    float fx = x - ix;
    float fy = y - iy;
    
    // Hash function for grid points
    auto hash = [seed](int x, int y) -> uint32_t {
        uint32_t h = seed;
        h ^= x * 374761393U;
        h ^= y * 668265263U;
        h = (h ^ (h >> 13)) * 1274126177U;
        return h ^ (h >> 16);
    };
    
    // Get values at grid corners
    float v00 = (hash(ix, iy) & 0xFF) / 255.0f;
    float v10 = (hash(ix + 1, iy) & 0xFF) / 255.0f;
    float v01 = (hash(ix, iy + 1) & 0xFF) / 255.0f;
    float v11 = (hash(ix + 1, iy + 1) & 0xFF) / 255.0f;
    
    // Smooth interpolation
    float sx = FADE(fx);
    float sy = FADE(fy);
    
    float i1 = LERP(sx, v00, v10);
    float i2 = LERP(sx, v01, v11);
    float result = LERP(sy, i1, i2);
    
    return static_cast<uint8_t>(result * 255.0f);
}

uint8_t fbm_noise(float x, float y, int octaves, float persistence, float lacunarity) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float max_value = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        value += pnoise(x * frequency, y * frequency, 256, 256) * amplitude;
        max_value += amplitude;
        
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    
    // Normalize to [0, 1] range
    value = (value / max_value + 1.0f) * 0.5f;
    return static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, value)) * 255.0f);
}

uint8_t cellular_noise(float x, float y, float scale) {
    x *= scale;
    y *= scale;
    
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    
    float min_dist = 1000.0f;
    
    // Check 3x3 grid of cells
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int cell_x = xi + dx;
            int cell_y = yi + dy;
            
            // Generate random point in cell
            uint32_t h = static_cast<uint32_t>(cell_x * 374761393U + cell_y * 668265263U);
            h = (h ^ (h >> 13)) * 1274126177U;
            h = h ^ (h >> 16);
            
            float px = cell_x + (h & 0xFF) / 255.0f;
            float py = cell_y + ((h >> 8) & 0xFF) / 255.0f;
            
            // Calculate distance to point
            float dx_dist = x - px;
            float dy_dist = y - py;
            float dist = std::sqrt(dx_dist * dx_dist + dy_dist * dy_dist);
            
            if (dist < min_dist) {
                min_dist = dist;
            }
        }
    }
    
    // Normalize distance to [0, 255]
    return static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, min_dist)) * 255.0f);
}

void generate_noise_texture(uint8_t* buffer, int width, int height, 
                          float scale, float offset_x, float offset_y,
                          int noise_type) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float nx = (x + offset_x) * scale;
            float ny = (y + offset_y) * scale;
            
            uint8_t noise_value;
            
            switch (noise_type) {
                case 0: // Perlin noise
                    {
                        float pn = pnoise(nx, ny, 256, 256);
                        noise_value = static_cast<uint8_t>((pn + 1.0f) * 127.5f);
                    }
                    break;
                    
                case 1: // Value noise
                    noise_value = value_noise(nx, ny, 12345);
                    break;
                    
                case 2: // FBM noise
                    noise_value = fbm_noise(nx, ny, 4, 0.5f, 2.0f);
                    break;
                    
                case 3: // Cellular noise
                    noise_value = cellular_noise(nx, ny, 1.0f);
                    break;
                    
                default:
                    noise_value = 128; // Gray fallback
                    break;
            }
            
            buffer[y * width + x] = noise_value;
        }
    }
}

} // namespace Noise
} // namespace enjin2