/**
 * @file packing_overhead_benchmark.cpp
 * @brief Direct comparison of Canvas8 vs Canvas4 to measure packing overhead
 * 
 * Tests identical operations on both canvas types to isolate the exact
 * performance cost of 4-bit packing/unpacking operations.
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <numeric>

#include "../include/enjin2/graphics/canvas.hpp"

using namespace std;
using namespace chrono;
using namespace enjin2;

/**
 * @brief Benchmark configuration
 */
struct PackingBenchConfig {
    static constexpr int CANVAS_WIDTH = 128;
    static constexpr int CANVAS_HEIGHT = 64;
    static constexpr int ITERATIONS = 10000;
    static constexpr int WARMUP = 1000;
};

/**
 * @brief Benchmark results structure
 */
struct BenchmarkResults {
    string canvasType;
    double setPixelTime;     // nanoseconds per operation
    double getPixelTime;     // nanoseconds per operation
    double clearTime;        // nanoseconds per operation
    double fillRectTime;     // nanoseconds per operation
    double hlineTime;        // nanoseconds per operation
    size_t memoryUsage;      // bytes
    double memoryEfficiency; // ops per KB
};

/**
 * @brief Canvas8 benchmark (no packing overhead)
 */
class Canvas8Benchmark {
private:
    Canvas8<PackingBenchConfig::CANVAS_WIDTH, PackingBenchConfig::CANVAS_HEIGHT> canvas;
    
public:
    BenchmarkResults run() {
        cout << "\\n=== Canvas8 Performance (No Packing) ===" << endl;
        
        BenchmarkResults results;
        results.canvasType = "Canvas8 (no packing)";
        
        // Test setPixel performance
        results.setPixelTime = benchmarkSetPixel();
        
        // Test getPixel performance
        results.getPixelTime = benchmarkGetPixel();
        
        // Test clear performance
        results.clearTime = benchmarkClear();
        
        // Test fillRect performance
        results.fillRectTime = benchmarkFillRect();
        
        // Test horizontal line performance
        results.hlineTime = benchmarkHLine();
        
        // Calculate memory usage
        results.memoryUsage = PackingBenchConfig::CANVAS_WIDTH * PackingBenchConfig::CANVAS_HEIGHT;
        results.memoryEfficiency = (1.0 / results.setPixelTime) / (results.memoryUsage / 1024.0);
        
        return results;
    }
    
private:
    double benchmarkSetPixel() {
        vector<double> times;
        times.reserve(PackingBenchConfig::ITERATIONS);
        
        // Warmup
        for (int i = 0; i < PackingBenchConfig::WARMUP; ++i) {
            canvas.setPixel(i % PackingBenchConfig::CANVAS_WIDTH, 
                          (i / PackingBenchConfig::CANVAS_WIDTH) % PackingBenchConfig::CANVAS_HEIGHT, 
                          i % 256);
        }
        
        // Benchmark
        for (int i = 0; i < PackingBenchConfig::ITERATIONS; ++i) {
            auto start = high_resolution_clock::now();
            
            canvas.setPixel(i % PackingBenchConfig::CANVAS_WIDTH, 
                          (i / PackingBenchConfig::CANVAS_WIDTH) % PackingBenchConfig::CANVAS_HEIGHT, 
                          i % 256);
                          
            auto end = high_resolution_clock::now();
            times.push_back(duration<double, nano>(end - start).count());
        }
        
        double avgTime = accumulate(times.begin(), times.end(), 0.0) / times.size();
        cout << "Canvas8 setPixel: " << fixed << setprecision(2) << avgTime << " ns" << endl;
        return avgTime;
    }
    
    double benchmarkGetPixel() {
        // Fill canvas first
        for (int y = 0; y < PackingBenchConfig::CANVAS_HEIGHT; ++y) {
            for (int x = 0; x < PackingBenchConfig::CANVAS_WIDTH; ++x) {
                canvas.setPixel(x, y, (x + y) % 256);
            }
        }
        
        vector<double> times;
        times.reserve(PackingBenchConfig::ITERATIONS);
        volatile uint8_t dummy = 0; // Prevent optimization
        
        // Warmup
        for (int i = 0; i < PackingBenchConfig::WARMUP; ++i) {
            dummy += canvas.getPixel(i % PackingBenchConfig::CANVAS_WIDTH, 
                                   (i / PackingBenchConfig::CANVAS_WIDTH) % PackingBenchConfig::CANVAS_HEIGHT);
        }
        
        // Benchmark
        for (int i = 0; i < PackingBenchConfig::ITERATIONS; ++i) {
            auto start = high_resolution_clock::now();
            
            dummy += canvas.getPixel(i % PackingBenchConfig::CANVAS_WIDTH, 
                                   (i / PackingBenchConfig::CANVAS_WIDTH) % PackingBenchConfig::CANVAS_HEIGHT);
                          
            auto end = high_resolution_clock::now();
            times.push_back(duration<double, nano>(end - start).count());
        }
        
        double avgTime = accumulate(times.begin(), times.end(), 0.0) / times.size();
        cout << "Canvas8 getPixel: " << fixed << setprecision(2) << avgTime << " ns (dummy: " << (int)dummy << ")" << endl;
        return avgTime;
    }
    
