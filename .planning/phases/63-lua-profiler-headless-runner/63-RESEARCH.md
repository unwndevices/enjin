# Phase 63: Lua Profiler & Headless Runner - Research

**Researched:** 2026-03-08
**Domain:** Lua 5.4 debug hooks (lua_sethook), GC measurement (lua_gc), headless CLI runner design, JSON/text output formatting in C++
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PROF-01 | C-level profiler via lua_sethook (LUA_MASKCALL \| LUA_MASKRET) with per-function call counts | `lua_sethook` with `LUA_MASKCALL` is the correct Lua 5.4 API; call counts stored in a fixed-capacity hash table (function pointer -> count) avoids dynamic allocation |
| PROF-02 | Memory tracking via lua_gc with per-frame GC pressure ring buffer | `lua_gc(L, LUA_GCCOUNT, 0)` + `LUA_GCCOUNTB` gives exact byte count; per-frame delta = current_bytes - prev_bytes; ring buffer uses fixed-size array with head index |
| PROF-03 | Zero overhead when profiler disabled (lua_sethook(L, NULL, 0, 0)) | `lua_sethook(L, NULL, 0, 0)` is the documented Lua 5.4 API to disable all hooks; confirmed in Lua 5.4 reference; must be the disabled path (no hook pointer installed) |
| PROF-04 | Headless CLI runner (enjin_run) with --profile --frames N script.lua | New standalone executable (not enjin2_sdl) with no SDL3, no display, no input; uses existing LuaScriptSystem infrastructure |
| PROF-05 | enjin_run produces JSON and text table output formats | JSON: hand-written to stdout (no external JSON lib needed for simple array output); text: printf-formatted table with aligned columns |
| PROF-06 | enjin_run stubs all platform APIs (gfx, input) as no-ops | LuaCanvas wrappers pointing to minimal no-op canvases; input.* bindings return 0/false safely via existing null-guard pattern in bindings_engine.cpp |
</phase_requirements>

---

## Summary

Phase 63 builds two related capabilities: a C-level Lua profiler using `lua_sethook` and a headless CLI runner (`enjin_run`) that simulates N frames without any display, SDL3 window, or input device. Together they let developers measure per-function call counts and per-frame GC pressure from a single command-line invocation.

The key architectural insight from the project's existing codebase is that null-pointer guards already exist throughout the bindings. `engine.input.*`, `engine.scene.*`, `engine.time.*`, and `engine.lua.*` all guard against `nullptr` injection points and silently return 0/false/nil when their host objects are not wired. The headless runner exploits this by never wiring pointers that require SDL3 or a real canvas, leaving all `engine.*` subtable calls as safe no-ops.

The profiler itself uses `lua_sethook` with `LUA_MASKCALL` (and optionally `LUA_MASKRET`) to intercept every function call. The hook callback increments a per-function counter stored in a fixed-capacity table keyed by the function pointer (a `const void*` from `lua_Debug`). Because function identity is stable within a single Lua state, pointer equality is the correct key. No hash collisions possible with a fixed-table linear scan up to 256 slots.

**Primary recommendation:** Implement enjin_run as a new standalone executable in `src/platform/headless/` following the same pattern as `src/platform/sdl/sdl_main.cpp`. The profiler lives in a new header `include/enjin2/scripting/lua_profiler.hpp` (header-only singleton). Wire it via `lua_sethook` in enjin_run's init path, print results to stdout at exit.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `lua_sethook` / `lua_Debug` (Lua 5.4.8) | Already project requirement | C-level call interception | Official Lua 5.4 debug API; zero-cost when hook is NULL; only supported approach for C-level profiling |
| `lua_gc(L, LUA_GCCOUNT, 0)` (Lua 5.4.8) | Already project requirement | Memory usage snapshot | Already used in `bindings_engine.cpp` (engine.lua.memory()); same call, same KB+bytes API |
| `<cstdio>` / `snprintf` | C stdlib | Text table and JSON output to stdout | Already used project-wide; zero allocation; no external JSON library needed for simple array output |
| `<cstring>` / `strcmp` | C stdlib | Function name extraction from lua_Debug | Already used in all bindings; `ar.name` from `lua_Debug` gives function name string |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `LuaScriptSystem` (project-internal) | v1.10 | Lua state lifecycle (init/shutdown/loadScript) | enjin_run reuses the same host type as sdl_main.cpp; no new Lua state management needed |
| `LuaEngine::getState()` (project-internal) | v1.10 | Access raw `lua_State*` for `lua_sethook` | `lua_sethook` requires the raw pointer; `LuaEngine::getState()` returns it |
| `LuaBindings::setTimeState()` (project-internal) | v1.10 | Simulate frame time for `engine.time.*` | Per-frame call in headless loop sets dt=fixed (e.g., 1/60), totalTime accumulates; prevents div-by-zero or nan in scripts |
| `LuaCanvas` null-canvas wrapper (project-internal) | v1.10 | No-op canvas for draw calls | Construct `LuaCanvas` pointing to a minimal `Canvas4<128,128>` that is never displayed; all draw calls are silently accepted |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Fixed-capacity `const void*` -> count table | `std::unordered_map<std::string, int>` | unordered_map allocates on heap; violates zero-alloc philosophy; also `const void*` is more reliable than `ar.name` (which can be NULL for anonymous functions) |
| `lua_sethook(LUA_MASKCALL)` | `lua_sethook(LUA_MASKCALL \| LUA_MASKRET)` | LUA_MASKRET adds ~2x hook invocations; for call count only, LUA_MASKCALL is sufficient; PROF-01 spec says "call counts" not "return counts" |
| Custom JSON writer (hand-rolled) | nlohmann/json or similar | enjin_run output is a flat array of {name, count} objects; ~20 lines of printf are adequate; no dependency warranted |
| New `MinimalLuaHost` class | Reuse existing `LuaScriptSystem` | LuaScriptSystem already wraps LuaEngine + LuaBindings in the correct init/shutdown lifecycle; adding a new class would duplicate this |

