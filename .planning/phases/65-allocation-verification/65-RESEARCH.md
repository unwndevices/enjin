# Phase 65: Allocation Verification - Research

**Researched:** 2026-03-08
**Domain:** C++ operator new interception, RAII allocation guards, hot-path zero-alloc verification, GitHub Actions CI integration
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| ALLOC-01 | Custom allocator wrapper counts malloc/free calls during benchmarked hot-path sections | `operator new` global override with thread_local counter; RAII `AllocGuard` enter/exit scope; binary exits non-zero if counter > 0 on scope exit |
| ALLOC-02 | CI check runs benchmarks under allocation counter and fails if any hot-path allocation detected | New `bench_alloc` binary added to `benchmarks/CMakeLists.txt`; `scripts/build-bench.sh` extended to build and run it; non-zero exit propagates through CI `bash scripts/build-bench.sh` step, failing the job |
| ALLOC-03 | Canvas operations, Component updates, and Lua binding calls to `engine.*` all pass the zero-alloc check in CI | Measured hot paths: `Canvas4::clear()`, `Canvas4::setPixel()`, `Canvas4::fillRect()`, `scene.update()` (N objects, no `addObject` inside guard), `engine.time.delta` via direct C call (not `executeString`) |
</phase_requirements>

---

## Summary

Phase 65 adds a zero-allocation verification layer on top of the existing nanobench benchmark suite. The mechanism is a custom global `operator new` override that increments a counter inside a defined scope; an RAII `AllocGuard` arms and disarms the counter. A new binary (`bench_alloc`) wraps each hot-path operation in a guard and exits with a non-zero status code if any heap allocation fires inside the guarded region. The CI workflow already runs `bash scripts/build-bench.sh`; extending that script to also build and run `bench_alloc` propagates the non-zero exit naturally and fails the GitHub Actions job.

The project's core value states "zero dynamic allocation" — Phase 65 makes this machine-verifiable rather than aspirational. The key architectural challenge is distinguishing *hot-path* allocations (per-frame operations that should never allocate) from *setup-path* allocations (`addComponent`, `new Object`, Lua state init) which are intentionally heap-allocated. The `AllocGuard` scope must be placed *after* setup completes, wrapping only the operations listed in ALLOC-03.

A second challenge is that the existing bench benchmarks invoke `eng.executeString("local t = engine.time.delta()")` — this path allocates inside Lua's string interning system. The allocation-free claim is that the *C binding function itself* (`lua_engine_time_delta`) does not allocate; the overhead of `executeString` parsing is not the hot path. The `bench_alloc` binary must call the C binding directly (by pushing a Lua function call through the already-loaded chunk) rather than via `executeString`, or alternatively benchmark a pre-compiled Lua closure to isolate the binding call.

**Primary recommendation:** Implement `AllocGuard` as a header-only RAII class in `include/enjin2/instrumentation/alloc_guard.hpp`. Create `benchmarks/bench_alloc.cpp` as the verification binary. Extend `scripts/build-bench.sh` to build and run it. Add an explicit `Allocation Verification` step in `benchmarks.yml` that runs only `bench_alloc`.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++ global `operator new` override | C++11 standard | Intercept all heap allocations without external tools | Defined in the C++ standard (§18.6.1); link-time override replaces libc++ `new` globally; no external dependency |
| C++ `thread_local` or `atomic` counter | C++11 | Count allocations inside guard scope | `thread_local` preferred: zero cross-thread overhead; benchmarks run single-threaded so no synchronization needed |
| RAII scope guard (header-only) | — | Arm/disarm counter; check on exit | Same RAII pattern used by `FrameTimingInstrumentation`; matches project style |
| nanobench 4.3.11 (vendored) | — | Structure the bench_alloc binary consistently with existing benchmarks | Already vendored in `vendor/nanobench.h`; provides `doNotOptimizeAway` needed to prevent dead-code elimination of hot paths |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `<stdexcept>` / `fprintf` + `exit(1)` | stdlib | Report alloc failure and non-zero exit | Use `fprintf` + `exit(1)` not `throw` — some build configs disable exceptions |
| `ENJIN2_ALLOC_VERIFICATION` CMake option | — | Guard the `operator new` override behind a compile flag | Prevents the global override from leaking into non-bench builds |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Global `operator new` override | Valgrind/massif | Valgrind cannot distinguish hot vs cold paths; project REQUIREMENTS.md Out-of-Scope explicitly excludes "Valgrind/ASan for alloc verification" |
| Global `operator new` override | `malloc` hook via `__malloc_hook` (glibc) | `__malloc_hook` is deprecated since glibc 2.34; not portable to macOS or ESP32 |
| Global `operator new` override | LD_PRELOAD wrapper | More complex CI setup; `operator new` override is simpler and portable |
| `thread_local` counter | `std::atomic<int>` | `atomic` adds unnecessary cross-thread cost; benchmarks are single-threaded |
| Direct C binding call | `eng.executeString("engine.time.delta()")` | `executeString` parses and compiles Lua source — allocates in Lua string interning. The hot path is the C function. Use `lua_getglobal`/`lua_pcall` on a pre-registered function instead. |

