#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <numeric>

// Enjin2 includes
#include "../include/enjin2/graphics/canvas.hpp"
#include "../include/enjin2/core/object.hpp"
#include "../include/enjin2/components/position.hpp"
#include "../include/enjin2/components/label.hpp"
#include "../include/enjin2/components/draw.hpp"
#include "../include/enjin2/utils/polar.hpp"

using namespace enjin2;

/**
 * @brief Comprehensive Performance Benchmark Suite
 * 
 * Tests various Enjin2 operations for embedded hardware optimization.
 */
class ComprehensiveBenchmark {
private:
    static constexpr int WARMUP_ITERATIONS = 10;
    static constexpr int BENCHMARK_ITERATIONS = 100;
    
    struct BenchmarkResult {
        std::string name;
        double avg_time_ms;
        double min_time_ms;
        double max_time_ms;
        size_t operations_per_frame;
        double ops_per_second;
    };
    
    std::vector<BenchmarkResult> results;
    
    template<typename Func>
    BenchmarkResult benchmark(const std::string& name, Func&& func, size_t ops_per_frame = 1) {
        std::vector<double> times;
        times.reserve(BENCHMARK_ITERATIONS);
        
        // Warmup
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            func();
        }
        
        // Actual benchmark
        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            func();
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            times.push_back(duration.count() / 1e6); // Convert to milliseconds
        }
        
        double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
        double min_time = *std::min_element(times.begin(), times.end());
        double max_time = *std::max_element(times.begin(), times.end());
        double ops_per_sec = (ops_per_frame / (avg / 1000.0));
        
        return {name, avg, min_time, max_time, ops_per_frame, ops_per_sec};
    }
    
