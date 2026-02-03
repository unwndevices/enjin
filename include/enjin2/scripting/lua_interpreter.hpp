/**
 * @file lua_interpreter.hpp
 * @brief Lua interpreter implementations for desktop and ESP32 platforms
 *
 * Provides full and minimal Lua interpreters with graphics integration
 * for scripting support across different platforms.
 */
#pragma once

#include "script_interface.hpp"
#include "lua_engine.hpp"
#include "bindings.hpp"
#include <memory>

namespace enjin2 {

/**
 * @brief Full Lua interpreter implementation
 * 
 * Desktop/VCV implementation using full Lua with complete enjin2 bindings.
 * Provides all Lua features and comprehensive graphics API.
 */
class LuaInterpreter : public IScriptInterpreter {
private:
    std::unique_ptr<LuaEngine> engine;       ///< Lua engine
    std::unique_ptr<LuaBindings> bindings;   ///< Lua bindings
    IScriptGraphics* graphics;               ///< Graphics interface
    bool initialized;                        ///< Initialization state
    
public:
    /**
     * @brief Constructor
     */
    LuaInterpreter();
    
    /**
     * @brief Destructor
     */
    ~LuaInterpreter() override;
    
    /**
     * @brief Set graphics interface
     * @param gfx Graphics interface to use
     */
    void setGraphics(IScriptGraphics* gfx);
    
    // IScriptInterpreter implementation
    bool initialize() override;
    void shutdown() override;
    bool isInitialized() const override { return initialized; }
    
    ScriptResult executeString(const std::string& code) override;
    ScriptResult executeFile(const std::string& filename) override;
    ScriptResult callFunction(const std::string& functionName) override;
    
    void setGlobal(const std::string& name, double value) override;
    void setGlobal(const std::string& name, const std::string& value) override;
    void setGlobal(const std::string& name, bool value) override;
    
    double getGlobalNumber(const std::string& name, double defaultValue = 0.0) override;
    std::string getGlobalString(const std::string& name, const std::string& defaultValue = "") override;
    bool getGlobalBool(const std::string& name, bool defaultValue = false) override;
    
    size_t getMemoryUsage() const override;
    const char* getTypeName() const override { return "Lua"; }
    
    /**
     * @brief Get access to underlying Lua engine (for advanced operations)
     * @return Lua engine reference
     */
    LuaEngine* getLuaEngine() { return engine.get(); }
    
    /**
     * @brief Get access to Lua bindings
     * @return Lua bindings reference
     */
    LuaBindings* getLuaBindings() { return bindings.get(); }

private:
    /**
     * @brief Convert LuaResult to ScriptResult
     * @param luaResult Lua execution result
     * @return Script execution result
     */
    ScriptResult convertResult(const LuaResult& luaResult);
};

/**
 * @brief Minimal Lua interpreter for ESP32
 * 
 * Lightweight Lua implementation for ESP32 with reduced memory footprint
 * and essential graphics functions only.
 */
class MinimalLuaInterpreter : public IScriptInterpreter {
private:
    // Simplified implementation for ESP32
    void* interpreterState;                  ///< Platform-specific interpreter state
    IScriptGraphics* graphics;               ///< Graphics interface
    bool initialized;                        ///< Initialization state
    
    // Memory management for ESP32
    static constexpr size_t ESP32_MEMORY_LIMIT = 16 * 1024; ///< 16KB memory limit
    char memoryPool[ESP32_MEMORY_LIMIT];    ///< Static memory pool
    size_t memoryUsed;                      ///< Current memory usage
    
public:
    /**
     * @brief Constructor
     */
    MinimalLuaInterpreter();
    
    /**
     * @brief Destructor
     */
    ~MinimalLuaInterpreter() override;
    
    /**
     * @brief Set graphics interface
     * @param gfx Graphics interface to use
     */
    void setGraphics(IScriptGraphics* gfx);
    
    // IScriptInterpreter implementation
    bool initialize() override;
    void shutdown() override;
    bool isInitialized() const override { return initialized; }
    
    ScriptResult executeString(const std::string& code) override;
    ScriptResult executeFile(const std::string& filename) override;
    ScriptResult callFunction(const std::string& functionName) override;
    
