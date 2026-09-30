#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/lua_api.hpp"
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

    // ── Vec2 ────────────────────────────────────────────────────────────
    static constexpr LuaApiEntry kVec2Meta[] = {
        luaFunction("__add", lua_Vec2_add, "(a:Vec2, b:Vec2) -> Vec2",
                    "a + b: a new Vec2, the component-wise sum.",
                    "a: left operand\n"
                    "b: right operand"),
        luaFunction("__sub", lua_Vec2_sub, "(a:Vec2, b:Vec2) -> Vec2",
                    "a - b: a new Vec2, the component-wise difference.",
                    "a: left operand\n"
                    "b: right operand"),
        luaFunction("__mul", lua_Vec2_mul,
                    "(v:Vec2, s:number) -> Vec2\n"
                    "(s:number, v:Vec2) -> Vec2",
                    "v * s or s * v: a new Vec2 scaled by s.",
                    "v: the vector\n"
                    "s: the scale factor; Vec2 * Vec2 raises")
            .note("Scalar only: there is no component-wise or dot product operator."),
        luaFunction("__div", lua_Vec2_div, "(v:Vec2, s:number) -> Vec2",
                    "v / s: a new Vec2 divided by s.",
                    "v: the vector\n"
                    "s: the divisor; 0 gives inf/nan, and s / v raises"),
        luaFunction("__unm", lua_Vec2_unm, "(v:Vec2) -> Vec2",
                    "-v: a new Vec2 pointing the other way.",
                    "v: the vector"),
        luaFunction("__eq", lua_Vec2_eq, "(a:Vec2, b:Vec2) -> boolean",
                    "a == b: exact float comparison of both components.",
                    "a: left operand\n"
                    "b: right operand"),
        luaFunction("__tostring", lua_Vec2_tostring, "(v:Vec2) -> string",
                    "tostring(v): \"Vec2(x, y)\" with 4 significant digits.",
                    "v: the vector"),
        luaFunction("__index", lua_Vec2_index, "(v:Vec2, key:string) -> any",
                    "v.x and v.y read the components; other keys look up a Vec2 method.",
                    "v: the vector\n"
                    "key: \"x\", \"y\" or a method name; unknown keys give nil"),
        luaFunction("__newindex", lua_Vec2_newindex,
                    "(v:Vec2, key:string, value:number) -> nil",
                    "v.x = n and v.y = n write in place; any other key raises.",
                    "v: the vector\n"
                    "key: \"x\" or \"y\"\n"
                    "value: the new component"),
    };
    static constexpr LuaApiModule kVec2MetaModule = luaApiModule(
        LuaApiScope::Metatable, "Vec2", "Vec2 operators and field access.", kVec2Meta);
    if (luaL_newmetatable(L, VEC2_METATABLE)) {
        luaApiSetFields(L, -1, kVec2MetaModule);
    }
    lua_pop(L, 1);

    static constexpr LuaApiEntry kVec2Methods[] = {
        luaFunction("length", lua_Vec2_length, "() -> number", "The vector's length."),
        luaFunction("lengthSquared", lua_Vec2_lengthSquared, "() -> number",
                    "The squared length; cheaper than length() for comparisons."),
        luaFunction("normalized", lua_Vec2_normalized, "() -> Vec2",
                    "A new unit-length Vec2 in the same direction; Vec2(0, 0) stays zero."),
        luaFunction("dot", lua_Vec2_dot, "(o:Vec2) -> number",
                    "The dot product with o.",
                    "o: the other vector"),
        luaFunction("cross", lua_Vec2_cross, "(o:Vec2) -> number",
                    "The 2D cross product x * o.y - y * o.x.",
                    "o: the other vector"),
        luaFunction("distance", lua_Vec2_distance, "(o:Vec2) -> number",
                    "The distance to the point o.",
                    "o: the other point"),
        luaFunction("angle", lua_Vec2_angle, "() -> number",
                    "The direction in radians, atan2(y, x)."),
        luaFunction("rotate", lua_Vec2_rotate, "(rad:number) -> Vec2",
                    "A new Vec2 rotated by rad; v itself is unchanged.",
                    "rad: angle in radians; positive turns +x toward +y (clockwise on screen)"),
    };
    static constexpr LuaApiModule kVec2MethodsModule = luaApiModule(
        LuaApiScope::Methods, "Vec2", "Vec2 methods, called as v:name(...).", kVec2Methods);
    lua_newtable(L);
    luaApiSetFields(L, -1, kVec2MethodsModule);
    lua_setfield(L, LUA_REGISTRYINDEX, "Vec2_methods");

    // ── Point ───────────────────────────────────────────────────────────
    static constexpr LuaApiEntry kPointMeta[] = {
        luaFunction("__add", lua_Point_add, "(a:Point, b:Point) -> Point",
                    "a + b: a new Point, the component-wise sum (int16, wraps).",
                    "a: left operand\n"
                    "b: right operand"),
        luaFunction("__sub", lua_Point_sub, "(a:Point, b:Point) -> Point",
                    "a - b: a new Point, the component-wise difference (int16, wraps).",
                    "a: left operand\n"
                    "b: right operand"),
        luaFunction("__eq", lua_Point_eq, "(a:Point, b:Point) -> boolean",
                    "a == b: both coordinates equal.",
                    "a: left operand\n"
                    "b: right operand"),
        luaFunction("__tostring", lua_Point_tostring, "(p:Point) -> string",
                    "tostring(p): \"Point(x, y)\".",
                    "p: the point"),
        luaFunction("__index", lua_Point_index, "(p:Point, key:string) -> int?",
                    "p.x and p.y read the coordinates; Point has no methods.",
                    "p: the point\n"
                    "key: \"x\" or \"y\"; other keys give nil"),
        luaFunction("__newindex", lua_Point_newindex,
                    "(p:Point, key:string, value:int) -> nil",
                    "p.x = n and p.y = n write in place (int16); any other key raises.",
                    "p: the point\n"
                    "key: \"x\" or \"y\"\n"
                    "value: the new coordinate; a fractional number raises"),
    };
    static constexpr LuaApiModule kPointMetaModule = luaApiModule(
        LuaApiScope::Metatable, "Point", "Point operators and field access.", kPointMeta);
    if (luaL_newmetatable(L, POINT_METATABLE)) {
        luaApiSetFields(L, -1, kPointMetaModule);
    }
    lua_pop(L, 1);

    // ── Rect ────────────────────────────────────────────────────────────
    static constexpr LuaApiEntry kRectMeta[] = {
        luaFunction("__eq", lua_Rect_eq, "(a:Rect, b:Rect) -> boolean",
                    "a == b: position and size all equal.",
                    "a: left operand\n"
                    "b: right operand"),
        luaFunction("__tostring", lua_Rect_tostring, "(r:Rect) -> string",
                    "tostring(r): \"Rect(x, y, width, height)\".",
                    "r: the rectangle"),
        luaFunction("__index", lua_Rect_index, "(r:Rect, key:string) -> any",
                    "r.x, r.y, r.width and r.height read the fields; other keys look up a Rect method.",
                    "r: the rectangle\n"
                    "key: a field or method name; unknown keys give nil"),
        luaFunction("__newindex", lua_Rect_newindex,
                    "(r:Rect, key:string, value:int) -> nil",
                    "r.x, r.y, r.width and r.height write in place; any other key raises.",
                    "r: the rectangle\n"
                    "key: \"x\", \"y\", \"width\" or \"height\"\n"
                    "value: the new value; x and y are int16, width and height uint16"),
    };
    static constexpr LuaApiModule kRectMetaModule = luaApiModule(
        LuaApiScope::Metatable, "Rect", "Rect comparison and field access.", kRectMeta);
    if (luaL_newmetatable(L, RECT_METATABLE)) {
        luaApiSetFields(L, -1, kRectMetaModule);
    }
    lua_pop(L, 1);

    static constexpr LuaApiEntry kRectMethods[] = {
        luaFunction("contains", lua_Rect_contains, "(px:int, py:int) -> boolean",
                    "Whether the point (px, py) is inside; the right and bottom edges are outside.",
                    "px: x coordinate\n"
                    "py: y coordinate"),
        luaFunction("intersects", lua_Rect_intersects, "(o:Rect) -> boolean",
                    "Whether this rectangle overlaps o.",
                    "o: the other rectangle"),
    };
    static constexpr LuaApiModule kRectMethodsModule = luaApiModule(
        LuaApiScope::Methods, "Rect", "Rect methods, called as r:name(...).", kRectMethods);
    lua_newtable(L);
    luaApiSetFields(L, -1, kRectMethodsModule);
    lua_setfield(L, LUA_REGISTRYINDEX, "Rect_methods");

    // ── Global constructors and math utilities ──────────────────────────
    // Bare globals (not math.*); all arithmetic is float32.
    static constexpr LuaApiEntry kMathGlobals[] = {
        luaFunction("Vec2", lua_Vec2_new, "(x:number?=0, y:number?=0) -> Vec2",
                    "A new float vector; call Vec2(x, y), there is no Vec2.new.",
                    "x: horizontal component\n"
                    "y: vertical component"),
        luaFunction("Point", lua_Point_new, "(x:int?=0, y:int?=0) -> Point",
                    "A new integer point (int16, wraps past 32767).",
                    "x: horizontal coordinate; a fractional number raises\n"
                    "y: vertical coordinate; a fractional number raises"),
        luaFunction("Rect", lua_Rect_new, "(x:int?=0, y:int?=0, w:int?=0, h:int?=0) -> Rect",
                    "A new rectangle; read its size back as r.width and r.height.",
                    "x: left edge\n"
                    "y: top edge\n"
                    "w: width (uint16; a negative value wraps)\n"
                    "h: height (uint16; a negative value wraps)"),
        luaFunction("clamp", lua_math_clamp, "(v:number, lo:number, hi:number) -> number",
                    "v limited to the range lo..hi.",
                    "v: the value\n"
                    "lo: lower bound\n"
                    "hi: upper bound"),
        luaFunction("lerp", lua_math_lerp, "(a:number, b:number, t:number) -> number",
                    "Linear interpolation from a to b; t is not clamped.",
                    "a: value at t = 0\n"
                    "b: value at t = 1\n"
                    "t: blend factor"),
        luaFunction("remap", lua_math_remap,
                    "(v:number, inLo:number, inHi:number, outLo:number, outHi:number) -> number",
                    "v mapped from the range inLo..inHi onto outLo..outHi, unclamped.",
                    "v: the value\n"
                    "inLo: input range start\n"
                    "inHi: input range end; must differ from inLo\n"
                    "outLo: output range start\n"
                    "outHi: output range end"),
        luaFunction("sign", lua_math_sign, "(v:number) -> number",
                    "-1, 0 or 1 by the sign of v (as a float).",
                    "v: the value"),
        luaFunction("smoothstep", lua_math_smoothstep,
                    "(e0:number, e1:number, x:number) -> number",
                    "The clamped Hermite step from 0 at e0 to 1 at e1.",
                    "e0: lower edge\n"
                    "e1: upper edge\n"
                    "x: the value"),
        luaFunction("distance", lua_math_distance,
                    "(x1:number, y1:number, x2:number, y2:number) -> number",
                    "The distance between two points given as scalars; for Vec2 use v:distance(o).",
                    "x1: first point x\n"
                    "y1: first point y\n"
                    "x2: second point x\n"
                    "y2: second point y"),
    };
    static constexpr LuaApiModule kMathGlobalsModule = luaApiModule(
        LuaApiScope::Globals, "", "Vector constructors and math helpers.", kMathGlobals);
    luaApiSetGlobals(L, kMathGlobalsModule);
}

} // namespace enjin2
