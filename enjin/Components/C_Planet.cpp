#include "C_Planet.hpp"
#include "../utils/Noise.hpp"
#include "../utils/Dither.hpp" // Include Dither utility
#include <algorithm>           // For std::max, std::min
#include <cmath>               // For roundf

namespace enjin
{
    C_Planet::C_Planet(Object *owner, uint8_t radius, uint8_t standby_radius, uint8_t ring_radius_offset)
        : Component(owner),
          // Component canvas size must accommodate the LARGEST possible size (initial ring size)
          C_Drawable((radius + ring_radius_offset) * 2 + 1, (radius + ring_radius_offset) * 2 + 1),
          radius(radius), // Current radius starts at initial radius
          standby_radius(standby_radius),
          zoom_radius(radius), // Assuming zoom_radius meant initial radius
          // Initialize initial values
          initial_radius(radius),
          initial_diameter(radius * 2 + 1),
          initial_ringRadius(radius + ring_radius_offset),
          initial_ring_diameter((radius + ring_radius_offset) * 2 + 1),
          // Size fixed resources based on initial dimensions
          textureCanvas(),                    // Planet texture map
          ringTextureCanvas(),           // Ring texture map  
          ditheredShadingMask(), // Shading mask
          phase(0.0f),
          speed(0.0f),
          ringPhase(0.0f),
          ringSpeed(0.0f)
    {
        position = owner->GetComponent<C_Position>();

        // Define and normalize light direction (e.g., top-right, slightly front)
        lightDir.x = 1.0f;
        lightDir.y = -1.0f;
        lightDir.z = 0.1f;
        lightDir.normalize();

        // Generate maps based on initial dimensions
        GenerateSphericalMap(sphericalMap, initial_radius);         // Generate planet map
        GenerateSphericalMap(ringSphericalMap, initial_ringRadius); // Generate ring map

        GenerateDitheredShadingMask(); // Generate shading mask (uses initial_radius implicitly now)
        GenerateTerrain();             // Generate planet texture
        GenerateRingTexture();         // Generate ring texture
    }

    void C_Planet::Update(uint16_t deltaTime)
    {
        // Update planet phase
        phase += speed * (deltaTime / 1000.0f);
        phase = fmod(phase, 1.0f);
        if (phase < 0.0f)
        {
            phase += 1.0f;
        }

        // Update ring phase
        ringPhase += ringSpeed * (deltaTime / 1000.0f);
        ringPhase = fmod(ringPhase, 1.0f);
        if (ringPhase < 0.0f)
        {
            ringPhase += 1.0f;
        }
    };

