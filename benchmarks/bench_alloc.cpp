// bench_alloc.cpp — zero-allocation verification for enjin2 hot paths
// Exits 0 if all guarded operations are allocation-free; exits 1 on first detected allocation.
//
// REQUIREMENTS: ALLOC-01, ALLOC-02, ALLOC-03
//
// Structure:
//   1. Define ENJIN2_ALLOC_VERIFICATION and thread_local counters
//   2. Override all six operator new/delete forms (BEFORE any allocating includes)
//   3. Include alloc_guard.hpp (sees extern declarations)
//   4. Include engine headers
//   5. Setup section (allocations allowed)
//   6. Hot-path section (AllocGuard wraps each op; exit(1) on alloc)

// --- (1) Enable verification and define thread_local storage ---
#define ENJIN2_ALLOC_VERIFICATION 1

// These are defined here (NOT static) — alloc_guard.hpp declares them extern.
thread_local int  g_alloc_guard_depth = 0;
thread_local long g_alloc_count       = 0;

// --- (2) Override all six operator new/delete forms ---
// Must come before any includes that could call new (std::string, std::vector, etc.)
// This override is linked into the bench_alloc binary only (ENJIN2_ALLOC_VERIFICATION guard
// in the CMakeLists.txt ensures no leakage into other targets).

#include <cstdlib>    // std::malloc, std::free
#include <cstdio>     // fprintf
#include <new>        // std::bad_alloc, std::size_t

void* operator new(std::size_t size) {
    if (g_alloc_guard_depth > 0) {
        g_alloc_count++;
    }
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
}

// Sized delete (C++14) — required on GCC/Clang with -fsized-deallocation (enabled by default)
void operator delete(void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

// --- (3) Include AllocGuard header (uses the extern declarations above) ---
#include <enjin2/instrumentation/alloc_guard.hpp>

// --- (4) Include engine headers and nanobench ---
// Each is a separate binary so ANKERL_NANOBENCH_IMPLEMENT is safe here.
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/primitives.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/scripting/bindings.hpp>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// Minimal concrete scene — same pattern as bench_ecs.cpp
class BenchScene : public enjin2::Scene {
public:
    BenchScene() : enjin2::Scene(1) {}
};

int main() {
    // =========================================================================
    // SETUP SECTION — allocations are allowed here
    // =========================================================================

    // Canvas and sprite on the stack (no heap allocation for Canvas4 itself)
    enjin2::Canvas4<128, 128> canvas;
    enjin2::Canvas4<128, 128> sprite;
    sprite.clear(enjin2::Pixel4(5));

    // Scene with 8 objects — addObject() calls operator new (intentional, setup only)
    BenchScene scene;
    scene.activate();
    for (int i = 0; i < 8; ++i) {
        scene.addObject<enjin2::Object>();
    }

    // Lua engine — initialize() allocates the Lua state (intentional, setup only)
    enjin2::LuaEngine eng;
    eng.initialize();

    enjin2::LuaBindings bindings(&eng);
    bindings.registerAll();

    // Pre-register engine.time.delta as a Lua registry reference for zero-alloc calling.
    // Calling via lua_rawgeti + lua_call does NOT parse Lua source (no luaL_loadstring).
    // executeString() allocates for chunk parsing — never use it inside an AllocGuard.
    lua_State* L = eng.getState();
    lua_getglobal(L, "engine");          // push engine table
    lua_getfield(L, -1, "time");         // push engine.time subtable
    lua_getfield(L, -1, "delta");        // push engine.time.delta C function
    int deltaRef = luaL_ref(L, LUA_REGISTRYINDEX);  // store ref, pops delta
    lua_pop(L, 2);                       // pop time, engine

    // Reset counter — setup allocations must not bleed into guarded sections
    g_alloc_count = 0;

    // =========================================================================
    // HOT PATH SECTION — each operation must be allocation-free
    // AllocGuard prints diagnostic and calls exit(1) if operator new fires.
    // =========================================================================

    // --- Canvas pixel operations (ALLOC-03 requirement 1) ---
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
    // scene.update() uses raw index loop — no std::function, no heap allocation
    {
        enjin2::AllocGuard g("scene::update x8 objects");
        scene.update(0.016f);
        ankerl::nanobench::doNotOptimizeAway(scene.getObjects().size());
    }

    // --- Lua binding call (ALLOC-03 requirement 3) ---
    // Call engine.time.delta via pre-registered registry reference.
    // lua_rawgeti + lua_call does NOT allocate — it only invokes the C function directly.
    // Note: Lua's internal allocator (l_alloc/realloc) is NOT caught by operator new override;
    // this test verifies the C++ binding code itself does not call operator new.
    {
        enjin2::AllocGuard g("lua binding: engine.time.delta call");
        lua_rawgeti(L, LUA_REGISTRYINDEX, deltaRef);  // push C function (no alloc)
        lua_call(L, 0, 1);                            // call with 0 args, 1 result
        ankerl::nanobench::doNotOptimizeAway(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }

    // =========================================================================
    // CLEANUP
    // =========================================================================

    luaL_unref(L, LUA_REGISTRYINDEX, deltaRef);
    eng.shutdown();

    fprintf(stdout, "[ALLOC-PASS] All hot-path allocation checks passed\n");
    return 0;
}
