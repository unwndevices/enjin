#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <fstream>
#include <sys/stat.h>

int main() {
    ankerl::nanobench::Bench bench;
    bench.title("lua").warmup(3).epochs(20).epochIterations(1);

    // --- Lua engine init+shutdown cycle ---
    bench.run("lua engine: init+shutdown", [&] {
        enjin2::LuaEngine eng;
        bool ok = eng.initialize();
        ankerl::nanobench::doNotOptimizeAway(ok);
        eng.shutdown();
    });

    // --- Script load, binding call overhead, GC pressure ---
    // Single LuaEngine initialized outside timed lambdas for reuse benchmarks
    {
        enjin2::LuaEngine eng;
        eng.initialize();

        // Script load: measure executeString compilation + execution cost
        bench.run("lua engine: executeString (noop script)", [&] {
            auto result = eng.executeString("local x = 1 + 1");
            ankerl::nanobench::doNotOptimizeAway(result.success);
        });

        // Binding call overhead — registerAll() once outside timed region
        // SAFE: registerAll() stores engine.* pointers in closures; no dereference during registration
        enjin2::LuaBindings bindings(&eng);
        bindings.registerAll();

        // Measure per-call C-to-Lua binding overhead for engine.time.delta()
        // engine.time.delta() reads m_timeState which is a value type (not a pointer dereference)
        bench.run("lua binding: engine.time.delta call", [&] {
            auto result = eng.executeString("local t = engine.time.delta()");
            ankerl::nanobench::doNotOptimizeAway(result.success);
        });

        // Math utility: math.clamp has no engine pointer dependency
        bench.run("lua binding: math.clamp call", [&] {
            auto result = eng.executeString("local v = math.clamp(0.5, 0, 1)");
            ankerl::nanobench::doNotOptimizeAway(result.success);
        });

        // GC pressure: full Lua garbage collection cycle
        bench.run("lua GC: full collect", [&] {
            lua_State* L = eng.getState();
            int before = lua_gc(L, LUA_GCCOUNT, 0);
            lua_gc(L, LUA_GCCOLLECT, 0);
            int after = lua_gc(L, LUA_GCCOUNT, 0);
            ankerl::nanobench::doNotOptimizeAway(before);
            ankerl::nanobench::doNotOptimizeAway(after);
        });

        eng.shutdown();
    }

    // Write JSON results
    mkdir("bench-results", 0755);
    std::ofstream out("bench-results/bench_lua.json");
    ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out);

    return 0;
}
