/**
 * @file bindings_tween.cpp
 * @brief engine.tween.* Lua sub-table — tween animation pool (Phase 50: TWEEN-01..TWEEN-03)
 *
 * Implements an 8-slot fixed tween pool with zero dynamic allocation.
 * Tweens animate Lua table fields (numbers) from a start value to an end value
 * over a given duration using one of four inline easing functions.
 * All easing uses only multiply/add — no std::pow, no libm calls.
 *
 * API (from Lua):
 *   id = engine.tween.to(target, {props}, duration, easing, done_cb)
 *         -- Animate target table fields; returns integer ID or nil if pool full
 *   engine.tween.cancel(id)      -- Cancel tween by ID; leaves value at current position
 *   engine.tween.cancelAll()     -- Cancel all active tweens
 */
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/bind_helpers.hpp"

#include <cstdio>
#include <cstring>

namespace enjin2 {

// ── Private helper: clear a single tween slot and unref target + done_cb ──────
template<typename Slot>
static void clearTweenSlot(Slot& slot, lua_State* L) {
    if (L) {
        if (slot.targetRef != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, slot.targetRef);
        }
        if (slot.doneCbRef != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, slot.doneCbRef);
        }
    }
    slot.targetRef  = LUA_NOREF;
    slot.doneCbRef  = LUA_NOREF;
    slot.propCount  = 0;
    slot.elapsed    = 0.0f;
    slot.id         = 0;
    slot.active     = false;
}

// ── Inline easing functions — multiply/add only, NO std::pow, NO libm ─────────
// Easing codes match TweenEasing enum (0=Linear,1=EaseIn,2=EaseOut,3=EaseInOut)
static inline float tweenEase(float t, uint8_t easingCode) {
    switch (easingCode) {
        case 1:  // EaseIn: quadratic
            return t * t;
        case 2:  // EaseOut: reverse quadratic
            return 1.0f - (1.0f - t) * (1.0f - t);
        case 3:  // EaseInOut: smoothstep
            return t * t * (3.0f - 2.0f * t);
        case 0:  // Linear
        default:
            return t;
    }
}

// ── TWEEN-01: engine.tween.to(target, props, duration, easing, done_cb) ───────
// Allocates a tween slot, samples start values, anchors refs.
// Returns integer ID on success, nil if pool full (no Lua error raised).
int LuaBindings::lua_engine_tween_to(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);   // target table
    luaL_checktype(L, 2, LUA_TTABLE);   // props table
    float duration = static_cast<float>(luaL_checknumber(L, 3));  // duration in seconds
    const char* easingStr = luaL_optstring(L, 4, "linear");        // easing name
    // arg 5: optional done callback (lua_isfunction check below)

    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) { lua_pushnil(L); return 1; }

    // Find a free slot via linear scan
    int freeIdx = -1;
    for (int i = 0; i < TWEEN_POOL_SIZE; ++i) {
        if (!b->m_tweenPool[i].active) {
            freeIdx = i;
            break;
        }
    }
    if (freeIdx < 0) {
        // Pool full — return nil per spec (no Lua error)
        lua_pushnil(L);
        return 1;
    }

    TweenSlot& slot = b->m_tweenPool[freeIdx];

    // Clamp duration: allow 0 for instant completion on next tick
    if (duration < 0.0f) duration = 0.0f;

    // Anchor target table in registry
    lua_pushvalue(L, 1);
    slot.targetRef = luaL_ref(L, LUA_REGISTRYINDEX);

    // Anchor done_cb if provided
    if (lua_isfunction(L, 5)) {
        lua_pushvalue(L, 5);
        slot.doneCbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        slot.doneCbRef = LUA_NOREF;
    }

    // Parse easing string
    TweenEasing easing = TweenEasing::Linear;
    if (strcmp(easingStr, "easeIn") == 0) {
        easing = TweenEasing::EaseIn;
    } else if (strcmp(easingStr, "easeOut") == 0) {
        easing = TweenEasing::EaseOut;
    } else if (strcmp(easingStr, "easeInOut") == 0) {
        easing = TweenEasing::EaseInOut;
    }
    // "linear" and unknown strings default to Linear

    // Iterate props table; sample start values from target table
    int propCount = 0;
    lua_pushnil(L);  // initial key for lua_next
    while (lua_next(L, 2) != 0) {
        // stack: ..., key, value
        if (propCount >= TWEEN_MAX_PROPS) {
            // Skip remaining props to avoid stack imbalance
            lua_pop(L, 2);
            break;
        }

        // Only process string keys with number values
        if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TNUMBER) {
            const char* key = lua_tostring(L, -2);
            float endVal    = static_cast<float>(lua_tonumber(L, -1));

            // Copy key into slot (truncate to TWEEN_KEY_MAX - 1)
            strncpy(slot.keys[propCount], key, TWEEN_KEY_MAX - 1);
            slot.keys[propCount][TWEEN_KEY_MAX - 1] = '\0';

            // Sample start value from target table
            lua_rawgeti(L, LUA_REGISTRYINDEX, slot.targetRef);  // push target
            lua_getfield(L, -1, key);                            // push target[key]
            float startVal = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 0.0f;
            lua_pop(L, 2);  // pop target[key] + target

            slot.startVals[propCount] = startVal;
            slot.endVals[propCount]   = endVal;
            ++propCount;
        }

        lua_pop(L, 1);  // pop value; keep key for next iteration
    }

    slot.propCount = propCount;
    slot.elapsed   = 0.0f;
    slot.duration  = duration;
    slot.easing    = easing;
    slot.id        = ++b->m_nextTweenId;
    slot.active    = true;

    lua_pushinteger(L, static_cast<lua_Integer>(slot.id));
    return 1;
}