    double benchmarkClear() {
        vector<double> times;
        times.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            auto start = high_resolution_clock::now();
            canvas.clear(i % 256);
            auto end = high_resolution_clock::now();
            times.push_back(duration<double, nano>(end - start).count());
        }
        
        double avgTime = accumulate(times.begin(), times.end(), 0.0) / times.size();
        cout << "Canvas8 clear: " << fixed << setprecision(0) << avgTime << " ns" << endl;
        return avgTime;
    }
    
    double benchmarkFillRect() {
        vector<double> times;
        times.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            auto start = high_resolution_clock::now();
            canvas.fill(Rect(10, 10, 50, 30), i % 256);
            auto end = high_resolution_clock::now();
            times.push_back(duration<double, nano>(end - start).count());
        }
        
        double avgTime = accumulate(times.begin(), times.end(), 0.0) / times.size();
        cout << "Canvas8 fill: " << fixed << setprecision(0) << avgTime << " ns" << endl;
        return avgTime;
    }
    
    double benchmarkHLine() {
        vector<double> times;
        times.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            auto start = high_resolution_clock::now();
            // Simulate horizontal line with individual pixels (Canvas8 doesn't have drawHLine)
            for (int x = 0; x < PackingBenchConfig::CANVAS_WIDTH; ++x) {
                canvas.setPixel(x, i % PackingBenchConfig::CANVAS_HEIGHT, i % 256);
            }
            auto end = high_resolution_clock::now();
            times.push_back(duration<double, nano>(end - start).count());
        }
        
        double avgTime = accumulate(times.begin(), times.end(), 0.0) / times.size();
        cout << "Canvas8 hline simulation: " << fixed << setprecision(0) << avgTime << " ns" << endl;
        return avgTime;
    }
};

/**
 * @brief Canvas4 benchmark (with packing overhead)
 */
class Canvas4Benchmark {
private:
    Canvas4<PackingBenchConfig::CANVAS_WIDTH, PackingBenchConfig::CANVAS_HEIGHT> canvas;
    
public:
    BenchmarkResults run() {
        cout << "\\n=== Canvas4 Performance (With Packing) ===" << endl;
        
        BenchmarkResults results;
        results.canvasType = "Canvas4 (with packing)";
        
        // Test setPixel performance
        results.setPixelTime = benchmarkSetPixel();
        
        // Test getPixel performance
        results.getPixelTime = benchmarkGetPixel();
        
        // Test clear performance
        results.clearTime = benchmarkClear();
        
        // Test fillRect performance
        results.fillRectTime = benchmarkFillRect();
        
        // Test horizontal line performance
        results.hlineTime = benchmarkHLine();
        
        // Calculate memory usage
        results.memoryUsage = (PackingBenchConfig::CANVAS_WIDTH * PackingBenchConfig::CANVAS_HEIGHT) / 2;
        results.memoryEfficiency = (1.0 / results.setPixelTime) / (results.memoryUsage / 1024.0);
        
        return results;
    }
    
private:
    double benchmarkSetPixel() {
        vector<double> times;
        times.reserve(PackingBenchConfig::ITERATIONS);
        
        // Warmup
        for (int i = 0; i < PackingBenchConfig::WARMUP; ++i) {
            canvas.setPixel(i % PackingBenchConfig::CANVAS_WIDTH, 
                          (i / PackingBenchConfig::CANVAS_WIDTH) % PackingBenchConfig::CANVAS_HEIGHT, 
                          Pixel4(i % 16));
        }
        
        // Benchmark
        for (int i = 0; i < PackingBenchConfig::ITERATIONS; ++i) {
            auto start = high_resolution_clock::now();
            
            canvas.setPixel(i % PackingBenchConfig::CANVAS_WIDTH, 
                          (i / PackingBenchConfig::CANVAS_WIDTH) % PackingBenchConfig::CANVAS_HEIGHT, 
                          Pixel4(i % 16));
                          
            auto end = high_resolution_clock::now();
            times.push_back(duration<double, nano>(end - start).count());
        }
        
        double avgTime = accumulate(times.begin(), times.end(), 0.0) / times.size();
        cout << "Canvas4 setPixel: " << fixed << setprecision(2) << avgTime << " ns" << endl;
        return avgTime;
    }
    
