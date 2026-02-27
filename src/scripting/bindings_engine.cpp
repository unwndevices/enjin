#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/bind_helpers.hpp"
#include "../../include/enjin2/core/scene.hpp"
#include "../../include/enjin2/core/scene_state_machine.hpp"
#include "../../include/enjin2/core/object.hpp"

namespace enjin2 {

//==============================================================================
// engine.* Global Table (ENG-01..ENG-06)
//==============================================================================

void LuaBindings::registerEngineTable() {
    lua_State* L = engine->getState();

    lua_newtable(L);                               // [engine_table]

    // --- engine.scene sub-table (ENG-01: switch, ENG-02: find) ---
    static const LuaFuncDef kSceneFuncs[] = {
        {"switch", lua_engine_scene_switch},
        {"find",   lua_engine_scene_find},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kSceneFuncs, ENJIN_ARRAY_LEN(kSceneFuncs));
    lua_setfield(L, -2, "scene");

    // --- engine.input sub-table (ENG-03) ---
    static const LuaFuncDef kInputFuncs[] = {
        {"held",          lua_engine_input_held},
        {"just_pressed",  lua_engine_input_just_pressed},
        {"just_released", lua_engine_input_just_released},
        {"axis",          lua_engine_input_axis},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kInputFuncs, ENJIN_ARRAY_LEN(kInputFuncs));
    lua_setfield(L, -2, "input");

    // --- engine.time sub-table (ENG-04) ---
    static const LuaFuncDef kTimeFuncs[] = {
        {"delta", lua_engine_time_delta},
        {"now",   lua_engine_time_now},
        {"frame", lua_engine_time_frame},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kTimeFuncs, ENJIN_ARRAY_LEN(kTimeFuncs));
    lua_setfield(L, -2, "time");

    // --- engine.collision sub-table ---
    static const LuaFuncDef kCollisionFuncs[] = {
        {"aabb",           lua_engine_collision_aabb},
        {"circleCircle",   lua_engine_collision_circleCircle},
        {"pointInRect",    lua_engine_collision_pointInRect},
        {"pointInCircle",  lua_engine_collision_pointInCircle},
        {"lineLine",       lua_engine_collision_lineLine},
        {"lineCircle",     lua_engine_collision_lineCircle},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kCollisionFuncs, ENJIN_ARRAY_LEN(kCollisionFuncs));
    lua_setfield(L, -2, "collision");

    // --- engine.lua sub-table (GC-01, GC-02) ---
    static const LuaFuncDef kLuaFuncs[] = {
        {"collect", lua_engine_lua_collect},
        {"memory",  lua_engine_lua_memory},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kLuaFuncs, ENJIN_ARRAY_LEN(kLuaFuncs));
    lua_setfield(L, -2, "lua");

    // --- engine.log top-level function (ENG-05) ---
    lua_pushcfunction(L, lua_engine_log);
    lua_setfield(L, -2, "log");

    lua_setglobal(L, "engine");                    // pops engine_table; stack is now balanced
}

// --- engine.scene.switch(id) — ENG-01 ---
// Calls SceneStateMachine::switchTo(uint32_t). Silent no-op when SSM is nullptr
// (SDL standalone mode has no SceneStateMachine).
int LuaBindings::lua_engine_scene_switch(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
    auto* ssm = static_cast<SceneStateMachine*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!ssm) return 0;  // no SSM installed — silent no-op
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    ssm->switchTo(id);
    return 0;
}

// --- engine.scene.find(name) — ENG-02 ---
// Returns lightuserdata (Object*) when found, nil when not found.
// Phase 32 upgrades lightuserdata to full ScriptProxy with metatable.
// Silent nil-return when activeScene is nullptr.
int LuaBindings::lua_engine_scene_find(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    auto* scene = static_cast<Scene*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!scene) { lua_pushnil(L); return 1; }
    const char* name = luaL_checkstring(L, 1);
    Object* obj = scene->findByName(name);
    if (!obj) {
        lua_pushnil(L);
    } else {
        lua_pushlightuserdata(L, obj);  // Phase 32 upgrades to full proxy
    }
    return 1;
}

// --- engine.input.held(btn) — ENG-03 ---
int LuaBindings::lua_engine_input_held(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->held(btn) ? 1 : 0);
    return 1;
}

