/**
 * @file enjin_comparison_benchmark.cpp
 * @brief Comprehensive benchmark comparing Old Enjin vs Enjin2
 * 
 * Tests the performance difference between:
 * - Legacy Enjin (shared_ptr based, Adafruit-GFX backend)
 * - Enjin2 (handle-based, 4-bit optimized canvas)
 * 
 * Measures memory usage, rendering speed, and typical UI operations
 * used in the Eisei firmware.
 */

#include <iostream>
#include <chrono>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>

// Enjin2 includes
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/components/planet.hpp>
#include <enjin2/components/satellite.hpp>
#include <enjin2/components/probe.hpp>
#include <enjin2/components/canvas.hpp>
#include <enjin2/core/object_collection.hpp>

// Legacy Enjin includes
#include "../../Libs/enjin/Enjin.h"
#include "../../Libs/enjin/Scene.hpp"
#include "../../Libs/enjin/ObjectCollection.hpp"
#include "../../Libs/enjin/Components/C_Position.hpp"
#include "../../Libs/enjin/Components/C_Planet.hpp"
#include "../../Libs/enjin/Components/C_Satellite.hpp"
#include "../../Libs/enjin/Components/C_Probe.hpp"

// Adafruit GFX for legacy comparison
#include "../../Libs/Adafruit-GFX-Library/Adafruit_GFX.h"

using namespace std;
using namespace chrono;

/**
 * @brief Benchmark configuration
 */
struct BenchmarkConfig {
    static constexpr int CANVAS_WIDTH = 127;
    static constexpr int CANVAS_HEIGHT = 127;
    static constexpr int NUM_FRAMES = 200;
    static constexpr int NUM_SATELLITES = 4;
    static constexpr int NUM_PROBES = 2;
    static constexpr int WARMUP_ITERATIONS = 10;
};

/**
 * @brief Benchmark results structure
 */
struct BenchmarkResults {
    string engineName;
    double avgFrameTime;      // milliseconds
    double peakFrameTime;     // milliseconds
    double minFrameTime;      // milliseconds
    size_t memoryUsage;       // bytes
    double totalTime;         // seconds
    double framesPerSecond;   // fps
    int componentsUsed;       // number of active components
    double memoryEfficiency;  // fps per KB
};

/**
 * @brief Memory usage tracker
 */
class MemoryTracker {
private:
    size_t baseline;
    
public:
    MemoryTracker() {
        // Simple memory estimation - in real embedded system would use actual heap tracking
        baseline = 0;
    }
    
    size_t getCurrentUsage() {
        // Estimate based on typical component sizes
        return baseline;
    }
    
    void addAllocation(size_t size) {
        baseline += size;
    }
};

/**
 * @brief Enjin2 benchmark implementation
 */
