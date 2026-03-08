#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/core/scene.hpp>
#include <fstream>
#include <sys/stat.h>

// Minimal concrete scene for ObjectProxy round-trip benchmark — no SDL, no canvas required
class BenchScene : public enjin2::Scene {
public:
    BenchScene() : enjin2::Scene(99) {}
};

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

        // ObjectProxy round-trip benchmark (BENCH-04 gap closure):
        // Wire a real Scene with a named Object into LuaBindings so engine.scene.find works.
        // NOTE: setActiveScene clears event bus handlers, coroutines, tweens, camera follow.
        // The event dispatch benchmark subscribes AFTER this call.
        BenchScene benchScene;
        benchScene.activate();
        auto* benchObj = benchScene.addObject<enjin2::Object>();
        benchObj->setName("bench_target");
        bindings.setActiveScene(&benchScene);

        // Measures: Lua engine.scene.find -> C++ findByName -> lua_newuserdata(ObjectProxy) ->
        // metatable attach -> setLuaProxy -> Lua p.name -> __index -> Object::getName() -> string
        bench.run("lua proxy: find+field round-trip", [&] {
            auto result = eng.executeString("local p = engine.scene.find('bench_target'); local n = p.name");
            ankerl::nanobench::doNotOptimizeAway(result.success);
        });

        // Event dispatch benchmark (BENCH-03 gap closure):
        // Subscribe a Lua no-op handler AFTER setActiveScene (which cleared prior handlers).
        // Measures: Lua engine.event.emit -> C++ LuaEventBus::emit -> channel lookup ->
        // snapshot refs -> lua_rawgeti + lua_pcall for callback -> return
        eng.executeString("engine.event.on('bench_evt', function() end)");
        bench.run("lua event: emit dispatch", [&] {
            auto result = eng.executeString("engine.event.emit('bench_evt')");
            ankerl::nanobench::doNotOptimizeAway(result.success);
        });

        // Detach scene cleanly before BenchScene destructor runs — prevents dangling pointer
        // in the Lua registry after the local BenchScene goes out of scope.
        bindings.setActiveScene(nullptr);

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
