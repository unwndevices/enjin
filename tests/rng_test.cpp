// rng_test.cpp — tests for engine.random.* seeded xorshift32 PRNG
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <cstdio>
#include <cstdlib>

using namespace enjin2;

#define ASSERT(cond, msg) \
    do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); exit(1); } } while(0)

int main() {
    printf("=== rng_test ===\n");

    LuaEngine engine;
    LuaBindings bindings(&engine);
    engine.initialize();
    bindings.registerAll();

    // ── TEST 1: Deterministic sequence — same seed produces same results ──
    {
        LuaResult r = engine.executeString(R"(
            engine.random.seed(42)
            local a1 = engine.random.integer(1, 100)
            local a2 = engine.random.integer(1, 100)
            local a3 = engine.random.integer(1, 100)

            engine.random.seed(42)
            local b1 = engine.random.integer(1, 100)
            local b2 = engine.random.integer(1, 100)
            local b3 = engine.random.integer(1, 100)

            assert(a1 == b1, "determinism: a1 == b1, got " .. a1 .. " vs " .. b1)
            assert(a2 == b2, "determinism: a2 == b2, got " .. a2 .. " vs " .. b2)
            assert(a3 == b3, "determinism: a3 == b3, got " .. a3 .. " vs " .. b3)
        )");
        ASSERT(r.success, "deterministic sequence");
        printf("  PASS: deterministic sequence\n");
    }

    // ── TEST 2: integer(a,b) range bounds ──
    {
        LuaResult r = engine.executeString(R"(
            engine.random.seed(123)
            for i = 1, 1000 do
                local v = engine.random.integer(5, 10)
                assert(v >= 5, "range: v >= 5, got " .. v)
                assert(v <= 10, "range: v <= 10, got " .. v)
            end
        )");
        ASSERT(r.success, "integer range bounds");
        printf("  PASS: integer(a,b) range bounds\n");
    }

    // ── TEST 3: integer(a,b) with reversed args ──
    {
        LuaResult r = engine.executeString(R"(
            engine.random.seed(456)
            for i = 1, 100 do
                local v = engine.random.integer(10, 5)
                assert(v >= 5, "reversed range: v >= 5, got " .. v)
                assert(v <= 10, "reversed range: v <= 10, got " .. v)
            end
        )");
        ASSERT(r.success, "integer reversed args");
        printf("  PASS: integer(a,b) reversed args\n");
    }

    // ── TEST 4: float() range [0, 1) ──
    {
        LuaResult r = engine.executeString(R"(
            engine.random.seed(789)
            for i = 1, 1000 do
                local v = engine.random.float()
                assert(v >= 0.0, "float range: v >= 0, got " .. v)
                assert(v < 1.0, "float range: v < 1, got " .. v)
            end
        )");
        ASSERT(r.success, "float() range [0,1)");
        printf("  PASS: float() range [0, 1)\n");
    }

    // ── TEST 5: float(a, b) range ──
    {
        LuaResult r = engine.executeString(R"(
            engine.random.seed(101)
            for i = 1, 1000 do
                local v = engine.random.float(3.0, 7.0)
                assert(v >= 3.0, "float(a,b) range: v >= 3, got " .. v)
                assert(v < 7.0, "float(a,b) range: v < 7, got " .. v)
            end
        )");
        ASSERT(r.success, "float(a,b) range");
        printf("  PASS: float(a, b) range\n");
    }

    // ── TEST 6: seed(0) uses non-zero default (no zero-lock) ──
    {
        LuaResult r = engine.executeString(R"(
            engine.random.seed(0)
            local v1 = engine.random.integer(1, 1000000)
            local v2 = engine.random.integer(1, 1000000)
            assert(v1 ~= v2, "seed(0) should not produce constant sequence, got " .. v1 .. " twice")
        )");
        ASSERT(r.success, "seed(0) non-zero fallback");
        printf("  PASS: seed(0) uses non-zero default\n");
    }

    // ── TEST 7: Different seeds produce different sequences ──
    {
        LuaResult r = engine.executeString(R"(
            engine.random.seed(100)
            local a = engine.random.integer(1, 1000000)
            engine.random.seed(200)
            local b = engine.random.integer(1, 1000000)
            assert(a ~= b, "different seeds should give different results, got " .. a .. " and " .. b)
        )");
        ASSERT(r.success, "different seeds produce different sequences");
        printf("  PASS: different seeds produce different sequences\n");
    }

    printf("=== rng_test: ALL PASSED ===\n");
    return 0;
}