**Installation:**
```bash
# No new dependencies — operator new override is pure C++11 standard library
# bench_alloc is built with the same ENJIN2_BUILD_BENCHMARKS=ON flag
```

---

## Architecture Patterns

### Recommended File Structure
```
include/enjin2/instrumentation/
├── frame_timing.hpp        # EXISTING
└── alloc_guard.hpp         # NEW — AllocGuard RAII + operator new override

benchmarks/
├── CMakeLists.txt          # EXTENDED — add bench_alloc target
├── bench_canvas.cpp        # EXISTING
├── bench_ecs.cpp           # EXISTING
├── bench_lua.cpp           # EXISTING
├── bench_smoke.cpp         # EXISTING
└── bench_alloc.cpp         # NEW — allocation verification binary

scripts/
└── build-bench.sh          # EXTENDED — add bench_alloc build + run

.github/workflows/
└── benchmarks.yml          # EXTENDED — add explicit alloc-verify step
```

### Pattern 1: Global `operator new` Override with Thread-Local Counter

**What:** Override `::operator new` and `::operator delete` globally in the compilation unit where the override is needed. Use a thread_local int flag to enable/disable counting inside a specific scope.

**When to use:** When you need to detect *any* heap allocation (malloc, new, std::make_shared, std::string copy, std::vector resize) during a specific code region.

**Critical detail:** The override must be defined in a `.cpp` file that is *linked into the final binary*. It must NOT be placed in a shared library that the benchmark binary links against — it must be in the `bench_alloc.cpp` TU itself or a dedicated override TU linked directly.

```cpp
// Source: C++ standard §18.6.1 — operator new replacement
// Place in benchmarks/bench_alloc.cpp (before any includes that could call new)

#include <cstdlib>
#include <cstdio>

// Thread-local enable flag: 0 = not counting, >0 = counting depth
// Depth allows nested guards (each guard increments/decrements)
static thread_local int g_alloc_guard_depth = 0;
static thread_local long g_alloc_count = 0;

void* operator new(std::size_t size) {
    if (g_alloc_guard_depth > 0) {
        g_alloc_count++;
    }
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void* operator new[](std::size_t size) {
    return operator new(size);
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
}

// Sized delete (C++14) — required to avoid linker ambiguity on GCC/Clang
void operator delete(void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    std::free(ptr);
}
```

**Important:** This override captures ALL `operator new` calls including those inside standard library containers (std::string internal SSO overflow, std::vector growth, etc.). This is the intended behavior — we want to detect any hidden allocation.

### Pattern 2: AllocGuard RAII Header

**What:** Header-only RAII class that arms the counter on construction and checks+disarms on destruction. Binary exits non-zero if any allocation fired inside the scope.

```cpp
// Source: project pattern matching FrameTimingInstrumentation; C++ standard RAII
// File: include/enjin2/instrumentation/alloc_guard.hpp

#pragma once
#include <cstdio>
#include <cstdlib>

// Access the thread_local globals defined in bench_alloc.cpp
// These symbols are ONLY defined when ENJIN2_ALLOC_VERIFICATION is defined.
// The header declares them as extern; the definition is in the bench binary.
#ifdef ENJIN2_ALLOC_VERIFICATION

extern thread_local int g_alloc_guard_depth;
extern thread_local long g_alloc_count;

namespace enjin2 {

/**
 * @brief RAII allocation guard for zero-alloc hot-path verification.
 *
 * Arms the global operator new counter on construction.
 * On destruction, checks whether any allocation fired — if so,
 * prints a diagnostic and calls exit(1) so the binary exits non-zero.
 *
 * Usage:
 *   {
 *     AllocGuard guard("canvas4: clear");
 *     canvas.clear(Pixel4(0));
 *   }  // exits non-zero if clear() allocated
 */
class AllocGuard {
public:
    explicit AllocGuard(const char* label)
        : m_label(label), m_count_before(g_alloc_count)
    {
        g_alloc_guard_depth++;
    }

    ~AllocGuard() {
        g_alloc_guard_depth--;
        long delta = g_alloc_count - m_count_before;
        if (delta > 0) {
            fprintf(stderr,
                "[ALLOC-FAIL] %s: %ld allocation(s) detected in hot path\n",
                m_label, delta);
            exit(1);   // non-zero exit — CI step fails
        }
    }

    // Non-copyable, non-movable
    AllocGuard(const AllocGuard&)            = delete;
    AllocGuard& operator=(const AllocGuard&) = delete;
    AllocGuard(AllocGuard&&)                 = delete;
    AllocGuard& operator=(AllocGuard&&)      = delete;

private:
    const char* m_label;
    long        m_count_before;
};

} // namespace enjin2

#else // ENJIN2_ALLOC_VERIFICATION not defined — no-op stub

namespace enjin2 {
class AllocGuard {
public:
    explicit AllocGuard(const char*) {}
    ~AllocGuard() {}
    AllocGuard(const AllocGuard&)            = delete;
    AllocGuard& operator=(const AllocGuard&) = delete;
};
} // namespace enjin2

#endif // ENJIN2_ALLOC_VERIFICATION
```

