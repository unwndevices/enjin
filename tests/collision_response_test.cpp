// collision_response_test.cpp — tests for aabbOverlap, circleResponse, reflect
// Tests both C++ functions (collision.hpp) and Lua bindings (engine.collision.*)
#include <enjin2/core/collision.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace enjin2;

#define ASSERT(cond, msg) \
    do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); exit(1); } } while(0)

#define ASSERT_NEAR(a, b, eps, msg) \
    do { if (std::fabs((a)-(b)) > (eps)) { printf("FAIL: %s — got %f, expected %f (line %d)\n", msg, (double)(a), (double)(b), __LINE__); exit(1); } } while(0)

int main() {
    printf("=== collision_response_test ===\n");

    // ── C++ aabbOverlap ──
    {
        float ox, oy, ow, oh;

        bool hit = collision::aabbOverlap(0, 0, 10, 10, 5, 5, 10, 10, &ox, &oy, &ow, &oh);
        ASSERT(hit, "aabbOverlap: overlapping rects should hit");
        ASSERT_NEAR(ox, 5.0f, 0.001f, "aabbOverlap: overlapX");
        ASSERT_NEAR(oy, 5.0f, 0.001f, "aabbOverlap: overlapY");
        ASSERT_NEAR(ow, 5.0f, 0.001f, "aabbOverlap: overlapW");
        ASSERT_NEAR(oh, 5.0f, 0.001f, "aabbOverlap: overlapH");
        printf("  PASS: C++ aabbOverlap (overlapping)\n");

        hit = collision::aabbOverlap(0, 0, 5, 5, 10, 10, 5, 5, &ox, &oy, &ow, &oh);
        ASSERT(!hit, "aabbOverlap: non-overlapping rects should not hit");
        printf("  PASS: C++ aabbOverlap (non-overlapping)\n");

        hit = collision::aabbOverlap(0, 0, 10, 10, 10, 0, 10, 10, &ox, &oy, &ow, &oh);
        ASSERT(!hit, "aabbOverlap: edge-touching should not hit");
        printf("  PASS: C++ aabbOverlap (edge-touching)\n");

        hit = collision::aabbOverlap(0, 0, 20, 20, 5, 5, 5, 5, &ox, &oy, &ow, &oh);
        ASSERT(hit, "aabbOverlap: contained should hit");
        ASSERT_NEAR(ow, 5.0f, 0.001f, "aabbOverlap: contained overlapW");
        ASSERT_NEAR(oh, 5.0f, 0.001f, "aabbOverlap: contained overlapH");
        printf("  PASS: C++ aabbOverlap (contained)\n");
    }

    // ── C++ circleCircleResponse ──
    {
        float nx, ny, depth;

        bool hit = collision::circleCircleResponse(0, 0, 5, 8, 0, 5, &nx, &ny, &depth);
        ASSERT(hit, "circleResponse: overlapping circles should hit");
        ASSERT_NEAR(nx, 1.0f, 0.001f, "circleResponse: normalX should be 1");
        ASSERT_NEAR(ny, 0.0f, 0.001f, "circleResponse: normalY should be 0");
        ASSERT_NEAR(depth, 2.0f, 0.001f, "circleResponse: depth should be 2");
        printf("  PASS: C++ circleCircleResponse (horizontal overlap)\n");

        hit = collision::circleCircleResponse(0, 0, 3, 10, 0, 3, &nx, &ny, &depth);
        ASSERT(!hit, "circleResponse: non-overlapping should not hit");
        printf("  PASS: C++ circleCircleResponse (non-overlapping)\n");

        hit = collision::circleCircleResponse(5, 5, 3, 5, 5, 3, &nx, &ny, &depth);
        ASSERT(hit, "circleResponse: exact overlap should hit");
        ASSERT_NEAR(depth, 6.0f, 0.001f, "circleResponse: exact overlap depth = 2*r");
        printf("  PASS: C++ circleCircleResponse (exact overlap)\n");
    }

    // ── C++ reflect ──
    {
        float outVx, outVy;

        collision::reflect(1.0f, 0.0f, -1.0f, 0.0f, &outVx, &outVy);
        ASSERT_NEAR(outVx, -1.0f, 0.001f, "reflect: vertical wall outVx");
        ASSERT_NEAR(outVy, 0.0f, 0.001f, "reflect: vertical wall outVy");
        printf("  PASS: C++ reflect (vertical wall)\n");

        collision::reflect(1.0f, -1.0f, 0.0f, 1.0f, &outVx, &outVy);
        ASSERT_NEAR(outVx, 1.0f, 0.001f, "reflect: floor outVx");
        ASSERT_NEAR(outVy, 1.0f, 0.001f, "reflect: floor outVy");
        printf("  PASS: C++ reflect (floor bounce)\n");

        collision::reflect(1.0f, 1.0f, -1.0f, 0.0f, &outVx, &outVy);
        ASSERT_NEAR(outVx, -1.0f, 0.001f, "reflect: side wall outVx");
        ASSERT_NEAR(outVy, 1.0f, 0.001f, "reflect: side wall outVy");
        printf("  PASS: C++ reflect (side wall)\n");
    }

    // ── Lua binding tests ──
    {
        LuaEngine engine;
        LuaBindings bindings(&engine);
        engine.initialize();
        bindings.registerAll();

        // Lua aabbOverlap
        LuaResult r = engine.executeString(R"(
            local hit, ox, oy, ow, oh = engine.collision.aabbOverlap(0,0,10,10, 5,5,10,10)
            assert(hit == true, "lua aabbOverlap: hit")
            assert(math.abs(ox - 5) < 0.01, "lua aabbOverlap: ox=" .. tostring(ox))
            assert(math.abs(oy - 5) < 0.01, "lua aabbOverlap: oy")
            assert(math.abs(ow - 5) < 0.01, "lua aabbOverlap: ow")
            assert(math.abs(oh - 5) < 0.01, "lua aabbOverlap: oh")
        )");
        ASSERT(r.success, "lua aabbOverlap script");
        printf("  PASS: Lua aabbOverlap\n");

        // Lua aabbOverlap miss
        r = engine.executeString(R"(
            local hit, ox, oy, ow, oh = engine.collision.aabbOverlap(0,0,5,5, 10,10,5,5)
            assert(hit == false, "lua aabbOverlap miss: hit should be false")
            assert(ox == nil, "lua aabbOverlap miss: ox should be nil")
        )");
        ASSERT(r.success, "lua aabbOverlap miss script");
        printf("  PASS: Lua aabbOverlap (miss)\n");

        // Lua circleResponse
        r = engine.executeString(R"(
            local hit, nx, ny, depth = engine.collision.circleResponse(0,0,5, 8,0,5)
            assert(hit == true, "lua circleResponse: hit")
            assert(math.abs(nx - 1) < 0.01, "lua circleResponse: nx")
            assert(math.abs(ny - 0) < 0.01, "lua circleResponse: ny")
            assert(math.abs(depth - 2) < 0.01, "lua circleResponse: depth")
        )");
        ASSERT(r.success, "lua circleResponse script");
        printf("  PASS: Lua circleResponse\n");

        // Lua reflect
        r = engine.executeString(R"(
            local vx, vy = engine.collision.reflect(1, -1, 0, 1)
            assert(math.abs(vx - 1) < 0.01, "lua reflect: vx")
            assert(math.abs(vy - 1) < 0.01, "lua reflect: vy")
        )");
        ASSERT(r.success, "lua reflect script");
        printf("  PASS: Lua reflect\n");
    }

    printf("=== collision_response_test: ALL PASSED ===\n");
    return 0;
}
