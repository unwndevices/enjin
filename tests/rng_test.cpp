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

    // ── Seeded generators: engine.random.new(seed) (Tomodachi #406) ──

    // TEST 8: two generators from one seed give one sequence
    {
        LuaResult r = engine.executeString(R"(
            local a, b = engine.random.new(42), engine.random.new(42)
            for i = 1, 50 do
                local x, y = a:integer(1, 1000), b:integer(1, 1000)
                assert(x == y, "same seed: draw " .. i .. " got " .. x .. " vs " .. y)
                local f, g = a:float(), b:float()
                assert(f == g, "same seed: float " .. i .. " got " .. f .. " vs " .. g)
            end
        )");
        ASSERT(r.success, "two generators from one seed agree");
        printf("  PASS: two generators from one seed agree\n");
    }

    // TEST 9: two seeds give two sequences
    {
        LuaResult r = engine.executeString(R"(
            local a, b = engine.random.new(100), engine.random.new(200)
            local same = 0
            for i = 1, 20 do
                if a:integer(1, 1000000) == b:integer(1, 1000000) then same = same + 1 end
            end
            assert(same < 20, "two seeds gave one sequence")
        )");
        ASSERT(r.success, "two seeds differ");
        printf("  PASS: two seeds differ\n");
    }

    // TEST 10: a generator never moves the global stream, nor another generator
    {
        LuaResult r = engine.executeString(R"(
            engine.random.seed(7)
            local want = {}
            for i = 1, 10 do want[i] = engine.random.integer(1, 1000) end
            local wantG = {}
            local solo = engine.random.new(9)
            for i = 1, 10 do wantG[i] = solo:integer(1, 1000) end

            engine.random.seed(7)
            local g, other = engine.random.new(7), engine.random.new(9)
            for i = 1, 10 do
                g:integer(1, 1000); g:float(); g:float(0, 5)
                local v = engine.random.integer(1, 1000)
                assert(v == want[i], "global draw " .. i .. " moved: " .. v .. " vs " .. want[i])
                local o = other:integer(1, 1000)
                assert(o == wantG[i], "generator draw " .. i .. " moved: " .. o .. " vs " .. wantG[i])
            end
        )");
        ASSERT(r.success, "a generator never moves the global stream");
        printf("  PASS: a generator never moves the global stream\n");
    }

    // TEST 11: a known seed gives a literal sequence (the cross-host check).
    // The literals are worked outside this code: the seed through MurmurHash3's
    // fmix32, then xorshift32 (13, 17, 5).
    {
        LuaResult r = engine.executeString(R"(
            local g = engine.random.new(42)
            local got = {}
            for i = 1, 6 do got[i] = g:integer(1, 100) end
            got = table.concat(got, ",")
            assert(got == "55,85,57,44,50,88", "seed 42 integers: " .. got)
            -- The bounds may come in either order, as the module function's.
            local h = engine.random.new(42)
            for i = 1, 6 do got = h:integer(100, 1) end
            assert(got == 88, "integer(100, 1): " .. got)

            local f = engine.random.new(2026)
            local want = { 0.781058706757859, 0.5538218288109223, 0.7985827975903133 }
            for i = 1, 3 do
                local v = f:float()
                assert(math.abs(v - want[i]) < 1e-12, "seed 2026 float " .. i .. ": " .. v)
            end
            local s = engine.random.new(2026)
            local v = s:float(10, 20)
            assert(math.abs(v - 17.81058706757859) < 1e-9, "float(10, 20): " .. v)
            -- Seed 0 is the same fixed seed as engine.random.seed(0).
            engine.random.seed(0)
            assert(engine.random.new(0):integer(1, 1000000) == engine.random.integer(1, 1000000),
                   "seed 0")
        )");
        if (!r.success) printf("    %s\n", r.error.c_str());
        ASSERT(r.success, "a known seed gives a literal sequence");
        printf("  PASS: a known seed gives a literal sequence\n");
    }

    // TEST 13: nearby seeds (1, 2, 3: a level number) give unrelated streams.
    // Raw xorshift32 starts every small seed near 0, so the first draws of
    // seeds 1..40 would all fall low.
    {
        LuaResult r = engine.executeString(R"(
            for draw = 1, 3 do
                local low = 0
                for seed = 1, 40 do
                    local g = engine.random.new(seed)
                    local v
                    for i = 1, draw do v = g:float() end
                    if v < 0.5 then low = low + 1 end
                end
                assert(low >= 10 and low <= 30,
                       "draw " .. draw .. " of seeds 1..40: " .. low .. " of 40 below 0.5")
            end
        )");
        if (!r.success) printf("    %s\n", r.error.c_str());
        ASSERT(r.success, "nearby seeds give unrelated streams");
        printf("  PASS: nearby seeds give unrelated streams\n");
    }

    // TEST 12: a generator's methods want the generator
    {
        LuaResult r = engine.executeString(R"(
            local g = engine.random.new(1)
            assert(not pcall(g.integer, {}, 1, 2), "integer on a non-generator raises")
            assert(not pcall(engine.random.new), "new without a seed raises")
        )");
        ASSERT(r.success, "methods check their generator");
        printf("  PASS: methods check their generator\n");
    }

    printf("=== rng_test: ALL PASSED ===\n");
    return 0;
}