**Installation:** No new dependencies. All required components are already present in the project.

---

## Architecture Patterns

### Recommended Project Structure

New files for this phase:

```
include/enjin2/scripting/
    lua_profiler.hpp          # Header-only: LuaProfiler struct, hook callback, output functions

src/platform/headless/
    headless_main.cpp         # enjin_run entry point (mirrors sdl_main.cpp pattern)

tests/
    lua_profiler_test.cpp     # Unit tests: hook installs/removes, count tracking, zero overhead
```

Modified files:
```
CMakeLists.txt                # New enjin_run executable target
tests/CMakeLists.txt          # lua_profiler_test target + ctest entry
```

### Pattern 1: lua_sethook Call Count Profiler

**What:** Install a C hook function via `lua_sethook` with `LUA_MASKCALL`. The hook receives a `lua_Debug` struct; call `lua_getinfo(L, "nS", &ar)` to populate `ar.name` and `ar.what`. Increment a per-function slot identified by `ar.currentline` + `ar.source` or by the function pointer obtained via `lua_getinfo(L, "f", &ar)`.

**When to use:** In the `--profile` code path only. When `--profile` is absent, call `lua_sethook(L, NULL, 0, 0)` to confirm zero overhead (PROF-03).

**Key implementation detail:** Function identity in Lua 5.4: after `lua_getinfo(L, "f", &ar)`, the function is pushed onto the stack. Use `lua_topointer(L, -1)` to get a stable `const void*` identity for that closure, then pop it. This is the canonical way to identify functions in a hook.

**Example:**

```c
// include/enjin2/scripting/lua_profiler.hpp
#pragma once
#include "lua_platform.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace enjin2 {

// Fixed-capacity profiler — 256 distinct functions max, zero heap allocation
struct LuaProfiler {
    static constexpr int MAX_FUNCTIONS = 256;
    static constexpr int MAX_NAME_LEN  = 64;

    struct FuncEntry {
        const void* ptr{nullptr};          // Function pointer identity (lua_topointer)
        char        name[MAX_NAME_LEN]{};  // Human-readable name (ar.name or "[?]")
        char        source[MAX_NAME_LEN]{}; // Source file (ar.short_src truncated)
        int         line{0};               // Definition line (ar.linedefined)
        uint32_t    callCount{0};          // Accumulated call count
    };

    FuncEntry entries[MAX_FUNCTIONS]{};
    int       entryCount{0};
    bool      active{false};

    static LuaProfiler& get() {
        static LuaProfiler s_instance;
        return s_instance;
    }

    void reset() {
        entryCount = 0;
        active     = false;
        for (int i = 0; i < MAX_FUNCTIONS; ++i) {
            entries[i] = FuncEntry{};
        }
    }

    // Hook callback installed via lua_sethook
    static void hookCallback(lua_State* L, lua_Debug* ar) {
        if (ar->event != LUA_HOOKCALL) return;

        // Push the function itself to get its identity pointer
        lua_getinfo(L, "nSf", ar);          // populate ar->name, ar->short_src; push function
        const void* fptr = lua_topointer(L, -1);
        lua_pop(L, 1);                      // pop the function

        LuaProfiler& p = LuaProfiler::get();
        if (!p.active) return;

        // Linear scan for existing entry (max 256 slots, fast in practice)
        for (int i = 0; i < p.entryCount; ++i) {
            if (p.entries[i].ptr == fptr) {
                p.entries[i].callCount++;
                return;
            }
        }
        // New function
        if (p.entryCount < MAX_FUNCTIONS) {
            FuncEntry& e = p.entries[p.entryCount++];
            e.ptr       = fptr;
            e.callCount = 1;
            if (ar->name) {
                strncpy(e.name, ar->name, MAX_NAME_LEN - 1);
                e.name[MAX_NAME_LEN - 1] = '\0';
            } else {
                strncpy(e.name, "[?]", MAX_NAME_LEN - 1);
            }
            strncpy(e.source, ar->short_src, MAX_NAME_LEN - 1);
            e.source[MAX_NAME_LEN - 1] = '\0';
            e.line = ar->linedefined;
        }
    }

    void install(lua_State* L) {
        active = true;
        lua_sethook(L, hookCallback, LUA_MASKCALL, 0);
    }

    void uninstall(lua_State* L) {
        // PROF-03: explicit NULL hook = confirmed zero overhead
        lua_sethook(L, NULL, 0, 0);
        active = false;
    }

    // Sort entries descending by callCount (simple insertion sort — 256 max)
    void sortByCount() {
        for (int i = 1; i < entryCount; ++i) {
            FuncEntry key = entries[i];
            int j = i - 1;
            while (j >= 0 && entries[j].callCount < key.callCount) {
                entries[j + 1] = entries[j];
                --j;
            }
            entries[j + 1] = key;
        }
    }

    // Print text table to stdout
    void printTable() {
        sortByCount();
        printf("%-40s %8s %6s %s\n", "Function", "Calls", "Line", "Source");
        printf("%-40s %8s %6s %s\n",
               "----------------------------------------",
               "--------", "------",
               "----------------------------------------------");
        for (int i = 0; i < entryCount; ++i) {
            printf("%-40s %8u %6d %s\n",
                   entries[i].name,
                   entries[i].callCount,
                   entries[i].line,
                   entries[i].source);
        }
    }

    // Write JSON array to stdout
    void printJSON() {
        sortByCount();
        printf("[\n");
        for (int i = 0; i < entryCount; ++i) {
            const FuncEntry& e = entries[i];
            printf("  {\"name\":\"%s\",\"calls\":%u,\"line\":%d,\"source\":\"%s\"}%s\n",
                   e.name, e.callCount, e.line, e.source,
                   (i < entryCount - 1) ? "," : "");
        }
        printf("]\n");
    }

    LuaProfiler(const LuaProfiler&)            = delete;
    LuaProfiler& operator=(const LuaProfiler&) = delete;

private:
    LuaProfiler() = default;
};

} // namespace enjin2
```

