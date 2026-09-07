/**
 * @file bindings_spring.cpp
 * @brief engine.tween.spring — retargetable chrome springs (wayfinder #17 / spec #30)
 *
 * A stateful spring pool alongside the tween pool (@ref bindings_tween.cpp). Where a
 * tween is a stateless one-shot, a spring carries position + velocity, so a new
 * target inherits the in-flight velocity for free — three fast encoder detents make
 * one continuous arc instead of three queued animations (R1). The physics lives in
 * @ref enjin2::Spring (ui/spring.hpp): semi-implicit Euler, fixed-dt sub-stepped for
 * stability, settling below the integer-pixel round so a rested spring can never
 * dirty a tile.
 *
 * API (from Lua):
 *   id = engine.tween.spring(obj, key, target, {duration=, bounce=})
 *         -- Spring obj[key] toward target with Apple perceptual params.
 *         -- If a spring for the same (obj, key) is already live, RETARGET it
 *         --   (keep position + velocity) rather than starting a second one.
 *         -- Returns integer ID, or nil if the pool is full.
 *
 * Defaults are the 'pop' preset (0.34 s / bounce 0.55). Springs are chrome-only —
 * the world uses no springs (R3). Values are in **pixel space**: the settle
 * threshold (@ref kSpringPosEps = 0.5 px) is tuned to the integer-pixel round, so
 * spring a position/offset, not a 0..1 scale/opacity (those would snap early).
 *
 * Untrusted Lua input is clamped: duration to @ref kSpringMinDurationS (a shorter
 * period would push ω·dt past the integrator's stability bound) and bounce to
 * @ref kSpringMaxBounce (bounce ≥ 1 gives ζ ≤ 0 — a divergent, energy-adding spring).
 */
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/bind_helpers.hpp"

#include <cstring>

namespace enjin2 {

// ── Private helper: free a spring slot and unref its target table ─────────────
template<typename Slot>
static void clearSpringSlot(Slot& slot, lua_State* L) {
    if (L && slot.targetRef != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, slot.targetRef);
    }
    slot.targetRef = LUA_NOREF;
    slot.id        = 0;
    slot.active    = false;
}

// ── engine.tween.spring(obj, key, target, {duration, bounce}) ─────────────────
// Retarget an existing (obj, key) spring, or allocate a new one. Returns int ID
// or nil if the pool is full (no Lua error raised, matching engine.tween.to).
int LuaBindings::lua_engine_tween_spring(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);                          // target object table
    const char* key = luaL_checkstring(L, 2);                 // property key
    float target = static_cast<float>(luaL_checknumber(L, 3)); // rest position

    // Perceptual params default to the 'pop' preset; the opts table overrides.
    float duration = 0.34f;
    float bounce   = 0.55f;
    if (lua_istable(L, 4)) {
        lua_getfield(L, 4, "duration");
        if (lua_isnumber(L, -1)) duration = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 4, "bounce");
        if (lua_isnumber(L, -1)) bounce = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    // Clamp untrusted Lua input to the integrator's safe envelope: a too-short
    // duration diverges (ω·dt > 2 even sub-stepped); bounce ≥ 1 gives ζ ≤ 0, an
    // undamped/energy-adding spring that never settles and leaks its slot.
    if (duration < kSpringMinDurationS) duration = kSpringMinDurationS;
    if (bounce < 0.0f) bounce = 0.0f;
    if (bounce > kSpringMaxBounce) bounce = kSpringMaxBounce;
    const SpringParams params = springParamsFromPerceptual(duration, bounce);

    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) { lua_pushnil(L); return 1; }

    // R1: at most one live spring per (obj, key). If one exists, retarget it —
    // change the goal and adopt the new params, but KEEP position + velocity.
    for (int i = 0; i < SPRING_POOL_SIZE; ++i) {
        SpringSlot& s = b->m_springPool[i];
        if (!s.active || strcmp(s.key, key) != 0) continue;
        lua_rawgeti(L, LUA_REGISTRYINDEX, s.targetRef);  // push stored target table
        const bool sameObj = lua_rawequal(L, 1, -1);
        lua_pop(L, 1);
        if (sameObj) {
            s.spring.setParams(params);
            s.spring.retarget(target);
            lua_pushinteger(L, static_cast<lua_Integer>(s.id));
            return 1;
        }
    }

    // Otherwise allocate a free slot.
    int freeIdx = -1;
    for (int i = 0; i < SPRING_POOL_SIZE; ++i) {
        if (!b->m_springPool[i].active) { freeIdx = i; break; }
    }
    if (freeIdx < 0) { lua_pushnil(L); return 1; }  // pool full — nil, no error

    SpringSlot& s = b->m_springPool[freeIdx];

    lua_pushvalue(L, 1);
    s.targetRef = luaL_ref(L, LUA_REGISTRYINDEX);
    strncpy(s.key, key, TWEEN_KEY_MAX - 1);
    s.key[TWEEN_KEY_MAX - 1] = '\0';

    // Start the spring from the field's current value (so it eases out of wherever
    // the property already sits), at rest.
    lua_getfield(L, 1, key);
    const float startVal = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 0.0f;
    lua_pop(L, 1);

    s.spring = Spring{};
    s.spring.x = startVal;
    s.spring.v = 0.0f;
    s.spring.setParams(params);
    s.spring.retarget(target);
    s.id     = ++b->m_nextSpringId;
    s.active = true;

    lua_pushinteger(L, static_cast<lua_Integer>(s.id));
    return 1;
}

