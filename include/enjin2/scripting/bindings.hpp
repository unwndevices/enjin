/**
 * @file bindings.hpp
 * @brief Lua bindings for Enjin graphics, canvas operations, and scripting interface
 *
 * Provides LuaCanvas wrapper for 4-bit/8-bit canvases and LuaBindings
 * for love2d.graphics-style API familiar to Lua developers.
 */
#pragma once

#include "lua_engine.hpp"
#include "lua_platform.hpp"
#include "../graphics/canvas.hpp"
#include "../graphics/primitives.hpp"
#include "../graphics/sprite.hpp"
#include "../input/input_state.hpp"
// #include "../ui/component.hpp"  // Conflicts with core/component.hpp in VCV build
// #include "../ui/components.hpp" // Not needed for basic canvas operations

namespace enjin2 {

/**
 * @brief Canvas wrapper for Lua bindings
 * 
 * Provides a type-erased canvas interface that can hold either
 * 4-bit or 8-bit canvases for Lua scripting.
 */
class LuaCanvas {
private:
    void* canvasPtr;        ///< Pointer to actual canvas
    bool is4Bit;           ///< Whether this is a 4-bit canvas
    uint16_t width;        ///< Canvas width
    uint16_t height;       ///< Canvas height
    
public:
    /**
     * @brief Constructor for 4-bit canvas
     * @tparam W Canvas width
     * @tparam H Canvas height
     * @param canvas 4-bit canvas pointer
     */
    template<uint16_t W, uint16_t H>
    LuaCanvas(Canvas4<W, H>* canvas)
        : canvasPtr(canvas), is4Bit(true), width(W), height(H) {}

    /**
     * @brief Constructor for 8-bit canvas
     * @tparam W Canvas width
     * @tparam H Canvas height
     * @param canvas 8-bit canvas pointer
     */
    template<uint16_t W, uint16_t H>
    LuaCanvas(Canvas8<W, H>* canvas) 
        : canvasPtr(canvas), is4Bit(false), width(W), height(H) {}
    
    /**
     * @brief Get canvas width
     * @return Canvas width in pixels
     */
    uint16_t getWidth() const { return width; }
    
    /**
     * @brief Get canvas height
     * @return Canvas height in pixels
     */
    uint16_t getHeight() const { return height; }
    
    /**
     * @brief Check if this is a 4-bit canvas
     * @return True if 4-bit, false if 8-bit
     */
    bool is4BitCanvas() const { return is4Bit; }
    
    /**
     * @brief Clear canvas with specified color
     * @param color Clear color (0-15 for 4-bit, 0-255 for 8-bit)
     */
    void clear(uint8_t color);
    
    /**
     * @brief Set pixel at coordinates
     * @param x X coordinate
     * @param y Y coordinate
     * @param color Pixel color
     */
    void setPixel(int16_t x, int16_t y, uint8_t color);
    
    /**
     * @brief Get pixel at coordinates
     * @param x X coordinate
     * @param y Y coordinate
     * @return Pixel color value
     */
    uint8_t getPixel(int16_t x, int16_t y) const;
    
    /**
     * @brief Draw line
     * @param x1 Start X coordinate
     * @param y1 Start Y coordinate
     * @param x2 End X coordinate
     * @param y2 End Y coordinate
     * @param color Line color
     */
    void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color);
    
    /**
     * @brief Draw rectangle outline
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Rectangle width
     * @param height Rectangle height
     * @param color Rectangle color
     */
    void drawRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color);
    
    /**
     * @brief Fill rectangle
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Rectangle width
     * @param height Rectangle height
     * @param color Fill color
     */
    void fillRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color);
    
    /**
     * @brief Draw circle outline
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param radius Circle radius
     * @param color Circle color
     */
    void drawCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color);
    
    /**
     * @brief Fill circle
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param radius Circle radius
     * @param color Fill color
     */
    void fillCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color);
    
    /**
     * @brief Draw triangle outline
     * @param x1 First vertex X
     * @param y1 First vertex Y
     * @param x2 Second vertex X
     * @param y2 Second vertex Y
     * @param x3 Third vertex X
     * @param y3 Third vertex Y
     * @param color Triangle color
     */
    void drawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, 
                     int16_t x3, int16_t y3, uint8_t color);
    
    /**
     * @brief Fill triangle
     * @param x1 First vertex X
     * @param y1 First vertex Y
     * @param x2 Second vertex X
     * @param y2 Second vertex Y
     * @param x3 Third vertex X
     * @param y3 Third vertex Y
     * @param color Fill color
     */
    void fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, 
                     int16_t x3, int16_t y3, uint8_t color);
};