### Pattern 2: Per-Frame GC Pressure Ring Buffer

**What:** Call `lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0)` before and after each simulated frame. Delta = current - previous bytes. Store deltas in a fixed-size ring buffer. At end of run, compute min/max/mean across the ring buffer.

**When to use:** In `--profile` path, per frame after the Lua frame completes. Complements call counts with memory allocation pressure data.

**Example:**

```c
// In headless_main.cpp, per-frame GC tracking
static constexpr int GC_RING_SIZE = 64;  // tracks last 64 frames
int32_t gcRing[GC_RING_SIZE]{};          // signed — GC can free memory (negative delta)
int     gcHead = 0;

// Per frame:
lua_State* L = g_lua.getEngine().getState();
int before_bytes = lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);
// ... run Lua update() and draw() ...
int after_bytes  = lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);
gcRing[gcHead % GC_RING_SIZE] = after_bytes - before_bytes;
gcHead++;
```

### Pattern 3: Headless Runner Main Loop

**What:** A `main()` that parses `--profile`, `--frames N`, and positional `script.lua` arguments. Initializes LuaScriptSystem, sets up minimal wiring (fake canvas, fake input), runs N frames calling `update()` and `draw()` each, then exits and prints results.

**When to use:** `enjin_run` only. sdl_main.cpp is unchanged by this phase.

**Key decisions:**
- Fixed dt = 1.0f / 60.0f (simulate 60fps) unless overridden by `--fps N`
- Input pointer left as nullptr (all engine.input.* bindings null-guard to 0/false already)
- Canvas wrappers point to a static `Canvas4<128, 128>` — never displayed
- `engine.scene.*`, `engine.camera.*` etc. null-guard safely when SSM and scene are nullptr

**Example (headless_main.cpp skeleton):**