    void setGlobal(const std::string& name, double value) override;
    void setGlobal(const std::string& name, const std::string& value) override;
    void setGlobal(const std::string& name, bool value) override;
    
    double getGlobalNumber(const std::string& name, double defaultValue = 0.0) override;
    std::string getGlobalString(const std::string& name, const std::string& defaultValue = "") override;
    bool getGlobalBool(const std::string& name, bool defaultValue = false) override;
    
    size_t getMemoryUsage() const override;
    const char* getTypeName() const override { return "MinimalLua"; }

private:
    /**
     * @brief Register minimal graphics functions
     */
    void registerMinimalBindings();
    
    /**
     * @brief Allocate memory from pool
     * @param size Size to allocate
     * @return Allocated memory or nullptr
     */
    void* allocateMemory(size_t size);
    
    /**
     * @brief Free memory back to pool
     * @param ptr Memory to free
     */
    void freeMemory(void* ptr);
};

/**
 * @brief Graphics adapter for enjin2 canvases
 * 
 * Implements IScriptGraphics interface using enjin2 canvas types.
 * Allows scripts to draw on any enjin2 canvas through the common interface.
 */
template<typename CanvasType>
class CanvasGraphicsAdapter : public IScriptGraphics {
private:
    CanvasType* canvas;     ///< Target canvas
    uint8_t currentColor;   ///< Current drawing color
    
public:
    /**
     * @brief Constructor
     * @param targetCanvas Canvas to draw on
     */
    CanvasGraphicsAdapter(CanvasType* targetCanvas) 
        : canvas(targetCanvas), currentColor(15) {}
    
    /**
     * @brief Set target canvas
     * @param targetCanvas New canvas to draw on
     */
    void setCanvas(CanvasType* targetCanvas) {
        canvas = targetCanvas;
    }
    
    /**
     * @brief Set current drawing color
     * @param color Color to use for drawing
     */
    void setColor(uint8_t color) {
        currentColor = color;
    }
    
    // IScriptGraphics implementation
    uint16_t getWidth() const override {
        return canvas ? canvas->getWidth() : 0;
    }
    
    uint16_t getHeight() const override {
        return canvas ? canvas->getHeight() : 0;
    }
    
    void clear(uint8_t color) override {
        if (canvas) canvas->clear(color);
    }
    
    void setPixel(int16_t x, int16_t y, uint8_t color) override {
        if (canvas) canvas->setPixel(x, y, color);
    }
    
    uint8_t getPixel(int16_t x, int16_t y) const override {
        return canvas ? canvas->getPixel(x, y) : 0;
    }
    
    void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) override {
        if (canvas) {
            // Use enjin2 primitives
            if constexpr (std::is_same_v<CanvasType, Canvas4<128, 128>>) {
                Primitives4::drawLine(*canvas, x1, y1, x2, y2, Pixel4(color));
            } else {
                Primitives8::drawLine(*canvas, x1, y1, x2, y2, color);
            }
        }
    }
    
    void drawRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) override {
        if (canvas) {
            if constexpr (std::is_same_v<CanvasType, Canvas4<128, 128>>) {
                Primitives4::drawRect(*canvas, Rect(x, y, width, height), Pixel4(color));
            } else {
                Primitives8::drawRect(*canvas, Rect(x, y, width, height), color);
            }
        }
    }
    
    void fillRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) override {
        if (canvas) {
            if constexpr (std::is_same_v<CanvasType, Canvas4<128, 128>>) {
                Primitives4::fillRect(*canvas, Rect(x, y, width, height), Pixel4(color));
            } else {
                Primitives8::fillRect(*canvas, Rect(x, y, width, height), color);
            }
        }
    }
    
    void drawCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) override {
        if (canvas) {
            if constexpr (std::is_same_v<CanvasType, Canvas4<128, 128>>) {
                Primitives4::drawCircle(*canvas, x, y, radius, Pixel4(color));
            } else {
                Primitives8::drawCircle(*canvas, x, y, radius, color);
            }
        }
    }
    
    void fillCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) override {
        if (canvas) {
            if constexpr (std::is_same_v<CanvasType, Canvas4<128, 128>>) {
                Primitives4::fillCircle(*canvas, x, y, radius, Pixel4(color));
            } else {
                Primitives8::fillCircle(*canvas, x, y, radius, color);
            }
        }
    }
};

} // namespace enjin2