/**
 * @brief Lua bindings for Enjin graphics and UI
 * 
 * Provides love2d.graphics-style API for familiar Lua scripting.
 * All functions are registered as global Lua functions.
 */
class LuaBindings {
private:
    LuaEngine* engine;          ///< Lua engine instance
    LuaCanvas* currentCanvas;   ///< Currently active canvas
    InputState* currentInput;   ///< Current frame's input state (set by host before each Lua call)

    // Drawing state
    uint8_t currentColor;       ///< Current drawing color
    uint16_t lineWidth;         ///< Current line width

    // ── Sprite pool ──────────────────────────────────────────────────────────
    static constexpr int LUA_SPRITE_POOL_SIZE = 16;  ///< Fixed pool — zero alloc

    /** Per-slot animation state for the Lua sprite pool. */
    struct SpriteState {
        SpriteSheet sheet;       ///< Sheet data (pointer to external pixel data)
        float       fps{8.0f};   ///< Playback rate in frames per second
        float       accumSec{0.0f}; ///< Accumulated seconds since last frame advance
        uint8_t     frame{0};    ///< Current frame index
        AnimMode    mode{AnimMode::Loop}; ///< Animation loop mode
        bool        forward{true};  ///< Ping-pong direction (true = forward)
        bool        done{false};    ///< Once mode: true when animation has completed
        bool        active{false};  ///< Whether this slot is in use
    };

    SpriteState spritePool[LUA_SPRITE_POOL_SIZE]; ///< Zero-alloc fixed sprite pool

    // ── Layer system ─────────────────────────────────────────────────────────
    static constexpr int MAX_LUA_LAYERS = 8;  ///< Ceiling matching ENJIN_LAYER_COUNT max
    LuaCanvas* layerCanvases[MAX_LUA_LAYERS]{};  ///< Null-initialized; pointers to per-layer LuaCanvas wrappers
    bool*      layerVisible{nullptr};            ///< Points to LayerCompositor::visible[] array (set by host)
    uint8_t    activeLayer{0};                   ///< Current C++ buffer index (0-based)
    uint8_t    layerCount{0};                    ///< Set by host via setLayers()

public:
    /**
     * @brief Constructor
     * @param luaEngine Lua engine to bind to
     */
    LuaBindings(LuaEngine* luaEngine);

    /**
     * @brief Register all bindings with Lua engine
     */
    void registerAll();

    /**
     * @brief Set current canvas for drawing operations
     * @param canvas Canvas to draw on
     */
    void setCanvas(LuaCanvas* canvas);

    /**
     * @brief Get current canvas
     * @return Current canvas or nullptr
     */
    LuaCanvas* getCanvas() const { return currentCanvas; }

    /**
     * @brief Set input state for this frame
     * @param input Current InputState pointer (updated by host after input_platform_poll)
     */
    void setInput(InputState* input);

    /**
     * @brief Wire layer canvas pointers from host (called once at init)
     * @param canvases Array of LuaCanvas pointers, one per layer
     * @param count Number of layers
     * @param visibleArr Pointer to LayerCompositor::visible[] array
     */
    void setLayers(LuaCanvas** canvases, uint8_t count, bool* visibleArr);

    /**
     * @brief Reset all sprite pool slots to inactive state
     *
     * Called automatically from registerAll() to ensure a clean sprite pool
     * on every Lua state reload. Also resets drawing state (currentColor, lineWidth).
     */
    void resetSpritePool();

private:
    // Canvas management functions
    static int lua_getWidth(lua_State* L);
    static int lua_getHeight(lua_State* L);
    static int lua_clear(lua_State* L);
    
    // Drawing functions (love2d.graphics style)
    static int lua_setColor(lua_State* L);
    static int lua_getColor(lua_State* L);
    static int lua_setLineWidth(lua_State* L);
    static int lua_getLineWidth(lua_State* L);
    