    void C_Planet::Draw(EiseiCanvas &canvas)
    {
        // Prevent division by zero if initial radius was 0
        if (initial_radius == 0)
            return;

        // Pre-calculate constants
        const float scaleFactor = (float)radius / initial_radius;
        if (scaleFactor <= 0.0f)
            return; // Avoid division by zero later and unnecessary work if scaled to nothing

        const float component_center_x = canvas.width() / 2.0f;
        const float component_center_y = canvas.height() / 2.0f;
        const int canvas_width = canvas.width();
        const int canvas_height = canvas.height();
        const int texture_width = textureCanvas.width();
        const int texture_height = textureCanvas.height();
        const int ring_texture_width = ringTextureCanvas.width();
        const int ring_texture_height = ringTextureCanvas.height();
        const float planet_phase_offset = phase * texture_width;
        const float ring_phase_offset = ringPhase * ring_texture_width;

        const int numLevels = 5; // Must match GenerateDitheredShadingMask
        const int neutralLevelOffset = numLevels / 2;

        // --- Inverse map canvas bounds to determine optimal original loop ranges ---
        // target_relative = orig_relative * scaleFactor  => orig_relative = target_relative / scaleFactor
        // orig = orig_relative + initial_radius
        // Target relative range: [-component_center_x, canvas_width - 1 - component_center_x]
        const float invScaleFactor = 1.0f / scaleFactor;
        const float target_rel_min_x = -component_center_x;
        const float target_rel_max_x = (float)canvas_width - 1.0f - component_center_x;
        const float target_rel_min_y = -component_center_y;
        const float target_rel_max_y = (float)canvas_height - 1.0f - component_center_y;

        // --- Planet Bounds ---
        int planet_min_orig_x = static_cast<int>(floorf(target_rel_min_x * invScaleFactor + initial_radius));
        int planet_max_orig_x = static_cast<int>(ceilf(target_rel_max_x * invScaleFactor + initial_radius));
        int planet_min_orig_y = static_cast<int>(floorf(target_rel_min_y * invScaleFactor + initial_radius));
        int planet_max_orig_y = static_cast<int>(ceilf(target_rel_max_y * invScaleFactor + initial_radius));

        // Clamp to actual initial diameter bounds
        planet_min_orig_x = std::max(0, planet_min_orig_x);
        planet_max_orig_x = std::min(initial_diameter - 1, planet_max_orig_x);
        planet_min_orig_y = std::max(0, planet_min_orig_y);
        planet_max_orig_y = std::min(initial_diameter - 1, planet_max_orig_y);

        // --- Draw Planet (Scaled and Optimized) ---
        if (planet_min_orig_x <= planet_max_orig_x && planet_min_orig_y <= planet_max_orig_y) // Only loop if bounds are valid
        {
            for (int orig_y = planet_min_orig_y; orig_y <= planet_max_orig_y; ++orig_y)
            {
                const int map_row_offset = orig_y * initial_diameter;
                const float orig_relative_y = (float)orig_y - initial_radius;
                const float target_relative_y = orig_relative_y * scaleFactor;

                for (int orig_x = planet_min_orig_x; orig_x <= planet_max_orig_x; ++orig_x)
                {
                    const Vector2 *_pos = &sphericalMap[map_row_offset + orig_x];
                    if (_pos->x >= 0) // Check for valid map coordinate on original map
                    {
                        int texture_y = static_cast<int>(_pos->y); // Assuming y is already integer-like from GenerateSphericalMap

                        // Ensure texture y coordinate is within bounds
                        if (texture_y < 0 || texture_y >= texture_height)
                            continue;

                        // Calculate texture x coordinate, wrapping around texture width
                        int texture_x = static_cast<int>(_pos->x + planet_phase_offset);
                        texture_x %= texture_width;
                        if (texture_x < 0)
                            texture_x += texture_width; // Ensure positive modulo result

                        // Get original color from planet texture
                        uint8_t original_color = textureCanvas.getPixel(texture_x, texture_y);

                        // Apply color factor logic
                        uint8_t processed_color;
                        if (color_factor == 0.0f)
                        {
                            processed_color = 0;
                        }
                        else if (color_factor > 0.0f)
                        {
                            processed_color = static_cast<uint8_t>(roundf(static_cast<float>(original_color) * color_factor));
                        }
                        else // color_factor < 0.0f
                        {
                            uint8_t inverted_color = 15 - original_color;
                            processed_color = static_cast<uint8_t>(roundf(static_cast<float>(inverted_color) * fabsf(color_factor)));
                        }
                        // Clamp processed color
                        processed_color = std::min(static_cast<uint8_t>(15), processed_color);

                        // Get shading adjustment from the pre-calculated mask (using original coords)
                        int8_t shadingAdjustment = ditheredShadingMask.getPixel(orig_x, orig_y) - neutralLevelOffset;

                        // Apply shading adjustment and clamp to 0-15 range
                        int finalColor_intermediate = static_cast<int>(processed_color) + shadingAdjustment;
                        uint8_t finalColor = std::max(0, std::min(15, finalColor_intermediate));

                        // Calculate target coordinates using pre-calculated relative Y
                        const float orig_relative_x = (float)orig_x - initial_radius;
                        const float target_relative_x = orig_relative_x * scaleFactor;
                        // Use static_cast<int>(value + 0.5f) for rounding
                        const int target_x = static_cast<int>(target_relative_x + component_center_x + 0.5f);
                        const int target_y = static_cast<int>(target_relative_y + component_center_y + 0.5f);

                        // Draw pixel on the component canvas if within bounds (still needed as safety)
                        if (target_x >= 0 && target_x < canvas_width && target_y >= 0 && target_y < canvas_height)
                        {
                            canvas.drawPixel(target_x, target_y, finalColor);
                        }
                    }
                }
            }
        }

        // Draw planet outline (uses current animated radius) - uses integer division for center
        canvas.drawCircle(canvas.width() / 2, canvas.height() / 2, radius, 14);

        // --- Draw Ring (Scaled, Conditional, and Optimized) ---

        // Skip drawing the ring if the planet is at or below standby size
        if (radius <= standby_radius)
        {
            return; // Exit draw function early
        }

        // Prevent division by zero if initial ring radius was 0 (already handled by scaleFactor check if radius > 0)
        if (initial_ringRadius == 0)
            return;

        // --- Ring Bounds ---
        // Use the same inverse scale factor but map relative to initial_ringRadius
        int ring_min_orig_x = static_cast<int>(floorf(target_rel_min_x * invScaleFactor + initial_ringRadius));
        int ring_max_orig_x = static_cast<int>(ceilf(target_rel_max_x * invScaleFactor + initial_ringRadius));
        int ring_min_orig_y = static_cast<int>(floorf(target_rel_min_y * invScaleFactor + initial_ringRadius));
        int ring_max_orig_y = static_cast<int>(ceilf(target_rel_max_y * invScaleFactor + initial_ringRadius));

        // Clamp to actual initial ring diameter bounds
        ring_min_orig_x = std::max(0, ring_min_orig_x);
        ring_max_orig_x = std::min(initial_ring_diameter - 1, ring_max_orig_x);
        ring_min_orig_y = std::max(0, ring_min_orig_y);
        ring_max_orig_y = std::min(initial_ring_diameter - 1, ring_max_orig_y);

        const uint8_t transparentColor = 16; // Define transparent color for ring

        if (ring_min_orig_x <= ring_max_orig_x && ring_min_orig_y <= ring_max_orig_y) // Only loop if bounds are valid
        {
            for (int orig_y = ring_min_orig_y; orig_y <= ring_max_orig_y; ++orig_y)
            {
                const int map_row_offset = orig_y * initial_ring_diameter;
                const float orig_relative_y = (float)orig_y - initial_ringRadius;
                const float target_relative_y = orig_relative_y * scaleFactor; // Use planet's scale factor

                for (int orig_x = ring_min_orig_x; orig_x <= ring_max_orig_x; ++orig_x)
                {
                    const Vector2 *_pos = &ringSphericalMap[map_row_offset + orig_x];
                    if (_pos->x >= 0) // Check valid original map coordinate
                    {
                        int texture_y = static_cast<int>(_pos->y); // Use y from original mapping

                        // Ensure texture coordinates are within bounds
                        if (texture_y < 0 || texture_y >= ring_texture_height)
                            continue;

                        // Calculate texture coordinates from original map
                        int texture_x = static_cast<int>(_pos->x + ring_phase_offset);
                        texture_x %= ring_texture_width;
                        if (texture_x < 0)
                            texture_x += ring_texture_width; // Ensure positive modulo result

                        // Get color from ring texture
                        uint8_t color = ringTextureCanvas.getPixel(texture_x, texture_y);

                        // Draw pixel ONLY if it's not the transparent color
                        if (color != transparentColor)
                        {
                            // Calculate target coordinates by scaling from the original ring map's center
                            const float orig_relative_x = (float)orig_x - initial_ringRadius;
                            const float target_relative_x = orig_relative_x * scaleFactor; // Use planet's scale factor
                            // Use static_cast<int>(value + 0.5f) for rounding
                            const int target_x = static_cast<int>(target_relative_x + component_center_x + 0.5f);
                            const int target_y = static_cast<int>(target_relative_y + component_center_y + 0.5f);

                            // Draw pixel on the component canvas if within bounds (still needed as safety)
                            if (target_x >= 0 && target_x < canvas_width && target_y >= 0 && target_y < canvas_height)
                            {
                                canvas.drawPixel(target_x, target_y, color);
                            }
                        }
                    }
                }
            }
        }
        // No separate ring outline needed usually, it's part of the texture mapping
    };