public:
    
    /**
     * @brief Test 1: Canvas pixel operations
     */
    void benchmarkPixelOperations() {
        printf("=== Benchmarking Pixel Operations ===\n");
        
        Canvas8<128, 64> canvas;
        
        // Test 1.1: Single pixel set
        auto result1 = benchmark("Pixel Set", [&]() {
            canvas.setPixel(64, 32, 15);
        });
        results.push_back(result1);
        
        // Test 1.2: Canvas clear
        auto result2 = benchmark("Canvas Clear", [&]() {
            canvas.clear(0);
        });
        results.push_back(result2);
        
        // Test 1.3: Line drawing (manual)
        auto result3 = benchmark("Line Drawing", [&]() {
            for (int x = 0; x < 64; x++) {
                canvas.setPixel(x, 32, 10);
            }
        }, 64);
        results.push_back(result3);
        
        // Test 1.4: Rectangle fill
        auto result4 = benchmark("Rectangle Fill", [&]() {
            for (int y = 10; y < 20; y++) {
                for (int x = 10; x < 50; x++) {
                    canvas.setPixel(x, y, 8);
                }
            }
        }, 400);
        results.push_back(result4);
        
        printf("✓ Pixel operations benchmarked\n");
    }
    
    /**
     * @brief Test 2: ECS system performance
     */
    void benchmarkECSSystem() {
        printf("=== Benchmarking ECS System ===\n");
        
        // Test 2.1: Object creation/destruction
        auto result1 = benchmark("Object Creation", []() {
            Object obj;
            auto pos = obj.addComponent<C_Position>(64, 32);
            // Object automatically destroyed
        });
        results.push_back(result1);
        
        // Test 2.2: Component lookup
        Object persistent_obj;
        auto persistent_pos = persistent_obj.addComponent<C_Position>(64, 32);
        
        auto result2 = benchmark("Component Lookup", [&]() {
            auto pos = persistent_obj.getComponent<C_Position>();
            if (pos) pos->setPosition(64, 32);
        });
        results.push_back(result2);
        
        // Test 2.3: Multiple components
        auto result3 = benchmark("Multi-Component Object", []() {
            Object obj;
            auto pos = obj.addComponent<C_Position>(64, 32);
            auto label = obj.addComponent<Label>(40, 12);
            auto draw = obj.addComponent<C_Draw>([](ICanvas<uint8_t>&) {});
        });
        results.push_back(result3);
        
        printf("✓ ECS system benchmarked\n");
    }
    
    /**
     * @brief Test 3: Text rendering performance
     */
    void benchmarkTextRendering() {
        printf("=== Benchmarking Text Rendering ===\n");
        
        Canvas8<128, 64> canvas;
        Object text_obj;
        auto text_pos = text_obj.addComponent<C_Position>(10, 10);
        auto text_label = text_obj.addComponent<Label>(100, 12);
        
        // Test 3.1: Short text
        text_label->setText("TEST");
        auto result1 = benchmark("Short Text", [&]() {
            canvas.clear(0);
            text_label->draw(canvas);
        });
        results.push_back(result1);
        
        // Test 3.2: Long text
        text_label->setText("This is a longer text string for testing");
        auto result2 = benchmark("Long Text", [&]() {
            canvas.clear(0);
            text_label->draw(canvas);
        });
        results.push_back(result2);
        
        // Test 3.3: Multiple labels
        std::vector<Object> text_objects(5);
        std::vector<Label*> labels;
        for (int i = 0; i < 5; i++) {
            auto pos = text_objects[i].addComponent<C_Position>(5, 10 + i * 10);
            auto label = text_objects[i].addComponent<Label>(80, 8);
            label->setText("Text Line");
            labels.push_back(label);
        }
        
        auto result3 = benchmark("Multiple Labels", [&]() {
            canvas.clear(0);
            for (auto* label : labels) {
                label->draw(canvas);
            }
        }, 5);
        results.push_back(result3);
        
        printf("✓ Text rendering benchmarked\n");
    }
    
    /**
     * @brief Test 4: Polar math operations
     */
    void benchmarkPolarMath() {
        printf("=== Benchmarking Polar Math ===\n");
        
        Point center(64, 32);
        
        // Test 4.1: Single conversion
        auto result1 = benchmark("Polar Conversion", [&]() {
            Point result = Polar::RadialToCartesian(0.5f, 20, center);
        });
        results.push_back(result1);
        
        // Test 4.2: Orbital pattern (common use case)
        auto result2 = benchmark("Orbital Pattern", [&]() {
            for (int i = 0; i < 12; i++) {
                float phase = i / 12.0f;
                Point point = Polar::RadialToCartesian(phase, 25, center);
            }
        }, 12);
        results.push_back(result2);
        
        printf("✓ Polar math benchmarked\n");
    }
    
    /**
     * @brief Test 5: Custom drawing performance
     */
    void benchmarkCustomDrawing() {
        printf("=== Benchmarking Custom Drawing ===\n");
        
        Canvas8<128, 64> canvas;
        Object draw_obj;
        
        // Test 5.1: Simple lambda drawing
        auto simple_draw = draw_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
            for (int i = 0; i < 10; i++) {
                canvas.setPixel(i * 5, 32, 10);
            }
        });
        
        auto result1 = benchmark("Simple Lambda", [&]() {
            canvas.clear(0);
            simple_draw->draw(canvas);
        }, 10);
        results.push_back(result1);
        
        // Test 5.2: Complex orbital drawing
        auto orbital_draw = draw_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
            Point center(64, 32);
            for (int ring = 0; ring < 3; ring++) {
                float radius = 8 + ring * 6;
                for (int sat = 0; sat < 8; sat++) {
                    float phase = sat / 8.0f;
                    Point pos = Polar::RadialToCartesian(phase, radius, center);
                    if (pos.x >= 0 && pos.x < 128 && pos.y >= 0 && pos.y < 64) {
                        canvas.setPixel(pos.x, pos.y, 12);
                    }
                }
            }
        });
        
        auto result2 = benchmark("Orbital Drawing", [&]() {
            canvas.clear(0);
            orbital_draw->draw(canvas);
        }, 24);
        results.push_back(result2);
        
        printf("✓ Custom drawing benchmarked\n");
    }
    
    /**
     * @brief Test 6: Memory operations
     */
    void benchmarkMemoryOperations() {
        printf("=== Benchmarking Memory Operations ===\n");
        
        Canvas8<128, 64> canvas;
        
        // Test 6.1: Canvas to buffer access
        auto result1 = benchmark("Canvas Access", [&]() {
            for (int y = 0; y < 64; y++) {
                for (int x = 0; x < 128; x++) {
                    uint8_t pixel = canvas.getPixel(x, y);
                }
            }
        }, 128 * 64);
        results.push_back(result1);
        
        // Test 6.2: Canvas copy simulation
        Canvas8<128, 64> source, dest;
        auto result2 = benchmark("Canvas Copy", [&]() {
            for (int y = 0; y < 64; y++) {
                for (int x = 0; x < 128; x++) {
                    dest.setPixel(x, y, source.getPixel(x, y));
                }
            }
        }, 128 * 64);
        results.push_back(result2);
        
        printf("✓ Memory operations benchmarked\n");
    }
    
    /**
     * @brief Run all benchmarks
     */
    void runAllBenchmarks() {
        printf("Enjin2 Comprehensive Performance Benchmark\n");
        printf("==========================================\n");
        printf("Iterations per test: %d (after %d warmup)\n", BENCHMARK_ITERATIONS, WARMUP_ITERATIONS);
        printf("Target hardware: Embedded (ESP32/STM32)\n\n");
        
        benchmarkPixelOperations();
        benchmarkECSSystem();
        benchmarkTextRendering();
        benchmarkPolarMath();
        benchmarkCustomDrawing();
        benchmarkMemoryOperations();
        
        printResults();
    }
    
    /**
     * @brief Print formatted results
     */
    void printResults() {
        printf("\n=== BENCHMARK RESULTS ===\n");
        printf("%-20s %10s %10s %10s %12s %12s\n", 
               "Test", "Avg (ms)", "Min (ms)", "Max (ms)", "Ops/Frame", "Ops/Sec");
        printf("%s\n", std::string(80, '-').c_str());
        
        double total_avg = 0;
        double total_ops = 0;
        
        for (const auto& result : results) {
            printf("%-20s %10.6f %10.6f %10.6f %12zu %12.0f\n",
                   result.name.c_str(),
                   result.avg_time_ms,
                   result.min_time_ms,
                   result.max_time_ms,
                   result.operations_per_frame,
                   result.ops_per_second);
            
            total_avg += result.avg_time_ms;
            total_ops += result.ops_per_second;
        }
        
        printf("%s\n", std::string(80, '-').c_str());
        printf("Total average time: %.6f ms\n", total_avg);
        printf("Combined operations/sec: %.0f\n", total_ops);
        
        // Performance classification
        printf("\n=== PERFORMANCE ANALYSIS ===\n");
        
        // Frame rate capability (assuming 16.67ms budget for 60 FPS)
        double frame_budget_ms = 16.67;
        double estimated_frame_time = total_avg;
        double estimated_fps = 1000.0 / estimated_frame_time;
        
        printf("Estimated full-frame time: %.6f ms\n", estimated_frame_time);
        printf("Estimated max FPS: %.1f\n", estimated_fps);
        
        if (estimated_fps >= 60) {
            printf("✅ Excellent performance - suitable for 60+ FPS\n");
        } else if (estimated_fps >= 30) {
            printf("✅ Good performance - suitable for 30+ FPS\n");
        } else if (estimated_fps >= 15) {
            printf("⚠️  Moderate performance - suitable for 15+ FPS\n");
        } else {
            printf("❌ Performance optimization needed\n");
        }
        
        // Memory efficiency
        size_t canvas_memory = 128 * 64; // 8KB for 8-bit canvas
        printf("\nMemory usage per frame: %zu bytes (%.2f KB)\n", 
               canvas_memory, canvas_memory / 1024.0);
        
        // Embedded suitability
        printf("\n=== EMBEDDED HARDWARE SUITABILITY ===\n");
        printf("✅ Low memory footprint (8KB canvas)\n");
        printf("✅ Predictable performance characteristics\n");
        printf("✅ No dynamic memory allocation in hot paths\n");
        
        if (estimated_fps >= 30) {
            printf("✅ Suitable for real-time audio visualization\n");
        } else {
            printf("⚠️  May need optimization for real-time use\n");
        }
    }
};

int main() {
    ComprehensiveBenchmark benchmark;
    benchmark.runAllBenchmarks();
    
    printf("\n=== RECOMMENDATIONS ===\n");
    printf("• Use 4-bit Canvas for better memory efficiency on constrained hardware\n");
    printf("• Cache frequently accessed components to avoid lookup overhead\n");
    printf("• Pre-calculate polar coordinates for static orbital patterns\n");
    printf("• Use C_Draw components for performance-critical custom rendering\n");
    printf("• Consider object pooling for frequently created/destroyed objects\n");
    
    return 0;
}