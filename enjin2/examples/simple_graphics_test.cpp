/**
 * @file simple_graphics_test.cpp
 * @brief Simple test of graphics export functionality
 * 
 * Tests the ImageExporter and canvas system with basic shapes
 * to verify the graphics output system works properly.
 */

#include <iostream>
#include <cmath>

#include "../include/enjin2/graphics/canvas.hpp"
#include "../include/enjin2/graphics/image_export.hpp"
#include "../include/enjin2/core/types.hpp"

using namespace enjin2;

/**
 * @brief Draw a simple circle
 */
void drawCircle(Canvas4<128, 64>& canvas, Point center, float radius, Pixel4 color) {
    int r = static_cast<int>(radius);
    for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
            if (x * x + y * y <= r * r) {
                int px = center.x + x;
                int py = center.y + y;
                if (px >= 0 && px < 128 && py >= 0 && py < 64) {
                    canvas.setPixel(px, py, color);
                }
            }
        }
    }
}

/**
 * @brief Draw a circle with gradient/glow effect
 */
void drawGlowCircle(Canvas4<128, 64>& canvas, Point center, float innerRadius, float outerRadius, 
                    Pixel4 coreColor, Pixel4 glowColor) {
    int r = static_cast<int>(outerRadius);
    for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
            float distance = std::sqrt(x * x + y * y);
            
            int px = center.x + x;
            int py = center.y + y;
            if (px >= 0 && px < 128 && py >= 0 && py < 64) {
                if (distance <= innerRadius) {
                    // Core planet
                    canvas.setPixel(px, py, coreColor);
                } else if (distance <= outerRadius) {
                    // Atmosphere glow - fade from glow color to background
                    float glowFactor = 1.0f - (distance - innerRadius) / (outerRadius - innerRadius);
                    uint8_t glowIntensity = static_cast<uint8_t>(glowColor.value * glowFactor);
                    
                    // Only draw if the glow is brighter than existing pixel
                    Pixel4 existing = canvas.getPixel(px, py);
                    if (glowIntensity > existing.value) {
                        canvas.setPixel(px, py, Pixel4(glowIntensity));
                    }
                }
            }
        }
    }
}

/**
 * @brief Draw a simple line
 */
void drawLine(Canvas4<128, 64>& canvas, Point from, Point to, Pixel4 color) {
    int dx = abs(to.x - from.x);
    int dy = abs(to.y - from.y);
    int sx = from.x < to.x ? 1 : -1;
    int sy = from.y < to.y ? 1 : -1;
    int err = dx - dy;
    
    int x = from.x;
    int y = from.y;
    
    while (true) {
        if (x >= 0 && x < 128 && y >= 0 && y < 64) {
            canvas.setPixel(x, y, color);
        }
        if (x == to.x && y == to.y) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

/**
 * @brief Create an animated orbital scene manually
 */
void createOrbitalFrame(Canvas4<128, 64>& canvas, float time) {
    // Clear to dark space
    canvas.clear(Pixel4(2));
    
    // Add some stars
    for (int i = 0; i < 15; ++i) {
        int x = (i * 23 + 17) % 128;
        int y = (i * 37 + 29) % 64;
        canvas.setPixel(x, y, Pixel4(4 + (i % 2)));
    }
    
    // Central planet
    Point center(64, 32);
    float planetRadius = 12.0f;
    
    // Planet with pulsing effect and atmosphere glow
    float pulse = 0.8f + 0.2f * std::sin(time * 3.0f);
    float coreRadius = planetRadius * pulse;
    float atmosphereRadius = coreRadius * 1.4f;
    
    // Draw planet with atmosphere glow
    drawGlowCircle(canvas, center, coreRadius, atmosphereRadius, Pixel4(12), Pixel4(8));
    
    // Orbiting satellites
    for (int i = 0; i < 3; ++i) {
        float angle = time * (1.0f + i * 0.5f) + i * 2.1f;
        float radius = 25.0f + i * 8.0f;
        
        Point satPos(
            static_cast<int16_t>(center.x + radius * std::cos(angle)),
            static_cast<int16_t>(center.y + radius * std::sin(angle))
        );
        
        // Satellite
        drawCircle(canvas, satPos, 2.5f, Pixel4(15 - i * 2));
        
        // Orbital trail (partial)
        for (int j = 1; j <= 8; ++j) {
            float trailAngle = angle - j * 0.1f;
            Point trailPos(
                static_cast<int16_t>(center.x + radius * std::cos(trailAngle)),
                static_cast<int16_t>(center.y + radius * std::sin(trailAngle))
            );
            
            Pixel4 trailColor(std::max(1, (15 - i * 2) / (j + 1)));
            canvas.setPixel(trailPos.x, trailPos.y, trailColor);
        }
    }
    
    // Scanner beam
    float scanAngle = time * 2.5f;
    Point beamEnd(
        static_cast<int16_t>(20 + 15 * std::cos(scanAngle)),
        static_cast<int16_t>(50 + 15 * std::sin(scanAngle))
    );
    drawLine(canvas, Point(20, 50), beamEnd, Pixel4(15));
    drawCircle(canvas, Point(20, 50), 2.0f, Pixel4(14));
}

/**
 * @brief Main test application
 */
int main() {
    std::cout << "=== Simple Graphics Test ===\n";
    std::cout << "Testing ImageExporter with orbital animation\n\n";
    
    // Create canvas
    Canvas4<128, 64> canvas;
    
    // Generate and export animation frames
    const int numFrames = 12;
    const float timeStep = 0.5f;
    
    std::cout << "Generating " << numFrames << " animation frames...\n\n";
    
    for (int frame = 0; frame < numFrames; ++frame) {
        float time = frame * timeStep;
        
        // Create frame
        createOrbitalFrame(canvas, time);
        
        // Export as PGM
        char filename[64];
        snprintf(filename, sizeof(filename), "orbital_frame_%02d.pgm", frame);
        
        if (ImageExporter::exportToPGM(canvas, std::string(filename), 4)) {
            std::cout << "✓ Exported " << filename << "\n";
        } else {
            std::cout << "✗ Failed to export " << filename << "\n";
        }
        
        // Show improved ASCII every 3 frames
        if (frame % 3 == 0) {
            std::cout << "\n--- Frame " << frame << " Preview ---\n";
            ImageExporter::printColorVisual(canvas, "Orbital Animation");
            std::cout << "\n";
        }
    }
    
    std::cout << "\n=== Graphics Test Complete ===\n";
    std::cout << "Generated " << numFrames << " PGM files showing orbital animation\n";
    std::cout << "\nTo view the images:\n";
    std::cout << "- Linux: eog orbital_frame_*.pgm\n";
    std::cout << "- Convert to PNG: for f in *.pgm; do convert \"$f\" \"${f%.pgm}.png\"; done\n";
    std::cout << "\nThe animation shows:\n";
    std::cout << "- Central pulsing planet with atmosphere\n";
    std::cout << "- 3 orbiting satellites with trails\n";
    std::cout << "- Rotating scanner beam\n";
    std::cout << "- Stars in the background\n";
    
    return 0;
}