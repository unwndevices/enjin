/**
 * @file lua_ref_test.cpp
 * @brief lua::ScriptRef, the one holder of a Lua value across frames, and the
 *        bindings that hold values through it (Tomodachi #481)
 *
 *   - the ScriptRef contract: assign / push / reset / forget / move, nil holds
 *     nothing, a coroutine's ref is kept against the main thread;
 *   - the counts: liveCount() (ScriptRefs holding a value) and registryRefs(L)
 *     (luaL_ref slots live in L's registry, whoever took them);
 *   - teardown: a script that holds values through every enjin binding that
 *     keeps one (engine.event, engine.async, engine.tween + spring,
 *     engine.state, a spawned object's C_Timer and C_StateMachine) holds them
 *     all through ScriptRefs, and none is live once the scene and the
 *     LuaScriptSystem are gone.
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_ref.hpp>
#include <enjin2/core/scene.hpp>
#include <cstdio>
#include <cstring>
#include <utility>

using namespace enjin2;
using lua::ScriptRef;

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

struct State {
    lua_State* L;
    State() : L(luaL_newstate()) { luaL_openlibs(L); }
    ~State() { lua_close(L); }
};

struct MinimalScene : Scene {
    explicit MinimalScene(uint32_t id) : Scene(id) {}
};

} // namespace

static void test_assign_push_reset() {
    printf("--- assign captures, push retrieves, reset releases ---\n");
    State s;
    lua_State* L = s.L;
    const int live = ScriptRef::liveCount();

    lua_pushstring(L, "callback-marker");
    const int top = lua_gettop(L);
    ScriptRef ref;
    ASSERT(!ref.valid(), "a new ScriptRef holds nothing");
    ref.assign(L, -1);
    ASSERT(ref.valid(), "assign holds the value");
    ASSERT(lua_gettop(L) == top, "assign leaves the stack as it was");
    ASSERT(ScriptRef::liveCount() == live + 1, "liveCount counts the held ref");
    ASSERT(ScriptRef::registryRefs(L) == 1, "registryRefs sees the ref");
    lua_pop(L, 1);

    ref.push(L);
    ASSERT(lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), "callback-marker") == 0,
           "push brings the value back after the original is gone");
    lua_pop(L, 1);

    ref.reset();
    ASSERT(!ref.valid(), "reset clears");
    ASSERT(ScriptRef::liveCount() == live, "reset gives the count back");
    ASSERT(ScriptRef::registryRefs(L) == 0, "reset releases the registry slot");
    ref.push(L);
    ASSERT(lua_isnil(L, -1), "an empty ScriptRef pushes nil");
    lua_pop(L, 1);
}

static void test_assign_replaces() {
    printf("--- assign replaces without leaking ---\n");
    State s;
    lua_State* L = s.L;
    ScriptRef ref;
    lua_pushinteger(L, 1);
    ref.assign(L, -1);
    lua_pop(L, 1);
    lua_pushinteger(L, 2);
    ref.assign(L, -1);
    lua_pop(L, 1);
    ASSERT(ScriptRef::registryRefs(L) == 1, "the first value was released");
    ref.push(L);
    ASSERT(lua_tointeger(L, -1) == 2, "the second value is held");
    lua_pop(L, 1);
}

static void test_nil_holds_nothing() {
    printf("--- assigning nil holds nothing ---\n");
    State s;
    lua_State* L = s.L;
    const int live = ScriptRef::liveCount();
    ScriptRef ref;
    lua_pushnil(L);
    ref.assign(L, -1);
    lua_pop(L, 1);
    ASSERT(!ref.valid(), "nil is not held");
    ASSERT(ScriptRef::liveCount() == live, "nil is not counted");
}

static void test_forget() {
    printf("--- forget drops the ref without releasing it ---\n");
    const int live = ScriptRef::liveCount();
    ScriptRef ref;
    {
        State s;
        lua_pushstring(s.L, "x");
        ref.assign(s.L, -1);
        lua_pop(s.L, 1);
    }   // the state is closed under the ref
    ref.forget();
    ASSERT(!ref.valid(), "forget clears");
    ASSERT(ScriptRef::liveCount() == live, "forget gives the count back");

    State s2;
    lua_pushstring(s2.L, "y");
    ref.assign(s2.L, -1);   // no unref into the dead state
    lua_pop(s2.L, 1);
    ref.push(s2.L);
    ASSERT(strcmp(lua_tostring(s2.L, -1), "y") == 0, "a forgotten ref is reusable");
    lua_pop(s2.L, 1);
    ref.reset();            // before s2 closes
}

static void test_move() {
    printf("--- move transfers ownership ---\n");
    State s;
    lua_State* L = s.L;
    const int live = ScriptRef::liveCount();
    ScriptRef a;
    lua_pushstring(L, "moved");
    a.assign(L, -1);
    lua_pop(L, 1);
    ScriptRef b = std::move(a);
    ASSERT(!a.valid() && b.valid(), "the move leaves the source empty");
    ASSERT(ScriptRef::liveCount() == live + 1, "a move is not a second ref");
    ScriptRef c;
    c = std::move(b);
    c.push(L);
    ASSERT(strcmp(lua_tostring(L, -1), "moved") == 0, "move-assign carries the value");
    lua_pop(L, 1);
}

static void test_coroutine_ref_lives_on_the_main_thread() {
    printf("--- a ref taken on a coroutine is kept against the main thread ---\n");
    State s;
    lua_State* L = s.L;
    lua_State* co = lua_newthread(L);
    lua_pushstring(co, "from-co");
    ScriptRef ref;
    ref.assign(co, -1);
    lua_pop(co, 1);
    lua_pop(L, 1);          // the thread may be collected now
    lua_gc(L, LUA_GCCOLLECT, 0);
    ASSERT(ref.state() == L, "state() is the main thread");
    ref.reset();            // unrefs through L, not the dead coroutine
    ASSERT(ScriptRef::registryRefs(L) == 0, "released through the main thread");
}

static void test_registry_refs_sees_raw_refs() {
    printf("--- registryRefs counts refs taken without a ScriptRef ---\n");
    State s;
    lua_State* L = s.L;
    lua_pushboolean(L, 1);
    const int raw = luaL_ref(L, LUA_REGISTRYINDEX);
    ASSERT(ScriptRef::registryRefs(L) == 1, "a raw ref is seen");
    luaL_unref(L, LUA_REGISTRYINDEX, raw);
    ASSERT(ScriptRef::registryRefs(L) == 0, "and its release");
}

// Every enjin binding that keeps a Lua value, used once. While the script
// runs, every registry ref is a ScriptRef; once the scene and the system are
// gone, no ScriptRef is live.
static void test_teardown_releases_every_binding() {
    printf("--- teardown: every holding binding releases through ScriptRef ---\n");
    const int live = ScriptRef::liveCount();
    {
        LuaScriptSystem sys;
        sys.getBindings().setFeatures(LuaFeatures::all());
        ASSERT(sys.initialize(), "the system initializes");
        lua_State* L = sys.getEngine().getState();
        {
            MinimalScene scene(1u);
            scene.initialize();
            scene.activate();
            sys.getBindings().setActiveScene(&scene);

            // Each step and how many values it holds.
            struct Step { const char* code; int holds; };
            const Step steps[] = {
                {"box = {x = 0, y = 0}", 0},
                {"engine.event.on('ping', function() end)", 1},
                {"engine.async.start(function() engine.async.wait(10) end)", 1},
                {"engine.tween.to(box, {x = 10}, 5, 'linear', function() end)", 2},
                {"engine.tween.spring(box, 'y', 20)", 1},
                {"engine.state.on_enter('play', function() end)", 1},
                {"engine.state.on_exit('play', function() end)", 1},
                {"obj = engine.scene.spawn('holder')", 0},
                {"obj:add('C_Timer'):after(10, function() end)", 1},
                {"obj:add('C_Timer'):every(10, function() end)", 1},
                {"obj:add('C_StateMachine'):addState('idle', {\n"
                 "    enter = function() end, exit = function() end, update = function() end})", 3},
            };
            int held = 0;
            for (const Step& step : steps) {
                const int before = ScriptRef::liveCount();
                const LuaResult r = sys.executeScript(step.code);
                if (!r.success) fprintf(stderr, "  %s: %s\n", step.code, r.error.c_str());
                ASSERT(r.success, "a holding step runs");
                const int got = ScriptRef::liveCount() - before;
                if (got != step.holds) fprintf(stderr, "  %s: holds %d, want %d\n", step.code, got, step.holds);
                ASSERT(got == step.holds, "the step holds its values through ScriptRefs");
                held += got;
            }
            sys.getBindings().tickCoroutines(0.016f);   // the coroutine starts and waits
            ASSERT(ScriptRef::liveCount() - live == held, "the waiting coroutine is still held");
            ASSERT(ScriptRef::registryRefs(L) == held,
                   "every registry ref is a ScriptRef (no raw luaL_ref)");
            if (ScriptRef::registryRefs(L) != held)
                fprintf(stderr, "  registry %d, ScriptRefs %d\n", ScriptRef::registryRefs(L), held);
        }   // the scene and its object go first: the timer's and the FSM's 5
        ASSERT(ScriptRef::liveCount() - live == 7, "the object's callbacks went with it");
        sys.shutdown();   // the bindings' 7 go before the state closes
        ASSERT(ScriptRef::liveCount() == live, "shutdown releases the bindings' values");
    }
    ASSERT(ScriptRef::liveCount() == live, "no ScriptRef is live after the teardown");
}

int main() {
    printf("lua_ref_test — lua::ScriptRef and the bindings that hold Lua values (#481)\n\n");
    test_assign_push_reset();
    test_assign_replaces();
    test_nil_holds_nothing();
    test_forget();
    test_move();
    test_coroutine_ref_lives_on_the_main_thread();
    test_registry_refs_sees_raw_refs();
    test_teardown_releases_every_binding();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