class Enjin2Benchmark {
private:
    enjin2::Canvas4<BenchmarkConfig::CANVAS_WIDTH, BenchmarkConfig::CANVAS_HEIGHT> canvas;
    enjin2::ObjectCollection objects;
    enjin2::Scene scene;
    vector<enjin2::Object*> satellites;
    vector<enjin2::Object*> probes;
    enjin2::Object* planet;
    MemoryTracker memTracker;
    
public:
    BenchmarkResults run() {
        cout << "\\n=== Enjin2 Benchmark ===" << endl;
        
        // Setup scene
        setupScene();
        
        vector<double> frameTimes;
        frameTimes.reserve(BenchmarkConfig::NUM_FRAMES);
        
        // Warmup
        for (int i = 0; i < BenchmarkConfig::WARMUP_ITERATIONS; ++i) {
            updateFrame(i);
        }
        
        // Actual benchmark
        auto startTime = high_resolution_clock::now();
        
        for (int frame = 0; frame < BenchmarkConfig::NUM_FRAMES; ++frame) {
            auto frameStart = high_resolution_clock::now();
            
            updateFrame(frame);
            
            auto frameEnd = high_resolution_clock::now();
            double frameTime = duration<double, milli>(frameEnd - frameStart).count();
            frameTimes.push_back(frameTime);
            
            if (frame % 50 == 0) {
                cout << "Enjin2 Frame " << frame << " - " << fixed << setprecision(3) 
                     << frameTime << " ms" << endl;
            }
        }
        
        auto endTime = high_resolution_clock::now();
        double totalTime = duration<double>(endTime - startTime).count();
        
        // Calculate statistics
        BenchmarkResults results;
        results.engineName = "Enjin2";
        results.avgFrameTime = accumulate(frameTimes.begin(), frameTimes.end(), 0.0) / frameTimes.size();
        results.peakFrameTime = *max_element(frameTimes.begin(), frameTimes.end());
        results.minFrameTime = *min_element(frameTimes.begin(), frameTimes.end());
        results.memoryUsage = calculateMemoryUsage();
        results.totalTime = totalTime;
        results.framesPerSecond = BenchmarkConfig::NUM_FRAMES / totalTime;
        results.componentsUsed = satellites.size() + probes.size() + 1; // +1 for planet
        results.memoryEfficiency = results.framesPerSecond / (results.memoryUsage / 1024.0);
        
        return results;
    }
    
private:
    void setupScene() {
        // Create planet
        planet = objects.createObject();
        auto planetPos = planet->addComponent<enjin2::Position>();
        planetPos->x = BenchmarkConfig::CANVAS_WIDTH / 2;
        planetPos->y = BenchmarkConfig::CANVAS_HEIGHT / 2;
        
        auto planetComp = planet->addComponent<enjin2::Planet>();
        planetComp->radius = 20;
        planetComp->showRings = true;
        planetComp->showAtmosphere = true;
        
        memTracker.addAllocation(sizeof(enjin2::Object) + sizeof(enjin2::Position) + sizeof(enjin2::Planet));
        
        // Create satellites
        for (int i = 0; i < BenchmarkConfig::NUM_SATELLITES; ++i) {
            auto sat = objects.createObject();
            auto satPos = sat->addComponent<enjin2::Position>();
            auto satComp = sat->addComponent<enjin2::Satellite>();
            
            satPos->x = BenchmarkConfig::CANVAS_WIDTH / 2;
            satPos->y = BenchmarkConfig::CANVAS_HEIGHT / 2;
            
            satComp->orbitRadius = 30 + i * 8;
            satComp->orbitSpeed = 0.02f + i * 0.01f;
            satComp->showTrail = true;
            satComp->trailLength = 20;
            
            satellites.push_back(sat);
            memTracker.addAllocation(sizeof(enjin2::Object) + sizeof(enjin2::Position) + sizeof(enjin2::Satellite));
        }
        
        // Create probes
        for (int i = 0; i < BenchmarkConfig::NUM_PROBES; ++i) {
            auto probe = objects.createObject();
            auto probePos = probe->addComponent<enjin2::Position>();
            auto probeComp = probe->addComponent<enjin2::Probe>();
            
            probePos->x = 20 + i * 80;
            probePos->y = 20 + i * 40;
            
            probeComp->type = (i == 0) ? enjin2::ProbeType::Scanner : enjin2::ProbeType::Particle;
            probeComp->range = 25;
            probeComp->intensity = 0.8f;
            
            probes.push_back(probe);
            memTracker.addAllocation(sizeof(enjin2::Object) + sizeof(enjin2::Position) + sizeof(enjin2::Probe));
        }
        
        cout << "Enjin2: Created " << satellites.size() << " satellites, " 
             << probes.size() << " probes, 1 planet" << endl;
    }
    
