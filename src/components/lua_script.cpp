#include "../../include/enjin2/components/lua_script.hpp"
#include <cstring>

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
    if (scriptSystem) {
        scriptSystem->shutdown();
    }
}

bool C_LuaScript::initializeScriptSystem() {
    try {
        scriptSystem = std::unique_ptr<LuaScriptSystem>(new LuaScriptSystem());
        if (!scriptSystem->initialize()) {
            errorMessage = "Failed to initialize Lua script system";
            scriptError = true;
            return false;
        }
        
        // Expose component dimensions to script
        scriptSystem->getEngine().setGlobal("COMPONENT_WIDTH", static_cast<double>(getWidth()));
        scriptSystem->getEngine().setGlobal("COMPONENT_HEIGHT", static_cast<double>(getHeight()));
        
        return true;
    } catch (const std::exception& e) {
        errorMessage = std::string("Script system initialization error: ") + e.what();
        scriptError = true;
        return false;
    }
}

bool C_LuaScript::loadScript(const std::string& code) {
    if (!scriptSystem) {
        errorMessage = "Script system not initialized";
        scriptError = true;
        return false;
    }
    
    scriptCode = code;
    scriptPath.clear();
    
    return executeScript(code);
}

bool C_LuaScript::loadScriptFile(const std::string& filename) {
    if (!scriptSystem) {
        errorMessage = "Script system not initialized";
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
    errorMessage.clear();
    
    // Call init function if it exists
    callScriptFunctionSafe(INIT_FUNCTION);
    
    return true;
}

bool C_LuaScript::reloadScript() {
    if (scriptPath.empty() && scriptCode.empty()) {
        errorMessage = "No script to reload";
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
    errorMessage.clear();
}

bool C_LuaScript::executeScript(const std::string& code) {
    LuaResult result = scriptSystem->executeScript(code);
    if (!result.success) {
        handleScriptError(result);
        return false;
    }
    
    hasScript = true;
    scriptError = false;
    errorMessage.clear();
    
    // Call init function if it exists
    callScriptFunctionSafe(INIT_FUNCTION);
    
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

bool C_LuaScript::callScriptFunction(const std::string& functionName) {
    return callScriptFunctionSafe(functionName);
}

void C_LuaScript::update(float dt) {
    Component::update(dt);

    if (!hasScript || scriptError || !scriptSystem) {
        return;
    }

    lastUpdateTime += dt;

    // Expose delta time to script (dt is already seconds)
    setScriptVar("dt", static_cast<double>(dt));
    setScriptVar("time", static_cast<double>(lastUpdateTime));
    
    // Call update function if it exists
    callScriptFunctionSafe(UPDATE_FUNCTION);
}

template<typename CanvasType>
void C_LuaScript::setupLuaCanvas(CanvasType& canvas) {
    // Create LuaCanvas wrapper for the provided canvas
    luaCanvas = std::unique_ptr<LuaCanvas>(new LuaCanvas(&canvas));
    scriptSystem->setCanvas(luaCanvas.get());
}

void C_LuaScript::draw(ICanvas<Pixel4>& canvas) {
    if (!isVisible() || !hasScript || scriptError || !scriptSystem) {
        return;
    }
    
    drawCalls++;
    
    // Setup canvas for Lua
    setupLuaCanvas(canvas);
    
    // Call draw function if it exists
    callScriptFunctionSafe(DRAW_FUNCTION);
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
        errorMessage = std::string("Script function call error: ") + e.what();
        scriptError = true;
        return false;
    }
}

void C_LuaScript::handleScriptError(const LuaResult& result) {
    scriptError = true;
    errorMessage = result.error;
    // Could add logging here in the future
}

} // namespace enjin2