    void C_Planet::GenerateSphericalMap(std::vector<Vector2> &map, uint8_t map_radius)
    {
        // Prevent division by zero or negative size
        if (map_radius == 0)
        {
            map.clear();
            return;
        }
        uint8_t diameter = map_radius * 2 + 1;
        map.resize(diameter * diameter); // Pre-allocate memory

        // coordinate table generation
        for (int y = 0; y < diameter; y++)
        {
            for (int x = 0; x < diameter; x++)
            {
                Vector2 _pos;
                _pos.x = -1; // Default to invalid
                _pos.y = -1;

                float centeredX = (float)x - map_radius;
                float centeredY = (float)y - map_radius;
                float distSq = centeredX * centeredX + centeredY * centeredY;

                // Check if point is within the circle defined by map_radius
                if (distSq <= map_radius * map_radius)
                {
                    // Avoid division by zero at the exact center for longitude calculation later if needed
                    // Though latitude calculation handles poles.
                    if (map_radius > 0)
                    {
                        float sineLatitude = centeredY / map_radius;
                        // Clamp latitude to avoid domain errors with asin due to potential floating point inaccuracies
                        sineLatitude = std::max(-1.0f, std::min(1.0f, sineLatitude));
                        float latitude = asin(sineLatitude);

                        float circleRadius = map_radius * cos(latitude);

                        // Avoid division by zero or very small numbers if near the poles
                        if (abs(circleRadius) < 1e-5)
                        {
                            // At poles, longitude is ambiguous, map to a texture edge maybe?
                            // Or handle based on which pole. Let's map x simply for now.
                            // Example: map to center of texture horizontally
                            _pos.x = textureCanvas.width() / 2.0f; // Or ringTextureCanvas.width() if generating for ring
                        }
                        else
                        {
                            float sineLongitude = centeredX / circleRadius;
                            // Clamp longitude to avoid domain errors with asin
                            sineLongitude = std::max(-1.0f, std::min(1.0f, sineLongitude));

                            // Map longitude from [-PI/2, PI/2] + PI/2 to [0, PI] range for texture mapping
                            float longitude = asin(sineLongitude) + M_PI / 2.0f;

                            // Scale longitude to texture width (0 to 127 for 127 width)
                            _pos.x = (longitude / M_PI) * (float)(textureCanvas.width() - 1); // Use textureCanvas width - 1 as max index
                        }
                        // Map latitude to texture height. Example: Direct mapping [-PI/2, PI/2] to [0, height-1]
                        // Adjust if texture map expects different mapping (e.g., cylindrical)
                        // This simple mapping uses y directly, assuming texture rows correspond to map rows.
                        _pos.y = y; // Using the direct y coordinate for vertical texture mapping

                        // Ensure calculated texture coords are valid integers for pixel lookup later
                        _pos.x = floorf(_pos.x);
                        _pos.y = floorf(_pos.y);

                    } // End if map_radius > 0

                } // End if within circle

                map[y * diameter + x] = _pos; // Assign directly using calculated index
            }
        }
    }