    void updateFrame(int frameNum) {
        // Clear canvas
        canvas.clear();
        
        // Update satellite positions (orbital motion)
        for (size_t i = 0; i < satellites.size(); ++i) {
            auto pos = satellites[i]->getComponent<enjin2::Position>();
            auto sat = satellites[i]->getComponent<enjin2::Satellite>();
            
            float angle = frameNum * sat->orbitSpeed + i * (M_PI / 2);
            pos->x = (BenchmarkConfig::CANVAS_WIDTH / 2) + cos(angle) * sat->orbitRadius;
            pos->y = (BenchmarkConfig::CANVAS_HEIGHT / 2) + sin(angle) * sat->orbitRadius;
        }
        
        // Update probe scanning animation
        for (size_t i = 0; i < probes.size(); ++i) {
            auto probe = probes[i]->getComponent<enjin2::Probe>();
            probe->scanAngle = fmod(frameNum * 0.05f + i * M_PI, 2 * M_PI);
        }
        
        // Render all objects
        objects.updateAll(16.0f); // Simulate 60fps delta time
        
        // Draw to canvas (simulated)
        auto canvasComp = objects.createObject()->addComponent<enjin2::Canvas>();
        canvasComp->canvas = &canvas;
        
        // Simulate drawing operations
        for (int i = 0; i < 50; ++i) {
            canvas.setPixel(rand() % BenchmarkConfig::CANVAS_WIDTH, 
                          rand() % BenchmarkConfig::CANVAS_HEIGHT, 
                          rand() % 16);
        }
    }
    
    size_t calculateMemoryUsage() {
        size_t totalSize = 0;
        
        // Canvas memory
        totalSize += canvas.getMemorySize();
        
        // Object collection overhead
        totalSize += objects.getMemoryUsage();
        
        // Component memory (estimated)
        totalSize += memTracker.getCurrentUsage();
        
        return totalSize;
    }
};

/**
 * @brief Legacy Enjin benchmark implementation
 */
class LegacyEnjinBenchmark {
private:
    GFXcanvas8 canvas;
    enjin::ObjectCollection objects;
    enjin::Scene scene;
    vector<shared_ptr<enjin::Object>> satellites;
    vector<shared_ptr<enjin::Object>> probes;
    shared_ptr<enjin::Object> planet;
    MemoryTracker memTracker;
    
public:
    LegacyEnjinBenchmark() : canvas(BenchmarkConfig::CANVAS_WIDTH, BenchmarkConfig::CANVAS_HEIGHT) {
    }
    