### Pattern 3: bench_alloc.cpp Structure

**What:** The verification binary mirrors the pattern of existing bench binaries but wraps each hot-path operation in an `AllocGuard` instead of a nanobench `bench.run()` lambda. The binary exits 0 on success, non-zero on first alloc detection.

```cpp
// benchmarks/bench_alloc.cpp
// Source: project patterns from bench_canvas.cpp, bench_ecs.cpp, bench_lua.cpp

#define ENJIN2_ALLOC_VERIFICATION
#include <enjin2/instrumentation/alloc_guard.hpp>

// operator new override MUST come before any allocating includes
// (string, vector, etc.) to ensure the override is registered first.
// Define the thread_local storage here (linked into this TU).
thread_local int g_alloc_guard_depth = 0;
thread_local long g_alloc_count = 0;

// ... operator new/delete overrides (as shown in Pattern 1) ...

// Now safe to include engine headers (they don't call new at include-time)
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/primitives.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <cstdio>

int main() {
    // === SETUP SECTION (allowed to allocate) ===
    // All setup runs BEFORE any AllocGuard is constructed.

    enjin2::Canvas4<128, 128> canvas;     // stack-allocated — no heap
    enjin2::Canvas4<128, 128> sprite;     // stack-allocated — no heap
    sprite.clear(enjin2::Pixel4(5));

    // Scene + objects for update dispatch benchmark
    // addObject() calls operator new — must be outside guard
    // ... scene setup ...

    // Lua engine for binding call benchmark
    // LuaEngine::initialize() allocates Lua state — outside guard
    // ... lua setup ...

    // Reset alloc counter to zero after setup completes
    // (so setup allocations don't bleed into guarded sections)
    g_alloc_count = 0;

    // === HOT PATH SECTION (must be zero-alloc) ===

    // --- Canvas pixel ops (ALLOC-03 requirement 1) ---
    {
        enjin2::AllocGuard g("canvas4: setPixel");
        canvas.setPixel(64, 64, enjin2::Pixel4(7));
        ankerl::nanobench::doNotOptimizeAway(canvas.getPixel(64, 64));
    }
    {
        enjin2::AllocGuard g("canvas4: clear");
        canvas.clear(enjin2::Pixel4(0));
        ankerl::nanobench::doNotOptimizeAway(canvas.getPixel(0, 0));
    }
    {
        enjin2::AllocGuard g("canvas4: fillRect 32x32");
        canvas.fillRect(0, 0, 32, 32, enjin2::Pixel4(3));
        ankerl::nanobench::doNotOptimizeAway(canvas.getPixel(16, 16));
    }
    {
        enjin2::AllocGuard g("canvas4: blit 128x128 sprite");
        canvas.blit(sprite, 0, 0, enjin2::Pixel4(0));
        ankerl::nanobench::doNotOptimizeAway(canvas.getPixel(64, 64));
    }

    // --- Component update dispatch (ALLOC-03 requirement 2) ---
    // scene.update() iterates std::array<unique_ptr<Object>> — no allocs in the loop itself
    {
        enjin2::AllocGuard g("scene::update x8 objects");
        scene.update(0.016f);
        ankerl::nanobench::doNotOptimizeAway(scene.getObjects().size());
    }

    // --- Lua binding calls (ALLOC-03 requirement 3) ---
    // Call the C binding directly via pre-registered Lua function (not via executeString)
    {
        enjin2::AllocGuard g("lua binding: engine.time.delta call");
        // lua_getglobal + lua_pcall on the engine table function
        // This does NOT go through executeString (which allocates for chunk parsing)
        lua_State* L = eng.getState();
        lua_getglobal(L, "engine");
        lua_getfield(L, -1, "time");
        lua_getfield(L, -1, "delta");
        lua_pcall(L, 0, 1, 0);
        lua_pop(L, 3);
    }

    fprintf(stdout, "[ALLOC-PASS] All hot-path allocation checks passed\n");
    return 0;
}
```

