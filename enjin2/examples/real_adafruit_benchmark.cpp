/**
 * @file real_adafruit_benchmark.cpp
 * @brief Real performance benchmark: Enjin2 vs Adafruit-GFX
 * 
 * Compares actual orbital animation performance using:
 * - Enjin2 4-bit Canvas4<128,64> 
 * - Adafruit GFXcanvas8(128,64)
 * 
 * Measures memory usage, rendering speed, and frame rates.
 */

#include <iostream>
#include <chrono>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>

// Enjin2 includes
#include "../include/enjin2/graphics/canvas.hpp"
#include "../include/enjin2/core/types.hpp"

// Adafruit-GFX includes  
#include "../../Libs/Adafruit-GFX-Library/Adafruit_GFX.h"

using namespace enjin2;

/**
 * @brief Benchmark configuration
 */
struct BenchmarkConfig {
    int numFrames = 200;
    int numSatellites = 3;
    bool enableTrails = true;
    bool enableAtmosphere = true;
    static constexpr int CANVAS_WIDTH = 128;
    static constexpr int CANVAS_HEIGHT = 64;
};

/**
 * @brief Benchmark results
 */
struct BenchmarkResults {
    std::string libraryName;
    double avgFrameTime;      // milliseconds
    double peakFrameTime;     // milliseconds
    double minFrameTime;      // milliseconds
    size_t memoryUsage;       // bytes
    double totalTime;         // seconds
    double framesPerSecond;   // fps
    int pixelsDrawnPerFrame;  // complexity metric
};

/**
 * @brief Enjin2 4-bit benchmark implementation
 */
class Enjin2Benchmark {
private:
    Canvas4<BenchmarkConfig::CANVAS_WIDTH, BenchmarkConfig::CANVAS_HEIGHT> canvas;
    BenchmarkConfig config;
    int pixelCount;
    
public:
    Enjin2Benchmark(const BenchmarkConfig& cfg) : config(cfg), pixelCount(0) {}
    