    BenchmarkResults run() {
        cout << "\\n=== Legacy Enjin Benchmark ===" << endl;
        
        // Setup scene
        setupScene();
        
        vector<double> frameTimes;
        frameTimes.reserve(BenchmarkConfig::NUM_FRAMES);
        
        // Warmup
        for (int i = 0; i < BenchmarkConfig::WARMUP_ITERATIONS; ++i) {
            updateFrame(i);
        }
        
        // Actual benchmark
        auto startTime = high_resolution_clock::now();
        
        for (int frame = 0; frame < BenchmarkConfig::NUM_FRAMES; ++frame) {
            auto frameStart = high_resolution_clock::now();
            
            updateFrame(frame);
            
            auto frameEnd = high_resolution_clock::now();
            double frameTime = duration<double, milli>(frameEnd - frameStart).count();
            frameTimes.push_back(frameTime);
            
            if (frame % 50 == 0) {
                cout << "Legacy Frame " << frame << " - " << fixed << setprecision(3) 
                     << frameTime << " ms" << endl;
            }
        }
        
        auto endTime = high_resolution_clock::now();
        double totalTime = duration<double>(endTime - startTime).count();
        
        // Calculate statistics
        BenchmarkResults results;
        results.engineName = "Legacy Enjin";
        results.avgFrameTime = accumulate(frameTimes.begin(), frameTimes.end(), 0.0) / frameTimes.size();
        results.peakFrameTime = *max_element(frameTimes.begin(), frameTimes.end());
        results.minFrameTime = *min_element(frameTimes.begin(), frameTimes.end());
        results.memoryUsage = calculateMemoryUsage();
        results.totalTime = totalTime;
        results.framesPerSecond = BenchmarkConfig::NUM_FRAMES / totalTime;
        results.componentsUsed = satellites.size() + probes.size() + 1;
        results.memoryEfficiency = results.framesPerSecond / (results.memoryUsage / 1024.0);
        
        return results;
    }
    
private:
    void setupScene() {
        // Create planet
        planet = objects.CreateObject();
        auto planetPos = planet->AddComponent<enjin::C_Position>();
        planetPos->position.x = BenchmarkConfig::CANVAS_WIDTH / 2;
        planetPos->position.y = BenchmarkConfig::CANVAS_HEIGHT / 2;
        
        auto planetComp = planet->AddComponent<enjin::C_Planet>();
        planetComp->radius = 20;
        planetComp->ring_visible = true;
        
        memTracker.addAllocation(sizeof(enjin::Object) + sizeof(enjin::C_Position) + sizeof(enjin::C_Planet));
        
        // Create satellites
        for (int i = 0; i < BenchmarkConfig::NUM_SATELLITES; ++i) {
            auto sat = objects.CreateObject();
            auto satPos = sat->AddComponent<enjin::C_Position>();
            auto satComp = sat->AddComponent<enjin::C_Satellite>();
            
            satPos->position.x = BenchmarkConfig::CANVAS_WIDTH / 2;
            satPos->position.y = BenchmarkConfig::CANVAS_HEIGHT / 2;
            
            // Legacy satellite setup
            satComp->orbit_radius = 30 + i * 8;
            satComp->orbit_speed = 0.02f + i * 0.01f;
            satComp->show_trail = true;
            
            satellites.push_back(sat);
            memTracker.addAllocation(sizeof(enjin::Object) + sizeof(enjin::C_Position) + sizeof(enjin::C_Satellite));
        }
        
        // Create probes
        for (int i = 0; i < BenchmarkConfig::NUM_PROBES; ++i) {
            auto probe = objects.CreateObject();
            auto probePos = probe->AddComponent<enjin::C_Position>();
            auto probeComp = probe->AddComponent<enjin::C_Probe>();
            
            probePos->position.x = 20 + i * 80;
            probePos->position.y = 20 + i * 40;
            
            probes.push_back(probe);
            memTracker.addAllocation(sizeof(enjin::Object) + sizeof(enjin::C_Position) + sizeof(enjin::C_Probe));
        }
        
        cout << "Legacy: Created " << satellites.size() << " satellites, " 
             << probes.size() << " probes, 1 planet" << endl;
    }
    
    void updateFrame(int frameNum) {
        // Clear canvas
        canvas.fillScreen(0);
        
        // Update satellite positions
        for (size_t i = 0; i < satellites.size(); ++i) {
            auto pos = satellites[i]->GetComponent<enjin::C_Position>();
            auto sat = satellites[i]->GetComponent<enjin::C_Satellite>();
            
            float angle = frameNum * sat->orbit_speed + i * (M_PI / 2);
            pos->position.x = (BenchmarkConfig::CANVAS_WIDTH / 2) + cos(angle) * sat->orbit_radius;
            pos->position.y = (BenchmarkConfig::CANVAS_HEIGHT / 2) + sin(angle) * sat->orbit_radius;
        }
        
        // Update objects
        objects.UpdateAll();
        
        // Simulate drawing operations
        for (int i = 0; i < 50; ++i) {
            canvas.drawPixel(rand() % BenchmarkConfig::CANVAS_WIDTH, 
                           rand() % BenchmarkConfig::CANVAS_HEIGHT, 
                           rand() % 256);
        }
    }
    
    size_t calculateMemoryUsage() {
        size_t totalSize = 0;
        
        // Canvas memory (8-bit)
        totalSize += BenchmarkConfig::CANVAS_WIDTH * BenchmarkConfig::CANVAS_HEIGHT;
        
        // shared_ptr overhead + objects
        totalSize += (satellites.size() + probes.size() + 1) * (sizeof(shared_ptr<enjin::Object>) + sizeof(enjin::Object));
        
        // Component memory
        totalSize += memTracker.getCurrentUsage();
        
        return totalSize;
    }
};

/**
 * @brief Print benchmark comparison results
 */
