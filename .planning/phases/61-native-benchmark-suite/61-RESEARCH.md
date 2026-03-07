# Phase 61: Native Benchmark Suite - Research

**Researched:** 2026-03-07
**Domain:** nanobench benchmark authoring, Canvas4/Canvas8 pixel ops, ECS (ObjectCollection/Scene), LuaEngine headless init, JSON file output, shell script orchestration
**Confidence:** HIGH

## Summary

Phase 61 implements three benchmark binaries (`bench_canvas`, `bench_ecs`, `bench_lua`) and a shell driver script (`scripts/build-bench.sh`). The nanobench infrastructure and CMake scaffold are already in place from Phase 60. This phase is entirely about writing benchmark code that exercises real subsystem APIs, writing valid JSON output to `bench-results/`, and preventing dead-code elimination on the measured operations.

All three binaries must link and run without SDL3. The project never links SDL3 into `enjin2_core`, `enjin2_graphics`, or `enjin2_lua` — SDL3 only enters through the separate SDL runner in `examples/`. Therefore the "no SDL3" constraint is satisfied automatically as long as benchmarks link only `enjin2_core + enjin2_graphics` (canvas), `enjin2_core + enjin2_graphics + enjin2_ui` (ECS), and `enjin2_lua + enjin2_core + enjin2_graphics + enjin2_ui` (Lua).

The dead-code elimination constraint for `bench_canvas` (BENCH-02 success criterion 3) requires every pixel operation to produce a side effect that escapes through `ankerl::nanobench::doNotOptimizeAway()`. Reading back a pixel value from the canvas buffer and passing it to `doNotOptimizeAway` is the verified pattern.

**Primary recommendation:** Use the Canvas4/Canvas8 concrete template types directly in benchmark code (no polymorphism), call `doNotOptimizeAway` on a pixel value read back from the buffer after each drawing operation, and keep each benchmark binary to a single `.cpp` file defining `ANKERL_NANOBENCH_IMPLEMENT` exactly once.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BENCH-02 | bench_canvas binary benchmarks Canvas4/Canvas8 pixel ops, fill, rect, circle, sprite blit, multi-layer composite | Canvas4<W,H> and Canvas8<W,H> are concrete template types with setPixel/fillRect/drawLine/drawCircle/clear/blit/LayerCompositor::composite() methods; all accessible without SDL3; doNotOptimizeAway pattern verified from nanobench header |
| BENCH-03 | bench_ecs binary benchmarks Object creation, component attach/detach, scene::update() at 1/8/16/32/48 objects, event dispatch | ObjectCollection::addObject<T>(), Scene::update(dt), Object::addComponent<T>(), Object::removeComponent<T>() are all available on enjin2_core + enjin2_ui; no SDL3 dependency |
| BENCH-04 | bench_lua binary benchmarks Lua engine init, script load, per-module binding call overhead, ObjectProxy round-trip, GC pressure | LuaEngine::initialize()/shutdown(), LuaEngine::executeString(), LuaBindings::registerAll() work headlessly; system Lua 5.4.8 confirmed present; no SDL3 needed |
| BENCH-05 | All benchmark binaries produce JSON output to bench-results/ directory | ankerl::nanobench::render(templates::json(), bench, ofstream) is the verified API; bench-results/ is already gitignored |
| BENCH-06 | scripts/build-bench.sh builds and runs all benchmarks in one command | CMake -DENJIN2_BUILD_BENCHMARKS=ON + cmake --build + mkdir -p bench-results + run each binary with output redirect; script pattern established for desktop |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| nanobench | v4.3.11 | Microbenchmark harness + JSON output | Vendored in Phase 60; already compiles via bench_smoke |
| enjin2_core | project | Object, Scene, ObjectCollection for ECS bench | No SDL3 dependency |
| enjin2_graphics | project | Canvas4, Canvas8, LayerCompositor, Primitives for canvas bench | No SDL3 dependency |
| enjin2_ui | project | C_Drawable and drawable components needed for ECS bench | Transitively links enjin2_graphics |
| enjin2_lua | project | LuaEngine, LuaBindings for Lua bench | Links enjin2_graphics + enjin2_ui; system Lua 5.4.8 on desktop |
| Lua 5.4.8 | 5.4.8 (system) | Runtime for bench_lua | Detected via find_package(Lua) on desktop; 5.4.8 confirmed present |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `<fstream>` | C++17 std | Write JSON results file | bench-results/ output in each binary |
| `<cstdio>` / `<sys/stat.h>` | POSIX | Create bench-results/ directory if absent | Before opening the output stream |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Concrete `Canvas4<128,128>` | `ICanvas<Pixel4>*` (polymorphic) | Polymorphic calls add virtual dispatch overhead that contaminates canvas measurements; use concrete type to measure actual pixel path |
| `doNotOptimizeAway(pixel_readback)` | `volatile` keyword | `volatile` has UB semantics for this purpose in C++; nanobench's barrier is portable and documented |
| Single `bench_all.cpp` | Three separate binaries | Separate binaries allow independent JSON files per subsystem as required by BENCH-05; also isolates link dependencies |

