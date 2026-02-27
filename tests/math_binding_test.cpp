/**
 * @file math_binding_test.cpp
 * @brief Unit tests for math Lua bindings: Vec2, Point, Rect, and utility globals
 *
 * Verifies constructors, metamethods (+, -, *, /, ==, tostring), field access,
 * and methods (length, normalized, dot, cross, distance, angle, rotate;
 * Rect contains/intersects; clamp, lerp, remap, sign, smoothstep, distance).
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <cstdio>
#include <cmath>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

static const double EPS = 1e-5;

struct MathBindingFixture {
    LuaEngine engine;
    LuaBindings bindings;

    MathBindingFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name, -999.0);
    }

    std::string getStr(const char* name) {
        return engine.getGlobalString(name, "<<not set>>");
    }
};

// ============================================================
// Vec2: constructor and type
// ============================================================
static void test_Vec2_constructor_and_type() {
    printf("--- Vec2 constructor and type ---\n");
    MathBindingFixture f;
    LuaResult r = f.exec(
        "local v = Vec2(3, 4)\n"
        "ok = (v ~= nil) and 1 or 0\n"
        "tx = (type(v) == 'userdata') and 1 or 0\n"
    );
    ASSERT(r.success, "Vec2(3,4) script should succeed");
    ASSERT(f.getNum("ok") == 1.0, "Vec2() should return non-nil");
    ASSERT(f.getNum("tx") == 1.0, "type(Vec2()) should be userdata");
}

// ============================================================
// Vec2: field read/write
// ============================================================
static void test_Vec2_fields() {
    printf("--- Vec2 x/y fields ---\n");
    MathBindingFixture f;
    LuaResult r = f.exec(
        "local v = Vec2(1, 2)\n"
        "ax = v.x\n"
        "ay = v.y\n"
        "v.x = 10\n"
        "v.y = 20\n"
        "bx = v.x\n"
        "by = v.y\n"
    );
    ASSERT(r.success, "Vec2 field access script should succeed");
    ASSERT(f.getNum("ax") == 1.0, "v.x read should be 1");
    ASSERT(f.getNum("ay") == 2.0, "v.y read should be 2");
    ASSERT(f.getNum("bx") == 10.0, "v.x after write should be 10");
    ASSERT(f.getNum("by") == 20.0, "v.y after write should be 20");
}

// ============================================================
// Vec2: operators + - * / unm
// ============================================================
static void test_Vec2_operators() {
    printf("--- Vec2 operators ---\n");
    MathBindingFixture f;
    LuaResult r = f.exec(
        "local a = Vec2(1, 0)\n"
        "local b = Vec2(0, 1)\n"
        "local s = a + b\n"
        "sx = s.x\n"
        "sy = s.y\n"
        "local d = a - b\n"
        "dx = d.x\n"
        "dy = d.y\n"
        "local m = a * 3\n"
        "mx = m.x\n"
        "my = m.y\n"
        "local div = Vec2(6, 4) / 2\n"
        "divx = div.x\n"
        "divy = div.y\n"
        "local neg = -a\n"
        "negx = neg.x\n"
        "negy = neg.y\n"
    );
    ASSERT(r.success, "Vec2 operators script should succeed");
    ASSERT(f.getNum("sx") == 1.0 && f.getNum("sy") == 1.0, "a+b");
    ASSERT(f.getNum("dx") == 1.0 && f.getNum("dy") == -1.0, "a-b");
    ASSERT(f.getNum("mx") == 3.0 && f.getNum("my") == 0.0, "a*3");
    ASSERT(f.getNum("divx") == 3.0 && f.getNum("divy") == 2.0, "v/2");
    ASSERT(f.getNum("negx") == -1.0 && f.getNum("negy") == 0.0, "-a");
}

// ============================================================
// Vec2: equality and tostring
// ============================================================
static void test_Vec2_eq_tostring() {
    printf("--- Vec2 eq and tostring ---\n");
    MathBindingFixture f;
    LuaResult r = f.exec(
        "local a = Vec2(1, 2)\n"
        "local b = Vec2(1, 2)\n"
        "local c = Vec2(1, 3)\n"
        "eq_ab = (a == b) and 1 or 0\n"
        "eq_ac = (a == c) and 1 or 0\n"
        "str = tostring(a)\n"
    );
    ASSERT(r.success, "Vec2 eq/tostring script should succeed");
    ASSERT(f.getNum("eq_ab") == 1.0, "a == b when equal");
    ASSERT(f.getNum("eq_ac") == 0.0, "a == c when different");
    ASSERT(f.getStr("str").find("Vec2") != std::string::npos &&
           f.getStr("str").find("1") != std::string::npos &&
           f.getStr("str").find("2") != std::string::npos, "tostring contains Vec2 and coords");
}

// ============================================================
// Vec2: methods length, lengthSquared, normalized, dot, cross, distance, angle, rotate
// ============================================================
static void test_Vec2_methods() {
    printf("--- Vec2 methods ---\n");
    MathBindingFixture f;
    LuaResult r = f.exec(
        "local v = Vec2(3, 4)\n"
        "len = v:length()\n"
        "len2 = v:lengthSquared()\n"
        "local n = v:normalized()\n"
        "nx = n.x\n"
        "ny = n.y\n"
        "local u = Vec2(1, 0)\n"
        "local w = Vec2(0, 1)\n"
        "dot_uw = u:dot(w)\n"
        "dot_uu = u:dot(u)\n"
        "cross_uw = u:cross(w)\n"
        "local a = Vec2(0, 0)\n"
        "local b = Vec2(3, 4)\n"
        "dist = a:distance(b)\n"
        "ang = Vec2(1, 0):angle()\n"
        "local r = Vec2(1, 0):rotate(math.pi / 2)\n"
        "rx = r.x\n"
        "ry = r.y\n"
    );
    ASSERT(r.success, "Vec2 methods script should succeed");
    ASSERT(std::fabs(f.getNum("len") - 5.0) < EPS, "length(3,4) == 5");
    ASSERT(std::fabs(f.getNum("len2") - 25.0) < EPS, "lengthSquared(3,4) == 25");
    ASSERT(std::fabs(f.getNum("nx") - 0.6) < EPS && std::fabs(f.getNum("ny") - 0.8) < EPS, "normalized");
    ASSERT(std::fabs(f.getNum("dot_uw") - 0.0) < EPS, "dot(1,0),(0,1) == 0");
    ASSERT(std::fabs(f.getNum("dot_uu") - 1.0) < EPS, "dot(1,0),(1,0) == 1");
    ASSERT(std::fabs(f.getNum("cross_uw") - 1.0) < EPS, "cross(1,0),(0,1) == 1");
    ASSERT(std::fabs(f.getNum("dist") - 5.0) < EPS, "distance((0,0),(3,4)) == 5");
    ASSERT(std::fabs(f.getNum("ang") - 0.0) < EPS, "angle(1,0) == 0");
    ASSERT(std::fabs(f.getNum("rx")) < EPS && std::fabs(f.getNum("ry") - 1.0) < EPS, "rotate(1,0) by pi/2 -> (0,1)");
}

// ============================================================
// Point: constructor, fields, operators, tostring
// ============================================================
static void test_Point_binding() {
    printf("--- Point binding ---\n");
    MathBindingFixture f;
    LuaResult r = f.exec(
        "local p = Point(10, 20)\n"
        "px = p.x\n"
        "py = p.y\n"
        "p.x = 5\n"
        "p.y = 15\n"
        "qx = p.x\n"
        "qy = p.y\n"
        "local a = Point(1, 2)\n"
        "local b = Point(3, 4)\n"
        "local s = a + b\n"
        "sx = s.x\n"
        "sy = s.y\n"
        "eq = (Point(7,8) == Point(7,8)) and 1 or 0\n"
        "str = tostring(Point(1, 2))\n"
    );
    ASSERT(r.success, "Point script should succeed");
    ASSERT(f.getNum("px") == 10.0 && f.getNum("py") == 20.0, "Point read");
    ASSERT(f.getNum("qx") == 5.0 && f.getNum("qy") == 15.0, "Point write");
    ASSERT(f.getNum("sx") == 4.0 && f.getNum("sy") == 6.0, "Point add");
    ASSERT(f.getNum("eq") == 1.0, "Point equality");
    ASSERT(f.getStr("str").find("Point") != std::string::npos, "Point tostring");
}

// ============================================================
// Rect: constructor, fields, contains, intersects, tostring
// ============================================================
static void test_Rect_binding() {
    printf("--- Rect binding ---\n");
    MathBindingFixture f;
    LuaResult r = f.exec(
        "local r = Rect(10, 20, 30, 40)\n"
        "rx = r.x\n"
        "ry = r.y\n"
        "rw = r.width\n"
        "rh = r.height\n"
        "c1 = r:contains(15, 25) and 1 or 0\n"
        "c2 = r:contains(0, 0) and 1 or 0\n"
        "local other = Rect(5, 5, 20, 20)\n"
        "inter = r:intersects(other) and 1 or 0\n"
        "local far = Rect(100, 100, 5, 5)\n"
        "no_inter = r:intersects(far) and 1 or 0\n"
        "str = tostring(r)\n"
    );
    ASSERT(r.success, "Rect script should succeed");
    ASSERT(f.getNum("rx") == 10.0 && f.getNum("ry") == 20.0, "Rect x,y");
    ASSERT(f.getNum("rw") == 30.0 && f.getNum("rh") == 40.0, "Rect width, height");
    ASSERT(f.getNum("c1") == 1.0, "contains(15,25) inside");
    ASSERT(f.getNum("c2") == 0.0, "contains(0,0) outside");
    ASSERT(f.getNum("inter") == 1.0, "intersects overlapping");
    ASSERT(f.getNum("no_inter") == 0.0, "intersects disjoint");
    ASSERT(f.getStr("str").find("Rect") != std::string::npos, "Rect tostring");
}

// ============================================================
// Math utility globals: clamp, lerp, remap, sign, smoothstep, distance
// ============================================================
static void test_math_utilities() {
    printf("--- math utility globals ---\n");
    MathBindingFixture f;
    LuaResult r = f.exec(
        "c1 = clamp(50, 0, 100)\n"
        "c2 = clamp(-5, 0, 100)\n"
        "c3 = clamp(150, 0, 100)\n"
        "lerp_val = lerp(0, 100, 0.5)\n"
        "remap_val = remap(0.5, 0, 1, 10, 20)\n"
        "sign_neg = sign(-7)\n"
        "sign_pos = sign(3)\n"
        "sign_zero = sign(0)\n"
        "smooth = smoothstep(0, 1, 0.5)\n"
        "dist = distance(0, 0, 3, 4)\n"
    );
    ASSERT(r.success, "math utilities script should succeed");
    ASSERT(f.getNum("c1") == 50.0, "clamp(50,0,100) == 50");
    ASSERT(f.getNum("c2") == 0.0, "clamp(-5,0,100) == 0");
    ASSERT(f.getNum("c3") == 100.0, "clamp(150,0,100) == 100");
    ASSERT(std::fabs(f.getNum("lerp_val") - 50.0) < EPS, "lerp(0,100,0.5) == 50");
    ASSERT(std::fabs(f.getNum("remap_val") - 15.0) < EPS, "remap(0.5,0,1,10,20) == 15");
    ASSERT(f.getNum("sign_neg") == -1.0, "sign(-7) == -1");
    ASSERT(f.getNum("sign_pos") == 1.0, "sign(3) == 1");
    ASSERT(f.getNum("sign_zero") == 0.0, "sign(0) == 0");
    ASSERT(std::fabs(f.getNum("smooth") - 0.5) < EPS, "smoothstep(0,1,0.5) ~= 0.5");
    ASSERT(f.getNum("dist") == 5.0, "distance(0,0,3,4) == 5");
}

// ============================================================
// main
// ============================================================
int main() {
    printf("=== math_binding_test ===\n");

    test_Vec2_constructor_and_type();
    test_Vec2_fields();
    test_Vec2_operators();
    test_Vec2_eq_tostring();
    test_Vec2_methods();
    test_Point_binding();
    test_Rect_binding();
    test_math_utilities();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