void printComparison(const BenchmarkResults& enjin2, const BenchmarkResults& legacy) {
    cout << "\\n=== PERFORMANCE COMPARISON ===" << endl;
    cout << string(80, '=') << endl;
    
    cout << left << setw(25) << "Metric" 
         << setw(15) << "Enjin2" 
         << setw(15) << "Legacy" 
         << setw(15) << "Ratio" 
         << "Winner" << endl;
    cout << string(80, '-') << endl;
    
    auto printMetric = [](const string& name, double enjin2Val, double legacyVal, 
                         const string& unit, bool lowerIsBetter = true) {
        double ratio = legacyVal / enjin2Val;
        string winner = (lowerIsBetter ? (enjin2Val < legacyVal) : (enjin2Val > legacyVal)) ? "Enjin2" : "Legacy";
        
        cout << left << setw(25) << name
             << setw(15) << (to_string(enjin2Val) + unit)
             << setw(15) << (to_string(legacyVal) + unit)
             << setw(15) << (to_string(ratio) + "x")
             << winner << endl;
    };
    
    printMetric("Avg Frame Time", enjin2.avgFrameTime, legacy.avgFrameTime, "ms", true);
    printMetric("Peak Frame Time", enjin2.peakFrameTime, legacy.peakFrameTime, "ms", true);
    printMetric("Min Frame Time", enjin2.minFrameTime, legacy.minFrameTime, "ms", true);
    printMetric("Memory Usage", enjin2.memoryUsage / 1024.0, legacy.memoryUsage / 1024.0, "KB", true);
    printMetric("Frames Per Second", enjin2.framesPerSecond, legacy.framesPerSecond, "", false);
    printMetric("Memory Efficiency", enjin2.memoryEfficiency, legacy.memoryEfficiency, "fps/KB", false);
    
    cout << string(80, '=') << endl;
    
    // Summary
    double memoryReduction = ((double)(legacy.memoryUsage - enjin2.memoryUsage) / legacy.memoryUsage) * 100;
    double performanceRatio = enjin2.framesPerSecond / legacy.framesPerSecond;
    
    cout << "\\n=== SUMMARY ===" << endl;
    cout << "Memory Reduction: " << fixed << setprecision(1) << memoryReduction << "%" << endl;
    cout << "Performance Ratio: " << fixed << setprecision(2) << performanceRatio << "x ";
    cout << (performanceRatio > 1.0 ? "(Enjin2 faster)" : "(Legacy faster)") << endl;
    cout << "Memory Saved: " << (legacy.memoryUsage - enjin2.memoryUsage) / 1024.0 << " KB" << endl;
    
    if (memoryReduction > 20 && performanceRatio > 0.5) {
        cout << "\\n✅ RESULT: Enjin2 provides significant memory benefits with acceptable performance" << endl;
    } else if (performanceRatio > 1.2) {
        cout << "\\n🚀 RESULT: Enjin2 provides both memory and performance benefits" << endl;
    } else {
        cout << "\\n⚠️  RESULT: Trade-offs present - evaluate based on application needs" << endl;
    }
}

/**
 * @brief Main benchmark execution
 */
int main() {
    cout << "Enjin vs Enjin2 Performance Benchmark" << endl;
    cout << "=====================================" << endl;
    cout << "Canvas Size: " << BenchmarkConfig::CANVAS_WIDTH << "x" << BenchmarkConfig::CANVAS_HEIGHT << endl;
    cout << "Frames: " << BenchmarkConfig::NUM_FRAMES << endl;
    cout << "Components: " << BenchmarkConfig::NUM_SATELLITES << " satellites, " 
         << BenchmarkConfig::NUM_PROBES << " probes, 1 planet" << endl;
    
    try {
        // Run Enjin2 benchmark
        Enjin2Benchmark enjin2Bench;
        auto enjin2Results = enjin2Bench.run();
        
        // Run Legacy Enjin benchmark
        LegacyEnjinBenchmark legacyBench;
        auto legacyResults = legacyBench.run();
        
        // Print comparison
        printComparison(enjin2Results, legacyResults);
        
    } catch (const exception& e) {
        cerr << "Benchmark failed: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}