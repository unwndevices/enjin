/**
 * @file bindings_async.cpp
 * @brief engine.async.* Lua sub-table — coroutine scheduler (Phase 49: ASYNC-01..ASYNC-03)
 *
 * Implements an 8-slot fixed coroutine pool with zero dynamic allocation.
 * Coroutines are resumed via lua_resume OUTSIDE any pcall scope to avoid
 * yield-across-pcall boundary errors. Supports start/wait/cancel/cancelAll.
 *
 * API (from Lua):
 *   id = engine.async.start(fn)         -- Start coroutine, returns integer ID or nil if pool full
 *   engine.async.wait(seconds)           -- Yield current coroutine for N seconds
 *   engine.async.cancel(id)              -- Cancel coroutine by ID
 *   engine.async.cancelAll()             -- Cancel all active coroutines
 */
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/bind_helpers.hpp"

#include <cstdio>


namespace enjin2 {

// ── Private helper: clear a single slot and unref its thread ─────────────────
// Uses template so it works with the private CoroutineSlot type.
template<typename Slot>
static void clearSlot(Slot& slot, lua_State* L) {
    if (L && slot.threadRef != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, slot.threadRef);
    }
    slot.threadRef     = LUA_NOREF;
    slot.waitRemaining = 0.0f;
    slot.waitFrames    = 0;          // Phase 57: QOL-02
    slot.waitTweenId   = 0;          // Phase 57: QOL-01
    slot.id            = 0;
    slot.active        = false;
}

// ── ASYNC-01: engine.async.start(fn) ────────────────────────────────────────
// Creates a new coroutine from fn and schedules it for this frame.
// Returns an integer ID on success, or nil if the pool is full (no Lua error).
int LuaBindings::lua_engine_async_start(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);

    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) { lua_pushnil(L); return 1; }

    // Find a free slot via linear scan
    int freeIdx = -1;
    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        if (!b->m_coroutinePool[i].active) {
            freeIdx = i;
            break;
        }
    }
    if (freeIdx < 0) {
        // Pool full — return nil per plan spec (no Lua error)
        lua_pushnil(L);
        return 1;
    }

    CoroutineSlot& slot = b->m_coroutinePool[freeIdx];

    // Create a new coroutine thread
    lua_State* co = lua_newthread(L);           // [fn, thread]
    // Move the function from the calling thread into the new thread
    lua_pushvalue(L, 1);                        // [fn, thread, fn]
    lua_xmove(L, co, 1);                        // co=[fn]; L=[fn, thread]
    // Anchor the thread in the registry to prevent GC
    slot.threadRef     = luaL_ref(L, LUA_REGISTRYINDEX); // L=[fn]
    slot.waitRemaining = 0.0f;   // runs immediately this tick
    slot.id            = ++b->m_nextCoroutineId;
    slot.active        = true;

    lua_pushinteger(L, static_cast<lua_Integer>(slot.id));
    return 1;
}

// ── ASYNC-02: engine.async.wait(seconds) ────────────────────────────────────
// Yields the current coroutine for the given number of seconds.
// Must be called from within a coroutine (yieldable context).
// Negative values are clamped to 0.
int LuaBindings::lua_engine_async_wait(lua_State* L) {
    if (!lua_isyieldable(L)) {
        luaL_error(L, "engine.async.wait() called outside a coroutine");
        return 0;
    }

    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) {
        return lua_yield(L, 0);
    }

    double seconds = luaL_optnumber(L, 1, 0.0);
    if (seconds < 0.0) seconds = 0.0;

    // Find the calling coroutine in the pool by comparing lua_State pointers
    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        CoroutineSlot& slot = b->m_coroutinePool[i];
        if (!slot.active || slot.threadRef == LUA_NOREF) continue;

        // Retrieve the thread from the registry and compare
        lua_rawgeti(L, LUA_REGISTRYINDEX, slot.threadRef);
        lua_State* co = lua_tothread(L, -1);
        lua_pop(L, 1);

        if (co == L) {
            slot.waitFrames    = 0;  // mutual exclusion: clear frame-wait [Phase 57: QOL-02]
            slot.waitRemaining = static_cast<float>(seconds);
            break;
        }
    }

    return lua_yield(L, 0);
}

// ── ASYNC-02: engine.async.cancel(id) ────────────────────────────────────────
// Cancels a coroutine by its integer ID. Silent no-op for unknown IDs.
int LuaBindings::lua_engine_async_cancel(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;

    int cancelId = static_cast<int>(luaL_checkinteger(L, 1));
    lua_State* mainL = b->engine->getState();

    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        CoroutineSlot& slot = b->m_coroutinePool[i];
        if (slot.active && slot.id == cancelId) {
            clearSlot(slot, mainL);
            break;
        }
    }
    return 0;
}

// ── ASYNC-03: engine.async.cancelAll() ──────────────────────────────────────
// Cancels all active coroutines. Resets the ID counter.
int LuaBindings::lua_engine_async_cancelAll(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;

    lua_State* mainL = b->engine->getState();
    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        if (b->m_coroutinePool[i].active) {
            clearSlot(b->m_coroutinePool[i], mainL);
        }
    }
    b->m_nextCoroutineId = 0;
    return 0;
}

