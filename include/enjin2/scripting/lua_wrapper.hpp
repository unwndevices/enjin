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
    LuaEngine   engine;    ///< Underlying Lua state owner (must be declared before bindings)
    LuaBindings bindings;  ///< Engine API bindings registered on the Lua state

    LuaWrapper() : bindings(&engine) {}

    /**
     * @brief Initialize the Lua engine and register all bindings.
     * @return true on success, false if the engine failed to initialize
     */
    bool initialize() {
        if (!engine.initialize()) return false;
        bindings.registerAll();
        return true;
    }

    /// @brief Shut down the Lua engine and release its state.
    void shutdown() { engine.shutdown(); }

    /**
     * @brief Execute a Lua code string on the wrapped engine.
     * @param code Null-terminated Lua source to run
     * @return Result of the execution (status + error message on failure)
     */
    LuaResult execute(const char* code) {
        return engine.executeString(code);
    }

    /// @brief Access the underlying Lua engine.
    /// @return Reference to the wrapped LuaEngine
    LuaEngine&   getEngine()   { return engine; }
    /// @brief Access the registered bindings.
    /// @return Reference to the wrapped LuaBindings
    LuaBindings& getBindings() { return bindings; }

    /// @brief Inject the canvas the draw bindings render to.
    /// @param c Canvas pointer (not owned; caller must keep it alive while bindings use it)
    void setCanvas(LuaCanvas* c) { bindings.setCanvas(c); }
};

} // namespace enjin2