    double benchmarkGetPixel() {
        // Fill canvas first
        for (int y = 0; y < PackingBenchConfig::CANVAS_HEIGHT; ++y) {
            for (int x = 0; x < PackingBenchConfig::CANVAS_WIDTH; ++x) {
                canvas.setPixel(x, y, Pixel4((x + y) % 16));
            }
        }
        
        vector<double> times;
        times.reserve(PackingBenchConfig::ITERATIONS);
        volatile uint8_t dummy = 0; // Prevent optimization
        
        // Warmup
        for (int i = 0; i < PackingBenchConfig::WARMUP; ++i) {
            dummy += canvas.getPixel(i % PackingBenchConfig::CANVAS_WIDTH, 
                                   (i / PackingBenchConfig::CANVAS_WIDTH) % PackingBenchConfig::CANVAS_HEIGHT);
        }
        
        // Benchmark
        for (int i = 0; i < PackingBenchConfig::ITERATIONS; ++i) {
            auto start = high_resolution_clock::now();
            
            dummy += canvas.getPixel(i % PackingBenchConfig::CANVAS_WIDTH, 
                                   (i / PackingBenchConfig::CANVAS_WIDTH) % PackingBenchConfig::CANVAS_HEIGHT);
                          
            auto end = high_resolution_clock::now();
            times.push_back(duration<double, nano>(end - start).count());
        }
        
        double avgTime = accumulate(times.begin(), times.end(), 0.0) / times.size();
        cout << "Canvas4 getPixel: " << fixed << setprecision(2) << avgTime << " ns (dummy: " << (int)dummy << ")" << endl;
        return avgTime;
    }
    
    double benchmarkClear() {
        vector<double> times;
        times.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            auto start = high_resolution_clock::now();
            canvas.clear(Pixel4(i % 16));
            auto end = high_resolution_clock::now();
            times.push_back(duration<double, nano>(end - start).count());
        }
        
        double avgTime = accumulate(times.begin(), times.end(), 0.0) / times.size();
        cout << "Canvas4 clear: " << fixed << setprecision(0) << avgTime << " ns" << endl;
        return avgTime;
    }
    
    double benchmarkFillRect() {
        vector<double> times;
        times.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            auto start = high_resolution_clock::now();
            canvas.fillRect(10, 10, 50, 30, Pixel4(i % 16));
            auto end = high_resolution_clock::now();
            times.push_back(duration<double, nano>(end - start).count());
        }
        
        double avgTime = accumulate(times.begin(), times.end(), 0.0) / times.size();
        cout << "Canvas4 fillRect: " << fixed << setprecision(0) << avgTime << " ns" << endl;
        return avgTime;
    }
    
    double benchmarkHLine() {
        vector<double> times;
        times.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            auto start = high_resolution_clock::now();
            canvas.drawHLine(0, i % PackingBenchConfig::CANVAS_HEIGHT, PackingBenchConfig::CANVAS_WIDTH, Pixel4(i % 16));
            auto end = high_resolution_clock::now();
            times.push_back(duration<double, nano>(end - start).count());
        }
        
        double avgTime = accumulate(times.begin(), times.end(), 0.0) / times.size();
        cout << "Canvas4 drawHLine: " << fixed << setprecision(0) << avgTime << " ns" << endl;
        return avgTime;
    }
};

/**
 * @brief Print detailed comparison results
 */
