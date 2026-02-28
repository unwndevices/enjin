#include "../../include/enjin2/components/lua_script.hpp"
#include "../../include/enjin2/components/timer.hpp"
#include "../../include/enjin2/components/state_machine.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace enjin2 {

C_LuaScript::C_LuaScript(Object* owner, uint16_t width, uint16_t height)
    : C_Drawable(owner, width, height)
    , scriptSystem(nullptr)
    , luaCanvas(nullptr)
    , hasScript(false)
    , scriptError(false)
    , lastUpdateTime(0.0f)
    , drawCalls(0) {
    
    // Initialize script system
    initializeScriptSystem();
}

C_LuaScript::~C_LuaScript() {
    // Invalidate the proxy BEFORE closing the Lua state so that any Lua values
    // holding a reference to self via upvalue will safely get nil on next access.
    if (scriptSystem) {
        lua_State* L = scriptSystem->getEngine().getState();
        if (L && scriptSystem->getEngine().isInitialized()) {
            lua_pushlightuserdata(L, this);
            lua_gettable(L, LUA_REGISTRYINDEX);
            if (lua_isuserdata(L, -1)) {
                ScriptProxy* proxy = static_cast<ScriptProxy*>(lua_touserdata(L, -1));
                if (proxy) proxy->valid = false;
            }
            lua_pop(L, 1);
        }
        // TIMER-05: Release any pending timer callbacks before closing the Lua state.
        // This handles the destruction-order pitfall: if C_LuaScript destructs before
        // C_Timer (components[0] before components[1]), we must release luaL_ref handles
        // while the Lua state is still alive.
        if (owner) {
            C_Timer* timer = owner->getComponent<C_Timer>();
            if (timer) {
                timer->clearTimers();
            }
        }
        // Phase 41: Release FSM state callbacks before closing the Lua state.
        if (owner) {
            C_StateMachine* fsm = owner->getComponent<C_StateMachine>();
            if (fsm) {
                fsm->clearStates();
            }
        }
        scriptSystem->shutdown();
    }
}

bool C_LuaScript::initializeScriptSystem() {
    try {
        scriptSystem = std::unique_ptr<LuaScriptSystem>(new LuaScriptSystem());
        if (!scriptSystem->initialize()) {
            snprintf(errorMessage, sizeof(errorMessage), "%s", "Failed to initialize Lua script system");
            scriptError = true;
            return false;
        }
        
        // Expose component dimensions to script
        scriptSystem->getEngine().setGlobal("COMPONENT_WIDTH", static_cast<double>(GetWidth()));
        scriptSystem->getEngine().setGlobal("COMPONENT_HEIGHT", static_cast<double>(GetHeight()));
        
        return true;
    } catch (const std::exception& e) {
        snprintf(errorMessage, sizeof(errorMessage), "Script system initialization error: %s", e.what());
        scriptError = true;
        return false;
    }
}

bool C_LuaScript::loadScript(const std::string& code) {
    if (!scriptSystem) {
        snprintf(errorMessage, sizeof(errorMessage), "%s", "Script system not initialized");
        scriptError = true;
        return false;
    }
    
    scriptCode = code;
    scriptPath.clear();
    
    return executeScript(code);
}