// --- engine.input.just_pressed(btn) — ENG-03 ---
int LuaBindings::lua_engine_input_just_pressed(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->justPressed(btn) ? 1 : 0);
    return 1;
}

// --- engine.input.just_released(btn) — ENG-03 ---
int LuaBindings::lua_engine_input_just_released(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->justReleased(btn) ? 1 : 0);
    return 1;
}

// --- engine.input.axis(n) — ENG-03 ---
int LuaBindings::lua_engine_input_axis(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushnumber(L, 0.0); return 1; }
    int axis = static_cast<int>(luaL_checkinteger(L, 1));
    float val = (axis >= 0 && axis < 8) ? b->currentInput->axes[axis] : 0.0f;
    lua_pushnumber(L, static_cast<lua_Number>(val));
    return 1;
}

// --- engine.time.delta() — ENG-04 ---
int LuaBindings::lua_engine_time_delta(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_time");
    auto* ts = static_cast<EngineTimeState*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    lua_pushnumber(L, ts ? static_cast<lua_Number>(ts->dt) : 0.0);
    return 1;
}

// --- engine.time.now() — ENG-04 ---
int LuaBindings::lua_engine_time_now(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_time");
    auto* ts = static_cast<EngineTimeState*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    lua_pushnumber(L, ts ? static_cast<lua_Number>(ts->totalTime) : 0.0);
    return 1;
}

// --- engine.time.frame() — ENG-04 ---
int LuaBindings::lua_engine_time_frame(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_time");
    auto* ts = static_cast<EngineTimeState*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    lua_pushinteger(L, ts ? static_cast<lua_Integer>(ts->frameCount) : 0);
    return 1;
}

// --- engine.log(...) — ENG-05 ---
// Uses printf (not std::cout) — compatible with ESP32, Emscripten, and desktop.
// Handles all Lua types: strings/numbers coerce via lua_tostring;
// booleans/tables/nil fall back to lua_typename to avoid nullptr deref in printf.
int LuaBindings::lua_engine_log(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* s = lua_tostring(L, i);
        if (s) {
            printf("%s", s);
        } else {
            // lua_tostring returns nullptr for boolean, table, function, nil
            printf("(%s)", lua_typename(L, lua_type(L, i)));
        }
        if (i < n) printf("\t");
    }
    printf("\n");
    return 0;
}

//==============================================================================
// engine.collision.* bindings
//==============================================================================