**Key constraint:** `scene.update()` calls `objects.update(dt)` which iterates `std::array<std::unique_ptr<Object>>`. The iteration itself does not allocate. HOWEVER: `ObjectCollection::forEach()` takes `std::function<void(Object*)>` — this can allocate when the lambda is larger than the small buffer optimization (SBO) size (typically 16-32 bytes). The `update()` path does NOT call `forEach()` — it uses the raw index loop — so it is allocation-free. Verify this is still true before claiming it passes.

### Pattern 4: CMake Integration

```cmake
# benchmarks/CMakeLists.txt — extend with bench_alloc target
# Compile with ENJIN2_ALLOC_VERIFICATION to enable the operator new override

add_executable(bench_alloc bench_alloc.cpp)
target_include_directories(bench_alloc PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${LUA_INCLUDE_DIRS}
)
target_link_libraries(bench_alloc PRIVATE
    nanobench_vendor
    enjin2_core
    enjin2_graphics
    enjin2_ui
    $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>
)
target_compile_definitions(bench_alloc PRIVATE
    ENJIN2_ALLOC_VERIFICATION=1
)
```

**Note:** `bench_alloc` links against all engine libs. The `operator new` override defined in `bench_alloc.cpp` replaces `new` globally for the entire binary — including engine lib code. This is the intended behavior.

### Pattern 5: Shell Script Extension

```bash
# scripts/build-bench.sh — extend to include bench_alloc

# Build target (after existing targets):
cmake --build "${BUILD_DIR}" --target bench_canvas bench_ecs bench_lua bench_alloc -- -j"$(nproc)"

# Run (after existing bench runs):
echo ""
echo "--- running bench_alloc (allocation verification) ---"
"${BUILD_DIR}/benchmarks/bench_alloc"   # exits non-zero on alloc → script exits (set -euo pipefail)
```

The `set -euo pipefail` in `build-bench.sh` ensures the script exits immediately if `bench_alloc` returns non-zero, which propagates through the CI step.

### Pattern 6: CI Workflow Step

```yaml
# .github/workflows/benchmarks.yml — add explicit step for visibility
# (The bench_alloc failure would also propagate through "Build and run benchmarks"
#  but an explicit step makes the failure reason visible in the CI log)

- name: Allocation verification
  run: "${BUILD_DIR}/benchmarks/bench_alloc"
  # This step naturally fails if bench_alloc exits non-zero
```

Alternatively, integrate it into `build-bench.sh` (simpler — no separate workflow step needed).

### Anti-Patterns to Avoid

- **Placing `operator new` override in a header included by engine libraries:** The override must live in the `bench_alloc.cpp` TU, not in `alloc_guard.hpp`. If placed in a header that multiple TUs include, multiple definitions will cause linker errors (ODR violation).
- **Calling `executeString("engine.time.delta()")` inside an AllocGuard:** `luaL_loadstring` internally allocates for the Lua chunk and string interning. The *binding function* is allocation-free; the executeString wrapper is not. Call the binding via `lua_getglobal` + `lua_pcall` instead.
- **Running `addObject` or `addComponent` inside an AllocGuard:** Both call `new T(...)` intentionally. These are setup operations. Always run them before the guard scope.
- **Forgetting `doNotOptimizeAway` on the guarded operations:** Without it, the compiler may eliminate the call entirely (dead code), making the test trivially pass while proving nothing.
- **Using `g_alloc_count` without resetting after setup:** Setup allocations (Lua state init, std::string in `assetPath_`, etc.) accumulate before the guard. Reset `g_alloc_count = 0` explicitly after all setup completes.
- **Assuming `std::function` in `forEach` is SBO-safe:** `forEach(std::function<void(Object*)>)` may allocate if the closure captures more than ~16-32 bytes. The `update()` and `lateUpdate()` paths DO NOT call `forEach` — they use raw index loops. Do not use `forEach` inside any AllocGuard.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Detecting heap allocations | Custom malloc wrapper via LD_PRELOAD | Global `operator new` override | `operator new` is the standard C++ mechanism; LD_PRELOAD requires separate .so, complicates cmake, doesn't work on all platforms |
| Counting allocations | Separate per-object allocation tracking | Single `thread_local long g_alloc_count` | Per-object tracking is heavyweight; the guard only needs a count delta |
| CI failure propagation | Custom failure-reporting script | `set -euo pipefail` + non-zero exit | Shell already handles this; no extra code needed |
| "Allocation-free" claim documentation | Separate doc | ALLOC_GUARD fprintf output on success | The binary's stdout/stderr IS the documentation; CI log shows which paths passed |

