#pragma once

// engine.random's xorshift32 generator, and the seeded generator objects
// engine.random.new(seed) returns (Tomodachi #406; CONTEXT.md "seeded
// generator").
//
// The module functions (engine.random.seed / integer / float) draw from one
// global stream that LuaBindings keeps. A seeded generator is a userdata
// holding its own 32-bit state: the same step and the same ranges, so a seed
// gives one sequence on every host, and what one generator draws never moves
// another or the global stream. Its seed is hashed first (see
// randomGeneratorState), so new(n) is not the stream engine.random.seed(n)
// starts.
//
// Header-only and engine-owned (like effect_lua.hpp) so a host that does not
// link enjin's LuaBindings, or a bare VM in a test, can still make one:
// push randomNew as a function.

#include <cstdint>

#include "lua_api.hpp"
#include "lua_args.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace enjin2 {
namespace lua {

/// Metatable of a seeded generator.
inline const char* randomMt() { return "enjin2.Random"; }

/// The state a seed starts from: 0 would lock xorshift at 0, so it picks a
/// fixed non-zero seed instead.
inline uint32_t randomSeedState(lua_Integer seed) {
    const uint32_t s = static_cast<uint32_t>(seed);
    return s != 0 ? s : 0x12345678u;
}

/// The state a seeded generator starts from: the seed run through MurmurHash3's
/// 32-bit finaliser first. Raw xorshift32 starts every small seed near 0, so
/// seeds 1, 2, 3 (a level number) would open with nearly the same low draws;
/// the finaliser is a bijection, so two seeds still never share a stream.
/// 0 hashes to 0 and so gets randomSeedState's fixed seed.
inline uint32_t randomGeneratorState(lua_Integer seed) {
    uint32_t h = static_cast<uint32_t>(seed);
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return randomSeedState(static_cast<lua_Integer>(h));
}

/// One xorshift32 step: advances `state` and returns it.
inline uint32_t xorshift32(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

/// Push a random integer from the bounds at `arg` and `arg + 1`, both
/// included, in either order.
inline int pushRandomInteger(lua_State* L, uint32_t& state, int arg) {
    int a = lua::checkInt(L, arg);
    int b = lua::checkInt(L, arg + 1);
    if (a > b) { const int t = a; a = b; b = t; }
    const uint32_t r = xorshift32(state);
    const int range = b - a + 1;
    lua_pushinteger(L, a + static_cast<int>(r % static_cast<uint32_t>(range)));
    return 1;
}

/// Push a random number in (0, 1], or scaled to run from the values at `arg`
/// (for 0) and `arg + 1` (for 1) when both are given.
inline int pushRandomFloat(lua_State* L, uint32_t& state, int arg) {
    const uint32_t r = xorshift32(state);
    const double t = static_cast<double>(r) / static_cast<double>(0xFFFFFFFFu);
    if (lua_gettop(L) >= arg + 1) {
        const double a = luaL_checknumber(L, arg);
        const double b = luaL_checknumber(L, arg + 1);
        lua_pushnumber(L, a + t * (b - a));
    } else {
        lua_pushnumber(L, t);
    }
    return 1;
}

namespace detail {

inline uint32_t& checkRandom(lua_State* L) {
    return *static_cast<uint32_t*>(luaL_checkudata(L, 1, randomMt()));
}

inline int random_integer(lua_State* L) { return pushRandomInteger(L, checkRandom(L), 2); }
inline int random_float(lua_State* L) { return pushRandomFloat(L, checkRandom(L), 2); }

} // namespace detail

/// Register the seeded generator's metatable from its descriptors (__index =
/// its methods). Idempotent; the state is a plain uint32_t, so no __gc.
inline void ensureRandomMetatable(lua_State* L) {
    static constexpr LuaApiEntry kMethods[] = {
        luaFunction("integer", detail::random_integer, "(a:int, b:int) -> int",
                    "The generator's next integer from a to b, both included; the bounds may "
                    "come in either order.",
                    "a: one bound\n"
                    "b: the other bound"),
        luaFunction("float", detail::random_float,
                    "() -> number\n"
                    "(a:number, b:number) -> number",
                    "The generator's next number in (0, 1], or scaled to run from a to b.",
                    "a: the value for 0\n"
                    "b: the value for 1"),
    };
    static constexpr LuaApiModule kMethodsModule = luaApiModule(
        LuaApiScope::Methods, "Random",
        "Seeded generator methods, called as rng:name(...): engine.random's draws from a "
        "stream of its own.",
        kMethods);
    static constexpr LuaApiEntry kMeta[] = {
        luaTable("__index", kMethodsModule, "The methods."),
    };
    static constexpr LuaApiModule kMetaModule = luaApiModule(
        LuaApiScope::Metatable, "Random", "Seeded generator method lookup.", kMeta);
    if (luaL_newmetatable(L, randomMt())) {
        luaApiSetFields(L, -1, kMetaModule);
    }
    lua_pop(L, 1);
}

/// engine.random.new(seed): push a new seeded generator.
inline int randomNew(lua_State* L) {
    const uint32_t state = randomGeneratorState(luaL_checkinteger(L, 1));
    ensureRandomMetatable(L);
    *static_cast<uint32_t*>(lua_newuserdata(L, sizeof(uint32_t))) = state;
    luaL_setmetatable(L, randomMt());
    return 1;
}

} // namespace lua
} // namespace enjin2
