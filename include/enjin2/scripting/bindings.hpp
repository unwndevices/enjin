/**
 * @file bindings.hpp
 * @brief Lua bindings for Enjin graphics, canvas operations, and scripting interface
 *
 * Provides LuaCanvas wrapper for 4-bit/8-bit canvases and LuaBindings
 * for love2d.graphics-style API familiar to Lua developers.
 */
#pragma once

#include <cstring>
#include "lua_engine.hpp"
#include "lua_platform.hpp"
#include "object_proxy.hpp"
#include "../graphics/canvas.hpp"
#include "../graphics/gfxfont.h"
#include "../graphics/primitives.hpp"
#include "../graphics/sprite.hpp"
#include "../input/input_state.hpp"
#include "../core/math.hpp"
#include "../core/collision.hpp"

namespace enjin2 {

// Forward declarations for engine.* pointer injection
// Full includes are in bindings.cpp only (avoids circular include issues)
class Scene;
class SceneStateMachine;

// Forward declaration — prevents circular include with lua_script.hpp
class C_LuaScript;

/**
 * @brief Lua proxy userdata wrapping a C_LuaScript component.
 * Exposes component properties (x, y, visible, layer, name, active) via metamethods.
 * Validity flag prevents dangling-pointer access after Object destruction.
 */
struct ScriptProxy {
    C_LuaScript* component;   ///< Non-owning pointer to the component. Do NOT dereference if valid == false.
    bool valid;               ///< Set to false by C_LuaScript destructor before lua_close.
};

/**
 * @brief Time state for engine.time.* Lua bindings
 * Updated by host each frame via LuaBindings::setTimeState() before update() is called.
 */
struct EngineTimeState {
    float dt{0.0f};            ///< Current frame delta in seconds
    float totalTime{0.0f};     ///< Accumulated total time in seconds
    uint32_t frameCount{0};    ///< Frame counter, incremented each frame
};

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
     * @brief Constructor for abstract 4-bit canvas interface
     * Used when only an ICanvas<Pixel4>& is available (e.g., C_Drawable::draw()).
     * Width and height are read from the interface at construction time.
     * @param canvas Abstract 4-bit canvas pointer (non-owning)
     */
    explicit LuaCanvas(ICanvas<Pixel4>* canvas)
        : canvasPtr(canvas), is4Bit(true)
        , width(canvas ? canvas->getWidth() : 0)
        , height(canvas ? canvas->getHeight() : 0) {}

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

    /**
     * @brief Draw text at position using given color, size, and font
     * @param str Null-terminated string to draw
     * @param x X coordinate
     * @param y Y coordinate
     * @param color Text color (0-15 for 4-bit, 0-255 for 8-bit)
     * @param size Size multiplier (1=normal, 2=double, etc.)
     * @param font GFXfont pointer (nullptr = built-in 5x7)
     */
    void drawText(const char* str, int16_t x, int16_t y,
                  uint8_t color, uint8_t size, const GFXfont* font);

    /**
     * @brief Draw text with word wrapping within maxWidth
     * @param str Null-terminated string to draw
     * @param x X coordinate
     * @param y Y coordinate
     * @param maxWidth Maximum width in pixels before wrapping
     * @param color Text color
     * @param size Size multiplier
     * @param font GFXfont pointer (nullptr = built-in 5x7)
     */
    void drawTextWrapped(const char* str, int16_t x, int16_t y,
                        uint16_t maxWidth, uint8_t color, uint8_t size,
                        const GFXfont* font);

    /**
     * @brief Measure width of string in pixels with given size and font
     * @param str Null-terminated string to measure
     * @param size Size multiplier
     * @param font GFXfont pointer (nullptr = built-in 5x7)
     * @return Width in pixels
     */
    uint16_t measureTextWidth(const char* str, uint8_t size, const GFXfont* font);

    /**
     * @brief Measure character height in pixels with given size and font
     * @param size Size multiplier
     * @param font GFXfont pointer (nullptr = built-in 5x7)
     * @return Height in pixels
     */
    uint8_t measureTextHeight(uint8_t size, const GFXfont* font);
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

    // ── Text state ───────────────────────────────────────────────────────────
    uint8_t currentTextSize{1};       ///< Text size multiplier (1=normal, 2=double, etc.)
    const GFXfont* currentFont{nullptr}; ///< nullptr = built-in 5x7
    char currentFontName[32]{"default"};

    static constexpr int MAX_FONTS = 8; ///< Fixed font registry size
    struct FontEntry {
        char name[32];
        const GFXfont* font;
    };
    FontEntry fontRegistry[MAX_FONTS]{};
    uint8_t fontCount{0};

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

    // ── engine.* injection pointers ──────────────────────────────────────────────
    SceneStateMachine* m_ssm{nullptr};        ///< Non-owning; null in SDL standalone mode
    Scene*             m_activeScene{nullptr}; ///< Non-owning; current active scene
    EngineTimeState    m_timeState;            ///< Updated by host each frame

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
     * @brief Get current frame's input state
     * @return Pointer to InputState set by host this frame, or nullptr if not set
     */
    InputState* getInput() const { return currentInput; }

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
     * on every Lua state reload. Also resets drawing state (currentColor, lineWidth, text state).
     */
    void resetSpritePool();

    /**
     * @brief Register a named font for use from Lua via setFont(name).
     * @param name Name to use in Lua (e.g. "myfont"); "default" = built-in 5x7, "default8" is pre-registered.
     * @param font Pointer to GFXfont (non-owning); may be nullptr for "default" to mean built-in 5x7.
     */
    void registerFont(const char* name, const GFXfont* font);