void printComparison(const BenchmarkResults& canvas8, const BenchmarkResults& canvas4) {
    cout << "\\n=== PACKING OVERHEAD ANALYSIS ===" << endl;
    cout << string(100, '=') << endl;
    
    cout << left << setw(20) << "Operation" 
         << setw(15) << "Canvas8 (ns)"
         << setw(15) << "Canvas4 (ns)"
         << setw(15) << "Overhead"
         << setw(15) << "Slowdown"
         << "Analysis" << endl;
    cout << string(100, '-') << endl;
    
    auto printRow = [](const string& op, double canvas8Time, double canvas4Time) {
        double overhead = canvas4Time - canvas8Time;
        double slowdown = canvas4Time / canvas8Time;
        
        cout << left << setw(20) << op
             << setw(15) << fixed << setprecision(2) << canvas8Time
             << setw(15) << fixed << setprecision(2) << canvas4Time
             << setw(15) << fixed << setprecision(2) << overhead
             << setw(15) << fixed << setprecision(2) << slowdown << "x";
        
        if (slowdown < 1.5) {
            cout << " ✅ Low overhead";
        } else if (slowdown < 2.0) {
            cout << " ⚠️ Moderate overhead";
        } else {
            cout << " ❌ High overhead";
        }
        cout << endl;
    };
    
    printRow("setPixel", canvas8.setPixelTime, canvas4.setPixelTime);
    printRow("getPixel", canvas8.getPixelTime, canvas4.getPixelTime);
    printRow("clear", canvas8.clearTime, canvas4.clearTime);
    printRow("fillRect", canvas8.fillRectTime, canvas4.fillRectTime);
    printRow("drawHLine", canvas8.hlineTime, canvas4.hlineTime);
    
    cout << string(100, '=') << endl;
    
    // Memory analysis
    double memoryReduction = ((double)(canvas8.memoryUsage - canvas4.memoryUsage) / canvas8.memoryUsage) * 100;
    double avgSlowdown = (canvas4.setPixelTime + canvas4.getPixelTime) / (canvas8.setPixelTime + canvas8.getPixelTime);
    
    cout << "\\n=== MEMORY VS PERFORMANCE TRADE-OFF ===" << endl;
    cout << "Canvas8 memory usage: " << canvas8.memoryUsage << " bytes (" << (canvas8.memoryUsage/1024.0) << " KB)" << endl;
    cout << "Canvas4 memory usage: " << canvas4.memoryUsage << " bytes (" << (canvas4.memoryUsage/1024.0) << " KB)" << endl;
    cout << "Memory reduction: " << fixed << setprecision(1) << memoryReduction << "%" << endl;
    cout << "Average performance cost: " << fixed << setprecision(2) << avgSlowdown << "x" << endl;
    
    cout << "\\n=== RECOMMENDATIONS ===" << endl;
    
    if (avgSlowdown < 1.5) {
        cout << "✅ **Use Canvas4**: Low performance overhead, significant memory savings" << endl;
    } else if (avgSlowdown < 2.0) {
        cout << "⚖️  **Context-dependent**: Consider memory constraints vs performance needs" << endl;
    } else {
        cout << "⚠️  **Consider Canvas8**: High performance overhead may not justify memory savings" << endl;
    }
    
    cout << "\\n=== TARGET USE CASES ===" << endl;
    cout << "📱 **Canvas4 ideal for**: Embedded systems, mobile devices, memory-constrained environments" << endl;
    cout << "🖥️  **Canvas8 ideal for**: Desktop applications, high-performance graphics, unlimited memory" << endl;
    cout << "🎛️  **VCV Rack**: Canvas8 recommended for compatibility and performance" << endl;
    cout << "⚡ **Embedded audio**: Canvas4 recommended for memory efficiency" << endl;
    
    // Performance per KB analysis
    cout << "\\n=== EFFICIENCY ANALYSIS ===" << endl;
    cout << "Canvas8 efficiency: " << fixed << setprecision(0) << canvas8.memoryEfficiency << " ops/sec per KB" << endl;
    cout << "Canvas4 efficiency: " << fixed << setprecision(0) << canvas4.memoryEfficiency << " ops/sec per KB" << endl;
    cout << "Efficiency ratio: " << fixed << setprecision(2) << (canvas4.memoryEfficiency / canvas8.memoryEfficiency) << "x ";
    cout << (canvas4.memoryEfficiency > canvas8.memoryEfficiency ? "(Canvas4 more efficient)" : "(Canvas8 more efficient)") << endl;
}

/**
 * @brief Main benchmark execution
 */
int main() {
    cout << "ENJIN2 PACKING OVERHEAD ANALYSIS" << endl;
    cout << "================================" << endl;
    cout << "Canvas size: " << PackingBenchConfig::CANVAS_WIDTH << "x" << PackingBenchConfig::CANVAS_HEIGHT << endl;
    cout << "Pixel operations: " << PackingBenchConfig::ITERATIONS << " each" << endl;
    cout << "Bulk operations: 1000 each" << endl;
    
    try {
        // Run Canvas8 benchmark (no packing)
        Canvas8Benchmark canvas8Bench;
        auto canvas8Results = canvas8Bench.run();
        
        // Run Canvas4 benchmark (with packing)
        Canvas4Benchmark canvas4Bench;
        auto canvas4Results = canvas4Bench.run();
        
        // Print detailed comparison
        printComparison(canvas8Results, canvas4Results);
        
    } catch (const exception& e) {
        cerr << "Benchmark failed: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}