```cpp
// src/platform/headless/headless_main.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/layer_compositor.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_profiler.hpp>

static constexpr int CANVAS_W = 128;
static constexpr int CANVAS_H = 128;

static enjin2::LayerCompositor<CANVAS_W, CANVAS_H> g_compositor;
static enjin2::LuaScriptSystem g_lua;

static enjin2::LuaCanvas g_lua_layer0(&g_compositor.layers[0]);
static enjin2::LuaCanvas g_lua_layer1(&g_compositor.layers[1]);
static enjin2::LuaCanvas g_lua_layer2(&g_compositor.layers[2]);
static enjin2::LuaCanvas g_lua_layer3(&g_compositor.layers[3]);
static enjin2::LuaCanvas g_lua_layer4(&g_compositor.layers[4]);  // debug layer
static enjin2::LuaCanvas* g_lua_layers[4] = {
    &g_lua_layer0, &g_lua_layer1, &g_lua_layer2, &g_lua_layer3
};

int main(int argc, char* argv[]) {
    bool do_profile  = false;
    bool output_json = false;
    int  frames      = 100;
    const char* script = nullptr;

    // Arg parse: --profile, --frames N, --output json, positional script.lua
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--profile") == 0) {
            do_profile = true;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            ++i;
            if (strcmp(argv[i], "json") == 0) output_json = true;
        } else if (argv[i][0] != '-') {
            script = argv[i];
        }
    }

    if (!script) {
        fprintf(stderr, "usage: enjin_run [--profile] [--frames N] [--output json] script.lua\n");
        return 1;
    }

    if (!g_lua.initialize()) {
        fprintf(stderr, "[enjin_run] Lua init failed\n");
        return 1;
    }

    // Wire layers — input pointer intentionally nullptr (null-guards in all input bindings)
    g_lua.getBindings().setLayers(g_lua_layers, 4, g_compositor.visible);
    g_lua.getBindings().setDebugCanvas(&g_lua_layer4);
    // Note: setInput() NOT called — currentInput remains nullptr, all engine.input.* return 0/false

    // Install profiler hook BEFORE loadScript so init() function calls are counted
    if (do_profile) {
        enjin2::LuaProfiler::get().reset();
        enjin2::LuaProfiler::get().install(g_lua.getEngine().getState());
    } else {
        // PROF-03: explicit confirmation of zero-overhead disabled path
        lua_sethook(g_lua.getEngine().getState(), NULL, 0, 0);
    }

    enjin2::LuaResult r = g_lua.loadScript(script);
    if (!r.success) {
        fprintf(stderr, "[enjin_run] script load error: %s\n", r.error.c_str());
        g_lua.shutdown();
        return 1;
    }

    // Main headless loop
    const float dt       = 1.0f / 60.0f;
    float totalTime      = 0.0f;
    uint32_t frameCount  = 0;

    // GC ring buffer for PROF-02
    static constexpr int GC_RING = 256;
    int32_t gcDeltas[GC_RING]{};
    int gcHead = 0;

    lua_State* L = g_lua.getEngine().getState();

    for (int f = 0; f < frames; ++f) {
        totalTime += dt;
        g_lua.getBindings().setTimeState(dt, totalTime, frameCount++);

        // GC snapshot before
        int mem_before = lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);

        // Call update()
        lua_getglobal(L, "update");
        if (lua_isfunction(L, -1)) {
            lua_pushnil(L);
            lua_pushnumber(L, static_cast<lua_Number>(dt));
            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                fprintf(stderr, "[enjin_run] update() error: %s\n", err ? err : "?");
                lua_pop(L, 1);
                break;
            }
        } else {
            lua_pop(L, 1);
        }

        // Tick subsystems
        g_lua.getBindings().tickCameraFollow(dt);
        g_lua.getBindings().tickCoroutines(dt);
        g_lua.getBindings().tickTweens(dt);

        // Call draw()
        lua_getglobal(L, "draw");
        if (lua_isfunction(L, -1)) {
            lua_pushnil(L);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                fprintf(stderr, "[enjin_run] draw() error: %s\n", err ? err : "?");
                lua_pop(L, 1);
                break;
            }
        } else {
            lua_pop(L, 1);
        }

        // GC snapshot after
        int mem_after = lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);
        if (gcHead < GC_RING) {
            gcDeltas[gcHead++] = mem_after - mem_before;
        }
    }

    // Uninstall hook before shutdown
    if (do_profile) {
        enjin2::LuaProfiler::get().uninstall(L);
    }

    g_lua.shutdown();

    // Print results
    if (do_profile) {
        if (output_json) {
            enjin2::LuaProfiler::get().printJSON();
        } else {
            enjin2::LuaProfiler::get().printTable();
            // Also print GC summary
            if (gcHead > 0) {
                int32_t total = 0;
                int32_t mn = gcDeltas[0], mx = gcDeltas[0];
                for (int i = 0; i < gcHead; ++i) {
                    total += gcDeltas[i];
                    if (gcDeltas[i] < mn) mn = gcDeltas[i];
                    if (gcDeltas[i] > mx) mx = gcDeltas[i];
                }
                printf("\nGC pressure over %d frames:\n", gcHead);
                printf("  min delta: %d bytes\n", mn);
                printf("  max delta: %d bytes\n", mx);
                printf("  avg delta: %d bytes\n", total / gcHead);
            }
        }
    }

    return 0;
}
```

### Pattern 4: CMake Target for enjin_run

**What:** New CMake executable target, similar to `enjin2_sdl` but without SDL3 and without `ENJIN2_FRAME_TIMING`. Links `enjin2_lua`, `enjin2_core`, `enjin2_graphics`, `enjin2_ui`.

**Example (CMakeLists.txt addition):**

```cmake
# Headless Lua runner (PROF-04..PROF-06)
option(ENJIN2_BUILD_HEADLESS "Build headless Lua profiling runner (enjin_run)" OFF)

if(ENJIN2_BUILD_HEADLESS)
    if(EMSCRIPTEN OR ESP32)
        message(FATAL_ERROR "ENJIN2_BUILD_HEADLESS is not supported on WASM or ESP32 targets")
    endif()
    if(NOT TARGET enjin2_lua)
        message(FATAL_ERROR "ENJIN2_BUILD_HEADLESS requires ENJIN2_BUILD_LUA=ON")
    endif()
    add_executable(enjin_run
        src/platform/headless/headless_main.cpp
    )
    target_include_directories(enjin_run PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        ${LUA_INCLUDE_DIRS}
    )
    target_link_libraries(enjin_run PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_ui
        enjin2_input
        enjin2_lua
    )
    target_compile_definitions(enjin_run PRIVATE
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:ENJIN2_BUILD_LUA=1>
        ENJIN2_CANVAS_WIDTH=${ENJIN2_CANVAS_WIDTH}
        ENJIN2_CANVAS_HEIGHT=${ENJIN2_CANVAS_HEIGHT}
    )
endif()
```

### Null-Safety Map for Headless Mode

This is the critical enumeration of all `engine.*` pointer registrations and their headless behavior. From direct code audit of `bindings_engine.cpp` and `bindings.hpp`:

