/**
 * @file lua_wrapper.hpp
 * @brief Convenience wrapper combining LuaEngine + LuaBindings for test fixtures.
 *
 * Provides the simplified API that sprite_load_test.cpp and other GTest-based
 * tests expect: initialize(), execute(), shutdown(), getEngine(), getBindings().
 *
 * Header-only — all logic delegates to LuaEngine + LuaBindings which have their
 * own .cpp implementations. No companion .cpp file needed.
 */
#pragma once
#include "bindings.hpp"

namespace enjin2 {

/**
 * @brief Convenience wrapper combining LuaEngine + LuaBindings for test fixtures.
 *
 * Provides the simplified API that sprite_load_test.cpp and other GTest-based
 * tests expect: initialize(), execute(), shutdown(), getEngine(), getBindings().
 *
 * Member declaration order matters: engine MUST be declared before bindings
 * because C++ initializes members in declaration order, and the LuaBindings
 * constructor takes LuaEngine* (pointer is valid even before engine.initialize()).
 */
class LuaWrapper {
public:
    LuaEngine   engine;
    LuaBindings bindings;

    LuaWrapper() : bindings(&engine) {}

    bool initialize() {
        if (!engine.initialize()) return false;
        bindings.registerAll();
        return true;
    }

    void shutdown() { engine.shutdown(); }

    LuaResult execute(const char* code) {
        return engine.executeString(code);
    }

    LuaEngine&   getEngine()   { return engine; }
    LuaBindings& getBindings() { return bindings; }

    void setCanvas(LuaCanvas* c) { bindings.setCanvas(c); }
};

} // namespace enjin2