// ── Per-frame tick: resume ready coroutines ──────────────────────────────────
void LuaBindings::tickCoroutines(float dt) {
    if (!engine) return;
    lua_State* L = engine->getState();
    if (!L) return;

    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        CoroutineSlot& slot = m_coroutinePool[i];
        if (!slot.active) continue;

        // Wait-tween gate: coroutine is suspended waiting for a tween to complete.
        // The resume will fire from tickTweens when the tween completes — skip here.
        if (slot.waitTweenId != 0) continue;  // Phase 57: QOL-01

        // Frame-first dual-mode wait check (Phase 57: QOL-02)
        // Frame check MUST come before time check (mutual exclusion).
        if (slot.waitFrames > 0) {
            --slot.waitFrames;
            if (slot.waitFrames > 0) continue;  // still waiting — skip resume
            // fell through: waitFrames just hit 0, resume this frame
        } else if (slot.waitRemaining > 0.001f) {
            // Time-based wait — use 0.001f epsilon to handle float accumulation
            slot.waitRemaining -= dt;
            if (slot.waitRemaining > 0.001f) continue;  // still waiting
        }

        // Retrieve coroutine thread from registry
        lua_rawgeti(L, LUA_REGISTRYINDEX, slot.threadRef);
        lua_State* co = lua_tothread(L, -1);
        lua_pop(L, 1);

        if (!co) {
            // Orphaned ref — clear slot
            clearSlot(slot, L);
            continue;
        }

        // Resume coroutine — Lua 5.4 unconditional API
        int nres = 0;
        int status = lua_resume(co, L, 0, &nres);
        if (nres > 0) lua_pop(co, nres);

        if (status == LUA_YIELD) {
            // Coroutine yielded — for time-based waits subtract this frame's dt
            // so the current tick counts toward the wait duration.
            // (Without this, the first tick after a wait() wastes a full frame.)
            if (slot.waitFrames == 0) {
                slot.waitRemaining -= dt;
            }
        } else if (status == LUA_OK) {
            // Coroutine finished normally
            clearSlot(slot, L);
        } else {
            // Coroutine errored
            const char* err = lua_tostring(co, -1);
            fprintf(stderr, "[async error] %s\n", err ? err : "(unknown)");
            clearSlot(slot, L);
        }
    }
}

// ── clearCoroutines: cancel all, reset ID counter ─────────────────────────────
// Called from registerAll() (hot-reload) and setActiveScene() (scene transition).
void LuaBindings::clearCoroutines() {
    lua_State* L = engine ? engine->getState() : nullptr;
    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        if (m_coroutinePool[i].active) {
            clearSlot(m_coroutinePool[i], L);
        }
    }
    m_nextCoroutineId = 0;
}

// ── Phase 57 QOL-02: engine.async.wait_frames(n) ────────────────────────────
// Yields the calling coroutine for exactly n frames before resuming.
// wait_frames(0) and wait_frames(-1) return immediately without yielding.
// Mutually exclusive with wait(seconds): setting waitFrames clears waitRemaining.
int LuaBindings::lua_engine_async_wait_frames(lua_State* L) {
    if (!lua_isyieldable(L)) {
        luaL_error(L, "engine.async.wait_frames() called outside a coroutine");
        return 0;
    }
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return lua_yield(L, 0);

    int n = static_cast<int>(luaL_optinteger(L, 1, 0));
    if (n <= 0) return 0;  // resume immediately — no yield

    // Find the calling coroutine in the pool and set waitFrames.
    // Use n-1 because the tick where the coroutine calls wait_frames() counts as frame 1.
    // Example: wait_frames(3) with tick counting:
    //   Tick 1: coroutine starts, calls wait_frames(3) → waitFrames=2, yields
    //   Tick 2: waitFrames 2→1, skip
    //   Tick 3: waitFrames 1→0, resume (exactly 3 ticks from start)
    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        CoroutineSlot& slot = b->m_coroutinePool[i];
        if (!slot.active || slot.threadRef == LUA_NOREF) continue;
        lua_rawgeti(L, LUA_REGISTRYINDEX, slot.threadRef);
        lua_State* co = lua_tothread(L, -1);
        lua_pop(L, 1);
        if (co == L) {
            slot.waitRemaining = 0.0f;  // mutual exclusion: clear time-wait
            slot.waitFrames    = n - 1; // n-1: current tick counts as frame 1
            break;
        }
    }
    return lua_yield(L, 0);
}

// ── registerAsyncSubtable: engine.async.* (called from registerEngineTable) ──
void LuaBindings::registerAsyncSubtable(lua_State* L) {
    static const LuaFuncDef kAsyncFuncs[] = {
        {"start",       lua_engine_async_start},
        {"wait",        lua_engine_async_wait},
        {"cancel",      lua_engine_async_cancel},
        {"cancelAll",   lua_engine_async_cancelAll},
        {"wait_frames", lua_engine_async_wait_frames},  // Phase 57: QOL-02
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kAsyncFuncs, ENJIN_ARRAY_LEN(kAsyncFuncs));
    lua_setfield(L, -2, "async");
}

} // namespace enjin2