| engine.* call | Pointer required | Headless behavior | Safe? |
|--------------|-----------------|-------------------|-------|
| `engine.scene.switch(id)` | `m_ssm` (SceneStateMachine*) | `ssmPP == nullptr`: returns 0, no-op | YES — null guard line 363 |
| `engine.scene.find(name)` | `m_activeScene` (Scene*) | `scenePP == nullptr`: pushes nil | YES — null guard line 378 |
| `engine.scene.spawn()` | `m_activeScene` | pushes nil | YES — null guard line 468 |
| `engine.scene.destroy()` | `m_activeScene` | returns 0 | YES — null guard line 517 |
| `engine.scene.persist()` | `m_ssm` | prints warning, no-op | YES — null guard line 421 |
| `engine.scene.unpersist()` | `m_ssm` | no-op | YES — null guard line 449 |
| `engine.input.held(btn)` | `currentInput` (InputState*) | returns false | YES — null guard line 527 |
| `engine.input.just_pressed(btn)` | `currentInput` | returns false | YES — null guard line 536 |
| `engine.input.just_released(btn)` | `currentInput` | returns false | YES — null guard line 545 |
| `engine.input.axis(n)` | `currentInput` | returns 0.0 | YES — null guard line 554 |
| `engine.time.delta()` | `m_timeState` (value type) | reads from value — always valid | YES — no pointer |
| `engine.time.now()` | `m_timeState` | always valid | YES — no pointer |
| `engine.time.frame()` | `m_timeState` | always valid | YES — no pointer |
| `engine.lua.collect()` | `lua_State*` (from engine) | calls `lua_gc(L, LUA_GCSTEP, 0)` | YES — L is always valid when registered |
| `engine.lua.memory()` | `lua_State*` | calls `lua_gc` pair | YES |
| `engine.log(...)` | none | printf to stdout | YES |
| Canvas draw calls (draw, rectangle, etc.) | `currentCanvas` (LuaCanvas*) | MUST wire a valid LuaCanvas | REQUIRES wiring — see PITFALL 1 |
| `engine.camera.*` | `m_activeCamera` (C_Camera*) | all null-guarded in bindings_physics.cpp | YES (verified in Phase 44 pattern) |
| `engine.random.*` | `m_rngState` (value type) | always valid | YES |
| `engine.event.*` | `m_eventBus` (value type) | always valid | YES |
| `engine.store.*` | `m_store` (value type) | always valid, file ops will fail gracefully | YES |
| `engine.async.*` | coroutine pool (value type) | always valid | YES |
| `engine.tween.*` | tween pool (value type) | always valid | YES |
| `engine.debug.*` | `m_debugCanvas` | must provide a canvas or null-guard | WIRE debug canvas too |
| `engine.physics.*` | `m_gravityX/Y` (value types) | always valid | YES |

**Critical finding:** The ONLY pointers that MUST be wired for headless mode to avoid crashes are:
1. `currentCanvas` — direct drawing calls (`draw`, `rectangle`, etc.) dereference this without a null guard in `bindings_draw.cpp`
2. `layerCanvases` — `setLayer()`, `clearLayer()` etc. access via index

The fix: call `setLayers(g_lua_layers, 4, g_compositor.visible)` and `setDebugCanvas(&g_lua_layer4)` with static canvases pointing to the static compositor. This is the same setup as sdl_main.cpp minus the SDL3 display.

### Anti-Patterns to Avoid

- **Calling setInput() with a null pointer:** `input_advance_frame(nullptr)` would dereference. In headless mode, simply never call `setInput()` and never call `input_advance_frame()`. All `engine.input.*` bindings null-guard `currentInput` safely.
- **Installing hook after loadScript():** The initial script body (module-level code including `init()`) would not be profiled. Install hook BEFORE `loadScript()`.
- **Using `ar.name` as hash key:** `ar.name` can be NULL for anonymous functions or tail calls. Use `lua_topointer` after `lua_getinfo(L, "f", ar)` for reliable function identity.
- **Leaving hook active across lua_close():** Uninstall hook before `g_lua.shutdown()` to avoid hook firing during GC cleanup phase.
- **Not calling tickCoroutines/tickTweens in headless:** Scripts using `engine.async.*` or `engine.tween.*` will deadlock (coroutines never resume). Always tick these each frame.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Function identity in Lua | Custom function name string hash | `lua_topointer(L, -1)` after `lua_getinfo(L, "f", ar)` | Name can be NULL; pointer is stable and unique for the lifetime of the Lua state |
| JSON output | External JSON library | Hand-written `printf` with `{\"name\":\"%s\",\"calls\":%u}` | Output is a flat array of simple objects; 15 lines of printf suffice; no new dependency warranted |
| Sorting profiler results | std::sort | Insertion sort over 256 max entries | 256 entries, O(n^2) is instant; project avoids STL containers in instrumentation code |
| Lua state lifecycle in enjin_run | New `MinimalLuaHost` class | `LuaScriptSystem` + `LuaBindings` (existing) | LuaScriptSystem already handles all of initialize/shutdown/registerAll/loadScript |
| Canvas allocation for headless | Dynamic canvas on heap | Static `LayerCompositor<128,128>` + static `LuaCanvas` wrappers | Zero heap allocation; same pattern as sdl_main.cpp |

**Key insight:** The null-pointer guard pattern is already pervasive in the bindings. The headless runner does not need to implement any new guard logic — it simply does not wire the pointers that require display/input hardware.

---

## Common Pitfalls

### Pitfall 1: Null Canvas Dereference in Draw Bindings

**What goes wrong:** Script calls `draw()` which calls `rectangle(...)` which calls `lua_rectangle()` in `bindings_draw.cpp`, which accesses `b->currentCanvas` without a null guard — segfault.

**Why it happens:** Unlike `engine.input.*` which null-guards `currentInput`, the direct global drawing functions (`rectangle`, `line`, `clear`, etc.) access `currentCanvas` with the assumption the host always wires it. This is documented in `bindings.hpp` (LuaCanvas* currentCanvas always set by host).

**How to avoid:** Always call `setLayers(layers, 4, visible)` before loading the script. This sets `currentCanvas = layerCanvases[0]`. The 128x128 static canvas is a valid write target even in headless mode.

