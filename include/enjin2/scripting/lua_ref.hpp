#pragma once
//
// enjin2::lua::ScriptRef: the one way to hold a Lua value across frames
// (Tomodachi #481, docs/one-way.md). A move-only RAII owner of one registry
// reference (luaL_ref / luaL_unref), for a callback, a coroutine thread, a
// tween target or any value C++ keeps while Lua code runs. Nothing else in
// enjin or libtomo calls luaL_ref / luaL_unref.
//
//   assign(L, idx)   capture the value at stack index `idx` (not popped),
//                    releasing any value held before.
//   reset()          release the held value and clear (the destructor does).
//   forget()         drop the held ref WITHOUT releasing it: for when its Lua
//                    state was closed under it. Call it then, before the next
//                    assign() or reset(), or they would unref into a dead state.
//   valid()          whether a value is held.
//   push(L)          push the held value, or nil when nothing is held.
//   state()          the main thread of the state the held value lives in
//                    (nullptr if none).
//   mainThread(L)    L's main thread (L itself, or the state a coroutine runs in).
//
// Two counts, for tests (the bug class is a ref nobody releases):
//   liveCount()      ScriptRefs holding a value right now, in every state.
//                    Zero after an applet teardown: anything else is a holder
//                    that outlived its VM.
//   registryRefs(L)  the references live in L's registry, by whatever code
//                    took them. Equal to the ScriptRefs into L when every
//                    holder is a ScriptRef; more means a raw luaL_ref.

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <atomic>

namespace enjin2::lua {

class ScriptRef {
public:
    ScriptRef() = default;
    ~ScriptRef() { reset(); }

    ScriptRef(const ScriptRef&) = delete;
    ScriptRef& operator=(const ScriptRef&) = delete;

    ScriptRef(ScriptRef&& other) noexcept : L_(other.L_), ref_(other.ref_) {
        other.L_ = nullptr;
        other.ref_ = LUA_NOREF;
    }
    ScriptRef& operator=(ScriptRef&& other) noexcept {
        if (this != &other) {
            reset();
            L_ = other.L_;
            ref_ = other.ref_;
            other.L_ = nullptr;
            other.ref_ = LUA_NOREF;
        }
        return *this;
    }

    // L may be a coroutine's thread: the ref is kept against the state's main
    // thread, which lives as long as the registry does.
    void assign(lua_State* L, int idx) {
        reset();
        lua_pushvalue(L, idx);                  // copy the target to the top
        const int ref = luaL_ref(L, LUA_REGISTRYINDEX);  // pops it
        if (ref == LUA_REFNIL || ref == LUA_NOREF) return;  // nil holds nothing
        L_ = mainThread(L);
        ref_ = ref;
        s_live.fetch_add(1, std::memory_order_relaxed);
    }

    void reset() {
        if (ref_ != LUA_NOREF) {
            luaL_unref(L_, LUA_REGISTRYINDEX, ref_);
            s_live.fetch_sub(1, std::memory_order_relaxed);
        }
        L_ = nullptr;
        ref_ = LUA_NOREF;
    }

    void forget() {
        if (ref_ != LUA_NOREF) s_live.fetch_sub(1, std::memory_order_relaxed);
        L_ = nullptr;
        ref_ = LUA_NOREF;
    }

    bool valid() const { return ref_ != LUA_NOREF; }
    lua_State* state() const { return L_; }

    void push(lua_State* L) const {
        if (ref_ != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_);
        } else {
            lua_pushnil(L);
        }
    }

    static int liveCount() { return s_live.load(std::memory_order_relaxed); }

    // luaL_ref keeps L's references in the registry's array part, slots
    // freelist+1 .. #registry, with the released ones chained from
    // registry[freelist] (Lua 5.4.8's lauxlib). Live = taken - released.
    static int registryRefs(lua_State* L) {
        constexpr int kFreelist = LUA_RIDX_LAST + 1;
        if (lua_rawgeti(L, LUA_REGISTRYINDEX, kFreelist) == LUA_TNIL) {
            lua_pop(L, 1);
            return 0;                                // luaL_ref never ran
        }
        int next = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        int released = 0;
        while (next != 0) {
            ++released;
            lua_rawgeti(L, LUA_REGISTRYINDEX, next);
            next = static_cast<int>(lua_tointeger(L, -1));
            lua_pop(L, 1);
        }
        const int taken = static_cast<int>(lua_rawlen(L, LUA_REGISTRYINDEX)) - kFreelist;
        return taken - released;
    }

    static lua_State* mainThread(lua_State* L) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
        lua_State* main = lua_tothread(L, -1);
        lua_pop(L, 1);
        return main;
    }

private:
    lua_State* L_ = nullptr;
    int ref_ = LUA_NOREF;
    inline static std::atomic<int> s_live{0};
};

} // namespace enjin2::lua