    void C_Planet::GenerateTerrain()
    {
        // randomSeed(millis());
        if (first_generation)
        {
            seed = (rand() % 300 + 1) * 300;
            first_generation = false;
        }

        // texture generation
        for (int x = 0; x < textureCanvas.width(); x++) // Use textureCanvas width
        {
            for (int y = 0; y < textureCanvas.height(); y++) // Use textureCanvas height
            {
                // ridged function
                float nx = (float)x / textureCanvas.width() * noiseScaleX;
                float ny = (float)y * noiseScaleY;
                // uint8_t value = (uint8_t)(abs(pnoise(nx, ny, 127, radius * 2)) * -1.0f * 255.0f);
                uint8_t value = warped_pnoise(nx, ny, 127, radius * 2, seed);

                if (value < 110)
                    value = 1;
                else if (value < 150)
                    value = 0;
                else if (value < 180)
                    value = 8;
                else if (value < 200)
                    value = 10;
                else if (value < 220)
                    value = 14;
                else
                    value = 6;
                textureCanvas.drawPixel(x, y, value);
            }
        }
    }

    void C_Planet::GenerateRingTexture()
    {
        uint8_t transparentColor = 16;
        ringTextureCanvas.fillScreen(transparentColor); // Start with transparent background

        int ringCenterY = ringTextureCanvas.height() / 2;
        int bandHeight = 5;                    // How thick the main band of asteroids is
        int jitter = 10;                       // How much vertical jitter
        int numAsteroids = 25;                 // How many asteroids to attempt placing
        int width = ringTextureCanvas.width(); // Get canvas width for wrapping

        for (int i = 0; i < numAsteroids; ++i)
        {
            // Random horizontal position
            int x = rand() % width;
            // Random vertical position within the band + jitter
            int y = ringCenterY + (rand() % (bandHeight + 2 * jitter)) - (bandHeight / 2 + jitter);
            // Random size (radius) for the asteroid
            int asteroidRadius = (rand() % 6) + 1; // radius 1-6
            // Random grayscale color (non-transparent)
            uint8_t color = (rand() % 6) + 5; // Colors 5-10

            // Ensure y is within canvas bounds
            y = std::max(0, std::min(ringTextureCanvas.height() - 1, y));

            // Draw the main asteroid instance
            ringTextureCanvas.fillCircle(x, y, asteroidRadius, 1); // Using color 1 for fill (adjust if needed)
            ringTextureCanvas.drawCircle(x, y, asteroidRadius, color);

            // Check and draw wrapped portion if asteroid overlaps left edge
            if (x - asteroidRadius < 0)
            {
                ringTextureCanvas.fillCircle(x + width, y, asteroidRadius, 1);
                ringTextureCanvas.drawCircle(x + width, y, asteroidRadius, color);
            }

            // Check and draw wrapped portion if asteroid overlaps right edge
            if (x + asteroidRadius >= width)
            {
                ringTextureCanvas.fillCircle(x - width, y, asteroidRadius, 1);
                ringTextureCanvas.drawCircle(x - width, y, asteroidRadius, color);
            }
        }
    }

