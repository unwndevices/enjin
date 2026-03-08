/**
 * @file lua_profiler.hpp
 * @brief Header-only Lua call-count profiler using lua_sethook (C-level debug hook).
 *
 * Phase 63 — PROF-01, PROF-02, PROF-03
 *
 * Usage:
 *   // Install before loadScript so init() calls are counted
 *   enjin2::LuaProfiler::get().reset();
 *   enjin2::LuaProfiler::get().install(L);
 *
 *   // ... run Lua frames ...
 *
 *   // Uninstall before shutdown (PROF-03: confirmed zero-overhead disabled path)
 *   enjin2::LuaProfiler::get().uninstall(L);
 *   enjin2::LuaProfiler::get().printTable();
 *
 * Zero-overhead disabled path: When profiler is not active,
 * lua_sethook(L, NULL, 0, 0) is used — confirmed by Lua 5.4 reference
 * to produce zero hook overhead.
 */
#pragma once

#include "lua_platform.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace enjin2 {

/**
 * @brief Fixed-capacity Lua call-count profiler — Meyer's singleton, zero heap allocation.
 *
 * Intercepts every Lua function call via lua_sethook(LUA_MASKCALL) and tracks
 * per-function call counts in a fixed-size table keyed by function pointer.
 *
 * Capacity: 256 distinct functions maximum (sufficient for any game script).
 * Memory: all storage is inline (no dynamic allocation).
 * Thread safety: not thread-safe — call from Lua hook only (single-threaded Lua state).
 */
struct LuaProfiler {
    static constexpr int MAX_FUNCTIONS = 256;
    static constexpr int MAX_NAME_LEN  = 64;

    /**
     * @brief Per-function profiling entry.
     * Identity is keyed by ptr (lua_topointer result), which is stable
     * for the lifetime of the Lua state.
     */
    struct FuncEntry {
        const void* ptr{nullptr};           ///< Function identity (lua_topointer)
        char        name[MAX_NAME_LEN]{};   ///< Human-readable name (ar->name or "[?]")
        char        source[MAX_NAME_LEN]{}; ///< Source file (ar->short_src, truncated)
        int         line{0};                ///< Definition line (ar->linedefined)
        uint32_t    callCount{0};           ///< Accumulated call count for this function
    };

    FuncEntry entries[MAX_FUNCTIONS]{};
    int       entryCount{0};
    bool      active{false};

    /**
     * @brief Meyer's singleton — returns the single LuaProfiler instance.
     */
    static LuaProfiler& get() {
        static LuaProfiler s_instance;
        return s_instance;
    }

    /**
     * @brief Reset all profiling data and deactivate.
     * Call before install() to start a fresh profiling session.
     */
    void reset() {
        for (int i = 0; i < MAX_FUNCTIONS; ++i) {
            entries[i] = FuncEntry{};
        }
        entryCount = 0;
        active     = false;
    }

    /**
     * @brief Lua debug hook callback installed via lua_sethook.
     *
     * Called by Lua runtime for every LUA_MASKCALL event.
     * Identifies the function via lua_topointer and increments its counter.
     * Guards ar->name against NULL (C functions or anonymous Lua functions).
     *
     * PITFALL: lua_getinfo(L, "nSf", ar) MUST be called inside the hook to
     * populate ar->name, ar->short_src, ar->linedefined. These are NOT populated
     * by the hook event alone.
     *
     * PITFALL: The "f" option in lua_getinfo pushes the function on the stack.
     * Must pop it with lua_pop(L, 1) after extracting the pointer.
     *
     * @param L   Lua state
     * @param ar  Debug event info (only ar->event is populated by default)
     */
    static void hookCallback(lua_State* L, lua_Debug* ar) {
        if (ar->event != LUA_HOOKCALL) return;

        // Populate ar->name, ar->short_src, ar->linedefined; push function on stack
        lua_getinfo(L, "nSf", ar);
        const void* fptr = lua_topointer(L, -1);
        lua_pop(L, 1);  // pop the function pushed by "f" option

        LuaProfiler& p = LuaProfiler::get();
        if (!p.active) return;

        // Linear scan for existing entry (max 256 slots — fast in practice)
        for (int i = 0; i < p.entryCount; ++i) {
            if (p.entries[i].ptr == fptr) {
                p.entries[i].callCount++;
                return;
            }
        }

        // New function — register it if capacity allows
        if (p.entryCount < MAX_FUNCTIONS) {
            FuncEntry& e = p.entries[p.entryCount++];
            e.ptr        = fptr;
            e.callCount  = 1;
            e.line       = ar->linedefined;

            // Guard ar->name: NULL for C functions or anonymous Lua closures
            if (ar->name) {
                strncpy(e.name, ar->name, MAX_NAME_LEN - 1);
                e.name[MAX_NAME_LEN - 1] = '\0';
            } else {
                strncpy(e.name, "[?]", MAX_NAME_LEN - 1);
                e.name[MAX_NAME_LEN - 1] = '\0';
            }

            strncpy(e.source, ar->short_src, MAX_NAME_LEN - 1);
            e.source[MAX_NAME_LEN - 1] = '\0';
        }
    }

    /**
     * @brief Install the profiler hook on a Lua state.
     *
     * Sets active=true and registers hookCallback via lua_sethook(LUA_MASKCALL).
     * Install BEFORE loadScript so init() and module-level calls are counted.
     *
     * @param L Lua state to hook
     */
    void install(lua_State* L) {
        active = true;
        lua_sethook(L, hookCallback, LUA_MASKCALL, 0);
    }

    /**
     * @brief Uninstall the profiler hook from a Lua state.
     *
     * PROF-03: lua_sethook(L, NULL, 0, 0) is the documented Lua 5.4 API
     * to disable all hooks — confirmed zero-overhead path.
     *
     * Call BEFORE g_lua.shutdown() to avoid hook firing during GC cleanup.
     *
     * @param L Lua state to unhook
     */
    void uninstall(lua_State* L) {
        lua_sethook(L, NULL, 0, 0);  // PROF-03: confirmed zero-overhead disabled path
        active = false;
    }

    /**
     * @brief Sort entries descending by callCount (insertion sort, O(n^2) max 256 entries).
     */
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

    /**
     * @brief Print a sorted text table of profiling results to stdout.
     * Columns: Function (40), Calls (8), Line (6), Source.
     */
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

    /**
     * @brief Write profiling results as a JSON array to stdout.
     * Format: [{"name":"...","calls":N,"line":N,"source":"..."}, ...]
     */
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

    // Non-copyable singleton
    LuaProfiler(const LuaProfiler&)            = delete;
    LuaProfiler& operator=(const LuaProfiler&) = delete;

private:
    LuaProfiler() = default;
};

}  // namespace enjin2