bool C_LuaScript::loadScriptFile(const std::string& filename) {
    if (!scriptSystem) {
        snprintf(errorMessage, sizeof(errorMessage), "%s", "Script system not initialized");
        scriptError = true;
        return false;
    }
    
    scriptPath = filename;
    
    LuaResult result = scriptSystem->loadScript(filename);
    if (!result.success) {
        handleScriptError(result);
        return false;
    }
    
    hasScript = true;
    scriptError = false;
    errorMessage[0] = '\0';

    // Create ScriptProxy userdata and store in Lua registry (mirrors executeScript() block)
    {
        lua_State* L = scriptSystem->getEngine().getState();
        if (L) {
            // Invalidate old proxy if present (handles reload via loadScriptFile)
            lua_pushlightuserdata(L, this);
            lua_gettable(L, LUA_REGISTRYINDEX);
            if (lua_isuserdata(L, -1)) {
                ScriptProxy* oldProxy = static_cast<ScriptProxy*>(lua_touserdata(L, -1));
                if (oldProxy) oldProxy->valid = false;
            }
            lua_pop(L, 1);

            // TIMER-05: Clear any pending timer callbacks from the previous script load.
            if (owner) {
                C_Timer* timerComp = owner->getComponent<C_Timer>();
                if (timerComp) {
                    timerComp->clearTimers();
                }
            }
            // Phase 41: Clear FSM state callbacks from the previous script load.
            if (owner) {
                C_StateMachine* fsmComp = owner->getComponent<C_StateMachine>();
                if (fsmComp) {
                    fsmComp->clearStates();
                }
            }
            // Phase 42 (EVENT-05): Clear event bus handlers from the previous script load.
            // After clearHandlers() sets m_L=nullptr, setLuaState(L) re-arms for the new Lua state.
            scriptSystem->getBindings().getEventBus().clearHandlers();
            scriptSystem->getBindings().getEventBus().setLuaState(L);

            // Create new userdata and assign metatable
            ScriptProxy* proxy = static_cast<ScriptProxy*>(
                lua_newuserdata(L, sizeof(ScriptProxy)));
            proxy->component = this;
            proxy->valid = true;

            luaL_getmetatable(L, "ScriptProxy");
            if (lua_isnil(L, -1)) {
                // Metatable not registered yet — pop nil and the userdata; skip proxy creation
                lua_pop(L, 2);
            } else {
                lua_setmetatable(L, -2);  // pops metatable; userdata is now top

                // Store: registry[lightuserdata(this)] = proxy_userdata
                lua_pushlightuserdata(L, this);   // key
                lua_insert(L, -2);                // swap: key below value
                lua_settable(L, LUA_REGISTRYINDEX);
            }
        }
    }

    // Call init function if it exists (passes proxy as first arg)
    callWithProxy(INIT_FUNCTION, 0.0f, false);

    return true;
}

bool C_LuaScript::reloadScript() {
    if (scriptPath.empty() && scriptCode.empty()) {
        snprintf(errorMessage, sizeof(errorMessage), "%s", "No script to reload");
        scriptError = true;
        return false;
    }
    
    if (!scriptPath.empty()) {
        return loadScriptFile(scriptPath);
    } else {
        return loadScript(scriptCode);
    }
}

void C_LuaScript::clearScript() {
    scriptCode.clear();
    scriptPath.clear();
    hasScript = false;
    scriptError = false;
    errorMessage[0] = '\0';
}

bool C_LuaScript::executeScript(const std::string& code) {
    LuaResult result = scriptSystem->executeScript(code);
    if (!result.success) {
        handleScriptError(result);
        return false;
    }

    hasScript = true;
    scriptError = false;
    errorMessage[0] = '\0';

    // Create ScriptProxy userdata and store in Lua registry for reuse each frame.
    // Registry key: lightuserdata(this) — unique per C_LuaScript instance.
    {
        lua_State* L = scriptSystem->getEngine().getState();
        if (L) {
            // Check if a previous proxy exists and invalidate it first (handles reload)
            lua_pushlightuserdata(L, this);
            lua_gettable(L, LUA_REGISTRYINDEX);
            if (lua_isuserdata(L, -1)) {
                ScriptProxy* oldProxy = static_cast<ScriptProxy*>(lua_touserdata(L, -1));
                if (oldProxy) oldProxy->valid = false;
            }
            lua_pop(L, 1);

            // TIMER-05: Clear any pending timer callbacks from the previous script load.
            // This prevents stale Lua function refs from firing after hot-reload.
            if (owner) {
                C_Timer* timerComp = owner->getComponent<C_Timer>();
                if (timerComp) {
                    timerComp->clearTimers();
                }
            }
            // Phase 41: Clear FSM state callbacks from the previous script load.
            if (owner) {
                C_StateMachine* fsmComp = owner->getComponent<C_StateMachine>();
                if (fsmComp) {
                    fsmComp->clearStates();
                }
            }
            // Phase 42 (EVENT-05): Clear event bus handlers from the previous script load.
            // After clearHandlers() sets m_L=nullptr, setLuaState(L) re-arms for the new Lua state.
            scriptSystem->getBindings().getEventBus().clearHandlers();
            scriptSystem->getBindings().getEventBus().setLuaState(L);

            // Create new userdata and assign metatable
            ScriptProxy* proxy = static_cast<ScriptProxy*>(
                lua_newuserdata(L, sizeof(ScriptProxy)));
            proxy->component = this;
            proxy->valid = true;

            luaL_getmetatable(L, "ScriptProxy");
            if (lua_isnil(L, -1)) {
                // Metatable not registered yet — pop nil and the userdata; skip proxy creation
                lua_pop(L, 2);
            } else {
                lua_setmetatable(L, -2);  // pops metatable; userdata is now top

                // Store: registry[lightuserdata(this)] = proxy_userdata
                lua_pushlightuserdata(L, this);   // key
                lua_insert(L, -2);                // swap: key below value
                lua_settable(L, LUA_REGISTRYINDEX);
            }
        }
    }

    // Call init function if it exists (passes proxy as first arg)
    callWithProxy(INIT_FUNCTION, 0.0f, false);

    return true;
}