    // Method to generate the dithered shading mask
    void C_Planet::GenerateDitheredShadingMask()
    {
        // Prevent division by zero if initial radius was 0
        if (initial_radius == 0)
            return;

        const int numLevels = 5;
        // Use the defined levels array if needed, or calculate directly
        // const int8_t levels[numLevels] = {-10, -5, 0, 3, 9}; // Example levels
        const int neutralLevelOffset = numLevels / 2; // Should be 2

        const auto &bayerMatrix = Utils::bayer4x4;
        const int matrixSize = 4;
        const float thresholdLevels = 16.0f; // Number of steps in Bayer matrix

        // Use initial_diameter for loop bounds
        uint8_t diameter = initial_diameter;

        for (int y = 0; y < diameter; ++y)
        {
            for (int x = 0; x < diameter; ++x)
            {
                // Use initial_radius for centering and normal calculation
                float centeredX = (float)x - initial_radius;
                float centeredY = (float)y - initial_radius;
                uint8_t maskValue = neutralLevelOffset; // Default value (level 2, representing 0 adjustment)

                float distSq = centeredX * centeredX + centeredY * centeredY;
                if (distSq <= initial_radius * initial_radius)
                {
                    // Calculate normalized surface normal (nx, ny, nz) using initial_radius
                    float nx = centeredX / initial_radius;
                    float ny = centeredY / initial_radius;
                    float R_sq = nx * nx + ny * ny;
                    if (R_sq > 1.0f)
                        R_sq = 1.0f;               // Clamp due to potential float errors at edge
                    float nz = sqrtf(1.0f - R_sq); // Z component of normalized normal

                    // Calculate lighting intensity using dot product
                    // Ensure lightDir is normalized (done in constructor)
                    float dot = nx * lightDir.x + ny * lightDir.y + nz * lightDir.z; // dot is in [-1, 1]

                    // Map dot product (-1 to 1) to ideal level index (0.0 to numLevels-1, e.g., 0.0 to 4.0)
                    float idealLevelIndex = (dot + 1.0f) * 0.5f * (numLevels - 1);
                    idealLevelIndex = std::max(0.0f, std::min((float)numLevels - 1.0f, idealLevelIndex)); // Clamp

                    // Apply dithering
                    float threshold = (float)bayerMatrix[y % matrixSize][x % matrixSize]; // Threshold from 0-15
                    int ditheredLevelIndex = floorf(idealLevelIndex);                     // Integer part
                    float fractional = idealLevelIndex - ditheredLevelIndex;              // Fractional part [0, 1)

                    // If fractional part scaled by threshold levels exceeds matrix threshold, bump to next level
                    if (fractional * thresholdLevels > threshold)
                    {
                        ditheredLevelIndex++;
                    }
                    // Clamp final index to valid range [0, numLevels-1]
                    ditheredLevelIndex = std::max(0, std::min(numLevels - 1, ditheredLevelIndex));

                    // Set the mask value directly to the calculated level index (0-4)
                    maskValue = ditheredLevelIndex; // Mask now stores 0-4 instead of adjusted levels + offset
                }
                // else: point outside the sphere, keep default maskValue (neutralLevelOffset)

                ditheredShadingMask.drawPixel(x, y, maskValue);
            }
        }
    }
}