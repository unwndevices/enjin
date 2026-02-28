#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/bind_helpers.hpp"
#include <cmath>

namespace enjin2 {

//==============================================================================
// Math Bindings — Vec2 / Point / Rect userdata + utility globals
//==============================================================================

static constexpr const char* VEC2_METATABLE  = "Vec2";
static constexpr const char* POINT_METATABLE = "Point";
static constexpr const char* RECT_METATABLE  = "Rect";

// ── Helper: push a new Vec2 userdata with metatable ─────────────────────────

static enjin2::Vec2* pushVec2(lua_State* L, float x, float y) {
    auto* v = static_cast<enjin2::Vec2*>(lua_newuserdata(L, sizeof(enjin2::Vec2)));
    v->x = x;
    v->y = y;
    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);
    return v;
}

static enjin2::Vec2* checkVec2(lua_State* L, int idx) {
    return static_cast<enjin2::Vec2*>(luaL_checkudata(L, idx, VEC2_METATABLE));
}

static enjin2::Point* pushPoint(lua_State* L, int16_t x, int16_t y) {
    auto* p = static_cast<enjin2::Point*>(lua_newuserdata(L, sizeof(enjin2::Point)));
    p->x = x;
    p->y = y;
    luaL_getmetatable(L, POINT_METATABLE);
    lua_setmetatable(L, -2);
    return p;
}

static enjin2::Point* checkPoint(lua_State* L, int idx) {
    return static_cast<enjin2::Point*>(luaL_checkudata(L, idx, POINT_METATABLE));
}

static enjin2::Rect* pushRect(lua_State* L, int16_t x, int16_t y, uint16_t w, uint16_t h) {
    auto* r = static_cast<enjin2::Rect*>(lua_newuserdata(L, sizeof(enjin2::Rect)));
    r->x = x; r->y = y; r->width = w; r->height = h;
    luaL_getmetatable(L, RECT_METATABLE);
    lua_setmetatable(L, -2);
    return r;
}

static enjin2::Rect* checkRect(lua_State* L, int idx) {
    return static_cast<enjin2::Rect*>(luaL_checkudata(L, idx, RECT_METATABLE));
}

// ── Vec2 constructor ────────────────────────────────────────────────────────

int LuaBindings::lua_Vec2_new(lua_State* L) {
    float x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    float y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    pushVec2(L, x, y);
    return 1;
}

// ── Vec2 metamethods ────────────────────────────────────────────────────────

int LuaBindings::lua_Vec2_add(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    pushVec2(L, a->x + b->x, a->y + b->y);
    return 1;
}

int LuaBindings::lua_Vec2_sub(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    pushVec2(L, a->x - b->x, a->y - b->y);
    return 1;
}

int LuaBindings::lua_Vec2_mul(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        float s = static_cast<float>(lua_tonumber(L, 1));
        auto* v = checkVec2(L, 2);
        pushVec2(L, v->x * s, v->y * s);
    } else {
        auto* v = checkVec2(L, 1);
        float s = static_cast<float>(luaL_checknumber(L, 2));
        pushVec2(L, v->x * s, v->y * s);
    }
    return 1;
}

int LuaBindings::lua_Vec2_div(lua_State* L) {
    auto* v = checkVec2(L, 1);
    float s = static_cast<float>(luaL_checknumber(L, 2));
    pushVec2(L, v->x / s, v->y / s);
    return 1;
}

int LuaBindings::lua_Vec2_unm(lua_State* L) {
    auto* v = checkVec2(L, 1);
    pushVec2(L, -v->x, -v->y);
    return 1;
}