**Warning signs:** Segfault or null pointer dereference inside any `lua_rectangle`, `lua_clear`, `lua_line`, or similar binding function.

### Pitfall 2: Hook Fires During lua_close() GC Sweep

**What goes wrong:** `lua_sethook` hook is still active when `lua_close` runs. GC sweep calls finalizers which trigger call events. Hook accesses `LuaProfiler::get()` which may have already been modified by the output printing step.

**Why it happens:** `lua_close(L)` runs garbage collection on all live objects. If hooks are installed, they fire for finalizer calls.

**How to avoid:** Call `lua_sethook(L, NULL, 0, 0)` BEFORE `g_lua.shutdown()` (which calls `lua_close`). The `LuaProfiler::uninstall()` method handles this. Call `uninstall()` then print, then shutdown.

**Warning signs:** Unexpected entries in profiler output for `__gc` metamethods, or output appearing twice.

### Pitfall 3: ar.name NULL for C Functions

**What goes wrong:** `ar.name` is NULL for built-in C functions (like engine bindings) when called from Lua. Passing NULL to `strncpy` in the hook callback crashes.

**Why it happens:** Lua 5.4 `lua_Debug.name` is only populated for Lua functions that have a "name" discoverable from the debug info. C functions called from Lua have `ar.what == "C"` and `ar.name` can be NULL or the C function name.

**How to avoid:** Always check `if (ar->name)` before using it. Use `"[?]"` as fallback. Note: `lua_getinfo(L, "nSf", ar)` must be called inside the hook to populate `ar->name` (it is not populated by the hook event alone).

**Warning signs:** Crash inside `hookCallback` on the `strncpy(e.name, ar->name, ...)` line.

### Pitfall 4: `ar.name` Not Populated Without lua_getinfo

**What goes wrong:** `ar->name` is always NULL even for named Lua functions because `lua_getinfo` was not called inside the hook with the "n" option.

**Why it happens:** The `lua_Debug` structure passed to the hook callback only has `ar->event` populated by default. All other fields require calling `lua_getinfo(L, "nSf", ar)` from within the hook.

**How to avoid:** Always call `lua_getinfo(L, "nSf", ar)` at the start of the hook callback (before accessing any ar fields except `ar->event`). The "f" option pushes the function on the stack — remember to pop it with `lua_pop(L, 1)`.

**Warning signs:** All function names show as "[?]" in the profiler output.

### Pitfall 5: Coroutine resume never fires in headless

**What goes wrong:** Script uses `engine.async.start()` to launch a coroutine. The coroutine suspends on `engine.async.wait(1.0)`. Without `tickCoroutines(dt)` called each headless frame, the coroutine never resumes, the script hangs indefinitely (or rather, it completes frames but the coroutine body never runs).

**Why it happens:** The SDL runner explicitly calls `g_lua.getBindings().tickCoroutines(dt)` each frame. The headless runner must mirror this.

**How to avoid:** Call `tickCameraFollow(dt)`, `tickCoroutines(dt)`, `tickTweens(dt)` in the headless loop after the `update()` call, identical to sdl_main.cpp.

**Warning signs:** A script using `engine.async.start(function() ... end)` runs zero iterations of the coroutine body during profiling.

### Pitfall 6: Success Criteria 4 — engine.* subtable null-dereference

**What goes wrong:** A test script exercises ALL `engine.*` subtables (scene, input, time, lua, log) including calls that dereference pointers (e.g., `engine.scene.find("x")` when `m_activeScene` is nil but `scenePP` is a valid `Scene**` pointing to nullptr).

**Why it happens:** The pointer-to-pointer registry pattern (`&m_activeScene`) means `scenePP` is always a valid non-null `Scene**` — it points to a member of LuaBindings. The outer null check `*ssmPP == nullptr` is what catches the "not wired" case. This is correct.

**How to avoid:** Understand that the null guard pattern is: `if (ssmPP == nullptr || *ssmPP == nullptr)`. `ssmPP` will never be nullptr because the registry stores `&m_ssm` (address of the member). `*ssmPP` will be nullptr when the SSM was not injected. The existing null guards handle this correctly — no action needed.

**Warning signs:** Confusion about "which pointer is null" — the outer `Scene**` is never null; only `*scenePP` (the actual Scene pointer) is null in headless.

---

## Code Examples

Verified patterns from official Lua 5.4 API and project sources:

### lua_sethook API (Lua 5.4 Reference)
```c
// Install hook for every function call
lua_sethook(L, my_hook, LUA_MASKCALL, 0);

// Disable hook (PROF-03 confirmed zero overhead)
lua_sethook(L, NULL, 0, 0);

// Inside hook callback — get function info:
void my_hook(lua_State* L, lua_Debug* ar) {
    lua_getinfo(L, "nSf", ar);    // populate ar->name, ar->short_src; push function
    const void* fptr = lua_topointer(L, -1);
    lua_pop(L, 1);
    // ar->name may be NULL for C functions or anonymous Lua functions
    // ar->what: "Lua" for Lua functions, "C" for C functions, "main" for main chunk
    // ar->short_src: source file name (truncated)
    // ar->linedefined: line where function was defined
}
```

### GC Memory Query (from project bindings_engine.cpp, lines 735-736)
```c
// Exact byte count — combine KB count + remainder bytes
int kb  = lua_gc(L, LUA_GCCOUNT,  0);  // whole kilobytes used
int rem = lua_gc(L, LUA_GCCOUNTB, 0);  // remaining bytes < 1KB
int total_bytes = kb * 1024 + rem;
```

