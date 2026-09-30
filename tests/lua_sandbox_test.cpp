/**
 * @file lua_sandbox_test.cpp
 * @brief The script sandbox is the same on every platform (Tomodachi ADR-0012, #251)
 *
 * A LuaEngine on the desktop host (the build the web uses too) must expose what
 * the device exposes: no io/os/debug/package/dofile/loadfile/require, while
 * load, pcall and the pure libraries stay.
 */
#include <enjin2/scripting/lua_engine.hpp>
#include <cstdio>

using namespace enjin2;

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

static void test_sandboxed_globals_are_nil()
{
    printf("--- sandboxed globals are nil ---\n");
    LuaEngine engine;
    ASSERT(engine.initialize(), "LuaEngine::initialize should succeed");
    const LuaResult r = engine.executeString(
        "removed = (io == nil and os == nil and debug == nil and package == nil\n"
        "           and dofile == nil and loadfile == nil and require == nil)\n"
        "kept = (type(load) == 'function' and type(pcall) == 'function'\n"
        "        and type(string) == 'table' and type(table) == 'table'\n"
        "        and type(math) == 'table' and type(coroutine) == 'table'\n"
        "        and type(utf8) == 'table')\n");
    ASSERT(r.success, "probe script should run");
    ASSERT(engine.getGlobalBool("removed"),
           "io/os/debug/package/dofile/loadfile/require must be nil");
    ASSERT(engine.getGlobalBool("kept"),
           "load/pcall and the pure libraries must stay");
    engine.shutdown();
}

static void test_sandbox_survives_reinitialize()
{
    printf("--- sandbox after shutdown + initialize ---\n");
    LuaEngine engine;
    engine.initialize();
    engine.shutdown();
    ASSERT(engine.initialize(), "re-initialize should succeed");
    engine.executeString("removed = (os == nil and io == nil)\n");
    ASSERT(engine.getGlobalBool("removed"), "a fresh VM is sandboxed too");
    engine.shutdown();
}

int main()
{
    test_sandboxed_globals_are_nil();
    test_sandbox_survives_reinitialize();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