int LuaBindings::lua_Vec2_eq(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    lua_pushboolean(L, (a->x == b->x && a->y == b->y) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_Vec2_tostring(lua_State* L) {
    auto* v = checkVec2(L, 1);
    char buf[64];
    snprintf(buf, sizeof(buf), "Vec2(%.4g, %.4g)", v->x, v->y);
    lua_pushstring(L, buf);
    return 1;
}

// ── Vec2 methods (called via __index dispatch) ──────────────────────────────

int LuaBindings::lua_Vec2_length(lua_State* L) {
    auto* v = checkVec2(L, 1);
    lua_pushnumber(L, v->length());
    return 1;
}

int LuaBindings::lua_Vec2_lengthSquared(lua_State* L) {
    auto* v = checkVec2(L, 1);
    lua_pushnumber(L, v->lengthSquared());
    return 1;
}

int LuaBindings::lua_Vec2_normalized(lua_State* L) {
    auto* v = checkVec2(L, 1);
    enjin2::Vec2 n = v->normalized();
    pushVec2(L, n.x, n.y);
    return 1;
}

int LuaBindings::lua_Vec2_dot(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    lua_pushnumber(L, a->dot(*b));
    return 1;
}

int LuaBindings::lua_Vec2_cross(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    lua_pushnumber(L, a->cross(*b));
    return 1;
}

int LuaBindings::lua_Vec2_distance(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    lua_pushnumber(L, enjin2::Vec2::distance(*a, *b));
    return 1;
}

int LuaBindings::lua_Vec2_angle(lua_State* L) {
    auto* v = checkVec2(L, 1);
    lua_pushnumber(L, v->angle());
    return 1;
}

int LuaBindings::lua_Vec2_rotate(lua_State* L) {
    auto* v = checkVec2(L, 1);
    float rad = static_cast<float>(luaL_checknumber(L, 2));
    enjin2::Vec2 r = v->rotated(rad);
    pushVec2(L, r.x, r.y);
    return 1;
}

// ── Vec2 __index / __newindex ───────────────────────────────────────────────

int LuaBindings::lua_Vec2_index(lua_State* L) {
    auto* v = checkVec2(L, 1);
    const char* key = luaL_checkstring(L, 2);

    if (key[0] == 'x' && key[1] == '\0') { lua_pushnumber(L, v->x); return 1; }
    if (key[0] == 'y' && key[1] == '\0') { lua_pushnumber(L, v->y); return 1; }

    // Look up in methods table stored in registry
    lua_getfield(L, LUA_REGISTRYINDEX, "Vec2_methods");
    lua_pushstring(L, key);
    lua_rawget(L, -2);
    return 1;
}

int LuaBindings::lua_Vec2_newindex(lua_State* L) {
    auto* v = checkVec2(L, 1);
    const char* key = luaL_checkstring(L, 2);

    if (key[0] == 'x' && key[1] == '\0') { v->x = static_cast<float>(luaL_checknumber(L, 3)); return 0; }
    if (key[0] == 'y' && key[1] == '\0') { v->y = static_cast<float>(luaL_checknumber(L, 3)); return 0; }

    return luaL_error(L, "Vec2 has no writable field '%s'", key);
}

// ── Point constructor ───────────────────────────────────────────────────────

int LuaBindings::lua_Point_new(lua_State* L) {
    int16_t x = static_cast<int16_t>(luaL_optinteger(L, 1, 0));
    int16_t y = static_cast<int16_t>(luaL_optinteger(L, 2, 0));
    pushPoint(L, x, y);
    return 1;
}

int LuaBindings::lua_Point_add(lua_State* L) {
    auto* a = checkPoint(L, 1);
    auto* b = checkPoint(L, 2);
    pushPoint(L, a->x + b->x, a->y + b->y);
    return 1;
}

int LuaBindings::lua_Point_sub(lua_State* L) {
    auto* a = checkPoint(L, 1);
    auto* b = checkPoint(L, 2);
    pushPoint(L, a->x - b->x, a->y - b->y);
    return 1;
}

int LuaBindings::lua_Point_eq(lua_State* L) {
    auto* a = checkPoint(L, 1);
    auto* b = checkPoint(L, 2);
    lua_pushboolean(L, (a->x == b->x && a->y == b->y) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_Point_tostring(lua_State* L) {
    auto* p = checkPoint(L, 1);
    char buf[48];
    snprintf(buf, sizeof(buf), "Point(%d, %d)", static_cast<int>(p->x), static_cast<int>(p->y));
    lua_pushstring(L, buf);
    return 1;
}

int LuaBindings::lua_Point_index(lua_State* L) {
    auto* p = checkPoint(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (key[0] == 'x' && key[1] == '\0') { lua_pushinteger(L, p->x); return 1; }
    if (key[0] == 'y' && key[1] == '\0') { lua_pushinteger(L, p->y); return 1; }
    lua_pushnil(L);
    return 1;
}

int LuaBindings::lua_Point_newindex(lua_State* L) {
    auto* p = checkPoint(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (key[0] == 'x' && key[1] == '\0') { p->x = static_cast<int16_t>(luaL_checkinteger(L, 3)); return 0; }
    if (key[0] == 'y' && key[1] == '\0') { p->y = static_cast<int16_t>(luaL_checkinteger(L, 3)); return 0; }
    return luaL_error(L, "Point has no writable field '%s'", key);
}

// ── Rect constructor ────────────────────────────────────────────────────────

int LuaBindings::lua_Rect_new(lua_State* L) {
    int16_t  x = static_cast<int16_t>(luaL_optinteger(L, 1, 0));
    int16_t  y = static_cast<int16_t>(luaL_optinteger(L, 2, 0));
    uint16_t w = static_cast<uint16_t>(luaL_optinteger(L, 3, 0));
    uint16_t h = static_cast<uint16_t>(luaL_optinteger(L, 4, 0));
    pushRect(L, x, y, w, h);
    return 1;
}

int LuaBindings::lua_Rect_eq(lua_State* L) {
    auto* a = checkRect(L, 1);
    auto* b = checkRect(L, 2);
    lua_pushboolean(L, (a->x == b->x && a->y == b->y &&
                        a->width == b->width && a->height == b->height) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_Rect_tostring(lua_State* L) {
    auto* r = checkRect(L, 1);
    char buf[80];
    snprintf(buf, sizeof(buf), "Rect(%d, %d, %u, %u)",
             static_cast<int>(r->x), static_cast<int>(r->y),
             static_cast<unsigned>(r->width), static_cast<unsigned>(r->height));
    lua_pushstring(L, buf);
    return 1;
}

int LuaBindings::lua_Rect_contains(lua_State* L) {
    auto* r = checkRect(L, 1);
    int16_t px = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t py = static_cast<int16_t>(luaL_checkinteger(L, 3));
    lua_pushboolean(L, r->contains(px, py) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_Rect_intersects(lua_State* L) {
    auto* a = checkRect(L, 1);
    auto* b = checkRect(L, 2);
    lua_pushboolean(L, a->intersects(*b) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_Rect_index(lua_State* L) {
    auto* r = checkRect(L, 1);
    const char* key = luaL_checkstring(L, 2);

    if (strcmp(key, "x") == 0)      { lua_pushinteger(L, r->x);      return 1; }
    if (strcmp(key, "y") == 0)      { lua_pushinteger(L, r->y);      return 1; }
    if (strcmp(key, "width") == 0)  { lua_pushinteger(L, r->width);  return 1; }
    if (strcmp(key, "height") == 0) { lua_pushinteger(L, r->height); return 1; }

    lua_getfield(L, LUA_REGISTRYINDEX, "Rect_methods");
    lua_pushstring(L, key);
    lua_rawget(L, -2);
    return 1;
}

int LuaBindings::lua_Rect_newindex(lua_State* L) {
    auto* r = checkRect(L, 1);
    const char* key = luaL_checkstring(L, 2);

    if (strcmp(key, "x") == 0)      { r->x      = static_cast<int16_t>(luaL_checkinteger(L, 3));  return 0; }
    if (strcmp(key, "y") == 0)      { r->y      = static_cast<int16_t>(luaL_checkinteger(L, 3));  return 0; }
    if (strcmp(key, "width") == 0)  { r->width  = static_cast<uint16_t>(luaL_checkinteger(L, 3)); return 0; }
    if (strcmp(key, "height") == 0) { r->height = static_cast<uint16_t>(luaL_checkinteger(L, 3)); return 0; }

    return luaL_error(L, "Rect has no writable field '%s'", key);
}

// ── Math utility globals ────────────────────────────────────────────────────

int LuaBindings::lua_math_clamp(lua_State* L) {
    float val = static_cast<float>(luaL_checknumber(L, 1));
    float lo  = static_cast<float>(luaL_checknumber(L, 2));
    float hi  = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, enjin2::math::clamp(val, lo, hi));
    return 1;
}

int LuaBindings::lua_math_lerp(lua_State* L) {
    float a = static_cast<float>(luaL_checknumber(L, 1));
    float b = static_cast<float>(luaL_checknumber(L, 2));
    float t = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, enjin2::math::lerp(a, b, t));
    return 1;
}

int LuaBindings::lua_math_remap(lua_State* L) {
    float val    = static_cast<float>(luaL_checknumber(L, 1));
    float in_lo  = static_cast<float>(luaL_checknumber(L, 2));
    float in_hi  = static_cast<float>(luaL_checknumber(L, 3));
    float out_lo = static_cast<float>(luaL_checknumber(L, 4));
    float out_hi = static_cast<float>(luaL_checknumber(L, 5));
    lua_pushnumber(L, enjin2::math::map(val, in_lo, in_hi, out_lo, out_hi));
    return 1;
}

int LuaBindings::lua_math_sign(lua_State* L) {
    float val = static_cast<float>(luaL_checknumber(L, 1));
    lua_pushnumber(L, enjin2::math::sign(val));
    return 1;
}

int LuaBindings::lua_math_smoothstep(lua_State* L) {
    float e0 = static_cast<float>(luaL_checknumber(L, 1));
    float e1 = static_cast<float>(luaL_checknumber(L, 2));
    float x  = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, enjin2::math::smoothstep(e0, e1, x));
    return 1;
}

int LuaBindings::lua_math_distance(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    float dx = x2 - x1;
    float dy = y2 - y1;
    lua_pushnumber(L, std::sqrt(dx * dx + dy * dy));
    return 1;
}

// ── registerMathBindings ────────────────────────────────────────────────────

void LuaBindings::registerMathBindings() {
    lua_State* L = engine->getState();
    if (!L) return;

    // ── Vec2 metatable ──────────────────────────────────────────────────
    static const LuaFuncDef kVec2Meta[] = {
        {"__add",      lua_Vec2_add},
        {"__sub",      lua_Vec2_sub},
        {"__mul",      lua_Vec2_mul},
        {"__div",      lua_Vec2_div},
        {"__unm",      lua_Vec2_unm},
        {"__eq",       lua_Vec2_eq},
        {"__tostring", lua_Vec2_tostring},
        {"__index",    lua_Vec2_index},
        {"__newindex", lua_Vec2_newindex},
    };
    if (luaL_newmetatable(L, VEC2_METATABLE)) {
        luaBindFunctions(L, -1, kVec2Meta, ENJIN_ARRAY_LEN(kVec2Meta));
    }
    lua_pop(L, 1);

    // Vec2 methods table in registry for __index dispatch
    static const LuaFuncDef kVec2Methods[] = {
        {"length",        lua_Vec2_length},
        {"lengthSquared", lua_Vec2_lengthSquared},
        {"normalized",    lua_Vec2_normalized},
        {"dot",           lua_Vec2_dot},
        {"cross",         lua_Vec2_cross},
        {"distance",      lua_Vec2_distance},
        {"angle",         lua_Vec2_angle},
        {"rotate",        lua_Vec2_rotate},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kVec2Methods, ENJIN_ARRAY_LEN(kVec2Methods));
    lua_setfield(L, LUA_REGISTRYINDEX, "Vec2_methods");

    // ── Point metatable ─────────────────────────────────────────────────
    static const LuaFuncDef kPointMeta[] = {
        {"__add",      lua_Point_add},
        {"__sub",      lua_Point_sub},
        {"__eq",       lua_Point_eq},
        {"__tostring", lua_Point_tostring},
        {"__index",    lua_Point_index},
        {"__newindex", lua_Point_newindex},
    };
    if (luaL_newmetatable(L, POINT_METATABLE)) {
        luaBindFunctions(L, -1, kPointMeta, ENJIN_ARRAY_LEN(kPointMeta));
    }
    lua_pop(L, 1);

    // ── Rect metatable ──────────────────────────────────────────────────
    static const LuaFuncDef kRectMeta[] = {
        {"__eq",       lua_Rect_eq},
        {"__tostring", lua_Rect_tostring},
        {"__index",    lua_Rect_index},
        {"__newindex", lua_Rect_newindex},
    };
    if (luaL_newmetatable(L, RECT_METATABLE)) {
        luaBindFunctions(L, -1, kRectMeta, ENJIN_ARRAY_LEN(kRectMeta));
    }
    lua_pop(L, 1);

    // Rect methods table in registry for __index dispatch
    static const LuaFuncDef kRectMethods[] = {
        {"contains",   lua_Rect_contains},
        {"intersects", lua_Rect_intersects},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kRectMethods, ENJIN_ARRAY_LEN(kRectMethods));
    lua_setfield(L, LUA_REGISTRYINDEX, "Rect_methods");

    // ── Global constructors ─────────────────────────────────────────────
    lua_pushcfunction(L, lua_Vec2_new);  lua_setglobal(L, "Vec2");
    lua_pushcfunction(L, lua_Point_new); lua_setglobal(L, "Point");
    lua_pushcfunction(L, lua_Rect_new);  lua_setglobal(L, "Rect");

    // ── Math utility globals ────────────────────────────────────────────
    static const LuaFuncDef kMathGlobals[] = {
        {"clamp",      lua_math_clamp},
        {"lerp",       lua_math_lerp},
        {"remap",      lua_math_remap},
        {"sign",       lua_math_sign},
        {"smoothstep", lua_math_smoothstep},
        {"distance",   lua_math_distance},
    };
    luaBindGlobals(L, kMathGlobals, ENJIN_ARRAY_LEN(kMathGlobals));
}

} // namespace enjin2