    BenchmarkResults runBenchmark() {
        std::vector<double> frameTimes;
        
        std::cout << "Running Enjin2 4-bit benchmark..." << std::endl;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int frame = 0; frame < config.numFrames; ++frame) {
            auto frameStart = std::chrono::high_resolution_clock::now();
            
            pixelCount = 0;
            renderFrame(frame);
            
            auto frameEnd = std::chrono::high_resolution_clock::now();
            double frameTime = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
            frameTimes.push_back(frameTime);
            
            if (frame % 50 == 0) {
                std::cout << "  Frame " << frame << ": " << frameTime << "ms" << std::endl;
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double totalTime = std::chrono::duration<double>(endTime - startTime).count();
        
        // Calculate statistics
        double avgFrameTime = 0.0;
        double peakFrameTime = 0.0;
        double minFrameTime = frameTimes[0];
        
        for (double time : frameTimes) {
            avgFrameTime += time;
            peakFrameTime = std::max(peakFrameTime, time);
            minFrameTime = std::min(minFrameTime, time);
        }
        avgFrameTime /= frameTimes.size();
        
        BenchmarkResults results;
        results.libraryName = "Enjin2 (4-bit)";
        results.avgFrameTime = avgFrameTime;
        results.peakFrameTime = peakFrameTime;
        results.minFrameTime = minFrameTime;
        results.memoryUsage = calculateMemoryUsage();
        results.totalTime = totalTime;
        results.framesPerSecond = config.numFrames / totalTime;
        results.pixelsDrawnPerFrame = pixelCount / config.numFrames;
        
        return results;
    }
    
private:
    void renderFrame(int frameNum) {
        float time = frameNum * 0.016f; // 60fps time step
        
        // Clear background
        canvas.clear(Pixel4(2));
        pixelCount += BenchmarkConfig::CANVAS_WIDTH * BenchmarkConfig::CANVAS_HEIGHT;
        
        // Add stars
        for (int i = 0; i < 15; ++i) {
            int x = (i * 23 + 17) % BenchmarkConfig::CANVAS_WIDTH;
            int y = (i * 37 + 29) % BenchmarkConfig::CANVAS_HEIGHT;
            canvas.setPixel(x, y, Pixel4(4 + (i % 2)));
            pixelCount++;
        }
        
        // Central planet with atmosphere
        Point center(64, 32);
        float planetRadius = 12.0f;
        float pulse = 0.8f + 0.2f * std::sin(time * 3.0f);
        
        if (config.enableAtmosphere) {
            drawPlanetWithAtmosphere(center, planetRadius * pulse);
        } else {
            drawSimplePlanet(center, planetRadius * pulse);
        }
        
        // Orbiting satellites
        for (int i = 0; i < config.numSatellites; ++i) {
            float angle = time * (1.0f + i * 0.5f) + i * 2.1f;
            float radius = 25.0f + i * 8.0f;
            
            Point satPos(
                static_cast<int16_t>(center.x + radius * std::cos(angle)),
                static_cast<int16_t>(center.y + radius * std::sin(angle))
            );
            
            // Draw satellite
            drawCircle(satPos, 2.5f, Pixel4(15 - i * 2));
            
            // Draw trail if enabled
            if (config.enableTrails) {
                drawSatelliteTrail(center, radius, angle, i);
            }
        }
        
        // Scanner beam
        float scanAngle = time * 2.5f;
        Point beamEnd(
            static_cast<int16_t>(20 + 15 * std::cos(scanAngle)),
            static_cast<int16_t>(50 + 15 * std::sin(scanAngle))
        );
        drawLine(Point(20, 50), beamEnd, Pixel4(15));
        drawCircle(Point(20, 50), 2.0f, Pixel4(14));
    }
    
    void drawPlanetWithAtmosphere(Point center, float radius) {
        float atmosphereRadius = radius * 1.4f;
        drawGlowCircle(center, radius, atmosphereRadius, Pixel4(12), Pixel4(8));
    }
    
    void drawSimplePlanet(Point center, float radius) {
        drawCircle(center, radius, Pixel4(12));
    }
    
    void drawCircle(Point center, float radius, Pixel4 color) {
        int r = static_cast<int>(radius);
        for (int y = -r; y <= r; ++y) {
            for (int x = -r; x <= r; ++x) {
                if (x * x + y * y <= r * r) {
                    int px = center.x + x;
                    int py = center.y + y;
                    if (px >= 0 && px < BenchmarkConfig::CANVAS_WIDTH && 
                        py >= 0 && py < BenchmarkConfig::CANVAS_HEIGHT) {
                        canvas.setPixel(px, py, color);
                        pixelCount++;
                    }
                }
            }
        }
    }
    
    void drawGlowCircle(Point center, float innerRadius, float outerRadius, 
                        Pixel4 coreColor, Pixel4 glowColor) {
        int r = static_cast<int>(outerRadius);
        for (int y = -r; y <= r; ++y) {
            for (int x = -r; x <= r; ++x) {
                float distance = std::sqrt(x * x + y * y);
                
                int px = center.x + x;
                int py = center.y + y;
                if (px >= 0 && px < BenchmarkConfig::CANVAS_WIDTH && 
                    py >= 0 && py < BenchmarkConfig::CANVAS_HEIGHT) {
                    if (distance <= innerRadius) {
                        canvas.setPixel(px, py, coreColor);
                        pixelCount++;
                    } else if (distance <= outerRadius) {
                        float glowFactor = 1.0f - (distance - innerRadius) / (outerRadius - innerRadius);
                        uint8_t glowIntensity = static_cast<uint8_t>(glowColor.value * glowFactor);
                        
                        Pixel4 existing = canvas.getPixel(px, py);
                        if (glowIntensity > existing.value) {
                            canvas.setPixel(px, py, Pixel4(glowIntensity));
                            pixelCount++;
                        }
                    }
                }
            }
        }
    }
    
    void drawSatelliteTrail(Point center, float radius, float angle, int satelliteIndex) {
        for (int j = 1; j <= 8; ++j) {
            float trailAngle = angle - j * 0.1f;
            Point trailPos(
                static_cast<int16_t>(center.x + radius * std::cos(trailAngle)),
                static_cast<int16_t>(center.y + radius * std::sin(trailAngle))
            );
            
            Pixel4 trailColor(std::max(1, (15 - satelliteIndex * 2) / (j + 1)));
            if (trailPos.x >= 0 && trailPos.x < BenchmarkConfig::CANVAS_WIDTH && 
                trailPos.y >= 0 && trailPos.y < BenchmarkConfig::CANVAS_HEIGHT) {
                canvas.setPixel(trailPos.x, trailPos.y, trailColor);
                pixelCount++;
            }
        }
    }
    
    void drawLine(Point from, Point to, Pixel4 color) {
        int dx = abs(to.x - from.x);
        int dy = abs(to.y - from.y);
        int sx = from.x < to.x ? 1 : -1;
        int sy = from.y < to.y ? 1 : -1;
        int err = dx - dy;
        
        int x = from.x;
        int y = from.y;
        
        while (true) {
            if (x >= 0 && x < BenchmarkConfig::CANVAS_WIDTH && 
                y >= 0 && y < BenchmarkConfig::CANVAS_HEIGHT) {
                canvas.setPixel(x, y, color);
                pixelCount++;
            }
            if (x == to.x && y == to.y) break;
            
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx) { err += dx; y += sy; }
        }
    }
    