**Key insight:** The `operator new` override technique is the only approach that catches ALL heap allocations (including those inside STL containers, Lua VM, and compiler-generated code) without external tools.

---

## Common Pitfalls

### Pitfall 1: Lua `executeString` Allocates — Even for Simple Calls

**What goes wrong:** Wrapping `eng.executeString("engine.time.delta()")` inside an `AllocGuard` will always fail, even though the C binding function itself is allocation-free.

**Why it happens:** `luaL_loadstring` (called inside `executeString`) compiles the Lua source into a closure object. This involves at minimum: a `Proto*` allocation, string interning of the chunk name, and a `LClosure*` allocation. Lua 5.4 uses `l_alloc` (backed by `realloc`) for all its heap operations — and `realloc` calls translate to `operator new` replacements indirectly through `malloc`.

**How to avoid:** In `bench_alloc.cpp`, pre-register a Lua wrapper function and call it via `lua_getglobal` + `lua_pcall`. Or directly call the C binding: `LuaBindings::lua_engine_time_delta` is a `static int` function — it can be called directly after pushing appropriate stack values.

**Alternative approach for ALLOC-03:** Pre-load a Lua chunk *before* the guard that captures the binding into a local, then call the pre-compiled closure inside the guard. This is the closest analog to "calling engine.* from a running Lua script."

**Warning signs:** `[ALLOC-FAIL] lua binding: engine.time.delta call: N allocation(s)` where N is typically 2-5 (LClosure + Proto + string internment).

### Pitfall 2: Sized Delete ODR Conflict with STL

**What goes wrong:** On GCC/Clang with C++14+, both `operator delete(void*)` and `operator delete(void*, std::size_t)` must be overridden. If only the unsized form is overridden, the compiler may emit calls to the STL's sized delete, bypassing the override.

**Why it happens:** C++14 added sized deallocation (`-fsized-deallocation`). GCC enables it by default. The STL's sized delete will be called instead of the unsized form if only the unsized form is overridden.

**How to avoid:** Override all six forms: `operator new`, `operator new[]`, `operator delete`, `operator delete[]`, `operator delete(void*, std::size_t)`, `operator delete[](void*, std::size_t)`.

**Warning signs:** Allocations are detected via `operator new` but not released through `operator delete` (leak-like behavior in the count), OR some allocations bypass the counter entirely.

### Pitfall 3: `scene::update` May Allocate via `std::function` Indirectly

**What goes wrong:** If `scene.update()` is extended to call `forEach(lambda)` somewhere in the call chain (e.g., in `onUpdate()`), the `std::function` capture may allocate.

**Why it happens:** `std::function` uses small buffer optimization (SBO) — typically 16-32 bytes. Lambdas that capture more than the SBO threshold allocate. The current `update()` → `objects.update(dt)` path uses raw index loops (safe), but `renderObjects()` calls `objects.forEach(lambda)` — that lambda captures only `[&]` with local arrays on the stack, which should fit in SBO. However this is implementation-defined.

**How to avoid:** The `bench_alloc` test should verify `scene.update()` specifically (not `scene.render()`). Verify the `objects.update(dt)` path does NOT call `forEach` by reading the source (confirmed: it uses raw index loops).

**Warning signs:** `[ALLOC-FAIL] scene::update x8 objects: 1 allocation(s)` — investigate which lambda triggered the `std::function` heap allocation.

### Pitfall 4: Lua State Init Allocates Before Guard

**What goes wrong:** `LuaEngine::initialize()` calls `luaL_newstate()` which internally calls `lua_newstate()` + a large number of internal Lua allocations. If `g_alloc_count` is not reset after setup, the guard will see a non-zero baseline.

**Why it happens:** The `operator new` override fires for ALL allocations from binary start. `luaL_newstate` allocates many Lua-internal structures.

**How to avoid:** After all setup completes (LuaEngine init, registerAll, scene setup), explicitly set `g_alloc_count = 0` before entering any guarded section.

**Warning signs:** Every guard fails immediately with a large allocation count (hundreds) — indicates the reset was not applied.

### Pitfall 5: `assetPath_` std::string in LuaBindings Allocates at `setAssetPath()`

**What goes wrong:** `LuaBindings::assetPath_` is a `std::string`. Calling `bindings.setAssetPath(...)` in setup allocates. This is fine — but if `assetPath_` is set AFTER `g_alloc_count = 0`, it will pollute the count.

**Why it happens:** `std::string` assignment exceeding SSO threshold calls `operator new`.

**How to avoid:** Call `setAssetPath` before resetting `g_alloc_count`. The hot-path binding calls (`engine.time.delta`, `math.clamp`) do NOT touch `assetPath_`.

**Warning signs:** `[ALLOC-FAIL]` on first Lua binding test even though the binding itself is allocation-free.