// ── TWEEN-02: engine.tween.cancel(id) ─────────────────────────────────────────
// Cancels a tween by ID. Leaves value at current interpolated position.
// Does NOT snap to end, does NOT fire done_cb. Silent no-op for unknown IDs.
int LuaBindings::lua_engine_tween_cancel(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;

    int cancelId = static_cast<int>(luaL_checkinteger(L, 1));
    lua_State* mainL = b->engine->getState();

    for (int i = 0; i < TWEEN_POOL_SIZE; ++i) {
        TweenSlot& slot = b->m_tweenPool[i];
        if (slot.active && slot.id == cancelId) {
            clearTweenSlot(slot, mainL);
            break;
        }
    }
    return 0;
}

// ── TWEEN-02: engine.tween.cancelAll() ────────────────────────────────────────
// Cancels all active tweens. Resets the ID counter.
int LuaBindings::lua_engine_tween_cancelAll(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;

    lua_State* mainL = b->engine->getState();
    for (int i = 0; i < TWEEN_POOL_SIZE; ++i) {
        if (b->m_tweenPool[i].active) {
            clearTweenSlot(b->m_tweenPool[i], mainL);
        }
    }
    b->m_nextTweenId = 0;
    return 0;
}

// ── Per-frame tick: advance tweens and interpolate properties ─────────────────
void LuaBindings::tickTweens(float dt) {
    if (!engine) return;
    lua_State* L = engine->getState();
    if (!L) return;

    for (int i = 0; i < TWEEN_POOL_SIZE; ++i) {
        TweenSlot& slot = m_tweenPool[i];
        if (!slot.active) continue;

        slot.elapsed += dt;

        // Compute normalized time t; guard against zero duration
        float t = (slot.duration <= 0.0f) ? 1.0f : (slot.elapsed / slot.duration);
        if (t > 1.0f) t = 1.0f;

        float easedT = tweenEase(t, static_cast<uint8_t>(slot.easing));

        // Push target table from registry
        lua_rawgeti(L, LUA_REGISTRYINDEX, slot.targetRef);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            clearTweenSlot(slot, L);
            continue;
        }

        // Interpolate and write each property
        for (int p = 0; p < slot.propCount; ++p) {
            float val = slot.startVals[p] + (slot.endVals[p] - slot.startVals[p]) * easedT;
            lua_pushnumber(L, static_cast<lua_Number>(val));
            lua_setfield(L, -2, slot.keys[p]);
        }

        lua_pop(L, 1);  // pop target table

        // Check if tween has completed
        if (t >= 1.0f) {
            // Fire done_cb if provided
            if (slot.doneCbRef != LUA_NOREF) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, slot.doneCbRef);
                if (lua_isfunction(L, -1)) {
                    if (lua_pcall(L, 0, 0, 0) != 0) {
                        const char* err = lua_tostring(L, -1);
                        fprintf(stderr, "[tween done_cb error] %s\n", err ? err : "(unknown)");
                        lua_pop(L, 1);  // pop error message
                    }
                } else {
                    lua_pop(L, 1);  // pop non-function
                }
            }
            clearTweenSlot(slot, L);
        }
    }
}

// ── clearTweens: cancel all tweens, reset ID counter ──────────────────────────
// Called from registerAll() (hot-reload) and setActiveScene() (scene transition).
void LuaBindings::clearTweens() {
    lua_State* L = engine ? engine->getState() : nullptr;
    for (int i = 0; i < TWEEN_POOL_SIZE; ++i) {
        if (m_tweenPool[i].active) {
            clearTweenSlot(m_tweenPool[i], L);
        }
    }
    m_nextTweenId = 0;
}

// ── registerTweenSubtable: engine.tween.* (called from registerEngineTable) ───
void LuaBindings::registerTweenSubtable(lua_State* L) {
    static const LuaFuncDef kTweenFuncs[] = {
        {"to",        lua_engine_tween_to},
        {"cancel",    lua_engine_tween_cancel},
        {"cancelAll", lua_engine_tween_cancelAll},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kTweenFuncs, ENJIN_ARRAY_LEN(kTweenFuncs));
    lua_setfield(L, -2, "tween");
}

} // namespace enjin2