    /**
     * @brief Inject SceneStateMachine pointer for engine.scene.switch()
     * @param ssm Non-owning pointer; may be nullptr in SDL standalone mode
     */
    void setSceneStateMachine(SceneStateMachine* ssm) { m_ssm = ssm; }

    /**
     * @brief Inject active Scene pointer for engine.scene.find()
     * @param scene Non-owning pointer to the currently active scene; may be nullptr
     */
    void setActiveScene(Scene* scene) { m_activeScene = scene; }

    /**
     * @brief Update time state for engine.time.* bindings (call before each frame's update)
     * @param dt Current frame delta in seconds
     * @param totalTime Accumulated total time in seconds
     * @param frameCount Current frame counter
     */
    void setTimeState(float dt, float totalTime, uint32_t frameCount) {
        m_timeState.dt         = dt;
        m_timeState.totalTime  = totalTime;
        m_timeState.frameCount = frameCount;
    }

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

    // Text bindings
    static int lua_text(lua_State* L);
    static int lua_textWrapped(lua_State* L);
    static int lua_setTextSize(lua_State* L);
    static int lua_getTextSize(lua_State* L);
    static int lua_setFont(lua_State* L);
    static int lua_getFont(lua_State* L);
    static int lua_getTextWidth(lua_State* L);
    static int lua_getTextHeight(lua_State* L);

    // engine.* table binding functions (ENG-01..ENG-06)
    static int lua_engine_scene_switch(lua_State* L);
    static int lua_engine_scene_find(lua_State* L);
    static int lua_engine_input_held(lua_State* L);
    static int lua_engine_input_just_pressed(lua_State* L);
    static int lua_engine_input_just_released(lua_State* L);
    static int lua_engine_input_axis(lua_State* L);
    static int lua_engine_time_delta(lua_State* L);
    static int lua_engine_time_now(lua_State* L);
    static int lua_engine_time_frame(lua_State* L);
    static int lua_engine_log(lua_State* L);

    // engine.lua.* GC bindings (GC-01, GC-02)
    static int lua_engine_lua_collect(lua_State* L);
    static int lua_engine_lua_memory(lua_State* L);

    // engine.collision.* binding functions
    static int lua_engine_collision_aabb(lua_State* L);
    static int lua_engine_collision_circleCircle(lua_State* L);
    static int lua_engine_collision_pointInRect(lua_State* L);
    static int lua_engine_collision_pointInCircle(lua_State* L);
    static int lua_engine_collision_lineLine(lua_State* L);
    static int lua_engine_collision_lineCircle(lua_State* L);

    /**
     * @brief Register engine.* global table (called from registerAll())
     * Builds engine.scene, engine.input, engine.time, engine.lua sub-tables and engine.log.
     * All C++ pointers (SSM, activeScene, timeState) must be stored in the Lua registry
     * during this call so closures can retrieve them at call time.
     */
    void registerEngineTable();
    void registerProxyMetatable();

    /**
     * @brief Register the ObjectProxy metatable (called from registerAll()).
     * Provides __index (name/hasTag/position/enable read) and __newindex
     * (position write + enable/disable control via C_LuaScript::setEnabled()).
     */
    void registerObjectProxyMetatable();

    /**
     * @brief Register Vec2/Point/Rect metatables and math utility globals (called from registerAll())
     */
    void registerMathBindings();

    // ── Math type constructors ──────────────────────────────────────────────
    static int lua_Vec2_new(lua_State* L);
    static int lua_Point_new(lua_State* L);
    static int lua_Rect_new(lua_State* L);

    // ── Vec2 metamethods ────────────────────────────────────────────────────
    static int lua_Vec2_add(lua_State* L);
    static int lua_Vec2_sub(lua_State* L);
    static int lua_Vec2_mul(lua_State* L);
    static int lua_Vec2_div(lua_State* L);
    static int lua_Vec2_unm(lua_State* L);
    static int lua_Vec2_eq(lua_State* L);
    static int lua_Vec2_tostring(lua_State* L);
    static int lua_Vec2_index(lua_State* L);
    static int lua_Vec2_newindex(lua_State* L);

    // Vec2 methods (dispatched via __index)
    static int lua_Vec2_length(lua_State* L);
    static int lua_Vec2_lengthSquared(lua_State* L);
    static int lua_Vec2_normalized(lua_State* L);
    static int lua_Vec2_dot(lua_State* L);
    static int lua_Vec2_cross(lua_State* L);
    static int lua_Vec2_distance(lua_State* L);
    static int lua_Vec2_angle(lua_State* L);
    static int lua_Vec2_rotate(lua_State* L);

    // ── Point metamethods ───────────────────────────────────────────────────
    static int lua_Point_add(lua_State* L);
    static int lua_Point_sub(lua_State* L);
    static int lua_Point_eq(lua_State* L);
    static int lua_Point_tostring(lua_State* L);
    static int lua_Point_index(lua_State* L);
    static int lua_Point_newindex(lua_State* L);

    // ── Rect metamethods ────────────────────────────────────────────────────
    static int lua_Rect_eq(lua_State* L);
    static int lua_Rect_tostring(lua_State* L);
    static int lua_Rect_index(lua_State* L);
    static int lua_Rect_newindex(lua_State* L);

    // Rect methods (dispatched via __index)
    static int lua_Rect_contains(lua_State* L);
    static int lua_Rect_intersects(lua_State* L);

    // ── Math utility globals ────────────────────────────────────────────────
    static int lua_math_clamp(lua_State* L);
    static int lua_math_lerp(lua_State* L);
    static int lua_math_remap(lua_State* L);
    static int lua_math_sign(lua_State* L);
    static int lua_math_smoothstep(lua_State* L);
    static int lua_math_distance(lua_State* L);

    static LuaBindings* getBindings(lua_State* L);
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