void C_LuaScript::setScriptVar(const std::string& name, double value) {
    if (scriptSystem) {
        scriptSystem->getEngine().setGlobal(name, value);
    }
}

void C_LuaScript::setScriptVar(const std::string& name, const std::string& value) {
    if (scriptSystem) {
        scriptSystem->getEngine().setGlobal(name, value);
    }
}

void C_LuaScript::setScriptVar(const std::string& name, bool value) {
    if (scriptSystem) {
        scriptSystem->getEngine().setGlobal(name, value);
    }
}

double C_LuaScript::getScriptNumber(const std::string& name, double defaultValue) {
    if (scriptSystem) {
        return scriptSystem->getEngine().getGlobalNumber(name, defaultValue);
    }
    return defaultValue;
}

std::string C_LuaScript::getScriptString(const std::string& name, const std::string& defaultValue) {
    if (scriptSystem) {
        return scriptSystem->getEngine().getGlobalString(name, defaultValue);
    }
    return defaultValue;
}

bool C_LuaScript::getScriptBool(const std::string& name, bool defaultValue) {
    if (scriptSystem) {
        return scriptSystem->getEngine().getGlobalBool(name, defaultValue);
    }
    return defaultValue;
}

void C_LuaScript::setInput(InputState* input) {
    if (scriptSystem) {
        scriptSystem->getBindings().setInput(input);
    }
}

bool C_LuaScript::callScriptFunction(const std::string& functionName) {
    return callScriptFunctionSafe(functionName);
}

void C_LuaScript::update(float dt) {
    Component::update(dt);

    if (!hasScript || scriptError || !scriptSystem) {
        return;
    }

    // INPUT-03: fire input edge callbacks before update()
    InputState* input = scriptSystem->getBindings().getInput();
    if (input) {
        dispatchInputCallbacks(*input);
    }

    lastUpdateTime += dt;

    // Expose delta time to script (dt is already seconds)
    setScriptVar("dt", static_cast<double>(dt));
    setScriptVar("time", static_cast<double>(lastUpdateTime));

    // Call update function if it exists (passes proxy as first arg, dt as second)
    callWithProxy(UPDATE_FUNCTION, dt, true);
}

template<typename CanvasType>
void C_LuaScript::setupLuaCanvas(CanvasType& canvas) {
    // Create LuaCanvas wrapper for the provided canvas via abstract interface constructor
    luaCanvas = std::unique_ptr<LuaCanvas>(new LuaCanvas(static_cast<ICanvas<Pixel4>*>(&canvas)));
    scriptSystem->setCanvas(luaCanvas.get());
}

void C_LuaScript::draw(ICanvas<Pixel4>& canvas) {
    if (!isVisible() || !hasScript || scriptError || !scriptSystem) {
        return;
    }
    
    drawCalls++;
    
    // Setup canvas for Lua
    setupLuaCanvas(canvas);
    
    // Call draw function if it exists (passes proxy as first arg)
    callWithProxy(DRAW_FUNCTION, 0.0f, false);
}