### Headless Frame Loop Pattern (mirrors sdl_main.cpp structure)
```cpp
// From src/platform/sdl/sdl_main.cpp lines 302-387 adapted for headless:
float totalTime  = 0.0f;
uint32_t frameCount = 0u;
const float dt  = 1.0f / 60.0f;

for (int f = 0; f < frames; ++f) {
    totalTime += dt;
    g_lua.getBindings().setTimeState(dt, totalTime, frameCount++);

    // update()
    lua_getglobal(L, "update");
    if (lua_isfunction(L, -1)) {
        lua_pushnil(L);    // self = nil (headless has no proxy)
        lua_pushnumber(L, static_cast<lua_Number>(dt));
        lua_pcall(L, 2, 0, 0);
    } else { lua_pop(L, 1); }

    // tick subsystems (mirrors sdl_main.cpp lines 360-364)
    g_lua.getBindings().tickCameraFollow(dt);
    g_lua.getBindings().tickCoroutines(dt);
    g_lua.getBindings().tickTweens(dt);

    // draw()
    lua_getglobal(L, "draw");
    if (lua_isfunction(L, -1)) {
        lua_pushnil(L);
        lua_pcall(L, 1, 0, 0);
    } else { lua_pop(L, 1); }
}
```

### Unit Test Pattern (mirrors frame_timing_test.cpp)
```cpp
// tests/lua_profiler_test.cpp
#include <enjin2/scripting/lua_profiler.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <cstdio>

static int passes = 0, failures = 0;
#define ASSERT(cond, msg) \
    do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
         else { passes++; } } while(0)

static void test_hook_install_uninstall() {
    enjin2::LuaEngine eng;
    eng.initialize();
    lua_State* L = eng.getState();

    enjin2::LuaProfiler& p = enjin2::LuaProfiler::get();
    p.reset();
    p.install(L);
    ASSERT(p.active, "profiler active after install");

    // Execute a Lua function — should increment call count
    eng.executeString("function hello() end hello()");
    ASSERT(p.entryCount > 0, "at least one entry after Lua call");

    p.uninstall(L);
    ASSERT(!p.active, "profiler inactive after uninstall");
    eng.shutdown();
}

static void test_zero_overhead_disabled() {
    enjin2::LuaEngine eng;
    eng.initialize();
    lua_State* L = eng.getState();

    // Confirm lua_sethook(L, NULL, 0, 0) is the disabled path (PROF-03)
    lua_sethook(L, NULL, 0, 0);
    // Execute code — hook should not fire
    enjin2::LuaProfiler::get().reset();
    eng.executeString("function x() end x() x() x()");
    ASSERT(enjin2::LuaProfiler::get().entryCount == 0, "zero entries when hook disabled");
    eng.shutdown();
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `debug.sethook` (Lua-level) | `lua_sethook` (C-level) | Design decision (REQUIREMENTS.md: "Lua-level debug.sethook profiler — Hook overhead invalidates measurements") | C-level hook is in the correct path; project explicitly chose this |
| Tracy Profiler (out of scope) | Inline hook counter | Design decision | Matches zero-alloc constraint; no background thread needed |
| SDL3 runner with --headless flag | Separate enjin_run executable | Phase design | Cleaner separation; SDL3 not linked in headless; no display subsystem initialized |

**Deprecated/outdated:**
- `debug.sethook` (Lua-side): Explicitly out of scope per REQUIREMENTS.md — "Hook overhead invalidates measurements (PIL 23.3)"
- Tracy, Remotery: Out of scope — "Requires background thread + network; violates zero-alloc/zero-threading model"

---

## Open Questions

1. **Whether to count C function calls in the profiler output**
   - What we know: `ar.what == "C"` for engine bindings. These are valid call events and will appear in the profiler output (as the C function name if non-NULL, "[?]" otherwise).
   - What's unclear: Whether PROF-01 "per-function call counts" includes C binding calls or only Lua functions.
   - Recommendation: Include both — C function calls show up as engine binding overhead which is exactly what users want to measure. Filter by `ar.what == "Lua"` if text table is too noisy (make this a flag `--lua-only`).

2. **Fixed dt vs. real-time simulation in headless**
   - What we know: No real clock is needed in headless mode. Fixed dt = 1/60 is conventional.
   - What's unclear: Whether `--fps N` should be added as an argument to control simulated dt.
   - Recommendation: Add `--fps N` for completeness (mirrors sdl_main.cpp). Default 60. Planner should include this as part of PROF-04 success criterion 1 which says "100 simulated frames".

3. **JSON output file vs. stdout**
   - What we know: Success criterion 3 says "writes a JSON file of profiling results". Criterion 2 says "prints a sorted table to stdout".
   - What's unclear: Whether JSON is written to a file (and which path) or to stdout. The requirement says "writes a JSON file" suggesting a file.
   - Recommendation: For `--output json`, write to `profiler-results.json` in the current working directory. Also accept `--output-path FILE` to override. Simpler is to write to stdout (and let the shell redirect); the success criterion says "JSON file" so writing to a named file is safer for criterion 3 compliance.

---

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Project-own ASSERT macros (fprintf stderr) — same pattern as frame_timing_test.cpp, engine_table_test.cpp |
| Config file | `tests/CMakeLists.txt` — add `lua_profiler_test` entry |
| Quick run command | `cmake --build build --target lua_profiler_test && ./build/tests/lua_profiler_test` |
| Full suite command | `cmake --build build && ctest --output-on-failure` |

### Phase Requirements to Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PROF-01 | `lua_sethook(LUA_MASKCALL)` intercepts Lua function calls; per-function count increments correctly | unit | `./build/tests/lua_profiler_test` | Wave 0 |
| PROF-02 | GC memory query returns non-zero; delta between frames can be computed | unit | `./build/tests/lua_profiler_test` | Wave 0 |
| PROF-03 | `lua_sethook(L, NULL, 0, 0)` installed in non-profile path; LuaProfiler entryCount stays 0 | unit | `./build/tests/lua_profiler_test` | Wave 0 |
| PROF-04 | `enjin_run --frames 100 script.lua` exits 0, no crash | smoke | `./build-headless/enjin_run --frames 100 scripts/layer_demo.lua; echo $?` | Manual |
| PROF-04 | `enjin_run --profile --frames 100 script.lua` exits 0 | smoke | Manual | Manual |
| PROF-05 | Text table output has correct column headers and sorted rows | smoke | `./build-headless/enjin_run --profile --frames 10 scripts/layer_demo.lua \| head -5` | Manual |
| PROF-05 | JSON output is valid JSON array | smoke | `./build-headless/enjin_run --profile --output json --frames 10 scripts/layer_demo.lua` | Manual |
| PROF-06 | Script calling engine.scene.find/switch/input.held/time.delta/lua.memory/log runs without crash | unit | `./build/tests/lua_profiler_test` (null-safety test) | Wave 0 |

### Sampling Rate
- **Per task commit:** `cmake --build build --target lua_profiler_test && ./build/tests/lua_profiler_test`
- **Per wave merge:** Full `ctest --output-on-failure`
- **Phase gate:** All ctest tests green + manual smoke of `enjin_run --profile --frames 100 scripts/layer_demo.lua` showing non-empty table

### Wave 0 Gaps
- [ ] `include/enjin2/scripting/lua_profiler.hpp` — LuaProfiler struct, hookCallback, install/uninstall, printTable/printJSON
- [ ] `src/platform/headless/headless_main.cpp` — enjin_run entry point
- [ ] `tests/lua_profiler_test.cpp` — unit tests for PROF-01, PROF-02, PROF-03, PROF-06
- [ ] Entry in `tests/CMakeLists.txt` for `lua_profiler_test` (links enjin2_lua)
- [ ] `CMakeLists.txt` — `ENJIN2_BUILD_HEADLESS` option and `enjin_run` target

---

## Sources

### Primary (HIGH confidence)
- Lua 5.4 Reference Manual — `lua_sethook`, `lua_Debug`, `lua_getinfo`, `LUA_MASKCALL`, `LUA_MASKRET`, `lua_gc` APIs (https://www.lua.org/manual/5.4/manual.html#4.7)
- Project source: `src/platform/sdl/sdl_main.cpp` (full game loop read — headless loop mirrors this exactly minus SDL3 calls)
- Project source: `src/scripting/bindings_engine.cpp` — complete null-guard audit of all `engine.*` pointer dereferences (lines 363, 378, 421, 449, 468, 517, 527, 536, 545, 554)
- Project source: `include/enjin2/scripting/bindings.hpp` — LuaBindings member fields (currentInput, m_ssm, m_activeScene, currentCanvas, layerCanvases)
- Project source: `src/scripting/bindings_engine.cpp` lines 722-736 — existing `lua_gc` usage (LUA_GCSTEP, LUA_GCCOUNT, LUA_GCCOUNTB) confirming the memory query pattern
- Project source: `benchmarks/bench_lua.cpp` — headless LuaEngine + LuaBindings usage pattern (no SDL3, no canvas wired for benchmark; confirmed registerAll() is safe without canvas)
- Project source: `.planning/STATE.md` — explicit blocker note: "Phase 63: null-binding safety contract for headless enjin_run not yet mapped — enumerate all engine.* pointer registrations first (segfault risk)"
- Project source: `tests/frame_timing_test.cpp`, `tests/engine_table_test.cpp` — test structure patterns

### Secondary (MEDIUM confidence)
- Lua 5.4 source (ldo.c): `lua_sethook(L, NULL, 0, 0)` sets hook to NULL and mask to 0, disabling all hook calls — confirms PROF-03 zero overhead path
- REQUIREMENTS.md line: "Lua-level debug.sethook profiler — Hook overhead invalidates measurements (PIL 23.3)" — confirms C-level `lua_sethook` is the correct approach (not Lua-side)

### Tertiary (LOW confidence)
- None — all critical findings are verified against Lua 5.4 official docs or project source

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — Lua 5.4 `lua_sethook` API is stable and fully documented; lua_gc usage already verified in project source
- Architecture: HIGH — sdl_main.cpp fully read; all 20+ engine.* pointer registrations null-guard audited; bench_lua.cpp confirms headless LuaBindings pattern
- Pitfalls: HIGH — null dereference pattern identified from direct code audit; hook callback NULL check for ar->name verified against Lua source; pitfall 2 (hook during lua_close) is documented in Lua 5.4 reference
- Null-safety map: HIGH — derived from direct source audit of bindings_engine.cpp with line references

**Research date:** 2026-03-08
**Valid until:** 2026-06-08 (Lua 5.4 API is stable; project bindings are stable)
