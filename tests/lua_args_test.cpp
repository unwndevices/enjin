/**
 * @file lua_args_test.cpp
 * @brief enjin2::lua's typed getters: the names of a closed set (ADR-0030).
 *        A known name gives its index; an unknown one raises, naming the
 *        argument and the name; an absent optional one gives the default.
 */
#include <enjin2/scripting/lua_args.hpp>
#include <cstdio>
#include <string>

extern "C" {
#include "lualib.h"
}

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

namespace {

constexpr const char* kModes[] = {"forward", "reverse", "pingpong"};

int pick(lua_State* L) {
    lua_pushinteger(L, enjin2::lua::checkName(L, 1, kModes, "mode"));
    return 1;
}
int pickOpt(lua_State* L) {
    lua_pushinteger(L, enjin2::lua::optName(L, 1, kModes, "mode", 0));
    return 1;
}
int pickField(lua_State* L) {
    lua_pushinteger(L, enjin2::lua::optFieldName(L, lua_istable(L, 1) ? 1 : 0, "mode", kModes, "mode", 0));
    return 1;
}

// "" when `code` ran, else its error.
std::string run(lua_State* L, const char* code) {
    if (luaL_dostring(L, code) == LUA_OK) return "";
    std::string err = lua_tostring(L, -1);
    lua_pop(L, 1);
    return err;
}

bool truthy(lua_State* L, const char* expr) {
    const std::string code = std::string("return ") + expr;
    if (luaL_dostring(L, code.c_str()) != LUA_OK) return false;
    const bool v = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return v;
}

void test_names() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "pick", pick);
    lua_register(L, "pickOpt", pickOpt);
    lua_register(L, "pickField", pickField);

    ASSERT(truthy(L, "pick('reverse') == 1"), "a known name gives its index");
    const std::string err = run(L, "pick('fwd')");
    ASSERT(err.find("bad argument #1") != std::string::npos, "an unknown name raises on its argument");
    ASSERT(err.find("unknown mode 'fwd'") != std::string::npos, "and names what it is and the name");
    ASSERT(run(L, "pick(3)").find("unknown mode '3'") != std::string::npos, "a number is not a name");
    ASSERT(run(L, "pick()") != "", "a missing name raises");
    ASSERT(truthy(L, "pickOpt() == 0 and pickOpt(nil) == 0"), "an absent optional name is the default");
    ASSERT(truthy(L, "pickOpt('pingpong') == 2"), "a given optional name is checked like any");
    ASSERT(run(L, "pickOpt('walk')").find("unknown mode 'walk'") != std::string::npos,
           "an unknown optional name raises");
    ASSERT(truthy(L, "pickField() == 0 and pickField({}) == 0"), "an absent field is the default");
    ASSERT(truthy(L, "pickField({mode = 'reverse'}) == 1"), "a field's name gives its index");
    const std::string field = run(L, "pickField({mode = 'sideways'})");
    ASSERT(field.find("'mode'") != std::string::npos && field.find("unknown mode 'sideways'") != std::string::npos,
           "an unknown field name raises, naming the field");
    lua_close(L);
}

} // namespace

int main() {
    printf("lua_args_test — closed-set names raise when unknown (ADR-0030)\n\n");
    test_names();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
