/**
 * @file collision_test.cpp
 * @brief Unit tests for engine.collision.* — C++ collision.hpp and Lua bindings
 *
 * Verifies:
 * - C++ collision.hpp functions (aabb, circleCircle, pointInRect, pointInCircle, lineLine, lineCircle)
 * - Lua engine.collision.* flat-argument API
 * - Lua engine.collision.* userdata overloads (Rect, Point, Vec2)
 */
#include <enjin2/core/collision.hpp>
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

// ============================================================
// C++ collision.hpp — aabb
// ============================================================
static void test_cpp_aabb() {
    printf("--- C++ aabb ---\n");

    ASSERT(collision::aabb(0, 0, 10, 10, 5, 5, 10, 10), "overlapping rects");
    ASSERT(!collision::aabb(0, 0, 10, 10, 20, 20, 10, 10), "disjoint rects");
    ASSERT(!collision::aabb(0, 0, 10, 10, 10, 0, 5, 5), "touching edge — no overlap");
    ASSERT(collision::aabb(0, 0, 10, 10, 9, 9, 9, 9), "overlap at corner");
}

// ============================================================
// C++ collision.hpp — circleCircle
// ============================================================
static void test_cpp_circleCircle() {
    printf("--- C++ circleCircle ---\n");

    ASSERT(collision::circleCircle(0, 0, 5, 8, 0, 5), "overlapping circles");
    ASSERT(!collision::circleCircle(0, 0, 5, 20, 0, 5), "disjoint circles");
    ASSERT(collision::circleCircle(0, 0, 5, 10, 0, 5), "touching circles");
    ASSERT(collision::circleCircle(0, 0, 5, 0, 0, 3), "one inside other");
}

// ============================================================
// C++ collision.hpp — pointInRect
// ============================================================
static void test_cpp_pointInRect() {
    printf("--- C++ pointInRect ---\n");

    ASSERT(collision::pointInRect(5, 5, 0, 0, 10, 10), "point inside");
    ASSERT(!collision::pointInRect(15, 15, 0, 0, 10, 10), "point outside");
    ASSERT(collision::pointInRect(0, 0, 0, 0, 10, 10), "point at top-left");
    ASSERT(!collision::pointInRect(10, 10, 0, 0, 10, 10), "point at exclusive right edge");
}

// ============================================================
// C++ collision.hpp — pointInCircle
// ============================================================
static void test_cpp_pointInCircle() {
    printf("--- C++ pointInCircle ---\n");

    ASSERT(collision::pointInCircle(0, 0, 0, 0, 5), "point at center");
    ASSERT(collision::pointInCircle(5, 0, 0, 0, 5), "point on boundary");
    ASSERT(!collision::pointInCircle(6, 0, 0, 0, 5), "point outside");
}

// ============================================================
// C++ collision.hpp — lineLine
// ============================================================
static void test_cpp_lineLine() {
    printf("--- C++ lineLine ---\n");

    float ix, iy;
    bool hit = collision::lineLine(0, 0, 10, 10, 0, 10, 10, 0, &ix, &iy);
    ASSERT(hit, "crossing lines intersect");
    ASSERT(std::fabs(ix - 5.0f) < 1e-5f && std::fabs(iy - 5.0f) < 1e-5f, "intersection at (5,5)");

    ASSERT(!collision::lineLine(0, 0, 10, 0, 0, 10, 10, 10), "parallel lines");
    ASSERT(!collision::lineLine(0, 0, 5, 5, 10, 10, 15, 15), "collinear non-overlapping");
}

// ============================================================
// C++ collision.hpp — lineCircle
// ============================================================
static void test_cpp_lineCircle() {
    printf("--- C++ lineCircle ---\n");

    ASSERT(collision::lineCircle(0, 0, 10, 0, 5, 0, 2), "line through center");
    ASSERT(collision::lineCircle(0, 0, 10, 10, 5, 5, 10), "line through center diagonal");
    ASSERT(!collision::lineCircle(0, 0, 10, 0, 5, 10, 2), "line above circle");
    ASSERT(collision::lineCircle(0, 0, 10, 0, 5, 0, 0), "line through center, zero radius (touch)");
}

// ============================================================
// Lua fixture
// ============================================================
struct CollisionFixture {
    LuaEngine engine;
    LuaBindings bindings;

    CollisionFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name, -999.0);
    }
};

// ============================================================
// Lua — flat args (aabb, circleCircle, pointInRect, pointInCircle)
// ============================================================
static void test_lua_flat_args() {
    printf("--- Lua flat args ---\n");

    CollisionFixture f;

    LuaResult r1 = f.exec(
        "aabb_hit = engine.collision.aabb(0,0,10,10, 5,5,10,10) and 1 or 0\n"
        "aabb_miss = engine.collision.aabb(0,0,10,10, 20,20,10,10) and 1 or 0\n"
    );
    ASSERT(r1.success, "aabb flat");
    ASSERT(f.getNum("aabb_hit") == 1.0, "aabb hit");
    ASSERT(f.getNum("aabb_miss") == 0.0, "aabb miss");

    LuaResult r2 = f.exec(
        "cc_hit = engine.collision.circleCircle(0,0,5, 8,0,5) and 1 or 0\n"
        "cc_miss = engine.collision.circleCircle(0,0,5, 20,0,5) and 1 or 0\n"
    );
    ASSERT(r2.success, "circleCircle flat");
    ASSERT(f.getNum("cc_hit") == 1.0, "circleCircle hit");
    ASSERT(f.getNum("cc_miss") == 0.0, "circleCircle miss");

    LuaResult r3 = f.exec(
        "pr_hit = engine.collision.pointInRect(5,5, 0,0,10,10) and 1 or 0\n"
        "pr_miss = engine.collision.pointInRect(15,15, 0,0,10,10) and 1 or 0\n"
    );
    ASSERT(r3.success, "pointInRect flat");
    ASSERT(f.getNum("pr_hit") == 1.0, "pointInRect hit");
    ASSERT(f.getNum("pr_miss") == 0.0, "pointInRect miss");

    LuaResult r4 = f.exec(
        "pc_hit = engine.collision.pointInCircle(0,0, 0,0,5) and 1 or 0\n"
        "pc_miss = engine.collision.pointInCircle(10,10, 0,0,5) and 1 or 0\n"
    );
    ASSERT(r4.success, "pointInCircle flat");
    ASSERT(f.getNum("pc_hit") == 1.0, "pointInCircle hit");
    ASSERT(f.getNum("pc_miss") == 0.0, "pointInCircle miss");
}