    // Primitive drawing
    static int lua_point(lua_State* L);
    static int lua_line(lua_State* L);
    static int lua_rectangle(lua_State* L);
    static int lua_circle(lua_State* L);
    static int lua_triangle(lua_State* L);
    
    // Pixel access
    static int lua_setPixel(lua_State* L);
    static int lua_getPixel(lua_State* L);
    
    // Component/Entity functions
    static int lua_createEntity(lua_State* L);
    static int lua_destroyEntity(lua_State* L);
    static int lua_addComponent(lua_State* L);
    static int lua_removeComponent(lua_State* L);
    static int lua_getComponent(lua_State* L);
    
    // Utility functions
    static int lua_print(lua_State* L);
    static int lua_time(lua_State* L);
    
    // High-performance optimized drawing functions
    static int lua_fastFillRect(lua_State* L);
    static int lua_fastDrawLine(lua_State* L);

    // Palette functions
    static int lua_setPaletteColor(lua_State* L);
    static int lua_getPaletteColor(lua_State* L);
    static int lua_loadPalette(lua_State* L);
    static int lua_getPaletteSize(lua_State* L);

    // Input polling functions (INP-05)
    static int lua_isButtonHeld(lua_State* L);
    static int lua_isButtonJustPressed(lua_State* L);
    static int lua_isButtonJustReleased(lua_State* L);
    static int lua_getAxis(lua_State* L);

    // Sprite pool bindings (SPR-06)
    static int lua_newSprite(lua_State* L);
    static int lua_drawSprite(lua_State* L);
    static int lua_updateSprite(lua_State* L);
    static int lua_setFrame(lua_State* L);

    // Layer system bindings (LAYER-06)
    static int lua_setLayer(lua_State* L);
    static int lua_getLayer(lua_State* L);
    static int lua_clearLayer(lua_State* L);
    static int lua_getLayerCount(lua_State* L);
    static int lua_setLayerVisible(lua_State* L);
    static int lua_isLayerVisible(lua_State* L);

    /**
     * @brief Get LuaBindings instance from Lua state
     * @param L Lua state
     * @return LuaBindings instance
     */
    static LuaBindings* getBindings(lua_State* L);
    
    /**
     * @brief Register bindings in a table
     * @param tableName Name of table to create
     * @param functions Array of function bindings
     */
    void registerTable(const std::string& tableName, 
                       const std::vector<std::pair<std::string, lua_CFunction>>& functions);
};

/**
 * @brief High-level Lua scripting interface
 * 
 * Combines LuaEngine and LuaBindings for easy script execution
 * with graphics and UI capabilities.
 */
class LuaScriptSystem {
private:
    LuaEngine engine;       ///< Lua engine
    LuaBindings bindings;   ///< Lua bindings
    LuaCanvas* canvas;      ///< Current canvas
    
public:
    /**
     * @brief Constructor
     */
    LuaScriptSystem();
    
    /**
     * @brief Initialize the script system
     * @return True if successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown the script system
     */
    void shutdown();
    
    /**
     * @brief Set canvas for drawing operations
     * @param canvas Canvas to use
     */
    void setCanvas(LuaCanvas* canvas);
    
    /**
     * @brief Execute Lua script string
     * @param code Lua code to execute
     * @return Execution result
     */
    LuaResult executeScript(const std::string& code);
    
    /**
     * @brief Load and execute Lua script file
     * @param filename Script file path
     * @return Execution result
     */
    LuaResult loadScript(const std::string& filename);
    
    /**
     * @brief Call Lua function
     * @param functionName Function name
     * @param args Function arguments
     * @return Execution result
     */
    template<typename... Args>
    LuaResult callFunction(const std::string& functionName, Args... args) {
        return engine.callFunction(functionName, std::forward<Args>(args)...);
    }
    
    /**
     * @brief Get script system memory usage
     * @return Memory usage in bytes
     */
    size_t getMemoryUsage() const;
    
    /**
     * @brief Get Lua engine reference
     * @return Lua engine
     */
    LuaEngine& getEngine() { return engine; }
    
    /**
     * @brief Get bindings reference
     * @return Lua bindings
     */
    LuaBindings& getBindings() { return bindings; }
};

} // namespace enjin2