### Pitfall 6: Lua Proxy Round-Trip Allocates (Expected — Exclude from ALLOC-03)

**What goes wrong:** `engine.scene.find('bench_target')` calls `lua_newuserdata(L, sizeof(ObjectProxy))` — this is a Lua heap allocation that goes through the Lua allocator (which calls `malloc`/`realloc`, not C++ `operator new`). Lua's allocator may or may not route through `operator new` depending on build configuration.

**Why it happens:** Lua 5.4 uses `l_alloc` = `realloc`-based allocator by default. It does NOT call `::operator new`. So the proxy round-trip benchmark (`engine.scene.find + p.name`) will NOT be caught by the `operator new` override.

**How to avoid:** The ALLOC-03 requirement specifies "Lua binding calls to `engine.*`" — the success criteria specifically call out `engine.time.delta()` as the example. The `find + proxy` path is intentionally allocating (Lua userdata) and should NOT be in the zero-alloc guard.

**Warning signs:** Confusion about whether `lua_newuserdata` is caught by `operator new` override. It is NOT caught — Lua allocates through `l_alloc` (realloc), not C++ `::operator new`.

---

## Code Examples

Verified patterns from official sources and codebase analysis:

### Complete AllocGuard + operator new Override

```cpp
// benchmarks/bench_alloc.cpp — complete structure
// Source: C++ standard §18.6.1 (operator new replacement), project codebase patterns

#define ENJIN2_ALLOC_VERIFICATION 1

// Thread-local allocation counter — defined before any allocating includes
static thread_local int  g_alloc_guard_depth = 0;
static thread_local long g_alloc_count       = 0;

// Override all six forms to cover C++14 sized deallocation
void* operator new(std::size_t size) {
    if (g_alloc_guard_depth > 0) { g_alloc_count++; }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void  operator delete(void* p) noexcept { std::free(p); }
void  operator delete[](void* p) noexcept { std::free(p); }
void  operator delete(void* p, std::size_t) noexcept { std::free(p); }
void  operator delete[](void* p, std::size_t) noexcept { std::free(p); }

// Now include AllocGuard header (uses the thread_local variables above via extern)
#include <enjin2/instrumentation/alloc_guard.hpp>

// Now include engine headers
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>
#include <enjin2/graphics/canvas.hpp>
// ...
```

### Calling Lua Binding Without executeString

```cpp
// Source: Lua 5.4 reference manual — lua_getglobal, lua_getfield, lua_pcall
// This approach calls the C binding via the Lua stack without parsing any source.

lua_State* L = eng.getState();

// Setup (outside guard): push a Lua function that calls engine.time.delta
// and store it in the registry for repeated zero-alloc calls
lua_getglobal(L, "engine");      // push engine table
lua_getfield(L, -1, "time");     // push engine.time subtable
lua_getfield(L, -1, "delta");    // push engine.time.delta C function
// delta is a C function (lua_CFunction) — calling it does not allocate
// Store ref for repeated calls:
int deltaRef = luaL_ref(L, LUA_REGISTRYINDEX);  // pops delta
lua_pop(L, 2);  // pop time, engine

// Inside AllocGuard:
{
    enjin2::AllocGuard g("lua binding: engine.time.delta call");
    lua_rawgeti(L, LUA_REGISTRYINDEX, deltaRef);  // push C function
    lua_call(L, 0, 1);                            // call it
    ankerl::nanobench::doNotOptimizeAway(lua_tonumber(L, -1));
    lua_pop(L, 1);
}

// Cleanup:
luaL_unref(L, LUA_REGISTRYINDEX, deltaRef);
```

### Canvas Hot Path (Confirmed Allocation-Free)

```cpp
// Source: include/enjin2/graphics/canvas.hpp analysis
// Canvas4<W,H> stores pixels in a statically-sized uint8_t buffer member.
// clear(), setPixel(), fillRect() iterate over this buffer — no heap allocation.

enjin2::Canvas4<128, 128> canvas;  // stack-allocated — sizeof ~8192 bytes

{
    enjin2::AllocGuard g("canvas4: clear");
    canvas.clear(enjin2::Pixel4(0));
    ankerl::nanobench::doNotOptimizeAway(canvas.getPixel(0, 0));
}
// Expected: PASS — clear() is a memset-like loop over a fixed buffer
```

### Scene Update Hot Path (Confirmed Allocation-Free)