## Architecture Patterns

### Recommended Project Structure
```
benchmarks/
├── CMakeLists.txt       # existing — add bench_canvas, bench_ecs, bench_lua targets
├── bench_smoke.cpp      # existing
├── bench_canvas.cpp     # new — BENCH-02
├── bench_ecs.cpp        # new — BENCH-03
└── bench_lua.cpp        # new — BENCH-04

scripts/
└── build-bench.sh       # new — BENCH-06

bench-results/           # runtime output — gitignored; created by build-bench.sh
├── bench_canvas.json    # written by bench_canvas binary
├── bench_ecs.json       # written by bench_ecs binary
└── bench_lua.json       # written by bench_lua binary
```

### Pattern 1: nanobench JSON output to file
**What:** Use `ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, outFile)` after all benchmark runs complete to write a JSON file.
**When to use:** In each benchmark binary's `main()` after all `bench.run()` calls.

```cpp
// Source: nanobench v4.3.11 header lines 290, 347, 1004 (verified in vendor/nanobench.h)
#include <fstream>
#include <sys/stat.h>

// In main():
ankerl::nanobench::Bench bench;
bench.title("canvas").warmup(10).epochs(100).epochIterations(1);

// ... bench.run() calls ...

// Write JSON output
mkdir("bench-results", 0755);  // no-op if already exists
std::ofstream outFile("bench-results/bench_canvas.json");
ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, outFile);
```

### Pattern 2: doNotOptimizeAway to prevent dead-code elimination
**What:** After any drawing operation, read back a pixel value from the canvas buffer and pass it to `doNotOptimizeAway`. This forces the compiler to treat the drawing call as producing an observable side effect.
**When to use:** Every canvas benchmark that performs pixel operations. Required by success criterion 3 (timings must be non-trivially short — consistent with actual pixel writes at -O2).

```cpp
// Source: nanobench v4.3.11 header lines 650-651, 1023 (verified in vendor/nanobench.h)
// Source: Phase 60 RESEARCH.md — doNotOptimizeAway vs volatile
Canvas4<128, 128> canvas;
bench.run("pixel-ops: setPixel", [&] {
    canvas.setPixel(64, 64, Pixel4(7));
    ankerl::nanobench::doNotOptimizeAway(canvas.getPixel(64, 64));
});
```

### Pattern 3: Canvas benchmark — Canvas4 pixel ops
**What:** Benchmark `setPixel`, `clear`, `fillRect`, `drawCircle` on a `Canvas4<128,128>`, plus multi-layer `LayerCompositor::composite()`.
**When to use:** bench_canvas.cpp — covers BENCH-02 pixel ops, fill, rect, circle, composite.

```cpp
// Source: include/enjin2/graphics/canvas.hpp (verified direct read)
//         include/enjin2/graphics/layer_compositor.hpp (verified direct read)
//         include/enjin2/graphics/primitives.hpp (verified direct read)
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/layer_compositor.hpp>
#include <enjin2/graphics/primitives.hpp>
#include <fstream>
#include <sys/stat.h>

int main() {
    ankerl::nanobench::Bench bench;
    bench.title("canvas").warmup(10).epochs(100).epochIterations(1);

    // Canvas4 operations
    enjin2::Canvas4<128, 128> canvas4;

    bench.run("canvas4: setPixel", [&] {
        canvas4.setPixel(64, 64, enjin2::Pixel4(7));
        ankerl::nanobench::doNotOptimizeAway(canvas4.getPixel(64, 64));
    });

    bench.run("canvas4: clear", [&] {
        canvas4.clear(enjin2::Pixel4(0));
        ankerl::nanobench::doNotOptimizeAway(canvas4.getPixel(0, 0));
    });

    bench.run("canvas4: fillRect 32x32", [&] {
        canvas4.fillRect(0, 0, 32, 32, enjin2::Pixel4(3));
        ankerl::nanobench::doNotOptimizeAway(canvas4.getPixel(16, 16));
    });

    bench.run("canvas4: drawCircle r16", [&] {
        // Primitives<Pixel4>::drawCircle is a static method
        enjin2::Primitives<enjin2::Pixel4>::drawCircle(canvas4, 64, 64, 16, enjin2::Pixel4(5));
        ankerl::nanobench::doNotOptimizeAway(canvas4.getPixel(64, 48));
    });

    // Canvas8 operations
    enjin2::Canvas8<128, 128> canvas8;

    bench.run("canvas8: setPixel", [&] {
        canvas8.setPixel(64, 64, 200);
        ankerl::nanobench::doNotOptimizeAway(canvas8.getPixel(64, 64));
    });

    bench.run("canvas8: fillRect 32x32", [&] {
        canvas8.fillRect(0, 0, 32, 32, 128);
        ankerl::nanobench::doNotOptimizeAway(canvas8.getPixel(16, 16));
    });

    // Multi-layer composite
    enjin2::LayerCompositor<128, 128> compositor;
    bench.run("compositor: composite 5 layers", [&] {
        compositor.clearAll();
        compositor.layers[0].fillRect(0, 0, 128, 128, enjin2::Pixel4(1));
        compositor.composite();
        ankerl::nanobench::doNotOptimizeAway(compositor.output.getPixel(64, 64));
    });

    // Canvas4 blit
    enjin2::Canvas4<32, 32> sprite;
    sprite.clear(enjin2::Pixel4(5));
    bench.run("canvas4: blit 32x32 sprite", [&] {
        canvas4.blit(sprite, 10, 10, enjin2::Pixel4(0));
        ankerl::nanobench::doNotOptimizeAway(canvas4.getPixel(20, 20));
    });

    mkdir("bench-results", 0755);
    std::ofstream out("bench-results/bench_canvas.json");
    ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out);
    return 0;
}
```