bool C_LuaScript::callScriptFunctionSafe(const std::string& functionName) {
    if (!scriptSystem) {
        return false;
    }

    try {
        LuaResult result = scriptSystem->callFunction(functionName);
        if (!result.success) {
            // Don't treat missing functions as errors for optional lifecycle functions
            if (functionName == INIT_FUNCTION || functionName == UPDATE_FUNCTION || functionName == DRAW_FUNCTION) {
                return false; // Function doesn't exist, but that's okay
            }
            handleScriptError(result);
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        snprintf(errorMessage, sizeof(errorMessage), "Script function call error: %s", e.what());
        scriptError = true;
        return false;
    }
}

bool C_LuaScript::callWithProxy(const char* funcName, float dt, bool passDt) {
    if (!scriptSystem) return false;

    lua_State* L = scriptSystem->getEngine().getState();
    if (!L) return false;

    // Push function
    lua_getglobal(L, funcName);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return false;  // Function not defined — optional lifecycle function, not an error
    }

    // Retrieve stored proxy userdata from Lua registry (keyed by this pointer)
    lua_pushlightuserdata(L, this);
    lua_gettable(L, LUA_REGISTRYINDEX);

    // If proxy is not in registry (e.g. first call before executeScript stores it),
    // push nil as a safe fallback rather than crashing.
    // This should not happen in normal operation.

    int nargs = 1;  // proxy is always arg 1
    if (passDt) {
        lua_pushnumber(L, static_cast<lua_Number>(dt));
        nargs = 2;
    }

    int result = lua_pcall(L, nargs, 0, 0);
    if (result != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        snprintf(errorMessage, sizeof(errorMessage), "%s", err ? err : "unknown Lua error");
        lua_pop(L, 1);

        switch (errorPolicy) {
            case ScriptErrorPolicy::Disable:
                if (!scriptError) {
                    printf("[lua] script error (%s): %s\n", funcName, errorMessage);
                }
                scriptError = true;
                break;

            case ScriptErrorPolicy::Log:
                printf("[lua] script error (%s): %s\n", funcName, errorMessage);
                // scriptError intentionally NOT set — script runs again next frame
                break;

            case ScriptErrorPolicy::Panic:
                printf("[lua] PANIC (%s): %s\n", funcName, errorMessage);
#ifdef ESP32
                esp_restart();
#else
                std::abort();
#endif
                break;
        }
        return false;
    }
    return true;
}

bool C_LuaScript::callWithProxyAndBtn(const char* funcName, int btn) {
    if (!scriptSystem) return false;

    lua_State* L = scriptSystem->getEngine().getState();
    if (!L) return false;

    lua_getglobal(L, funcName);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return false;  // optional callback — not defined is not an error
    }

    // Retrieve stored proxy userdata from Lua registry (keyed by this pointer)
    lua_pushlightuserdata(L, this);
    lua_gettable(L, LUA_REGISTRYINDEX);

    // Push btn as integer second arg
    lua_pushinteger(L, static_cast<lua_Integer>(btn));

    // call: funcName(self, btn)  — 2 args, 0 results
    int result = lua_pcall(L, 2, 0, 0);
    if (result != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        snprintf(errorMessage, sizeof(errorMessage), "%s", err ? err : "unknown Lua error");
        lua_pop(L, 1);

        switch (errorPolicy) {
            case ScriptErrorPolicy::Disable:
                if (!scriptError) {
                    printf("[lua] script error (%s): %s\n", funcName, errorMessage);
                }
                scriptError = true;
                break;

            case ScriptErrorPolicy::Log:
                printf("[lua] script error (%s): %s\n", funcName, errorMessage);
                // scriptError intentionally NOT set — script runs again next frame
                break;

            case ScriptErrorPolicy::Panic:
                printf("[lua] PANIC (%s): %s\n", funcName, errorMessage);
#ifdef ESP32
                esp_restart();
#else
                std::abort();
#endif
                break;
        }
        return false;
    }
    return true;
}

void C_LuaScript::dispatchInputCallbacks(const InputState& input) {
    if (!hasScript || scriptError || !scriptSystem) return;
    for (int btn = 0; btn < 16; ++btn) {
        if (input.justPressed(btn)) {
            callWithProxyAndBtn("on_button_pressed", btn);
        }
        if (input.justReleased(btn)) {
            callWithProxyAndBtn("on_button_released", btn);
        }
    }
}

void C_LuaScript::handleScriptError(const LuaResult& result) {
    scriptError = true;
    snprintf(errorMessage, sizeof(errorMessage), "%s", result.error.c_str());
    // Could add logging here in the future
}

} // namespace enjin2