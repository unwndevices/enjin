/**
 * @file lua_script.hpp
 * @brief Platform-agnostic script-driven UI component
 *
 * A drawable component that executes scripts for custom UI rendering.
 * Uses LuaScriptSystem (full Lua) for script execution and LuaCanvas
 * for drawing. Exposes a ScriptProxy to Lua scripts as the first argument
 * to update(self, dt) and draw(self).
 */
#pragma once

#include "../components/drawable.hpp"
#include "../scripting/bindings.hpp"
#include "../core/types.hpp"
#include <string>
#include <memory>

namespace enjin2 {

/**
 * @brief Platform-agnostic script-driven UI component
 *
 * A drawable component that executes Lua scripts for custom UI rendering.
 * Uses LuaScriptSystem for script execution and LuaCanvas for drawing.
 * A ScriptProxy userdata is stored in the Lua registry and passed as the
 * first argument to update(self, dt) and draw(self).
 */
class C_LuaScript : public C_Drawable {
private:
    std::unique_ptr<LuaScriptSystem> scriptSystem;  ///< Lua script system instance
    std::unique_ptr<LuaCanvas> luaCanvas;           ///< Canvas wrapper for Lua bindings
    std::string scriptCode;                          ///< Current script code
    std::string scriptPath;                          ///< Script file path (if loaded from file)
    bool hasScript;                                  ///< Whether script is loaded
    bool scriptError;                                ///< Whether script has errors
    std::string errorMessage;                        ///< Last error message

    // Script lifecycle function names
    static constexpr const char* INIT_FUNCTION   = "init";
    static constexpr const char* UPDATE_FUNCTION = "update";
    static constexpr const char* DRAW_FUNCTION   = "draw";

    // Performance tracking
    float lastUpdateTime;  ///< Last update timestamp in seconds
    uint32_t drawCalls;    ///< Number of draw calls

public:
    /**
     * @brief Constructor with automatic Lua interpreter selection
     * @param owner Owner object
     * @param width Component width
     * @param height Component height
     */
    C_LuaScript(Object* owner, uint16_t width, uint16_t height);

    /**
     * @brief Destructor — invalidates ScriptProxy before closing Lua state
     */
    ~C_LuaScript();

    /**
     * @brief Load script from string
     * @param code Lua script code
     * @return True if loaded successfully
     */
    bool loadScript(const std::string& code);

    /**
     * @brief Load script from file
     * @param filename Path to Lua script file
     * @return True if loaded successfully
     */
    bool loadScriptFile(const std::string& filename);

    /**
     * @brief Reload current script (useful for development)
     * @return True if reloaded successfully
     */
    bool reloadScript();

    /**
     * @brief Clear current script
     */
    void clearScript();

    /**
     * @brief Check if script is loaded
     * @return True if script is loaded
     */
    bool hasLoadedScript() const { return hasScript; }

    /**
     * @brief Check if script has errors
     * @return True if script has errors
     */
    bool hasErrors() const { return scriptError; }

    /**
     * @brief Get last error message
     * @return Error message string
     */
    const std::string& getErrorMessage() const { return errorMessage; }

    /**
     * @brief Set script number variable (expose game state to script)
     * @param name Variable name
     * @param value Numeric value
     */
    void setScriptVar(const std::string& name, double value);

    /**
     * @brief Set script string variable
     * @param name Variable name
     * @param value String value
     */
    void setScriptVar(const std::string& name, const std::string& value);

    /**
     * @brief Set script boolean variable
     * @param name Variable name
     * @param value Boolean value
     */
    void setScriptVar(const std::string& name, bool value);

    /**
     * @brief Get script number variable
     * @param name Variable name
     * @param defaultValue Default value if not found
     * @return Variable value
     */
    double getScriptNumber(const std::string& name, double defaultValue = 0.0);

    /**
     * @brief Get script string variable
     * @param name Variable name
     * @param defaultValue Default value if not found
     * @return String value
     */
    std::string getScriptString(const std::string& name, const std::string& defaultValue = "");

    /**
     * @brief Get script boolean variable
     * @param name Variable name
     * @param defaultValue Default value if not found
     * @return Boolean value
     */
    bool getScriptBool(const std::string& name, bool defaultValue = false);

    /**
     * @brief Call custom script function by name
     * @param functionName Function name to call
     * @return True if call was successful
     */
    bool callScriptFunction(const std::string& functionName);

    /**
     * @brief Update component (calls Lua update(self, dt))
     * @param dt Time since last update in seconds
     */
    void update(float dt) override;

    /**
     * @brief Draw component using 4-bit canvas (calls Lua draw(self))
     * @param canvas 4-bit canvas to draw on
     */
    void draw(ICanvas<Pixel4>& canvas) override;

    /**
     * @brief Get performance stats
     * @return Number of draw calls since creation
     */
    uint32_t getDrawCalls() const { return drawCalls; }

private:
    /**
     * @brief Initialize LuaScriptSystem and expose component dimensions
     * @return True if initialization successful
     */
    bool initializeScriptSystem();

    /**
     * @brief Execute script code with error handling
     * @param code Script code to execute
     * @return True if execution successful
     */
    bool executeScript(const std::string& code);

    /**
     * @brief Call a script lifecycle function with error handling (no proxy argument)
     * @param functionName Function name
     * @return True if call successful
     */
    bool callScriptFunctionSafe(const std::string& functionName);

    /**
     * @brief Push stored ScriptProxy userdata as first arg, then call Lua function via pcall.
     * @param funcName Lua global function name ("init", "update", "draw")
     * @param dt Delta time in seconds — only pushed if passDt is true
     * @param passDt Whether to push dt as second argument (true for update, false for init/draw)
     * @return true if function was found and called without error, false otherwise
     */
    bool callWithProxy(const char* funcName, float dt, bool passDt);

    /**
     * @brief Setup LuaCanvas wrapper for the current draw canvas
     * @param canvas Canvas to wrap
     */
    template<typename CanvasType>
    void setupLuaCanvas(CanvasType& canvas);

    /**
     * @brief Handle script error — sets scriptError flag and records message
     * @param result LuaResult from a failed execute or load call
     */
    void handleScriptError(const LuaResult& result);
};

} // namespace enjin2