    size_t calculateMemoryUsage() {
        // 4-bit canvas: 128 * 64 / 2 = 4KB + class overhead
        return sizeof(canvas) + sizeof(BenchmarkConfig) + 256; // ~4.3KB total
    }
};

/**
 * @brief Adafruit-GFX 8-bit benchmark implementation
 */
class AdafruitBenchmark {
private:
    GFXcanvas8 canvas;
    BenchmarkConfig config;
    int pixelCount;
    
public:
    AdafruitBenchmark(const BenchmarkConfig& cfg) 
        : canvas(BenchmarkConfig::CANVAS_WIDTH, BenchmarkConfig::CANVAS_HEIGHT), 
          config(cfg), pixelCount(0) {}
    
    BenchmarkResults runBenchmark() {
        std::vector<double> frameTimes;
        
        std::cout << "Running Adafruit-GFX 8-bit benchmark..." << std::endl;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int frame = 0; frame < config.numFrames; ++frame) {
            auto frameStart = std::chrono::high_resolution_clock::now();
            
            pixelCount = 0;
            renderFrame(frame);
            
            auto frameEnd = std::chrono::high_resolution_clock::now();
            double frameTime = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
            frameTimes.push_back(frameTime);
            
            if (frame % 50 == 0) {
                std::cout << "  Frame " << frame << ": " << frameTime << "ms" << std::endl;
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double totalTime = std::chrono::duration<double>(endTime - startTime).count();
        
        // Calculate statistics
        double avgFrameTime = 0.0;
        double peakFrameTime = 0.0;
        double minFrameTime = frameTimes[0];
        
        for (double time : frameTimes) {
            avgFrameTime += time;
            peakFrameTime = std::max(peakFrameTime, time);
            minFrameTime = std::min(minFrameTime, time);
        }
        avgFrameTime /= frameTimes.size();
        
        BenchmarkResults results;
        results.libraryName = "Adafruit-GFX (8-bit)";
        results.avgFrameTime = avgFrameTime;
        results.peakFrameTime = peakFrameTime;
        results.minFrameTime = minFrameTime;
        results.memoryUsage = calculateMemoryUsage();
        results.totalTime = totalTime;
        results.framesPerSecond = config.numFrames / totalTime;
        results.pixelsDrawnPerFrame = pixelCount / config.numFrames;
        
        return results;
    }
    
private:
    void renderFrame(int frameNum) {
        float time = frameNum * 0.016f;
        
        // Clear background
        canvas.fillScreen(32);
        pixelCount += BenchmarkConfig::CANVAS_WIDTH * BenchmarkConfig::CANVAS_HEIGHT;
        
        // Add stars
        for (int i = 0; i < 15; ++i) {
            int x = (i * 23 + 17) % BenchmarkConfig::CANVAS_WIDTH;
            int y = (i * 37 + 29) % BenchmarkConfig::CANVAS_HEIGHT;
            canvas.drawPixel(x, y, 64 + (i % 2) * 32);
            pixelCount++;
        }
        
        // Central planet
        int centerX = 64;
        int centerY = 32;
        float planetRadius = 12.0f;
        float pulse = 0.8f + 0.2f * std::sin(time * 3.0f);
        
        if (config.enableAtmosphere) {
            drawPlanetWithAtmosphere(centerX, centerY, planetRadius * pulse);
        } else {
            canvas.fillCircle(centerX, centerY, static_cast<int16_t>(planetRadius * pulse), 192);
            pixelCount += static_cast<int>(3.14159f * (planetRadius * pulse) * (planetRadius * pulse));
        }
        
        // Orbiting satellites
        for (int i = 0; i < config.numSatellites; ++i) {
            float angle = time * (1.0f + i * 0.5f) + i * 2.1f;
            float radius = 25.0f + i * 8.0f;
            
            int satX = static_cast<int16_t>(centerX + radius * std::cos(angle));
            int satY = static_cast<int16_t>(centerY + radius * std::sin(angle));
            
            // Draw satellite
            canvas.fillCircle(satX, satY, 2, 255 - i * 40);
            pixelCount += 12; // approximate pixels in small circle
            
            // Draw trail if enabled
            if (config.enableTrails) {
                drawSatelliteTrail(centerX, centerY, radius, angle, i);
            }
        }
        
        // Scanner beam
        float scanAngle = time * 2.5f;
        int beamEndX = static_cast<int16_t>(20 + 15 * std::cos(scanAngle));
        int beamEndY = static_cast<int16_t>(50 + 15 * std::sin(scanAngle));
        
        canvas.drawLine(20, 50, beamEndX, beamEndY, 255);
        canvas.fillCircle(20, 50, 2, 240);
        pixelCount += abs(beamEndX - 20) + abs(beamEndY - 50) + 12;
    }
    
    void drawPlanetWithAtmosphere(int centerX, int centerY, float radius) {
        // Draw atmosphere (larger, dimmer circle)
        float atmosphereRadius = radius * 1.4f;
        canvas.fillCircle(centerX, centerY, static_cast<int16_t>(atmosphereRadius), 128);
        
        // Draw core planet (smaller, brighter circle)
        canvas.fillCircle(centerX, centerY, static_cast<int16_t>(radius), 192);
        
        pixelCount += static_cast<int>(3.14159f * atmosphereRadius * atmosphereRadius);
    }
    
    void drawSatelliteTrail(int centerX, int centerY, float radius, float angle, int satelliteIndex) {
        for (int j = 1; j <= 8; ++j) {
            float trailAngle = angle - j * 0.1f;
            int trailX = static_cast<int16_t>(centerX + radius * std::cos(trailAngle));
            int trailY = static_cast<int16_t>(centerY + radius * std::sin(trailAngle));
            
            uint8_t trailColor = std::max(32, 255 - satelliteIndex * 40 - j * 20);
            canvas.drawPixel(trailX, trailY, trailColor);
            pixelCount++;
        }
    }
    
    size_t calculateMemoryUsage() {
        // 8-bit canvas: 128 * 64 = 8KB + class overhead
        return (BenchmarkConfig::CANVAS_WIDTH * BenchmarkConfig::CANVAS_HEIGHT) + 
               sizeof(GFXcanvas8) + sizeof(BenchmarkConfig) + 256; // ~8.3KB total
    }
};

/**
 * @brief Print benchmark results
 */
void printResults(const BenchmarkResults& results) {
    std::cout << "\n=== " << results.libraryName << " RESULTS ===" << std::endl;
    std::cout << "Average frame time: " << results.avgFrameTime << " ms" << std::endl;
    std::cout << "Min frame time: " << results.minFrameTime << " ms" << std::endl;
    std::cout << "Peak frame time: " << results.peakFrameTime << " ms" << std::endl;
    std::cout << "Frames per second: " << results.framesPerSecond << " fps" << std::endl;
    std::cout << "Memory usage: " << results.memoryUsage << " bytes (" 
              << (results.memoryUsage / 1024.0) << " KB)" << std::endl;
    std::cout << "Total benchmark time: " << results.totalTime << " seconds" << std::endl;
    std::cout << "Avg pixels per frame: " << results.pixelsDrawnPerFrame << std::endl;
}

/**
 * @brief Compare benchmark results
 */
void compareResults(const BenchmarkResults& enjin2, const BenchmarkResults& adafruit) {
    std::cout << "\n=== PERFORMANCE COMPARISON ===" << std::endl;
    
    double speedImprovement = adafruit.avgFrameTime / enjin2.avgFrameTime;
    double memoryReduction = (double)(adafruit.memoryUsage - enjin2.memoryUsage) / adafruit.memoryUsage * 100.0;
    double fpsImprovement = enjin2.framesPerSecond / adafruit.framesPerSecond;
    double peakSpeedImprovement = adafruit.peakFrameTime / enjin2.peakFrameTime;
    
    std::cout << "Speed improvement: " << speedImprovement << "x faster (avg frame time)" << std::endl;
    std::cout << "Peak improvement: " << peakSpeedImprovement << "x faster (worst case)" << std::endl;
    std::cout << "Memory reduction: " << memoryReduction << "% less memory" << std::endl;
    std::cout << "FPS improvement: " << fpsImprovement << "x higher framerate" << std::endl;
    
    double memoryKBSaved = (adafruit.memoryUsage - enjin2.memoryUsage) / 1024.0;
    std::cout << "Memory saved: " << memoryKBSaved << " KB" << std::endl;
    
    std::cout << "\n=== SUMMARY ===" << std::endl;
    if (speedImprovement > 1.1) {
        std::cout << "✅ Enjin2 is " << speedImprovement << "x faster" << std::endl;
    } else if (speedImprovement > 0.9) {
        std::cout << "➖ Similar performance (within 10%)" << std::endl;
    } else {
        std::cout << "❌ Enjin2 is slower" << std::endl;
    }
    
    if (memoryReduction > 30) {
        std::cout << "✅ Enjin2 uses " << memoryReduction << "% less memory" << std::endl;
    } else if (memoryReduction > 10) {
        std::cout << "➖ Enjin2 uses moderately less memory" << std::endl;
    } else {
        std::cout << "❌ Similar memory usage" << std::endl;
    }
    
    std::cout << "\n=== EMBEDDED DEVICE IMPACT ===" << std::endl;
    if (memoryReduction > 40) {
        std::cout << "🚀 Significant memory savings for embedded systems" << std::endl;
    }
    if (speedImprovement > 1.5) {
        std::cout << "⚡ Much better battery life due to reduced CPU usage" << std::endl;
    }
    if (fpsImprovement > 1.2) {
        std::cout << "🎯 Smoother animations on constrained hardware" << std::endl;
    }
}

/**
 * @brief Main benchmark application
 */
int main() {
    std::cout << "=== ENJIN2 vs ADAFRUIT-GFX REAL BENCHMARK ===" << std::endl;
    std::cout << "Comparing orbital animation performance on Linux..." << std::endl;
    std::cout << "Canvas size: " << BenchmarkConfig::CANVAS_WIDTH << "x" << BenchmarkConfig::CANVAS_HEIGHT << std::endl;
    
    BenchmarkConfig config;
    config.numFrames = 200;
    config.numSatellites = 3;
    config.enableTrails = true;
    config.enableAtmosphere = true;
    
    std::cout << "\nBenchmark configuration:" << std::endl;
    std::cout << "- Frames: " << config.numFrames << std::endl;
    std::cout << "- Satellites: " << config.numSatellites << std::endl;
    std::cout << "- Trails: " << (config.enableTrails ? "enabled" : "disabled") << std::endl;
    std::cout << "- Atmosphere: " << (config.enableAtmosphere ? "enabled" : "disabled") << std::endl;
    
    // Run Enjin2 benchmark
    std::cout << "\n" << std::string(50, '=') << std::endl;
    Enjin2Benchmark enjin2Bench(config);
    auto enjin2Results = enjin2Bench.runBenchmark();
    
    // Run Adafruit benchmark
    std::cout << "\n" << std::string(50, '=') << std::endl;
    AdafruitBenchmark adafruitBench(config);
    auto adafruitResults = adafruitBench.runBenchmark();
    
    // Print results
    printResults(enjin2Results);
    printResults(adafruitResults);
    
    // Compare results
    compareResults(enjin2Results, adafruitResults);
    
    return 0;
}