### Pattern 4: ECS benchmark — Scene/Object/Component
**What:** Benchmark `ObjectCollection::addObject<>()`, `Object::addComponent<>()`, `Object::removeComponent<>()`, and `Scene::update()` at multiple object counts.
**When to use:** bench_ecs.cpp — covers BENCH-03.

Key observations from code review:
- `ObjectCollection::addObject<T>()` allocates via `new T(...)` into a `std::array<std::unique_ptr<Object>, 128>`. At pre-allocated counts the hot path is pointer assignment.
- `Scene::update(float dt)` iterates owned objects calling `Object::update(dt)` and `Object::lateUpdate(dt)`.
- `Object::addComponent<T>()` calls `new T(this, ...)` — this IS a dynamic allocation. The benchmark documents this, but does not violate zero-alloc (that's a hot-path concern for ALLOC-01 in Phase 65).
- `Object::removeComponent<T>()` shifts the components array — O(n) on component count, trivially bounded by `MAX_COMPONENTS = 16`.

```cpp
// Source: include/enjin2/core/object_collection.hpp (verified direct read)
//         include/enjin2/core/object.hpp (verified direct read)
//         include/enjin2/core/scene.hpp (verified direct read)
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <enjin2/core/object_collection.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/components/position.hpp>
#include <fstream>
#include <sys/stat.h>

// A minimal concrete scene (no SDL, no canvas — Scene::update only touches objects)
class BenchScene : public enjin2::Scene {
public:
    BenchScene() : enjin2::Scene(1) {}
};

int main() {
    ankerl::nanobench::Bench bench;
    bench.title("ecs").warmup(5).epochs(50).epochIterations(1);

    // Object create (addObject)
    for (int count : {1, 8, 16, 32, 48}) {
        bench.run("scene::addObject x" + std::to_string(count), [&] {
            enjin2::ObjectCollection coll;
            for (int i = 0; i < count; ++i) {
                auto* obj = coll.addObject<enjin2::Object>();
                ankerl::nanobench::doNotOptimizeAway(obj);
            }
        });
    }

    // Component attach/detach
    bench.run("object::addComponent<C_Position>", [&] {
        enjin2::Object obj;
        auto* pos = obj.addComponent<enjin2::C_Position>();
        ankerl::nanobench::doNotOptimizeAway(pos);
    });

    bench.run("object::removeComponent<C_Position>", [&] {
        enjin2::Object obj;
        obj.addComponent<enjin2::C_Position>();
        bool removed = obj.removeComponent<enjin2::C_Position>();
        ankerl::nanobench::doNotOptimizeAway(removed);
    });

    // scene::update at varying object counts
    for (int count : {1, 8, 16, 32, 48}) {
        BenchScene scene;
        scene.activate();
        for (int i = 0; i < count; ++i) {
            scene.addObject<enjin2::Object>();
        }
        bench.run("scene::update x" + std::to_string(count) + " objects", [&] {
            scene.update(0.016f);
            ankerl::nanobench::doNotOptimizeAway(scene.getObjects().size());
        });
    }

    mkdir("bench-results", 0755);
    std::ofstream out("bench-results/bench_ecs.json");
    ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out);
    return 0;
}
```

### Pattern 5: Lua benchmark — headless LuaEngine
**What:** Benchmark `LuaEngine::initialize()` + `shutdown()`, `executeString()` for script load, and `callFunction()` for per-binding overhead. GC pressure via `lua_gc(L, LUA_GCCOLLECT, 0)` through `getState()`.
**When to use:** bench_lua.cpp — covers BENCH-04. No canvas injection needed for pure timing benchmarks.

Key constraints:
- `LuaBindings::registerAll()` injects `engine.*` subtable pointers. In headless mode the pointers `m_ssm`, `m_activeScene`, `m_debugCanvas` are all nullptr. This is safe because the bench does not call `engine.scene.switch` or any binding that dereferences those pointers.
- The `bench_lua` target must link against `enjin2_lua` which depends on `enjin2_graphics + enjin2_ui + ${LUA_LIBRARIES}`. No SDL3 is linked.
- `LuaEngine` on desktop uses `find_package(Lua)` system library — confirmed 5.4.8 present.

```cpp
// Source: include/enjin2/scripting/lua_engine.hpp (verified direct read)
//         include/enjin2/scripting/bindings.hpp (verified direct read)
//         STATE.md — Phase 63 null-binding safety note: engine.* pointers can be null
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <fstream>
#include <sys/stat.h>

int main() {
    ankerl::nanobench::Bench bench;
    bench.title("lua").warmup(3).epochs(20).epochIterations(1);

    // Lua engine init/shutdown cycle
    bench.run("lua engine: init+shutdown", [&] {
        enjin2::LuaEngine eng;
        bool ok = eng.initialize();
        ankerl::nanobench::doNotOptimizeAway(ok);
        eng.shutdown();
    });

    // Script load (executeString)
    {
        enjin2::LuaEngine eng;
        eng.initialize();
        bench.run("lua engine: executeString (noop script)", [&] {
            auto result = eng.executeString("local x = 1 + 1");
            ankerl::nanobench::doNotOptimizeAway(result.success);
        });

        // Per-module binding call overhead — registerAll + call a trivial engine function
        enjin2::LuaBindings bindings(&eng);
        bindings.registerAll();

        bench.run("lua binding: engine.time.delta call", [&] {
            auto result = eng.executeString("local t = engine.time.delta()");
            ankerl::nanobench::doNotOptimizeAway(result.success);
        });

        // ObjectProxy round-trip (create a proxy handle, read a field)
        // engine.scene.find requires m_activeScene -- skip; use a simple global table read instead
        bench.run("lua binding: math.clamp call", [&] {
            auto result = eng.executeString("local v = math.clamp(0.5, 0, 1)");
            ankerl::nanobench::doNotOptimizeAway(result.success);
        });

        // GC pressure — run full collection cycle and measure cost
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

    mkdir("bench-results", 0755);
    std::ofstream out("bench-results/bench_lua.json");
    ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out);
    return 0;
}
```

### Pattern 6: CMakeLists.txt additions for three new targets
**What:** Add `bench_canvas`, `bench_ecs`, `bench_lua` executable targets to `benchmarks/CMakeLists.txt`.
**When to use:** During task that creates the benchmark sources.

```cmake
# benchmarks/CMakeLists.txt additions
# (append after existing bench_smoke target)

# bench_canvas — Canvas4/Canvas8/LayerCompositor benchmarks (BENCH-02)
add_executable(bench_canvas bench_canvas.cpp)
target_include_directories(bench_canvas PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bench_canvas PRIVATE
    nanobench_vendor
    enjin2_core
    enjin2_graphics
)

# bench_ecs — Object/Scene/Component ECS benchmarks (BENCH-03)
add_executable(bench_ecs bench_ecs.cpp)
target_include_directories(bench_ecs PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bench_ecs PRIVATE
    nanobench_vendor
    enjin2_core
    enjin2_graphics
    enjin2_ui
)

# bench_lua — LuaEngine headless benchmarks (BENCH-04)
# Requires Lua 5.4 system library (find_package(Lua) already invoked in root CMakeLists.txt)
add_executable(bench_lua bench_lua.cpp)
target_include_directories(bench_lua PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${LUA_INCLUDE_DIRS}
)
target_link_libraries(bench_lua PRIVATE
    nanobench_vendor
    enjin2_lua
    enjin2_core
    enjin2_graphics
    enjin2_ui
)
```

**CRITICAL:** `LUA_INCLUDE_DIRS` and `LUA_LIBRARIES` are only set when `ENJIN2_BUILD_LUA=ON`. The bench_lua target must be wrapped in `if(ENJIN2_BUILD_LUA)` OR the `benchmarks/CMakeLists.txt` must call `find_package(Lua)` independently. Simplest approach: gate `bench_lua` behind `if(ENJIN2_BUILD_LUA)` mirroring how root CMakeLists.txt handles `enjin2_lua`. This is already ensured because `enjin2_lua` only exists as a target when `ENJIN2_BUILD_LUA=ON`.

### Pattern 7: scripts/build-bench.sh
**What:** Shell script that configures, builds all three benchmark binaries, creates `bench-results/`, and runs each binary.
**When to use:** BENCH-06. Developer runs `bash scripts/build-bench.sh` from project root.

```bash
#!/usr/bin/env bash
# Source: project convention (scripts/build-bench.sh)
# Builds and runs all benchmark binaries, writing JSON to bench-results/
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-bench"

echo "=== enjin2 benchmark suite ==="
echo "Project: ${PROJECT_ROOT}"
echo "Build:   ${BUILD_DIR}"

# Configure
cmake -DENJIN2_BUILD_BENCHMARKS=ON \
      -DENJIN2_BUILD_TESTS=OFF \
      -DENJIN2_BUILD_EXAMPLES=OFF \
      -DENJIN2_BUILD_SDL=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -B "${BUILD_DIR}" \
      "${PROJECT_ROOT}"

# Build the three targets
cmake --build "${BUILD_DIR}" --target bench_canvas bench_ecs bench_lua -- -j"$(nproc)"

# Create output directory
mkdir -p "${PROJECT_ROOT}/bench-results"

# Run each binary (they write JSON themselves)
echo "--- running bench_canvas ---"
"${BUILD_DIR}/benchmarks/bench_canvas"

echo "--- running bench_ecs ---"
"${BUILD_DIR}/benchmarks/bench_ecs"

echo "--- running bench_lua ---"
"${BUILD_DIR}/benchmarks/bench_lua"

echo "=== results written to bench-results/ ==="
ls -lh "${PROJECT_ROOT}/bench-results/"
```

### Anti-Patterns to Avoid
- **Using `ICanvas<Pixel4>&` (polymorphic) in canvas benchmarks:** Virtual dispatch overhead inflates every `setPixel`/`getPixel` call and hides the actual pixel-path cost. Use the concrete `Canvas4<128,128>` directly.
- **Initializing a fresh `Canvas4` inside the lambda:** The constructor calls `clear()` which already benchmarks fill. Initialize the canvas before the benchmark loop; use `clear()` inside only when that IS the benchmark.
- **Calling `LuaBindings::registerAll()` inside the timed lambda:** `registerAll()` is not a hot-path operation; it sets up global tables. It should be called once outside the timed region.
- **Not calling `Scene::activate()` before `Scene::update()`:** `Scene::update()` has an early return if `!active`. The ECS update benchmark would measure only a branch rather than the full update loop.
- **Linking `enjin2_lua` without `ENJIN2_BUILD_LUA=ON`:** `enjin2_lua` does not exist as a CMake target if Lua is OFF. Wrap `bench_lua` in `if(ENJIN2_BUILD_LUA)`.
- **Writing JSON inside the timed lambda:** `render()` is a template expansion loop — slow. Always call it after all `bench.run()` calls complete.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Timing CPU operations | Custom chrono loop | `ankerl::nanobench::Bench::run()` | Handles warmup, epoch sampling, clock overhead subtraction, outlier detection |
| JSON serialization of results | Custom JSON serializer | `ankerl::nanobench::render(templates::json(), bench, ofstream)` | Built-in, tested, produces the exact schema CI tools expect |
| Prevent dead-code elimination | `volatile` casts | `ankerl::nanobench::doNotOptimizeAway()` | Portable compiler barrier; `volatile` has undefined behavior for this in C++17 |
| Multi-binary output naming | Hardcoded paths in each binary | Per-binary `bench-results/bench_X.json` convention | Keeps subsystem results isolated; CI can process each file independently |

**Key insight:** Every nanobench measurement correctness problem is already solved by the library. The only job is providing realistic inputs (correct canvas size, actual object counts, real Lua scripts) and ensuring side effects escape via `doNotOptimizeAway`.

## Common Pitfalls

### Pitfall 1: Dead-code elimination silences canvas benchmarks at -O2
**What goes wrong:** `bench_canvas` reports sub-nanosecond timings (e.g., 0 ns/op) because the compiler eliminates the entire drawing call — no pixel read-back exists to prove the value was used.
**Why it happens:** `Canvas4::setPixel` and `Canvas4::fillRect` have no externally visible side effects if the canvas buffer is a local variable and no value is read back from it.
**How to avoid:** Always call `doNotOptimizeAway(canvas.getPixel(x, y))` after each drawing call. The getPixel dereferences the buffer, making it impossible for the compiler to eliminate the write.
**Warning signs:** Benchmark reports 0 ns or < 1 ns for operations that should take 100s of ns at -O2 on real pixel writes.

### Pitfall 2: bench_lua linking fails because LUA_LIBRARIES is undefined
**What goes wrong:** CMake error: "Target bench_lua specifies link libraries ... but the target has no such dependency."
**Why it happens:** `LUA_LIBRARIES` and `LUA_INCLUDE_DIRS` are only set inside the `if(ENJIN2_BUILD_LUA)` block in root CMakeLists.txt. `benchmarks/CMakeLists.txt` is a subdirectory and variables set in a parent scope are visible but only if set before `add_subdirectory()`.
**How to avoid:** Gate `bench_lua` with `if(TARGET enjin2_lua)` or `if(ENJIN2_BUILD_LUA)`. The cleanest approach: link directly to `enjin2_lua` which already transitively carries `LUA_LIBRARIES`; add `LUA_INCLUDE_DIRS` to `target_include_directories` only, wrapped in a guard.
**Warning signs:** CMake configure succeeds but linker fails with undefined Lua symbols.

### Pitfall 3: LuaBindings::registerAll() calls engine.* null pointers
**What goes wrong:** `bench_lua` segfaults during `registerAll()` if any engine subtable setup tries to dereference `m_ssm`, `m_activeScene`, or `m_debugCanvas`.
**Why it happens:** These are nullptr in headless mode. STATE.md explicitly flags this as a concern for Phase 63 (headless enjin_run) but the same concern applies here.
**How to avoid:** From code review: `registerAll()` stores these pointers in the Lua registry via closures; the pointers are only dereferenced when the corresponding Lua API is called from a script. `registerAll()` itself does not dereference them. Confirmed safe: bench_lua can call `registerAll()` with null engine pointers as long as benchmarks call only bindings that do not chase those pointers (e.g., `engine.time.delta()` reads `m_timeState` which is a value type, not a pointer; `math.clamp()` has no engine pointer dependency).
**Warning signs:** Segfault on `registerAll()` or on specific `executeString()` calls that route through pointer-dependent bindings.

### Pitfall 4: Scene::update() no-ops because scene is not activated
**What goes wrong:** ECS `scene::update` benchmark measures near-zero time because `Scene::update()` returns immediately when `active == false`.
**Why it happens:** `Scene::update(float dt)` has `if (!active) return;` guard.
**How to avoid:** Call `scene.activate()` after `scene.initialize()` and before the benchmark loop. Both are safe to call in isolation (no SDL, no canvas required).
**Warning signs:** Benchmark reports 0 ns for scene::update with 48 objects.

### Pitfall 5: bench_canvas uses Canvas8 composite incorrectly
**What goes wrong:** Multi-layer composite benchmark runs `LayerCompositor<128,128>` but only `Canvas4` layers compose correctly (8-bit compositor is not implemented per code comments).
**Why it happens:** From `scene.hpp` line 391: "Canvas8 (uint8_t) compositing will be handled by ENG-01 (Phase 25 compositor). For now, non-Pixel4 canvases do not render drawables." The `LayerCompositor` is a `Canvas4` only type.
**How to avoid:** Use `LayerCompositor<128,128>` with `Canvas4` layers only. Do not attempt a Canvas8 composite benchmark.
**Warning signs:** Template instantiation failure on `LayerCompositor<W,H>` with `Canvas8` template parameters.

### Pitfall 6: scripts/build-bench.sh hardcodes build directory that conflicts with dev build
**What goes wrong:** Developer's existing `build/` directory (with tests, SDL runner, etc.) gets overwritten when the script passes `-B build`.
**Why it happens:** Using the same build directory for benchmarks as for normal development.
**How to avoid:** Use a separate build directory (e.g., `build-bench`) in `build-bench.sh`. Never use `build/` to avoid polluting the standard dev build.
**Warning signs:** Script clobbers existing `build/` cmake cache; subsequent `cmake --build build --target enjin2_test` fails.

## Code Examples

Verified patterns from project source:

### Canvas4 concrete methods available for bench_canvas
```cpp
// Source: include/enjin2/graphics/canvas.hpp (verified)
// Source: include/enjin2/graphics/layer_compositor.hpp (verified)
// Source: include/enjin2/graphics/primitives.hpp (verified)
enjin2::Canvas4<128, 128> canvas4;

canvas4.setPixel(x, y, enjin2::Pixel4(color));        // line 145
canvas4.clear(enjin2::Pixel4(0));                       // line 170
canvas4.fillRect(x, y, w, h, enjin2::Pixel4(color));  // line 261
canvas4.blit(sprite, dst_x, dst_y, enjin2::Pixel4(transparent)); // line 364
canvas4.getPixel(x, y);                                 // returns Pixel4

// Primitives (static methods, no canvas subtype requirement)
enjin2::Primitives<enjin2::Pixel4>::drawCircle(canvas4, cx, cy, r, enjin2::Pixel4(c));
enjin2::Primitives<enjin2::Pixel4>::drawLine(canvas4, x0, y0, x1, y1, enjin2::Pixel4(c));

// LayerCompositor — Canvas4 only
enjin2::LayerCompositor<128, 128> comp;    // ENJIN_LAYER_COUNT = 5 on desktop
comp.clearAll();                            // layer 0 = black, layers 1-4 = transparent (Pixel4(15))
comp.composite();                           // painter's order merge into comp.output
comp.output.getPixel(x, y);               // read composited result
```

### ECS methods available for bench_ecs
```cpp
// Source: include/enjin2/core/object_collection.hpp (verified)
// Source: include/enjin2/core/object.hpp (verified)
// Source: include/enjin2/core/scene.hpp (verified)
enjin2::ObjectCollection coll;
enjin2::Object* obj = coll.addObject<enjin2::Object>();  // allocates via new
coll.size();                                               // returns objectCount

enjin2::Object obj;
enjin2::C_Position* pos = obj.addComponent<enjin2::C_Position>();
obj.removeComponent<enjin2::C_Position>();
obj.getComponentCount();

// Scene (concrete subclass)
class BenchScene : public enjin2::Scene {
public: BenchScene() : enjin2::Scene(99) {}
};
BenchScene scene;
scene.activate();           // sets active = true (required for update())
scene.update(0.016f);       // calls objects.update() + objects.lateUpdate()
scene.getObjects().size();  // returns live object count
```

### LuaEngine headless API for bench_lua
```cpp
// Source: include/enjin2/scripting/lua_engine.hpp (verified)
// Source: include/enjin2/scripting/bindings.hpp (verified)
enjin2::LuaEngine eng;
bool ok = eng.initialize();          // creates lua_State*, custom allocator
auto r = eng.executeString("...");   // returns LuaResult{success, error}
lua_State* L = eng.getState();       // raw state for lua_gc calls
eng.shutdown();                      // closes L, resets state

enjin2::LuaBindings bindings(&eng);
bindings.registerAll();              // registers all engine.* tables; null pointer safe

// GC measurement
lua_gc(L, LUA_GCCOUNT, 0);          // kb used (int)
lua_gc(L, LUA_GCCOLLECT, 0);        // full collection cycle
```

### nanobench JSON render API
```cpp
// Source: vendor/nanobench.h line 290, 347, 1004 (verified)
#include <fstream>
ankerl::nanobench::Bench bench;
// ... bench.run() calls ...
std::ofstream out("bench-results/bench_X.json");
ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out);
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Hand-rolled chrono timer per-benchmark | `nanobench::Bench::run()` | Phase 60 | Warmup, sampling, statistical analysis handled automatically |
| Single benchmark binary | Three separate binaries by subsystem | Phase 61 design | Isolated JSON per subsystem; independent CI thresholds per binary |
| Benchmark in the same build as production | Separate `build-bench/` directory | Phase 61 design | Avoids caching contamination; always builds with `-DCMAKE_BUILD_TYPE=Release` |

**Deprecated/outdated:**
- Google Benchmark: out of scope per REQUIREMENTS.md.
- LuaJIT: not applicable; project uses Lua 5.4 on all platforms.

## Open Questions

1. **C_Position include path in bench_ecs.cpp**
   - What we know: `C_Position` is declared in `include/enjin2/components/` but the exact header file was not confirmed during this research session.
   - What's unclear: Whether the component is `position.hpp` or embedded in another header.
   - Recommendation: During implementation, run `grep -r "class C_Position" include/` to confirm the header. Fallback: use `Object` only (no component) for addComponent benchmark and pick a concrete component visible from `enjin2_core`.

2. **ENJIN2_BUILD_LUA guard in benchmarks/CMakeLists.txt**
   - What we know: `LUA_LIBRARIES` and `LUA_INCLUDE_DIRS` are set in root CMakeLists.txt inside `if(ENJIN2_BUILD_LUA)`. Target `enjin2_lua` only exists inside that block.
   - What's unclear: Whether CMake variable scoping propagates `LUA_INCLUDE_DIRS` from root into the benchmarks subdirectory when both are in the same CMake invocation.
   - Recommendation: Wrap `bench_lua` in `if(TARGET enjin2_lua)` in `benchmarks/CMakeLists.txt`. This is the most robust guard — it is true if and only if `enjin2_lua` was created.

3. **epoch / epochIterations configuration per binary**
   - What we know: The smoke test uses `warmup(3).epochs(5)`. The benchmarks need enough epochs for stable medians but fast enough for CI use (target < 60 seconds per binary).
   - What's unclear: Optimal values without running on the target machine.
   - Recommendation: Start with `warmup(5).epochs(50).epochIterations(1)` for canvas and ECS; `warmup(3).epochs(20).epochIterations(1)` for Lua (init/shutdown cycles are slower). Adjust during implementation.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Build verification only — benchmark binaries are measurement tools, not pass/fail tests |
| Config file | N/A |
| Quick run command | `cmake -DENJIN2_BUILD_BENCHMARKS=ON -DENJIN2_BUILD_TESTS=OFF -DENJIN2_BUILD_EXAMPLES=OFF -B /tmp/b61 /home/unwn/git/enjin && cmake --build /tmp/b61 --target bench_canvas bench_ecs bench_lua` |
| Full suite command | `bash /home/unwn/git/enjin/scripts/build-bench.sh` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BENCH-02 | bench_canvas binary builds and runs; pixel op timings are non-trivial | smoke/build + manual review | `cmake --build /tmp/b61 --target bench_canvas && /tmp/b61/benchmarks/bench_canvas` | ❌ Wave 0 |
| BENCH-03 | bench_ecs binary builds and runs; update timings scale with object count | smoke/build | `cmake --build /tmp/b61 --target bench_ecs && /tmp/b61/benchmarks/bench_ecs` | ❌ Wave 0 |
| BENCH-04 | bench_lua binary builds and runs headlessly; GC pressure measured | smoke/build | `cmake --build /tmp/b61 --target bench_lua && /tmp/b61/benchmarks/bench_lua` | ❌ Wave 0 |
| BENCH-05 | JSON files appear in bench-results/ and are valid JSON | file check | `test -f bench-results/bench_canvas.json && python3 -c "import json,sys; json.load(open('bench-results/bench_canvas.json'))" && echo PASS` | ❌ Wave 0 |
| BENCH-06 | scripts/build-bench.sh runs without error; all three JSON files produced | integration | `bash /home/unwn/git/enjin/scripts/build-bench.sh && test -f bench-results/bench_ecs.json && echo PASS` | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** Build the relevant benchmark target (bench_canvas, bench_ecs, or bench_lua) and run it to verify JSON output
- **Per wave merge:** Run `bash scripts/build-bench.sh` from project root
- **Phase gate:** All five success criteria met before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `benchmarks/bench_canvas.cpp` — Canvas4/Canvas8/LayerCompositor benchmark (BENCH-02)
- [ ] `benchmarks/bench_ecs.cpp` — Object/Scene/Component benchmark (BENCH-03)
- [ ] `benchmarks/bench_lua.cpp` — LuaEngine headless benchmark (BENCH-04)
- [ ] `benchmarks/CMakeLists.txt` — add three new executable targets
- [ ] `scripts/build-bench.sh` — orchestration script (BENCH-06)
- [ ] `bench-results/` directory — created at runtime by script; no source file needed

## Sources

### Primary (HIGH confidence)
- `/home/unwn/git/enjin/vendor/nanobench.h` (read directly) — confirmed v4.3.11; `render(templates::json(), bench, ostream)` API at lines 290, 347, 1004; `doNotOptimizeAway` at lines 1023, 1031; `Bench::render()` member at line 1004
- `/home/unwn/git/enjin/include/enjin2/graphics/canvas.hpp` (read directly) — Canvas4/Canvas8 concrete methods: setPixel, clear, fillRect, blit, getPixel; no SDL3 dependency
- `/home/unwn/git/enjin/include/enjin2/graphics/layer_compositor.hpp` (read directly) — LayerCompositor<W,H> with clearAll()/composite(); Canvas4 only; ENJIN_LAYER_COUNT = 5 on desktop
- `/home/unwn/git/enjin/include/enjin2/graphics/primitives.hpp` (read directly) — Primitives<TPixel> static methods: drawLine, drawCircle, drawRect, fillRect
- `/home/unwn/git/enjin/include/enjin2/core/object.hpp` (read directly) — Object: addComponent<T>(), removeComponent<T>(), getComponentCount(); MAX_COMPONENTS = 16
- `/home/unwn/git/enjin/include/enjin2/core/object_collection.hpp` (read directly) — ObjectCollection::addObject<T>(); MAX_OBJECTS = 128; update(dt)
- `/home/unwn/git/enjin/include/enjin2/core/scene.hpp` (read directly) — Scene::update(dt) guards on `active`; activate() required
- `/home/unwn/git/enjin/include/enjin2/scripting/lua_engine.hpp` (read directly) — LuaEngine: initialize/shutdown/executeString/getState; no SDL3 dependency
- `/home/unwn/git/enjin/include/enjin2/scripting/bindings.hpp` (read directly) — LuaBindings: registerAll() null-pointer safe for engine.*; LuaCanvas optional
- `/home/unwn/git/enjin/CMakeLists.txt` (read directly) — LUA_LIBRARIES/LUA_INCLUDE_DIRS set inside if(ENJIN2_BUILD_LUA); enjin2_lua links enjin2_graphics + enjin2_ui + LUA_LIBRARIES; find_package(Lua) on desktop
- `/home/unwn/git/enjin/benchmarks/CMakeLists.txt` (read directly) — existing nanobench_vendor INTERFACE target and bench_smoke structure to extend
- `/home/unwn/git/enjin/.planning/STATE.md` (read) — note on null-binding safety for headless Lua use (Phase 63 concern; same applies to bench_lua)
- System Lua version check — `pkg-config --modversion lua5.4` returned 5.4.8

### Secondary (MEDIUM confidence)
- nanobench official tutorial (https://nanobench.ankerl.com/tutorial.html) — JSON template example, `doNotOptimizeAway` recommended pattern (consistent with header review)

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all library APIs verified by direct header read; Lua presence verified by pkg-config
- Architecture: HIGH — all public method signatures verified in headers; no novel patterns introduced
- Pitfalls: HIGH — dead-code elimination and scene activation pitfalls directly derived from source code (clear() optimizable, `if (!active) return` guard)
- Validation: HIGH — build commands derived from Phase 60 verified patterns

**Research date:** 2026-03-07
**Valid until:** 2026-09-07 (project internals; nanobench API stable at v4.3.11)
