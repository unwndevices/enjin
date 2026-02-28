#include "../../include/enjin2/scripting/bindings.hpp"

namespace enjin2 {

//==============================================================================
// LuaScriptSystem Implementation
//==============================================================================

LuaScriptSystem::LuaScriptSystem() : bindings(&engine), canvas(nullptr) {
}

bool LuaScriptSystem::initialize() {
    if (!engine.initialize()) {
        return false;
    }

    bindings.registerAll();
    return true;
}

void LuaScriptSystem::shutdown() {
    engine.shutdown();
}

void LuaScriptSystem::setCanvas(LuaCanvas* canvas) {
    this->canvas = canvas;
    bindings.setCanvas(canvas);
}

LuaResult LuaScriptSystem::executeScript(const std::string& code) {
    return engine.executeString(code);
}

LuaResult LuaScriptSystem::loadScript(const std::string& filename) {
    return engine.executeFile(filename);
}

size_t LuaScriptSystem::getMemoryUsage() const {
    return engine.getMemoryUsage();
}

} // namespace enjin2