// ── Per-frame tick: fixed-dt sub-stepped integrate, write back, settle+free ───
void LuaBindings::tickSprings(float dt) {
    if (!engine) return;
    lua_State* L = engine->getState();
    if (!L) return;

    // Drain real frame time in fixed 30 fps chunks, each integrated in N sub-steps
    // (semi-implicit Euler diverges at ω·dt > 2 — the sub-step keeps it stable).
    m_springAccumulator += dt;
    if (m_springAccumulator > kSpringMaxAccum) m_springAccumulator = kSpringMaxAccum;

    bool stepped = false;
    while (m_springAccumulator >= kSpringFixedDt) {
        for (int i = 0; i < SPRING_POOL_SIZE; ++i) {
            SpringSlot& s = m_springPool[i];
            if (!s.active) continue;
            for (int n = 0; n < kSpringSubSteps; ++n) s.spring.step(kSpringSubDt);
        }
        m_springAccumulator -= kSpringFixedDt;
        stepped = true;
    }
    if (!stepped) return;  // no fixed step elapsed — leave the fields untouched

    for (int i = 0; i < SPRING_POOL_SIZE; ++i) {
        SpringSlot& s = m_springPool[i];
        if (!s.active) continue;

        // Below the pixel round -> snap to exact rest so no tile dirties after settle.
        const bool settled = s.spring.settled();
        if (settled) s.spring.snapToRest();

        lua_rawgeti(L, LUA_REGISTRYINDEX, s.targetRef);
        if (!lua_istable(L, -1)) {   // target table gone — drop the spring
            lua_pop(L, 1);
            clearSpringSlot(s, L);
            continue;
        }
        lua_pushnumber(L, static_cast<lua_Number>(s.spring.x));
        lua_setfield(L, -2, s.key);
        lua_pop(L, 1);

        if (settled) clearSpringSlot(s, L);  // free the slot once at rest
    }
}

// ── clearSprings: free all springs, reset ID counter and accumulator ──────────
// Called from registerAll() (hot-reload) and setActiveScene() (scene transition).
void LuaBindings::clearSprings() {
    lua_State* L = engine ? engine->getState() : nullptr;
    for (int i = 0; i < SPRING_POOL_SIZE; ++i) {
        if (m_springPool[i].active) clearSpringSlot(m_springPool[i], L);
    }
    m_nextSpringId      = 0;
    m_springAccumulator = 0.0f;
}

} // namespace enjin2