// ============================================================
// Lua — lineLine (returns bool, ix, iy on hit)
// ============================================================
static void test_lua_lineLine() {
    printf("--- Lua lineLine ---\n");

    CollisionFixture f;

    LuaResult r = f.exec(
        "local hit, ix, iy = engine.collision.lineLine(0,0,10,10, 0,10,10,0)\n"
        "ll_hit = hit and 1 or 0\n"
        "ll_ix = ix or 0\n"
        "ll_iy = iy or 0\n"
    );
    ASSERT(r.success, "lineLine script");
    ASSERT(f.getNum("ll_hit") == 1.0, "lineLine hit");
    double ix = f.getNum("ll_ix");
    double iy = f.getNum("ll_iy");
    ASSERT(std::fabs(ix - 5.0) < EPS, "lineLine ix ~= 5");
    ASSERT(std::fabs(iy - 5.0) < EPS, "lineLine iy ~= 5");

    LuaResult r2 = f.exec(
        "ll_miss = engine.collision.lineLine(0,0,10,0, 0,10,10,10) and 1 or 0\n"
    );
    ASSERT(r2.success, "lineLine parallel");
    ASSERT(f.getNum("ll_miss") == 0.0, "lineLine parallel returns false");
}

// ============================================================
// Lua — lineCircle
// ============================================================
static void test_lua_lineCircle() {
    printf("--- Lua lineCircle ---\n");

    CollisionFixture f;

    LuaResult r = f.exec(
        "lc_hit = engine.collision.lineCircle(0,0,10,0, 5,0,2) and 1 or 0\n"
        "lc_miss = engine.collision.lineCircle(0,0,10,0, 5,10,2) and 1 or 0\n"
    );
    ASSERT(r.success, "lineCircle script");
    ASSERT(f.getNum("lc_hit") == 1.0, "lineCircle hit");
    ASSERT(f.getNum("lc_miss") == 0.0, "lineCircle miss");
}

// ============================================================
// Lua — userdata overloads (aabb, pointInRect, pointInCircle)
// ============================================================
static void test_lua_userdata_overloads() {
    printf("--- Lua userdata overloads ---\n");

    CollisionFixture f;

    LuaResult r1 = f.exec(
        "local r1 = Rect(0,0,10,10)\n"
        "local r2 = Rect(5,5,10,10)\n"
        "ud_aabb_hit = engine.collision.aabb(r1, r2) and 1 or 0\n"
        "local r3 = Rect(50,50,5,5)\n"
        "ud_aabb_miss = engine.collision.aabb(r1, r3) and 1 or 0\n"
    );
    ASSERT(r1.success, "aabb Rect userdata");
    ASSERT(f.getNum("ud_aabb_hit") == 1.0, "aabb(Rect,Rect) hit");
    ASSERT(f.getNum("ud_aabb_miss") == 0.0, "aabb(Rect,Rect) miss");

    LuaResult r2 = f.exec(
        "local p = Point(5, 5)\n"
        "local r = Rect(0,0,10,10)\n"
        "ud_pr_hit = engine.collision.pointInRect(p, r) and 1 or 0\n"
        "local p2 = Point(15,15)\n"
        "ud_pr_miss = engine.collision.pointInRect(p2, r) and 1 or 0\n"
    );
    ASSERT(r2.success, "pointInRect Point+Rect userdata");
    ASSERT(f.getNum("ud_pr_hit") == 1.0, "pointInRect(Point,Rect) hit");
    ASSERT(f.getNum("ud_pr_miss") == 0.0, "pointInRect(Point,Rect) miss");

    LuaResult r3 = f.exec(
        "local v = Vec2(3, 4)\n"
        "ud_pc_hit = engine.collision.pointInCircle(v, 0, 0, 5) and 1 or 0\n"
        "local v2 = Vec2(10, 10)\n"
        "ud_pc_miss = engine.collision.pointInCircle(v2, 0, 0, 5) and 1 or 0\n"
    );
    ASSERT(r3.success, "pointInCircle Vec2 userdata");
    ASSERT(f.getNum("ud_pc_hit") == 1.0, "pointInCircle(Vec2, cx,cy,r) hit");
    ASSERT(f.getNum("ud_pc_miss") == 0.0, "pointInCircle(Vec2, cx,cy,r) miss");
}

// ============================================================
// main
// ============================================================
int main() {
    printf("=== collision_test ===\n");

    test_cpp_aabb();
    test_cpp_circleCircle();
    test_cpp_pointInRect();
    test_cpp_pointInCircle();
    test_cpp_lineLine();
    test_cpp_lineCircle();

    test_lua_flat_args();
    test_lua_lineLine();
    test_lua_lineCircle();
    test_lua_userdata_overloads();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