int LuaBindings::lua_engine_collision_aabb(lua_State* L) {
    float x1, y1, w1, h1, x2, y2, w2, h2;
    auto* r1 = static_cast<Rect*>(luaL_testudata(L, 1, "Rect"));
    auto* r2 = static_cast<Rect*>(luaL_testudata(L, 2, "Rect"));
    if (r1 && r2) {
        x1 = static_cast<float>(r1->x); y1 = static_cast<float>(r1->y);
        w1 = static_cast<float>(r1->width); h1 = static_cast<float>(r1->height);
        x2 = static_cast<float>(r2->x); y2 = static_cast<float>(r2->y);
        w2 = static_cast<float>(r2->width); h2 = static_cast<float>(r2->height);
    } else {
        x1 = static_cast<float>(luaL_checknumber(L, 1));
        y1 = static_cast<float>(luaL_checknumber(L, 2));
        w1 = static_cast<float>(luaL_checknumber(L, 3));
        h1 = static_cast<float>(luaL_checknumber(L, 4));
        x2 = static_cast<float>(luaL_checknumber(L, 5));
        y2 = static_cast<float>(luaL_checknumber(L, 6));
        w2 = static_cast<float>(luaL_checknumber(L, 7));
        h2 = static_cast<float>(luaL_checknumber(L, 8));
    }
    lua_pushboolean(L, enjin2::collision::aabb(x1, y1, w1, h1, x2, y2, w2, h2) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_circleCircle(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float r1 = static_cast<float>(luaL_checknumber(L, 3));
    float x2 = static_cast<float>(luaL_checknumber(L, 4));
    float y2 = static_cast<float>(luaL_checknumber(L, 5));
    float r2 = static_cast<float>(luaL_checknumber(L, 6));
    lua_pushboolean(L, enjin2::collision::circleCircle(x1, y1, r1, x2, y2, r2) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_pointInRect(lua_State* L) {
    float px, py, rx, ry, rw, rh;
    auto* p = static_cast<Point*>(luaL_testudata(L, 1, "Point"));
    auto* v = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    auto* r = static_cast<Rect*>(luaL_testudata(L, 2, "Rect"));
    if ((p || v) && r) {
        px = p ? static_cast<float>(p->x) : v->x;
        py = p ? static_cast<float>(p->y) : v->y;
        rx = static_cast<float>(r->x); ry = static_cast<float>(r->y);
        rw = static_cast<float>(r->width); rh = static_cast<float>(r->height);
    } else {
        px = static_cast<float>(luaL_checknumber(L, 1));
        py = static_cast<float>(luaL_checknumber(L, 2));
        rx = static_cast<float>(luaL_checknumber(L, 3));
        ry = static_cast<float>(luaL_checknumber(L, 4));
        rw = static_cast<float>(luaL_checknumber(L, 5));
        rh = static_cast<float>(luaL_checknumber(L, 6));
    }
    lua_pushboolean(L, enjin2::collision::pointInRect(px, py, rx, ry, rw, rh) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_pointInCircle(lua_State* L) {
    float px, py, cx, cy, r;
    auto* p = static_cast<Point*>(luaL_testudata(L, 1, "Point"));
    auto* v = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    if (p || v) {
        px = p ? static_cast<float>(p->x) : v->x;
        py = p ? static_cast<float>(p->y) : v->y;
        cx = static_cast<float>(luaL_checknumber(L, 2));
        cy = static_cast<float>(luaL_checknumber(L, 3));
        r  = static_cast<float>(luaL_checknumber(L, 4));
    } else {
        px = static_cast<float>(luaL_checknumber(L, 1));
        py = static_cast<float>(luaL_checknumber(L, 2));
        cx = static_cast<float>(luaL_checknumber(L, 3));
        cy = static_cast<float>(luaL_checknumber(L, 4));
        r  = static_cast<float>(luaL_checknumber(L, 5));
    }
    lua_pushboolean(L, enjin2::collision::pointInCircle(px, py, cx, cy, r) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_lineLine(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    float x3 = static_cast<float>(luaL_checknumber(L, 5));
    float y3 = static_cast<float>(luaL_checknumber(L, 6));
    float x4 = static_cast<float>(luaL_checknumber(L, 7));
    float y4 = static_cast<float>(luaL_checknumber(L, 8));
    float ix, iy;
    bool hit = enjin2::collision::lineLine(x1, y1, x2, y2, x3, y3, x4, y4, &ix, &iy);
    lua_pushboolean(L, hit ? 1 : 0);
    if (hit) {
        lua_pushnumber(L, static_cast<lua_Number>(ix));
        lua_pushnumber(L, static_cast<lua_Number>(iy));
        return 3;
    }
    return 1;
}

int LuaBindings::lua_engine_collision_lineCircle(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    float cx = static_cast<float>(luaL_checknumber(L, 5));
    float cy = static_cast<float>(luaL_checknumber(L, 6));
    float r  = static_cast<float>(luaL_checknumber(L, 7));
    lua_pushboolean(L, enjin2::collision::lineCircle(x1, y1, x2, y2, cx, cy, r) ? 1 : 0);
    return 1;
}

// --- engine.lua.collect() — GC-01 ---
// Performs one incremental GC step. Uses LUA_GCSTEP (NOT LUA_GCCOLLECT) to avoid
// a stop-the-world pause that would spike frame budget on embedded targets (ESP32).
// data=0: one minimal step. Scripts can call multiple times per frame if needed.
int LuaBindings::lua_engine_lua_collect(lua_State* L) {
    lua_gc(L, LUA_GCSTEP, 0);
    return 0;
}

// --- engine.lua.memory() — GC-02 ---
// Returns Lua heap size in bytes as a number.
// Combines LUA_GCCOUNT (whole KB) + LUA_GCCOUNTB (remaining bytes) for exact byte count.
// Identical formula to LuaPlatform::getMemoryUsage() in lua_platform.cpp.
int LuaBindings::lua_engine_lua_memory(lua_State* L) {
    int kb  = lua_gc(L, LUA_GCCOUNT,  0);
    int rem = lua_gc(L, LUA_GCCOUNTB, 0);
    lua_pushnumber(L, static_cast<lua_Number>(kb * 1024 + rem));
    return 1;
}

} // namespace enjin2
