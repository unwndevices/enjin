/**
 * @file headless_main.cpp
 * @brief enjin_run — headless Lua profiling runner (no window, no SDL3).
 *
 * Phase 63 — PROF-04, PROF-05, PROF-06
 *
 * Usage:
 *   enjin_run [--profile] [--frames N] [--output json] <script.lua>
 *
 *   --profile        Enable call-count profiler (LuaProfiler::install)
 *   --frames N       Simulate N frames at 60 fps (default: 100)
 *   --output json    Print JSON array instead of text table (requires --profile)
 *   <script.lua>     Path to Lua script to execute
 *
 * Without --profile: lua_sethook(L, NULL, 0, 0) — confirmed zero-overhead (PROF-03).
 * With --profile: prints sorted call count table or JSON array to stdout.
 * GC pressure summary (min/max/avg byte delta) printed in text mode (PROF-02).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/layer_compositor.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_profiler.hpp>

// ── Static globals (mirrors sdl_main.cpp pattern — zero heap allocation) ───
static enjin2::LayerCompositor<ENJIN2_CANVAS_WIDTH, ENJIN2_CANVAS_HEIGHT> g_compositor;
static enjin2::LuaScriptSystem                                              g_lua;

// 5 LuaCanvas wrappers: 4 game layers (index 0-3) + 1 debug layer (index 4)
static enjin2::LuaCanvas g_lua_layer0(&g_compositor.layers[0]);
static enjin2::LuaCanvas g_lua_layer1(&g_compositor.layers[1]);
static enjin2::LuaCanvas g_lua_layer2(&g_compositor.layers[2]);
static enjin2::LuaCanvas g_lua_layer3(&g_compositor.layers[3]);
static enjin2::LuaCanvas g_lua_layer4(&g_compositor.layers[4]);  // debug layer

// Pointer array for setLayers() — 4 game layers only (debug is separate)
static enjin2::LuaCanvas* g_lua_layers[4] = {
    &g_lua_layer0,
    &g_lua_layer1,
    &g_lua_layer2,
    &g_lua_layer3,
};

// ── Usage ───────────────────────────────────────────────────────────────────
static void printUsage(const char* argv0) {
    fprintf(stderr,
            "Usage: %s [--profile] [--frames N] [--output json] <script.lua>\n"
            "\n"
            "  --profile         Enable Lua call-count profiler\n"
            "  --frames N        Simulate N frames at 60 fps (default: 100)\n"
            "  --output json     Output JSON array (requires --profile)\n"
            "  <script.lua>      Lua script to execute\n",
            argv0);
}

// ── main ────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // ── Argument parsing ─────────────────────────────────────────────────
    bool        do_profile  = false;
    int         frames      = 100;
    bool        output_json = false;
    const char* script      = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--profile") == 0) {
            do_profile = true;
        } else if (strcmp(argv[i], "--frames") == 0) {
            if (i + 1 < argc) {
                frames = atoi(argv[++i]);
                if (frames <= 0) {
                    fprintf(stderr, "error: --frames must be a positive integer\n");
                    return 1;
                }
            } else {
                fprintf(stderr, "error: --frames requires an argument\n");
                printUsage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                const char* fmt = argv[++i];
                if (strcmp(fmt, "json") == 0) {
                    output_json = true;
                } else {
                    fprintf(stderr, "error: --output only supports 'json'\n");
                    return 1;
                }
            } else {
                fprintf(stderr, "error: --output requires an argument\n");
                printUsage(argv[0]);
                return 1;
            }
        } else if (argv[i][0] != '-') {
            // Positional argument: script path
            script = argv[i];
        } else {
            fprintf(stderr, "error: unknown option: %s\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }

    if (!script) {
        fprintf(stderr, "error: no script provided\n");
        printUsage(argv[0]);
        return 1;
    }

    // ── LuaScriptSystem initialization ───────────────────────────────────
    if (!g_lua.initialize()) {
        fprintf(stderr, "error: LuaScriptSystem::initialize() failed\n");
        return 1;
    }

    // Wire layer canvases — CRITICAL: prevents null canvas dereference in draw calls
    g_lua.getBindings().setLayers(g_lua_layers, 4, g_compositor.visible);
    g_lua.getBindings().setDebugCanvas(&g_lua_layer4);

    // NOTE: setInput() is NOT called — currentInput remains nullptr.
    //       All engine.input.* bindings null-guard currentInput safely.

    // ── Profiler wiring (BEFORE loadScript so init() calls are counted) ──
    lua_State* L = g_lua.getEngine().getState();

    if (do_profile) {
        enjin2::LuaProfiler::get().reset();
        enjin2::LuaProfiler::get().install(L);
    } else {
        // PROF-03: explicit zero-overhead confirmation when profiler is disabled
        lua_sethook(L, nullptr, 0, 0);
    }

    // ── Script loading ───────────────────────────────────────────────────
    enjin2::LuaResult loadResult = g_lua.loadScript(script);
    if (!loadResult.success) {
        fprintf(stderr, "error: failed to load script '%s': %s\n",
                script, loadResult.error.c_str());
        g_lua.shutdown();
        return 1;
    }

    // ── Headless frame loop (mirrors sdl_main.cpp update/draw pattern) ──
    static constexpr float    dt          = 1.0f / 60.0f;
    float                     totalTime   = 0.0f;
    uint32_t                  frameCount  = 0;

    // GC ring buffer — track per-frame memory delta (PROF-02)
    static constexpr int GC_RING = 256;
    int32_t              gcDeltas[GC_RING]{};
    int                  gcHead = 0;

    bool frame_ok = true;

    for (int f = 0; f < frames && frame_ok; ++f) {
        totalTime += dt;
        g_lua.getBindings().setTimeState(dt, totalTime, frameCount++);

        // GC snapshot before
        int mem_before = lua_gc(L, LUA_GCCOUNT, 0) * 1024
                       + lua_gc(L, LUA_GCCOUNTB, 0);

        // ── Call update(self, dt) ────────────────────────────────────────
        lua_getglobal(L, "update");
        if (lua_isfunction(L, -1)) {
            lua_pushnil(L);                              // self = nil
            lua_pushnumber(L, static_cast<lua_Number>(dt));
            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                fprintf(stderr, "[lua error] update: %s\n", err ? err : "unknown");
                lua_pop(L, 1);
                frame_ok = false;
            }
        } else {
            lua_pop(L, 1);  // not a function — that's fine
        }

        // Tick subsystems (mirrors SDL runner post-update order)
        if (frame_ok) {
            g_lua.getBindings().tickCameraFollow(dt);
            g_lua.getBindings().tickCoroutines(dt);
            g_lua.getBindings().tickTweens(dt);
        }

        // ── Call draw(self) ──────────────────────────────────────────────
        if (frame_ok) {
            lua_getglobal(L, "draw");
            if (lua_isfunction(L, -1)) {
                lua_pushnil(L);  // self = nil
                if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L, -1);
                    fprintf(stderr, "[lua error] draw: %s\n", err ? err : "unknown");
                    lua_pop(L, 1);
                    frame_ok = false;
                }
            } else {
                lua_pop(L, 1);
            }
        }

        // GC snapshot after — record delta if ring not full
        if (gcHead < GC_RING) {
            int mem_after = lua_gc(L, LUA_GCCOUNT, 0) * 1024
                          + lua_gc(L, LUA_GCCOUNTB, 0);
            gcDeltas[gcHead++] = mem_after - mem_before;
        }
    }

    // ── Cleanup ──────────────────────────────────────────────────────────
    // Uninstall hook BEFORE shutdown (prevents hook firing during lua_close GC sweep)
    if (do_profile) {
        enjin2::LuaProfiler::get().uninstall(L);
    }

    g_lua.shutdown();

    // ── Output ───────────────────────────────────────────────────────────
    if (do_profile) {
        if (output_json) {
            enjin2::LuaProfiler::get().printJSON();
        } else {
            enjin2::LuaProfiler::get().printTable();

            // GC pressure summary (PROF-02)
            if (gcHead > 0) {
                int32_t gcMin = gcDeltas[0];
                int32_t gcMax = gcDeltas[0];
                int64_t gcSum = 0;
                for (int i = 0; i < gcHead; ++i) {
                    if (gcDeltas[i] < gcMin) gcMin = gcDeltas[i];
                    if (gcDeltas[i] > gcMax) gcMax = gcDeltas[i];
                    gcSum += gcDeltas[i];
                }
                int32_t gcAvg = static_cast<int32_t>(gcSum / gcHead);
                printf("\nGC pressure (%d frames): min=%+d  max=%+d  avg=%+d bytes/frame\n",
                       gcHead, gcMin, gcMax, gcAvg);
            }
        }
    }

    return frame_ok ? 0 : 1;
}
