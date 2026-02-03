/**
 * @file script_interface.hpp
 * @brief Platform-agnostic interfaces for script interpreters and graphics
 *
 * Provides abstract interfaces IScriptInterpreter and IScriptGraphics
 * allowing multiple scripting languages and graphics backends.
 */
#pragma once

#include "../core/types.hpp"
#include <string>
#include <vector>
#include <functional>

namespace enjin2 {

/**
 * @brief Script execution result
 */
struct ScriptResult {
    bool success;           ///< Whether execution was successful
    std::string error;      ///< Error message if execution failed
    
    /**
     * @brief Constructor for successful result
     */
    ScriptResult() : success(true) {}
    
    /**
     * @brief Constructor for failed result
     * @param errorMsg Error message
     */
    ScriptResult(const std::string& errorMsg) : success(false), error(errorMsg) {}
};

/**
 * @brief Platform-agnostic script interpreter interface
 * 
 * Abstract interface for script interpreters to allow platform-specific
 * implementations (full Lua on desktop, lightweight interpreters on ESP32).
 */
class IScriptInterpreter {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IScriptInterpreter() = default;
    
    /**
     * @brief Initialize the interpreter
     * @return True if initialization successful
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Shutdown the interpreter
     */
    virtual void shutdown() = 0;
    
    /**
     * @brief Check if interpreter is initialized
     * @return True if initialized
     */
    virtual bool isInitialized() const = 0;
    
    /**
     * @brief Execute script code string
     * @param code Script code to execute
     * @return Execution result
     */
    virtual ScriptResult executeString(const std::string& code) = 0;
    
    /**
     * @brief Load and execute script file
     * @param filename Path to script file
     * @return Execution result
     */
    virtual ScriptResult executeFile(const std::string& filename) = 0;
    
    /**
     * @brief Call script function
     * @param functionName Name of function to call
     * @return Execution result
     */
    virtual ScriptResult callFunction(const std::string& functionName) = 0;
    
    /**
     * @brief Set global variable
     * @param name Variable name
     * @param value Variable value
     */
    virtual void setGlobal(const std::string& name, double value) = 0;
    virtual void setGlobal(const std::string& name, const std::string& value) = 0;
    virtual void setGlobal(const std::string& name, bool value) = 0;
    
    /**
     * @brief Get global variable
     * @param name Variable name
     * @param defaultValue Default value if not found
     * @return Variable value
     */
    virtual double getGlobalNumber(const std::string& name, double defaultValue = 0.0) = 0;
    virtual std::string getGlobalString(const std::string& name, const std::string& defaultValue = "") = 0;
    virtual bool getGlobalBool(const std::string& name, bool defaultValue = false) = 0;
    
    /**
     * @brief Get current memory usage
     * @return Memory usage in bytes
     */
    virtual size_t getMemoryUsage() const = 0;
    
    /**
     * @brief Get interpreter type name
     * @return Type name string (e.g., "Lua", "JavaScript", "MicroPython")
     */
    virtual const char* getTypeName() const = 0;
};

/**
 * @brief Platform-agnostic graphics interface for scripts
 * 
 * Provides drawing functions that work across different canvas types
 * and can be implemented by platform-specific graphics backends.
 */
class IScriptGraphics {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IScriptGraphics() = default;
    
    /**
     * @brief Get canvas width
     * @return Canvas width in pixels
     */
    virtual uint16_t getWidth() const = 0;
    
    /**
     * @brief Get canvas height
     * @return Canvas height in pixels
     */
    virtual uint16_t getHeight() const = 0;
    
    /**
     * @brief Clear canvas with specified color
     * @param color Clear color
     */
    virtual void clear(uint8_t color) = 0;
    
    /**
     * @brief Set pixel at coordinates
     * @param x X coordinate
     * @param y Y coordinate
     * @param color Pixel color
     */
    virtual void setPixel(int16_t x, int16_t y, uint8_t color) = 0;
    
    /**
     * @brief Get pixel at coordinates
     * @param x X coordinate
     * @param y Y coordinate
     * @return Pixel color value
     */
    virtual uint8_t getPixel(int16_t x, int16_t y) const = 0;
    
    /**
     * @brief Draw line
     * @param x1 Start X coordinate
     * @param y1 Start Y coordinate
     * @param x2 End X coordinate
     * @param y2 End Y coordinate
     * @param color Line color
     */
    virtual void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) = 0;
    
    /**
     * @brief Draw rectangle outline
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Rectangle width
     * @param height Rectangle height
     * @param color Rectangle color
     */
    virtual void drawRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) = 0;
    
    /**
     * @brief Fill rectangle
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Rectangle width
     * @param height Rectangle height
     * @param color Fill color
     */
    virtual void fillRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) = 0;
    
    /**
     * @brief Draw circle outline
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param radius Circle radius
     * @param color Circle color
     */
    virtual void drawCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) = 0;
    
    /**
     * @brief Fill circle
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param radius Circle radius
     * @param color Fill color
     */
    virtual void fillCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) = 0;
};

/**
 * @brief Script system factory for creating platform-specific interpreters
 */
class ScriptFactory {
public:
    /**
     * @brief Interpreter type enum
     */
    enum class InterpreterType {
        LUA_FULL,           ///< Full Lua interpreter (desktop)
        LUA_MINIMAL,        ///< Minimal Lua interpreter (ESP32)
        JAVASCRIPT,         ///< JavaScript interpreter (future)
        MICROPYTHON,        ///< MicroPython interpreter (future)
        SIMPLE_BASIC        ///< Simple BASIC-like interpreter (very constrained systems)
    };
    
    /**
     * @brief Create interpreter for current platform
     * @param type Interpreter type to create
     * @return Unique pointer to interpreter instance
     */
    static std::unique_ptr<IScriptInterpreter> createInterpreter(InterpreterType type);
    
    /**
     * @brief Get recommended interpreter for current platform
     * @return Recommended interpreter type
     */
    static InterpreterType getRecommendedInterpreter();
    
    /**
     * @brief Check if interpreter type is available on current platform
     * @param type Interpreter type to check
     * @return True if available
     */
    static bool isInterpreterAvailable(InterpreterType type);
};

} // namespace enjin2