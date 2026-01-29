#include <emscripten/bind.h>
#include "../../include/enjin2/scripting/lua_engine.hpp"
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/graphics/canvas.hpp"
#include "../../include/enjin2/core/types.hpp"

using namespace emscripten;
using namespace enjin2;

/**
 * @brief Emscripten bindings for enjin2 Lua scripting system
 * 
 * This file exposes enjin2's Lua engine and graphics classes to JavaScript
 * through WebAssembly, allowing DROP to use the same engine as hardware.
 */

// Simple test function to verify embind is working
int testFunction() {
    return 42;
}

// Force linking of key classes by referencing them
static void forceSymbolLinking() {
    // Create instances to force symbol inclusion
    LuaScriptSystem dummy1;
    Canvas4<128, 128> dummy2;
    LuaCanvas dummy3(&dummy2);
    // Don't actually call anything, just reference the types
    (void)dummy1;
    (void)dummy2; 
    (void)dummy3;
}

EMSCRIPTEN_BINDINGS(enjin2_test) {
    // Force symbol linking
    forceSymbolLinking();
    
    // Simple test function first
    function("testFunction", &testFunction);
    // Pixel4 value type - 4-bit pixel for memory-efficient graphics
    class_<Pixel4>("Pixel4")
        .constructor<>()
        .constructor<uint8_t>()
        .function("getValue", +[](const Pixel4& p) { return static_cast<uint8_t>(p.value); })
        .function("setValue", +[](Pixel4& p, uint8_t v) { p = Pixel4(v); });

    // LuaResult value object
    value_object<LuaResult>("LuaResult")
        .field("success", &LuaResult::success)
        .field("error", &LuaResult::error);

    // LuaEngine class
    class_<LuaEngine>("LuaEngine")
        .constructor<>()
        .function("initialize", &LuaEngine::initialize)
        .function("shutdown", &LuaEngine::shutdown)
        .function("isInitialized", &LuaEngine::isInitialized)
        .function("executeString", &LuaEngine::executeString)
        .function("executeFile", &LuaEngine::executeFile)
        .function("getMemoryUsage", &LuaEngine::getMemoryUsage)
        .function("clearScripts", &LuaEngine::clearScripts);

    // LuaCanvas class  
    class_<LuaCanvas>("LuaCanvas")
        .function("getWidth", &LuaCanvas::getWidth)
        .function("getHeight", &LuaCanvas::getHeight)
        .function("is4BitCanvas", &LuaCanvas::is4BitCanvas)
        .function("clear", &LuaCanvas::clear)
        .function("setPixel", &LuaCanvas::setPixel)
        .function("getPixel", &LuaCanvas::getPixel)
        .function("drawLine", &LuaCanvas::drawLine)
        .function("drawRect", &LuaCanvas::drawRect)
        .function("fillRect", &LuaCanvas::fillRect)
        .function("drawCircle", &LuaCanvas::drawCircle)
        .function("fillCircle", &LuaCanvas::fillCircle)
        .function("drawTriangle", &LuaCanvas::drawTriangle)
        .function("fillTriangle", &LuaCanvas::fillTriangle);

    // LuaBindings class
    class_<LuaBindings>("LuaBindings")
        .constructor<LuaEngine*>()
        .function("registerAll", &LuaBindings::registerAll)
        .function("setCanvas", &LuaBindings::setCanvas, allow_raw_pointers())
        .function("getCanvas", &LuaBindings::getCanvas, allow_raw_pointers());

    // LuaScriptSystem class - Main high-level interface
    class_<LuaScriptSystem>("LuaScriptSystem")
        .constructor<>()
        .function("initialize", &LuaScriptSystem::initialize)
        .function("shutdown", &LuaScriptSystem::shutdown)
        .function("setCanvas", &LuaScriptSystem::setCanvas, allow_raw_pointers())
        .function("executeScript", &LuaScriptSystem::executeScript)
        .function("loadScript", &LuaScriptSystem::loadScript)
        .function("getMemoryUsage", &LuaScriptSystem::getMemoryUsage);

    // Canvas4 template specialization for 128x128 (even width required for 4-bit packing)
    class_<Canvas4<128, 128>>("Canvas4_128x128")
        .constructor<>()
        .function("clear", +[](Canvas4<128, 128>& canvas, uint8_t color) {
            canvas.clear(Pixel4(color));
        })
        .function("setPixel", +[](Canvas4<128, 128>& canvas, int x, int y, uint8_t color) {
            canvas.setPixel(x, y, Pixel4(color));
        })
        .function("getPixel", +[](Canvas4<128, 128>& canvas, int x, int y) -> uint8_t {
            return static_cast<uint8_t>(canvas.getPixel(x, y));
        });

    // Canvas4 template specialization for 64x32 (for examples)
    class_<Canvas4<64, 32>>("Canvas4_64x32")
        .constructor<>()
        .function("clear", +[](Canvas4<64, 32>& canvas, uint8_t color) {
            canvas.clear(Pixel4(color));
        })
        .function("setPixel", +[](Canvas4<64, 32>& canvas, int x, int y, uint8_t color) {
            canvas.setPixel(x, y, Pixel4(color));
        })
        .function("getPixel", +[](Canvas4<64, 32>& canvas, int x, int y) -> uint8_t {
            return static_cast<uint8_t>(canvas.getPixel(x, y));
        });

    // Factory functions for creating LuaCanvas with specific canvas types  
    function("createLuaCanvas128", +[]() -> LuaCanvas* {
        static Canvas4<128, 128> canvas;
        return new LuaCanvas(&canvas);
    }, allow_raw_pointers());
    
    // Function to create LuaCanvas from existing Canvas4_128x128
    function("createLuaCanvasFromCanvas128", +[](Canvas4<128, 128>& canvas) -> LuaCanvas* {
        return new LuaCanvas(&canvas);
    }, allow_raw_pointers());

    function("createLuaCanvas64x32", +[]() -> LuaCanvas* {
        static Canvas4<64, 32> canvas;
        return new LuaCanvas(&canvas);
    }, allow_raw_pointers());

    // Helper functions for getting canvas data as typed array
    function("getCanvasData128", +[](Canvas4<128, 128>& canvas) -> val {
        // Create a simple Uint8Array with pixel data by reading each pixel
        auto width = 128;
        auto height = 128;
        auto data = new uint8_t[width * height];
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                data[y * width + x] = canvas.getPixel(x, y).value;
            }
        }
        
        return val(typed_memory_view(width * height, data));
    });

    function("getCanvasData64x32", +[](Canvas4<64, 32>& canvas) -> val {
        // Create a simple Uint8Array with pixel data by reading each pixel
        auto width = 64;
        auto height = 32;
        auto data = new uint8_t[width * height];
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                data[y * width + x] = canvas.getPixel(x, y).value;
            }
        }
        
        return val(typed_memory_view(width * height, data));
    });

    // Helper for setting canvas data from JavaScript TypedArray
    function("setCanvasData128", +[](Canvas4<128, 128>& canvas, val jsArray) {
        auto length = jsArray["length"].as<unsigned>();
        auto width = 128;
        auto height = 128;
        
        for (unsigned i = 0; i < std::min(length, static_cast<unsigned>(width * height)); ++i) {
            int x = i % width;
            int y = i / width;
            uint8_t value = jsArray[i].as<uint8_t>();
            canvas.setPixel(x, y, Pixel4{value});
        }
    });
    
    // Fast bulk pixel operations
    function("fastFillRect", +[](Canvas4<128, 128>& canvas, int x, int y, int w, int h, uint8_t color) {
        Pixel4 pixel{color};
        int x2 = std::min(x + w, 128);
        int y2 = std::min(y + h, 128);
        x = std::max(x, 0);
        y = std::max(y, 0);
        
        for (int py = y; py < y2; py++) {
            for (int px = x; px < x2; px++) {
                canvas.setPixel(px, py, pixel);
            }
        }
    });
    
    function("fastDrawLine", +[](Canvas4<128, 128>& canvas, int x1, int y1, int x2, int y2, uint8_t color) {
        Pixel4 pixel{color};
        
        // Bresenham's line algorithm
        int dx = abs(x2 - x1);
        int dy = abs(y2 - y1);
        int sx = x1 < x2 ? 1 : -1;
        int sy = y1 < y2 ? 1 : -1;
        int err = dx - dy;
        
        int x = x1, y = y1;
        while (true) {
            if (x >= 0 && x < 128 && y >= 0 && y < 128) {
                canvas.setPixel(x, y, pixel);
            }
            if (x == x2 && y == y2) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx) { err += dx; y += sy; }
        }
    });

    function("setCanvasData64x32", +[](Canvas4<64, 32>& canvas, val jsArray) {
        auto length = jsArray["length"].as<unsigned>();
        auto width = 64;
        auto height = 32;
        
        for (unsigned i = 0; i < std::min(length, static_cast<unsigned>(width * height)); ++i) {
            int x = i % width;
            int y = i / width;
            uint8_t value = jsArray[i].as<uint8_t>();
            canvas.setPixel(x, y, Pixel4{value});
        }
    });
    
    // High-performance batch drawing functions to reduce embind overhead
    function("drawPixelsBatch", +[](Canvas4<128, 128>& canvas, val jsPixelArray) -> void {
        auto length = jsPixelArray["length"].as<unsigned>();
        // Expect array of [x, y, color, x, y, color, ...]
        for (unsigned i = 0; i + 2 < length; i += 3) {
            int x = jsPixelArray[i].as<int>();
            int y = jsPixelArray[i + 1].as<int>();
            uint8_t color = jsPixelArray[i + 2].as<uint8_t>();
            if (x >= 0 && x < 128 && y >= 0 && y < 128) {
                canvas.setPixel(x, y, Pixel4{color});
            }
        }
    });
    
    function("drawLinesBatch", +[](Canvas4<128, 128>& canvas, val jsLineArray) -> void {
        auto length = jsLineArray["length"].as<unsigned>();
        // Expect array of [x1, y1, x2, y2, color, x1, y1, x2, y2, color, ...]
        for (unsigned i = 0; i + 4 < length; i += 5) {
            int x1 = jsLineArray[i].as<int>();
            int y1 = jsLineArray[i + 1].as<int>();
            int x2 = jsLineArray[i + 2].as<int>();
            int y2 = jsLineArray[i + 3].as<int>();
            uint8_t color = jsLineArray[i + 4].as<uint8_t>();
            
            // Simple line drawing (Bresenham's algorithm)
            int dx = abs(x2 - x1);
            int dy = abs(y2 - y1);
            int sx = x1 < x2 ? 1 : -1;
            int sy = y1 < y2 ? 1 : -1;
            int err = dx - dy;
            
            int x = x1, y = y1;
            while (true) {
                if (x >= 0 && x < 128 && y >= 0 && y < 128) {
                    canvas.setPixel(x, y, Pixel4{color});
                }
                if (x == x2 && y == y2) break;
                int e2 = 2 * err;
                if (e2 > -dy) { err -= dy; x += sx; }
                if (e2 < dx) { err += dx; y += sy; }
            }
        }
    });
    
    function("fillRectsBatch", +[](Canvas4<128, 128>& canvas, val jsRectArray) -> void {
        auto length = jsRectArray["length"].as<unsigned>();
        // Expect array of [x, y, w, h, color, x, y, w, h, color, ...]
        for (unsigned i = 0; i + 4 < length; i += 5) {
            int x = jsRectArray[i].as<int>();
            int y = jsRectArray[i + 1].as<int>();
            int w = jsRectArray[i + 2].as<int>();
            int h = jsRectArray[i + 3].as<int>();
            uint8_t color = jsRectArray[i + 4].as<uint8_t>();
            
            for (int py = y; py < y + h && py < 128; py++) {
                for (int px = x; px < x + w && px < 128; px++) {
                    if (px >= 0 && py >= 0) {
                        canvas.setPixel(px, py, Pixel4{color});
                    }
                }
            }
        }
    });

    // Helper to debug Lua function bindings
    function("debugLuaBindings", +[](LuaScriptSystem& scriptSystem) -> bool {
        auto result = scriptSystem.executeScript(R"(
-- Debug: Check if functions are registered
local functions_to_check = {"clear", "setColor", "point", "line", "getWidth", "getHeight"}
for _, func_name in ipairs(functions_to_check) do
    if _G[func_name] then
        print("✅ " .. func_name .. " is registered")
    else
        print("❌ " .. func_name .. " is NOT registered")
    end
end
        )");
        return result.success;
    });

    // Helper to setup global Lua functions for a script system
    function("setupGlobalLuaFunctions", +[](LuaScriptSystem& scriptSystem) -> bool {
        auto result = scriptSystem.executeScript(R"(
-- enjin2 Global Lua Functions
-- These functions are automatically registered by the C++ bindings

function getWidth()
    return 128  -- Default for now, will be overridden by actual canvas
end

function getHeight()
    return 128  -- Default for now, will be overridden by actual canvas
end

-- Note: clear, setPixel, getPixel, etc. should be registered directly by C++ bindings
-- These are just fallbacks in case the bindings aren't working
function clear(color)
    -- This should be overridden by C++ binding
    print("Warning: clear() not bound to C++")
end

function setPixel(x, y, color)
    -- This should be overridden by C++ binding  
    print("Warning: setPixel() not bound to C++")
end

function getPixel(x, y)
    -- This should be overridden by C++ binding
    print("Warning: getPixel() not bound to C++")
    return 0
end

function line(x1, y1, x2, y2, color)
    -- This should be overridden by C++ binding
    print("Warning: line() not bound to C++")
end

function rectangle(mode, x, y, w, h, color)
    -- This should be overridden by C++ binding
    print("Warning: rectangle() not bound to C++")
end

function circle(mode, x, y, radius, color)
    -- This should be overridden by C++ binding
    print("Warning: circle() not bound to C++")
end

function triangle(x1, y1, x2, y2, x3, y3, color)
    -- This should be overridden by C++ binding
    print("Warning: triangle() not bound to C++")
end

function point(x, y, color)
    setPixel(x, y, color)
end
)");
        return result.success;
    });
}