```cpp
// Source: include/enjin2/core/object_collection.hpp update() analysis
// update(dt) uses raw index loop over std::array<unique_ptr<Object>> —
// no forEach, no std::function, no heap allocation in the iteration itself.

// Setup (outside guard):
BenchScene scene;
scene.activate();
for (int i = 0; i < 8; ++i) {
    scene.addObject<enjin2::Object>();  // allocates — outside guard
}
g_alloc_count = 0;  // reset after setup

// Guarded hot path:
{
    enjin2::AllocGuard g("scene::update x8 objects");
    scene.update(0.016f);
    ankerl::nanobench::doNotOptimizeAway(scene.getObjects().size());
}
// Expected: PASS — update() calls objects[i]->update(dt) in a raw loop
// Component::update() on C_Position (default component) does nothing (virtual no-op)
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Valgrind massif for allocation profiling | Custom `operator new` override with scope guard | Established pattern (2010s) | No Valgrind overhead; catches allocations in real benchmark runs; platform-portable |
| `__malloc_hook` (glibc extension) | Global `operator new` override | Deprecated glibc 2.34 (2021) | `__malloc_hook` is non-standard and removed; `operator new` override is standard C++ |
| AddressSanitizer for alloc tracking | `operator new` override | — | ASan tracks memory safety, not allocation counts; different tool for different problem |
| Manual code review for allocations | Instrumented CI verification | Phase 65 | Machine-verifiable instead of aspirational |

**Deprecated/outdated:**
- `__malloc_hook`: Deprecated in glibc 2.34; do not use on modern Linux.
- Running under Valgrind in CI: Excluded by REQUIREMENTS.md Out-of-Scope section.

---

## Hot Path Allocation Status (Pre-Phase Analysis)

This section documents what the code analysis reveals about each required hot path BEFORE instrumentation is added. The planner should create tasks to verify these claims with actual `AllocGuard` runs.

| Hot Path | Expected | Reason | Risk |
|----------|----------|--------|------|
| `Canvas4::clear()` | PASS (zero alloc) | Iterates fixed `uint8_t[]` member; no heap | LOW |
| `Canvas4::setPixel()` | PASS (zero alloc) | Single array index write | LOW |
| `Canvas4::fillRect()` | PASS (zero alloc) | Row loop over fixed buffer | LOW |
| `Canvas4::blit()` | PASS (zero alloc) | Row-by-row copy over fixed buffers | LOW |
| `scene.update()` x8 objects | PASS (zero alloc) | Raw index loop; no std::function | MEDIUM |
| `engine.time.delta` C binding | PASS (zero alloc) | Reads `lua_touserdata` + `lua_pushnumber` | LOW |
| `math.clamp` C binding | PASS (zero alloc) | Reads 3 number args + push result | LOW |
| `scene::addObject` | FAIL (allocates) | Calls `new T(...)` — intentional | NOT TESTED (setup only) |
| `addComponent<C_Position>` | FAIL (allocates) | `std::unique_ptr<T> component(new T(...))` | NOT TESTED (setup only) |
| `engine.scene.find + proxy` | ALLOCATES | `lua_newuserdata` via Lua's l_alloc | NOT TESTED (operator new won't catch lua allocs) |

**Important nuance for `scene.update()`:** The `update()` path calls each `Object::update(dt)` → `Component::update(dt)` in a raw loop. The `C_Position` component's `update()` is a virtual call that does nothing (the base `Component::update` is a virtual no-op). No allocation. The `Object::update()` also calls `start()` if not started — `start()` calls `awake()` — both are virtual no-ops for plain `Object`. All objects in the bench scene will have already been started (scene is `activate()`'d), so the guard fires are safe.

---

## Open Questions

1. **Does `Object::update()` calling `start()` on first frame count as a hot-path allocation?**
   - What we know: `start()` calls `awake()` which is a virtual no-op for plain `Object`. No allocation expected.
   - What's unclear: If any future component override introduces an allocation in `awake()`/`start()`.
   - Recommendation: Ensure test bench objects have been through `activate()` before the guard fires — the scene must be activated before the first `update()` call. The existing bench pattern already does `scene.activate()` before the benchmark loop.

2. **Does the `operator new` override catch Lua's internal allocations?**
   - What we know: Lua 5.4 uses `lua_Alloc l_alloc` which calls `realloc()`/`malloc()` directly, NOT `::operator new`. So Lua-internal allocations are NOT counted.
   - What's unclear: Whether this is a limitation or a feature for ALLOC-03 (which targets C++ binding calls, not Lua VM internals).
   - Recommendation: This is a feature — the alloc guard verifies that the C++ binding code itself doesn't call `new`. The Lua VM's own allocator is intentionally excluded. Document this in the binary's success message.

3. **What happens when `bench_alloc` is run in CI without `bench-results/` directory?**
   - What we know: `bench_alloc` does not write JSON output (it exits 0 or non-zero only). It does not call `mkdir("bench-results", ...)`.
   - What's unclear: Whether the CI `convert-bench.py` step needs updating.
   - Recommendation: `bench_alloc` writes no JSON. The `convert-bench.py` step only processes the three named JSON files. No changes needed there.

---

## Validation Architecture

> `workflow.nyquist_validation` is absent from `.planning/config.json` — treating as enabled.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Self-validating binary (`bench_alloc`) — exits 0 on pass, 1 on first detected allocation |
| Config file | `benchmarks/CMakeLists.txt` (target: `bench_alloc`) |
| Quick run command | `"${BUILD_DIR}/benchmarks/bench_alloc"` |
| Full suite command | `bash scripts/build-bench.sh` (includes `bench_alloc` after extension) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| ALLOC-01 | `AllocGuard` RAII exits non-zero if `operator new` fires inside scope | unit (binary exit code) | `./bench_alloc && echo PASS \|\| exit 1` | ❌ Wave 0 |
| ALLOC-02 | CI workflow step fails when hot-path allocation detected | smoke (CI) | Push a commit with intentional alloc inside a guarded path; verify Actions fails | ❌ Wave 0 |
| ALLOC-03 | Canvas ops, Component update, Lua binding calls all pass zero-alloc check | integration | `./bench_alloc` (success = all three pass) | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `cmake --build "${BUILD_DIR}" --target bench_alloc && "${BUILD_DIR}/benchmarks/bench_alloc"`
- **Per wave merge:** `bash scripts/build-bench.sh` (full suite including alloc verification)
- **Phase gate:** `bench_alloc` exits 0 on CI runner before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `include/enjin2/instrumentation/alloc_guard.hpp` — AllocGuard RAII class + extern declarations (ALLOC-01)
- [ ] `benchmarks/bench_alloc.cpp` — `operator new` override + hot-path guard sections (ALLOC-01, ALLOC-03)
- [ ] `benchmarks/CMakeLists.txt` — `bench_alloc` target with `ENJIN2_ALLOC_VERIFICATION=1` (ALLOC-01)
- [ ] `scripts/build-bench.sh` — add `bench_alloc` to build targets and run step (ALLOC-02)
- [ ] `.github/workflows/benchmarks.yml` — explicit `Allocation Verification` step or rely on build-bench.sh propagation (ALLOC-02)

---

## Sources

### Primary (HIGH confidence)
- `/home/unwn/git/enjin/include/enjin2/core/object_collection.hpp` — `update()` uses raw index loop (no `std::function`); `forEach()` uses `std::function` (avoid in guards)
- `/home/unwn/git/enjin/include/enjin2/core/object.hpp` — `addComponent` uses `std::unique_ptr<T> component(new T(...))` — allocates; NOT a hot path
- `/home/unwn/git/enjin/benchmarks/bench_canvas.cpp` — confirmed hot-path operations: `clear`, `setPixel`, `fillRect`, `blit`
- `/home/unwn/git/enjin/benchmarks/bench_ecs.cpp` — confirmed scene::update pattern (setup outside timed loop)
- `/home/unwn/git/enjin/benchmarks/bench_lua.cpp` — confirmed `executeString` wrapping issue for Lua binding tests
- `/home/unwn/git/enjin/src/scripting/bindings_engine.cpp:562` — `lua_engine_time_delta` reads `lua_touserdata` + `lua_pushnumber` — no `operator new` calls
- `/home/unwn/git/enjin/.github/workflows/benchmarks.yml` — confirmed `bash scripts/build-bench.sh` propagates non-zero exit to CI step
- `/home/unwn/git/enjin/scripts/build-bench.sh` — confirmed `set -euo pipefail`; confirmed build targets
- C++ standard §18.6.1 — global `operator new` replacement rules
- Lua 5.4 source (`lmem.h`, `lmem.c`) — `l_alloc` uses `realloc`/`malloc` directly, not `::operator new`

### Secondary (MEDIUM confidence)
- C++14 standard — sized deallocation requires overriding `operator delete(void*, std::size_t)` to avoid bypass
- GCC documentation — `-fsized-deallocation` enabled by default since GCC 5; sized delete override required

### Tertiary (LOW confidence)
- `std::function` SBO threshold — typically 16-32 bytes but implementation-defined; verify empirically that the `forEach` lambda in `renderObjects` fits in SBO (not relevant for `update()` path which avoids `forEach`)

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — `operator new` replacement is C++ standard; all engine hot paths verified by direct source reading
- Architecture: HIGH — operator new + AllocGuard pattern is well-established; verified against project conventions (RAII header, compile-time flag)
- Pitfalls: HIGH — derived directly from source analysis (Lua allocator, executeString, sized delete)

**Research date:** 2026-03-08
**Valid until:** 2026-06-08 (C++ standard; Lua allocator behavior stable across 5.4.x)
