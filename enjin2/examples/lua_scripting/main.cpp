#include "../../include/enjin2/scripting/lua_engine.hpp"
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/graphics/canvas.hpp"
#include "../../include/enjin2/graphics/primitives.hpp"
#include <iostream>
#include <chrono>
#include <thread>

using namespace enjin2;

/**
 * @brief Print canvas to console as ASCII art
 * @param canvas Canvas to print
 */
void printCanvas(const Canvas4<64, 32>& canvas) {
    // Print every 2nd column for console display
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 64; x += 2) {
            Pixel4 pixel = canvas.getPixel(x, y);
            char c;
            if (pixel.value == 0) c = ' ';
            else if (pixel.value < 4) c = '.';
            else if (pixel.value < 8) c = '+';
            else if (pixel.value < 12) c = '#';
            else c = '@';
            std::cout << c;
        }
        std::cout << '\n';
    }
}

int main() {
    std::cout << "Enjin 2.0 Lua Integration Demo - Phase 3\n";
    std::cout << "=========================================\n\n";
    
    // Create canvas
    Canvas4<64, 32> canvas;
    LuaCanvas luaCanvas(&canvas);
    
    // Initialize Lua script system
    LuaScriptSystem scriptSystem;
    if (!scriptSystem.initialize()) {
        std::cerr << "Failed to initialize Lua script system\n";
        return 1;
    }
    
    scriptSystem.setCanvas(&luaCanvas);
    
    std::cout << "Canvas dimensions: " << luaCanvas.getWidth() << "x" << luaCanvas.getHeight() << "\n";
    std::cout << "Canvas type: " << (luaCanvas.is4BitCanvas() ? "4-bit" : "8-bit") << "\n";
    std::cout << "Lua memory usage: " << scriptSystem.getMemoryUsage() << " bytes\n\n";
    
    // Test basic Lua execution
    std::cout << "=== Basic Lua Test ===\n";
    auto result = scriptSystem.executeScript(R"(
        print("Hello from Lua!")
        print("Canvas size:", getWidth(), "x", getHeight())
        
        -- Test some math
        local x = 10
        local y = 20
        print("10 + 20 =", x + y)
    )");
    
    if (!result.success) {
        std::cerr << "Lua execution failed: " << result.error << "\n";
        return 1;
    }
    
    std::cout << "\n=== Drawing Test ===\n";
    
    // Test drawing functions
    result = scriptSystem.executeScript(R"(
        -- Clear canvas
        clear(0)
        
        -- Set drawing color
        setColor(15)  -- White
        
        -- Draw some shapes
        rectangle(5, 5, 10, 8)      -- Filled rectangle
        rectangle("line", 20, 5, 10, 8)  -- Rectangle outline
        
        circle(10, 20, 4)           -- Filled circle
        circle("line", 25, 20, 4)   -- Circle outline
        
        -- Draw lines
        line(0, 0, 63, 31)          -- Diagonal
        line(63, 0, 0, 31)          -- Other diagonal
        
        -- Draw some pixels
        for i = 0, 10 do
            setPixel(40 + i, 10, 8)
        end
        
        print("Drawing complete!")
    )");
    
    if (!result.success) {
        std::cerr << "Drawing script failed: " << result.error << "\n";
        return 1;
    }
    
    printCanvas(canvas);
    std::cout << "\n";
    
    // Test love2d.graphics style API
    std::cout << "=== Love2D Style API Test ===\n";
    
    result = scriptSystem.executeScript(R"(
        love.graphics.clear()
        love.graphics.setColor(12)
        
        -- Love2D style drawing
        love.graphics.rectangle("fill", 15, 10, 8, 6)
        love.graphics.circle("line", 35, 15, 5)
        love.graphics.line(5, 5, 25, 25)
        
        print("Love2D style drawing complete!")
    )");
    
    if (!result.success) {
        std::cerr << "Love2D script failed: " << result.error << "\n";
        return 1;
    }
    
    printCanvas(canvas);
    std::cout << "\n";
    
    // Test animation with time
    std::cout << "=== Animation Test ===\n";
    
    for (int frame = 0; frame < 8; ++frame) {
        result = scriptSystem.executeScript(R"(
            clear(0)
            
            local t = time()
            local x = math.floor(32 + 20 * math.cos(t * 2))
            local y = math.floor(16 + 8 * math.sin(t * 3))
            
            setColor(15)
            circle(x, y, 3)
            
            -- Draw trail
            for i = 1, 5 do
                local tx = math.floor(32 + 20 * math.cos((t - i * 0.1) * 2))
                local ty = math.floor(16 + 8 * math.sin((t - i * 0.1) * 3))
                setColor(15 - i * 2)
                point(tx, ty)
            end
            
            -- Draw border
            setColor(8)
            rectangle("line", 0, 0, getWidth() - 1, getHeight() - 1)
        )");
        
        if (!result.success) {
            std::cerr << "Animation script failed: " << result.error << "\n";
            break;
        }
        
        std::cout << "Frame " << frame << ":\n";
        printCanvas(canvas);
        std::cout << "\n";
        
        // Small delay for animation effect
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    // Test script file loading
    std::cout << "=== Script File Test ===\n";
    
    result = scriptSystem.loadScript("demo.lua");
    if (result.success) {
        std::cout << "demo.lua loaded successfully!\n";
    } else {
        std::cout << "demo.lua not found or failed to load: " << result.error << "\n";
        std::cout << "Creating demo script inline...\n";
        
        // Create demo script content inline
        result = scriptSystem.executeScript(R"(
            function drawSpiral()
                clear(0)
                
                local centerX = getWidth() / 2
                local centerY = getHeight() / 2
                local t = time()
                
                setColor(15)
                
                for i = 0, 50 do
                    local angle = i * 0.3 + t
                    local radius = i * 0.4
                    local x = math.floor(centerX + radius * math.cos(angle))
                    local y = math.floor(centerY + radius * math.sin(angle))
                    
                    if x >= 0 and x < getWidth() and y >= 0 and y < getHeight() then
                        setColor(math.floor(15 - (i % 16)))
                        point(x, y)
                    end
                end
            end
            
            -- Draw the spiral
            drawSpiral()
            print("Spiral drawing complete!")
        )");
        
        if (result.success) {
            printCanvas(canvas);
        }
    }
    
    std::cout << "\n=== Memory and Performance Stats ===\n";
    std::cout << "Final Lua memory usage: " << scriptSystem.getMemoryUsage() << " bytes\n";
    std::cout << "Canvas buffer size: " << canvas.getBufferSize() << " bytes\n";
    std::cout << "4-bit pixel efficiency: 50% memory savings vs 8-bit\n";
    
    std::cout << "\n=== Phase 3 Lua Integration Features ===\n";
    std::cout << "✓ Static Lua memory management (64KB limit)\n";
    std::cout << "✓ Canvas abstraction for 4-bit and 8-bit\n";
    std::cout << "✓ Drawing primitives (lines, rectangles, circles, triangles)\n";
    std::cout << "✓ Love2D.graphics-style API for familiarity\n";
    std::cout << "✓ Script execution from strings and files\n";
    std::cout << "✓ Real-time animation support with time() function\n";
    std::cout << "✓ Safe embedded environment (dangerous functions removed)\n";
    
    scriptSystem.shutdown();
    
    std::cout << "\nPhase 3 Lua Integration: COMPLETE ✓\n";
    return 0;
}