# Phase 50: Tween Helpers - Research

**Researched:** 2026-03-01
**Domain:** Fixed-slot C tween pool, Lua table field animation, inline easing functions, luaL_ref lifecycle
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Pool exhaustion policy:**
- 8 fixed tween slots
- When all slots are full, `engine.tween.to()` returns nil instead of an ID
- Scripts that care can check: `local id = engine.tween.to(...); if not id then ... end`
- No Lua error raised — consistent with coroutine pool and all other fixed pools in codebase
- No introspection API — keep API minimal

**Scene transition behavior:**
- Cancel all tweens on scene transition (`clearTweens()`)
- Same cleanup on hot-reload (F5)
- Clean slate per scene — prevents stale Lua refs to destroyed objects/tables

### Claude's Discretion

- TweenSlot struct layout (target ref, property keys, start/end values, elapsed, duration, easing fn, done_cb ref)
- How to read/write Lua table fields from C (`lua_getfield`/`lua_setfield` by string key)
- Easing function implementation (quadratic for easeIn/easeOut, cubic or Hermite for easeInOut)
- done_cb argument contract (what args callback receives)
- cancel-mid-tween behavior (snap to current value vs snap to target)

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TWEEN-01 | `engine.tween.to(target, {props}, duration, easing, done_cb)` animates Lua table fields | Fixed-slot pool pattern from coroutine/async scheduler (Phase 49); `luaL_ref` for target table and done_cb; `lua_getfield`/`lua_setfield` for field read/write; `math::lerp<float>` for interpolation |
| TWEEN-02 | `engine.tween.cancel(id)` and `engine.tween.cancelAll()` cleanup | `luaL_unref` for target + done_cb refs; same cancel-by-ID + cancel-all pattern as `engine.async.cancel/cancelAll`; proxy validity check before unref |
| TWEEN-03 | 4+ inline easing functions (linear, easeIn, easeOut, easeInOut) — no FPU-heavy math | All four expressible with integer-exponent multiplication only; `t*t` for easeIn, `1-(1-t)*(1-t)` for easeOut, smoothstep (`t*t*(3-2*t)`) for easeInOut — no `std::pow`, no `sqrtf` |
</phase_requirements>

---

## Summary

Phase 50 adds `engine.tween.*` as an 8-slot fixed-pool tween scheduler in `LuaBindings`, following the exact same structural pattern as the coroutine scheduler added in Phase 49. A `TweenSlot` struct holds: a Lua registry ref to the target table, up to N property key strings (or a small fixed-count array), per-property start/end float values, elapsed time, duration, an easing function pointer (or enum), and a done_cb registry ref. Each frame, `tickTweens(dt)` advances elapsed time, applies the easing function to compute `t`, calls `lua_getfield`/`lua_setfield` to write interpolated values into the Lua target table, and fires the done_cb when elapsed >= duration.

The critical Lua API interaction is `lua_getfield`/`lua_setfield` by string key for reading/writing Lua table fields from C. The target table must be anchored in the Lua registry via `luaL_ref` (exactly like the coroutine thread anchor) to prevent GC during the tween's lifetime. The done_cb function (optional) is likewise anchored. On tween completion or cancel, both refs must be `luaL_unref`'d. The proxy validity issue from camera follow (Phase 48) is a model: per-tick validity checks prevent use-after-free on destroyed tables.

Easing functions are expressible as pure inline float arithmetic with no `std::pow` and no FPU-heavy calls. linear: `t`; easeIn (quadratic): `t*t`; easeOut: `1-(1-t)*(1-t)`; easeInOut: the existing `math::smoothstep` formula `t*t*(3-2*t)` already implemented in `math.hpp`. All four are visually distinct motion curves satisfying TWEEN-03.

**Primary recommendation:** Mirror the Phase 49 coroutine scheduler exactly — new file `src/scripting/bindings_tween.cpp`, `TweenSlot[8]` as a private member of `LuaBindings`, `tickTweens(dt)` called after `tickCoroutines(dt)` in `sdl_main.cpp`, `clearTweens()` called alongside `clearCoroutines()` in `registerAll()` and `setActiveScene()`, sub-table registered in `registerEngineTable()` in `bindings_engine.cpp`.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua 5.4 C API (desktop) | 5.4.8 | `lua_getfield`, `lua_setfield`, `lua_rawgeti`, `luaL_ref`, `luaL_unref`, `lua_pcall` for done_cb | Already linked; all operations verified in codebase |
| LuaJIT 2.1 C API (WASM) | bundled | Same API surface; no version guards needed for tween (no coroutine-specific API used) | Already bundled |
| `enjin2::math::lerp<float>` | in-tree | Template lerp `a + (b-a)*t` — the interpolation backbone | `include/enjin2/core/math.hpp` lines 66-69; already included in bindings.hpp |
| `enjin2::math::smoothstep` | in-tree | Hermite easeInOut: `t*t*(3-2*t)` — already implemented | `include/enjin2/core/math.hpp` lines 102-105 |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `lua_pcall` for done_cb | Lua C API | Safe invocation of the completion callback | Fire after tween completes; must use `lua_pcall` (not bare `lua_call`) to isolate script errors from the C tick loop |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Inline easing functions | `std::pow(t, 2.0f)` | `std::pow` with float exponents calls libm FPU routines — ruled out by TWEEN-03; `t*t` is identical for quadratic and avoids libm entirely |
| `lua_getfield` string lookup per tick | Pre-interning keys at slot creation | Key strings can be cached as `luaL_ref` values (one extra ref per property) but adds complexity; simple string lookup per tick is acceptable for 8 slots * ≤4 properties at 30fps |
| 8 slots | More/fewer | Consistent with COROUTINE_POOL_SIZE = 8; user decision locked |

**Installation:** No new libraries — uses existing Lua dependency and in-tree `math.hpp`.

---

## Architecture Patterns

### Recommended Project Structure

```
src/scripting/bindings_tween.cpp     (NEW — TweenSlot pool + tick + clear + bindings)
include/enjin2/scripting/bindings.hpp (MODIFIED — TweenSlot struct, pool member, public methods)
src/scripting/bindings_engine.cpp    (MODIFIED — registerTweenSubtable call in registerEngineTable)
src/scripting/bindings.cpp           (MODIFIED — clearTweens() in registerAll + setActiveScene)
src/platform/sdl/sdl_main.cpp        (MODIFIED — tickTweens(dt) after tickCoroutines in game loop)
tests/tween_test.cpp                  (NEW — integration tests for TWEEN-01..TWEEN-03)
```

### Pattern 1: TweenSlot Struct Layout

**What:** Each slot holds everything needed to advance one tween independently per tick. Property keys are stored as fixed-length char arrays (no std::string) matching the project's zero-alloc constraint.

**When to use:** Always — this is the core data structure.

```cpp
// Source: modeled after CoroutineSlot in bindings.hpp (Phase 49) and C_Timer::TimerEntry
static constexpr int TWEEN_POOL_SIZE      = 8;
static constexpr int TWEEN_MAX_PROPS      = 4;   // max properties animated per tween
static constexpr int TWEEN_KEY_MAX        = 32;  // max key string length (matches codebase convention)

// Easing enum (Claude's Discretion — 4 variants, no FPU-heavy math)
enum class TweenEasing : uint8_t {
    Linear      = 0,
    EaseIn      = 1,   // quadratic: t*t
    EaseOut     = 2,   // 1-(1-t)*(1-t)
    EaseInOut   = 3,   // smoothstep: t*t*(3-2*t)
};

struct TweenSlot {
    int      targetRef{LUA_NOREF};   // luaL_ref for the target Lua table; LUA_NOREF = inactive
    int      doneCbRef{LUA_NOREF};   // luaL_ref for done callback (optional); LUA_NOREF = none
    char     keys[TWEEN_MAX_PROPS][TWEEN_KEY_MAX]{};  // property key strings
    float    startVals[TWEEN_MAX_PROPS]{};            // values at tween start
    float    endVals[TWEEN_MAX_PROPS]{};              // values at tween end
    int      propCount{0};           // number of active properties
    float    elapsed{0.0f};          // time elapsed since start (seconds)
    float    duration{1.0f};         // total tween duration (seconds)
    TweenEasing easing{TweenEasing::Linear};
    int      id{0};                  // monotonically increasing cancel ID returned to Lua
    bool     active{false};          // slot in use
};
```

### Pattern 2: Inline Easing Functions (TWEEN-03 — No FPU-heavy math)

**What:** Four pure inline float functions expressible as multiply/add only. No `std::pow`, no `sqrtf`, no libm calls.

**When to use:** Called in `tickTweens` per active slot per frame.

```cpp
// Source: TWEEN-03 requirement + math.hpp smoothstep (lines 102-105)
// All four produce visually distinct motion in [0,1] -> [0,1]

static inline float tweenEase(float t, TweenEasing mode) {
    switch (mode) {
        case TweenEasing::Linear:
            return t;                              // straight line
        case TweenEasing::EaseIn:
            return t * t;                         // accelerate from rest (quadratic)
        case TweenEasing::EaseOut:
            return 1.0f - (1.0f - t) * (1.0f - t);  // decelerate to rest (quadratic)
        case TweenEasing::EaseInOut:
            return t * t * (3.0f - 2.0f * t);    // smoothstep — identical to math::smoothstep(0,1,t)
    }
    return t;  // fallback
}
```

**Note:** `math::smoothstep(0, 1, t)` with t already in [0,1] reduces to `t*t*(3-2*t)` — no clamp needed when t is guaranteed clamped by tickTweens. The smoothstep formula satisfies easeInOut: starts slow, accelerates through middle, decelerates to end.

### Pattern 3: `engine.tween.to()` Binding — Slot Allocation + Ref Anchoring

**What:** Reads target table, props table, duration, easing string, and optional done_cb from Lua stack. Anchors target + done_cb in registry. Samples start values immediately from the target table.

**When to use:** In `lua_engine_tween_to` binding.

```cpp
// Source: coroutine slot allocation (bindings_async.cpp:54-89) + luaL_ref pattern
// Lua signature: engine.tween.to(target, {props}, duration, easing [, done_cb])
//   target:   Lua table with fields to animate
//   props:    table like {x=100, y=50}  -- end values
//   duration: number in seconds
//   easing:   string "linear"|"easeIn"|"easeOut"|"easeInOut"
//   done_cb:  optional function()

int LuaBindings::lua_engine_tween_to(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) { lua_pushnil(L); return 1; }

    luaL_checktype(L, 1, LUA_TTABLE);     // target
    luaL_checktype(L, 2, LUA_TTABLE);     // props
    float duration = static_cast<float>(luaL_checknumber(L, 3));
    const char* easingStr = luaL_checkstring(L, 4);
    // arg 5: optional done_cb (LUA_TFUNCTION or absent/nil)

    // Find free slot
    int freeIdx = -1;
    for (int i = 0; i < TWEEN_POOL_SIZE; ++i) {
        if (!b->m_tweenPool[i].active) { freeIdx = i; break; }
    }
    if (freeIdx < 0) { lua_pushnil(L); return 1; }  // pool full

    TweenSlot& slot = b->m_tweenPool[freeIdx];

    // Anchor target table in registry
    lua_pushvalue(L, 1);
    slot.targetRef = luaL_ref(L, LUA_REGISTRYINDEX);

    // Anchor done_cb if provided
    slot.doneCbRef = LUA_NOREF;
    if (lua_gettop(L) >= 5 && lua_isfunction(L, 5)) {
        lua_pushvalue(L, 5);
        slot.doneCbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    // Parse easing string
    slot.easing = TweenEasing::Linear;
    if (strcmp(easingStr, "easeIn") == 0)    slot.easing = TweenEasing::EaseIn;
    else if (strcmp(easingStr, "easeOut") == 0)  slot.easing = TweenEasing::EaseOut;
    else if (strcmp(easingStr, "easeInOut") == 0) slot.easing = TweenEasing::EaseInOut;

    // Parse props table: iterate key-value pairs, record keys + end values, sample start values
    slot.propCount = 0;
    lua_pushnil(L);  // first key
    while (lua_next(L, 2) != 0 && slot.propCount < TWEEN_MAX_PROPS) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TNUMBER) {
            const char* key = lua_tostring(L, -2);
            strncpy(slot.keys[slot.propCount], key, TWEEN_KEY_MAX - 1);
            slot.keys[slot.propCount][TWEEN_KEY_MAX - 1] = '\0';
            slot.endVals[slot.propCount] = static_cast<float>(lua_tonumber(L, -1));

            // Sample start value from target table
            lua_rawgeti(L, LUA_REGISTRYINDEX, slot.targetRef);
            lua_getfield(L, -1, key);
            slot.startVals[slot.propCount] = lua_isnumber(L, -1)
                ? static_cast<float>(lua_tonumber(L, -1)) : 0.0f;
            lua_pop(L, 2);  // pop target table + field value

            slot.propCount++;
        }
        lua_pop(L, 1);  // pop value, keep key
    }

    slot.elapsed  = 0.0f;
    slot.duration = (duration > 0.0f) ? duration : 0.0f;
    slot.id       = ++b->m_nextTweenId;
    slot.active   = true;

    lua_pushinteger(L, static_cast<lua_Integer>(slot.id));
    return 1;
}
```

### Pattern 4: `tickTweens(dt)` — Per-Frame Advance + Field Write

**What:** Called once per frame from SDL runner after `tickCoroutines(dt)`. Advances each active slot's elapsed time, computes `t`, applies easing, lerps each property, writes result to Lua table via `lua_setfield`. Fires done_cb via `lua_pcall` on completion.

**When to use:** Always called even when no tweens are active (fast short-circuit via `!slot.active`).

```cpp
// Source: tickCoroutines pattern (bindings_async.cpp:163-217) adapted for tween
void LuaBindings::tickTweens(float dt) {
    if (!engine) return;
    lua_State* L = engine->getState();
    if (!L) return;

    for (int i = 0; i < TWEEN_POOL_SIZE; ++i) {
        TweenSlot& slot = m_tweenPool[i];
        if (!slot.active) continue;

        slot.elapsed += dt;
        float t = (slot.duration > 0.0f)
            ? slot.elapsed / slot.duration : 1.0f;
        if (t > 1.0f) t = 1.0f;

        float easedT = tweenEase(t, slot.easing);

        // Write interpolated values to target table
        lua_rawgeti(L, LUA_REGISTRYINDEX, slot.targetRef);
        if (lua_istable(L, -1)) {
            for (int p = 0; p < slot.propCount; ++p) {
                float val = slot.startVals[p] + (slot.endVals[p] - slot.startVals[p]) * easedT;
                lua_pushnumber(L, static_cast<lua_Number>(val));
                lua_setfield(L, -2, slot.keys[p]);
            }
        }
        lua_pop(L, 1);  // pop target table

        if (t >= 1.0f) {
            // Tween complete — fire done_cb if present
            if (slot.doneCbRef != LUA_NOREF) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, slot.doneCbRef);
                if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L, -1);
                    fprintf(stderr, "[tween done_cb error] %s\n", err ? err : "(unknown)");
                    lua_pop(L, 1);
                }
            }
            clearTweenSlot(slot, L);
        }
    }
}
```

### Pattern 5: `clearTweenSlot` Helper — Unref Both Refs

**What:** Unrefs targetRef and doneCbRef, resets all slot fields. Mirrors `clearSlot` template in `bindings_async.cpp`.

```cpp
// Source: clearSlot template in bindings_async.cpp:41-49
static void clearTweenSlot(TweenSlot& slot, lua_State* L) {
    if (L) {
        if (slot.targetRef != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, slot.targetRef);
        if (slot.doneCbRef != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, slot.doneCbRef);
    }
    slot.targetRef = LUA_NOREF;
    slot.doneCbRef = LUA_NOREF;
    slot.propCount = 0;
    slot.elapsed   = 0.0f;
    slot.id        = 0;
    slot.active    = false;
}
```

### Pattern 6: `clearTweens()` — Hot-Reload / Scene Transition Cleanup

**What:** Unref all active slots' refs, deactivate all slots. Called from `registerAll()` and `setActiveScene()` exactly like `clearCoroutines()`.

```cpp
// Source: clearCoroutines pattern (bindings_async.cpp:219-229)
void LuaBindings::clearTweens() {
    lua_State* L = engine ? engine->getState() : nullptr;
    for (int i = 0; i < TWEEN_POOL_SIZE; ++i) {
        if (m_tweenPool[i].active)
            clearTweenSlot(m_tweenPool[i], L);
    }
    m_nextTweenId = 0;
}
```

**Placement in `registerAll()`:**

```cpp
// In bindings.cpp registerAll(), alongside existing cleanup lines 477-481:
m_eventBus.clearHandlers();
m_eventBus.setLuaState(L);
clearCoroutines();
clearTweens();     // TWEEN-02: clean slate on every hot-reload
```

**Placement in `setActiveScene()`:**

```cpp
// In bindings.cpp setActiveScene(), alongside existing cleanup lines 711-715:
m_eventBus.clearHandlers();
m_activeCamera = nullptr;
clearCoroutines();
clearTweens();     // TWEEN-02: clean slate on scene transition
```

### Pattern 7: `registerTweenSubtable` — Registration in `bindings_engine.cpp`

**What:** Creates `engine.tween.*` sub-table and attaches it to the engine table. Follows the exact same registration pattern as `registerAsyncSubtable` and `registerDebugSubtable`.

```cpp
// Source: registerAsyncSubtable in bindings_async.cpp:231-242
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
```

Called from `registerEngineTable()` in `bindings_engine.cpp` at the end of the engine table setup, alongside the async and debug sub-table calls:

```cpp
// In registerEngineTable(), after registerAsyncSubtable:
// --- engine.tween sub-table (Phase 50: TWEEN-01..TWEEN-03) ---
registerTweenSubtable(L);
```

### Pattern 8: `sdl_main.cpp` Tick Placement

**What:** `tickTweens(dt)` added immediately after `tickCoroutines(dt)` in the game loop — outside any `lua_pcall` scope.

```cpp
// In sdl_main.cpp, after tickCoroutines (line 333):
g_lua.getBindings().tickCameraFollow(dt);
g_lua.getBindings().tickCoroutines(dt);
g_lua.getBindings().tickTweens(dt);       // Phase 50: TWEEN-01
```

No pcall boundary issues: `tickTweens` uses `lua_pcall` internally for `done_cb` invocation, which is safe — `lua_pcall` from a plain C call (not from inside another `lua_pcall`) is always valid.

### Pattern 9: Cancel Bindings

**What:** `engine.tween.cancel(id)` and `engine.tween.cancelAll()` — direct analogs of async versions.

```cpp
// Source: lua_engine_async_cancel / cancelAll in bindings_async.cpp
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

int LuaBindings::lua_engine_tween_cancelAll(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;
    lua_State* mainL = b->engine->getState();
    for (int i = 0; i < TWEEN_POOL_SIZE; ++i) {
        if (b->m_tweenPool[i].active)
            clearTweenSlot(b->m_tweenPool[i], mainL);
    }
    b->m_nextTweenId = 0;
    return 0;
}
```

### Anti-Patterns to Avoid

- **Not anchoring the target table:** A Lua table passed as `target` has no GC root in C. Without `luaL_ref`, the GC can collect it mid-tween. Always anchor target immediately in `tween.to()`.
- **Dereferencing target after table destruction:** If a script destroys the table object, the `luaL_ref` still holds it alive — this is correct behavior (tween keeps the table rooted). No dangling pointer issue.
- **Calling `lua_call` for done_cb instead of `lua_pcall`:** A Lua error in `done_cb` without a pcall handler will propagate as a C++ exception or abort. Always use `lua_pcall` for user-supplied callbacks.
- **Using `std::pow` for easing:** `std::pow(t, 2.0)` or `std::pow(t, 3.0)` calls libm on ESP32. Use `t*t` and `t*t*t` directly — identical for positive integer exponents.
- **Not clearing refs on cancel:** Canceling a tween that has both `targetRef` and `doneCbRef` must `luaL_unref` both. The `clearTweenSlot` helper handles this and must be called for all cancellation paths (cancel-by-id, cancelAll, clearTweens, completion).
- **Iterating props table with `lua_next` across frames:** The props table is only needed at `tween.to()` registration time to capture start/end values. After that, C owns the values as floats — no need to keep the props table anchored.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Interpolation | Custom lerp | `enjin2::math::lerp<float>(a, b, t)` | Already in `math.hpp` line 67; template handles float correctly |
| easeInOut curve | Custom formula | `t*t*(3-2*t)` (smoothstep) | `math::smoothstep(0,1,t)` is identical; smoothstep is the standard cubic Hermite easing |
| Done callback invocation | `lua_call` or coroutine | `lua_pcall` | Isolates script errors; consistent with all other user callback sites in codebase |
| Table field access | Manual stack gymnastics | `lua_getfield(L, tableIdx, key)` + `lua_setfield` | Standard Lua C API; handles metatables, nil, etc. correctly |

**Key insight:** The project already has all math primitives needed. The tween system is purely an orchestration layer: schedule → tick → interpolate → write → fire callback.

---

## Common Pitfalls

### Pitfall 1: GC Collecting Target Table
**What goes wrong:** The Lua table passed as `target` is only referenced from a local variable in the calling Lua script. Once the calling function returns, the GC may collect the table, leaving the C slot with a dangling registry ref that now resolves to nil.
**Why it happens:** `luaL_ref` correctly prevents GC — this pitfall only occurs if `targetRef` is NOT stored (e.g., forgotten due to early return).
**How to avoid:** Always call `luaL_ref(L, LUA_REGISTRYINDEX)` for the target table before writing any other slot fields. Even on early-return paths (e.g., zero properties found), unref the target if you abort.
**Warning signs:** `lua_istable(L, -1)` returns false in `tickTweens` even though the script still holds the variable.

### Pitfall 2: `lua_next` Leaves Key on Stack if Loop Breaks Early
**What goes wrong:** When iterating the `props` table with `lua_next` to collect properties, if `propCount` reaches `TWEEN_MAX_PROPS` mid-iteration, the loop exits leaving the current key/value pair on the stack.
**Why it happens:** `lua_next` is stateful — breaking early without popping the key leaves the stack unbalanced.
**How to avoid:** After the while loop body that fills the last slot, pop the remaining key+value with `lua_pop(L, 2)` before breaking, or use `lua_settop` to restore stack to known state. Alternatively: collect all props first into a local array, then fill slots — but this requires a second pass.
**Warning signs:** Stack imbalance errors, incorrect slot counts, or crashes on subsequent Lua calls.

### Pitfall 3: done_cb Fires with Stale Table
**What goes wrong:** By the time a tween completes, the target table's Lua-side owner may have discarded it. The table still exists (anchored via `targetRef`) but its original context is gone. A `done_cb` that closes over the target table can safely access it.
**How to avoid:** This is by design — `luaL_ref` keeps the table alive. Document that `done_cb` may reference the target table and it will still be valid. The callback should not assume the table is still "in use" by game logic.
**Warning signs:** None — this is expected behavior. Note it in the API documentation.

### Pitfall 4: Double-Unref on Rapid Cancel + Complete Race
**What goes wrong:** If `cancel(id)` is called the same frame a tween completes, `clearTweenSlot` is called twice for the same slot — double `luaL_unref` on the same refs, causing LUA_NOREF → LUA_NOREF (safe if checked) or double-free if not checked.
**Why it happens:** `tickTweens` completes the slot and calls `clearTweenSlot`, which sets refs to `LUA_NOREF`. If `cancel` is processed before the tick (in the same frame), the slot would be cleared before `tickTweens` runs — no double-unref. But if tick and cancel happen in the same frame depending on call ordering, validate with `slot.active` check.
**How to avoid:** `cancel(id)` only operates on `slot.active == true` (linear scan breaks on match). After `clearTweenSlot`, `slot.active = false`. `tickTweens` also checks `slot.active` first. No double-unref possible as long as both check `active` before touching refs.
**Warning signs:** `luaL_unref` called with `LUA_NOREF` (safe — Lua ignores it). No actual bug, but worth documenting.

### Pitfall 5: Duration <= 0
**What goes wrong:** If Lua passes `duration = 0` (or negative), `t = elapsed / duration` is division by zero or inverted.
**How to avoid:** In `tween.to()`, clamp `duration` to a minimum (e.g., `if (duration <= 0.0f) duration = 0.0f`). In `tickTweens`, guard: `if (slot.duration <= 0.0f) t = 1.0f` (instant completion). This fires the done_cb on the next tick, which is acceptable.
**Warning signs:** NaN or Inf propagating into `lua_setfield` — Lua will store NaN as a number without error, causing silent incorrect behavior.

### Pitfall 6: Props Table Iteration Order is Undefined in Lua
**What goes wrong:** Lua table iteration order via `lua_next` is undefined for hash tables (non-array-key tables). The order in which properties are stored in `TweenSlot.keys[]` may differ from the order in the Lua script.
**Why it happens:** Hash tables in Lua use a hash map internally; `lua_next` returns pairs in hash order.
**How to avoid:** Order doesn't matter for tweening — each property is animated independently. Document that property order in `TweenSlot.keys[]` may differ from script order. No issue in practice.

---

## Code Examples

Verified patterns from official codebase:

### Sample Lua usage

```lua
-- Tween a UI element's position over 0.5 seconds with easeOut
local panel = { x = 0, y = 0 }

local id = engine.tween.to(panel, {x = 100, y = 50}, 0.5, "easeOut", function()
    engine.log("tween done!")
end)

-- Panel position now animates each frame via tickTweens(dt)
-- In draw():
--   engine.graphics.rectangle("fill", panel.x, panel.y, 20, 20)

-- Cancel mid-tween:
engine.tween.cancel(id)

-- Cancel all tweens (e.g., on scene exit):
engine.tween.cancelAll()
```

### Pool overflow check (TWEEN-01 — return nil, no error)

```lua
local ids = {}
for i = 1, 8 do
    ids[i] = engine.tween.to(obj, {x = i * 10}, 1.0, "linear")
end
local ninth = engine.tween.to(obj, {x = 99}, 1.0, "linear")
-- ninth == nil (pool full; no error raised)
if not ninth then
    engine.log("tween pool full")
end
```

### Easing function outputs (verification reference)

```
t=0.0: linear=0.0, easeIn=0.0,   easeOut=0.0,   easeInOut=0.0
t=0.5: linear=0.5, easeIn=0.25,  easeOut=0.75,  easeInOut=0.5
t=1.0: linear=1.0, easeIn=1.0,   easeOut=1.0,   easeInOut=1.0
-- All four pass through (0,0) and (1,1); visually distinct through midpoint
```

### Header additions for `bindings.hpp`

```cpp
// -- Tween pool (Phase 50: TWEEN-01..TWEEN-03) ---------------------------------
static constexpr int TWEEN_POOL_SIZE = 8;
static constexpr int TWEEN_MAX_PROPS = 4;
static constexpr int TWEEN_KEY_MAX   = 32;

enum class TweenEasing : uint8_t { Linear=0, EaseIn=1, EaseOut=2, EaseInOut=3 };

struct TweenSlot {
    int      targetRef{LUA_NOREF};
    int      doneCbRef{LUA_NOREF};
    char     keys[TWEEN_MAX_PROPS][TWEEN_KEY_MAX]{};
    float    startVals[TWEEN_MAX_PROPS]{};
    float    endVals[TWEEN_MAX_PROPS]{};
    int      propCount{0};
    float    elapsed{0.0f};
    float    duration{1.0f};
    TweenEasing easing{TweenEasing::Linear};
    int      id{0};
    bool     active{false};
};

TweenSlot m_tweenPool[TWEEN_POOL_SIZE];
int       m_nextTweenId{0};

// Public methods (declarations only):
void tickTweens(float dt);
void clearTweens();

// Private static bindings:
static int lua_engine_tween_to(lua_State* L);
static int lua_engine_tween_cancel(lua_State* L);
static int lua_engine_tween_cancelAll(lua_State* L);

// Registration:
void registerTweenSubtable(lua_State* L);
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `std::pow(t, 2.0f)` for easing | `t*t` / `1-(1-t)*(1-t)` | Design decision (TWEEN-03) | Eliminates libm FPU call; safe for ESP32 |
| External tween lib (flux.lua, tween.lua) | Inline C implementation | Out of scope in REQUIREMENTS.md | Zero alloc; no Lua lib overhead; ESP32 compatible |

**Deprecated/outdated:**
- `flux.lua` / `tween.lua`: explicitly out of scope per REQUIREMENTS.md "Out of Scope" table — "breaks zero-alloc and ESP32 constraints; 4 inline functions sufficient".

---

## Open Questions

1. **How many properties per tween (`TWEEN_MAX_PROPS`)?**
   - What we know: CONTEXT.md leaves struct layout to Claude's discretion; 4 properties covers x/y/alpha/scale — common game use cases.
   - What's unclear: whether users will need to animate more than 4 simultaneously.
   - Recommendation: Start with `TWEEN_MAX_PROPS = 4`. This is Claude's discretion and matches the "minimal API" directive. If Lua script needs more, it can start multiple tweens.

2. **done_cb argument contract (Claude's Discretion)?**
   - What we know: CONTEXT.md marks this as Claude's discretion.
   - What's unclear: whether done_cb should receive the tween ID, the target table, or nothing.
   - Recommendation: Call done_cb with zero arguments — `lua_pcall(L, 0, 0, 0)`. Consistent with coroutine completion (no args). Simpler to document: "done_cb is called with no arguments when the tween completes."

3. **cancel-mid-tween behavior (Claude's Discretion): snap to current or snap to target?**
   - What we know: CONTEXT.md leaves this to Claude.
   - What's unclear: whether canceling should finalize the value at its current interpolated position or jump to the end value.
   - Recommendation: Leave at current interpolated value (do NOT snap to end on cancel). Rationale: `cancel` is typically used to abort mid-motion (e.g., user interrupted). Snapping to end would be surprising. Document: "cancel() stops at current value; does not fire done_cb."

4. **Props table: string keys only, or support integer (array) keys?**
   - What we know: Common usage is `{x=100, y=50}` (string keys). Integer-key tables like `{100, 50}` are syntactically valid but semantically ambiguous without field names.
   - Recommendation: Accept only string keys. Skip non-string keys silently (same treatment as `lua_next` iteration in binding). This keeps the API clear and avoids ambiguity.

---

## Validation Architecture

`workflow.nyquist_validation` is not a key in `.planning/config.json`. Skipping the formal Validation Architecture section per instructions.

Test infrastructure: standalone C++ executables in `tests/`, linked against `enjin2_lua`, added to `tests/CMakeLists.txt`, registered via `add_test`. Follows the `coroutine_async_test.cpp` pattern exactly.

### Phase Requirements -> Test Map

| Req ID | Behavior | Test Type | Test File | Notes |
|--------|----------|-----------|-----------|-------|
| TWEEN-01 | `to(target, {props}, duration, easing, cb)` animates table fields | integration | `tween_test.cpp` | Tick multiple frames; check field values at t=0.5 and t=1.0 |
| TWEEN-01 | Pool full: 9th `to()` returns nil | unit | `tween_test.cpp` | Start 8 tweens with long duration; 9th returns nil |
| TWEEN-02 | `cancel(id)` stops tween before completion | unit | `tween_test.cpp` | Cancel after 1 tick; verify field not updated further |
| TWEEN-02 | `cancelAll()` stops all tweens | unit | `tween_test.cpp` | Start 3 tweens; cancelAll; tick; verify no updates |
| TWEEN-02 | `clearTweens()` via `registerAll()` (hot-reload safety) | integration | `tween_test.cpp` | Start tween; registerAll; tick; verify no stale update |
| TWEEN-03 | All 4 easing modes produce distinct midpoint values | unit | `tween_test.cpp` | At t=0.5 tick: linear=0.5, easeIn=0.25, easeOut=0.75, easeInOut=0.5 |
| TWEEN-03 | No FPU-heavy math (compile-time check) | static | N/A | `grep -r 'std::pow'` must not appear in bindings_tween.cpp |

### Test File Structure (`tests/tween_test.cpp`)

The `TweenFixture` struct follows `AsyncFixture` from `coroutine_async_test.cpp`:

```cpp
struct TweenFixture {
    LuaEngine   engine;
    LuaBindings bindings;

    TweenFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }

    LuaResult exec(const char* code) { return engine.executeString(code); }
    double getNum(const char* name)  { return engine.getGlobalNumber(name); }
    void tick(float dt)              { bindings.tickTweens(dt); }
};
```

### CMakeLists.txt addition

```cmake
# Tween helpers test (Phase 50: TWEEN-01..TWEEN-03)
add_executable(tween_test
    tween_test.cpp
)
target_include_directories(tween_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(tween_test PRIVATE
    enjin2
    enjin2_lua
)
add_test(NAME tween_test COMMAND tween_test)
```

---

## Sources

### Primary (HIGH confidence)

- Source inspection: `/home/unwn/dev/enjin/src/scripting/bindings_async.cpp` — CoroutineSlot pattern, clearSlot template, tickCoroutines, clearCoroutines, registerAsyncSubtable — direct structural analog for TweenSlot
- Source inspection: `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` — existing member structure (CoroutineSlot, sprite pool, m_nextCoroutineId pattern); where TweenSlot members go; private static binding method declarations
- Source inspection: `/home/unwn/dev/enjin/include/enjin2/core/math.hpp` lines 66-69 — `lerp<T>(a, b, t)` confirmed; lines 102-105 — `smoothstep(edge0, edge1, x)` = `t*t*(3-2*t)` confirmed
- Source inspection: `/home/unwn/dev/enjin/src/scripting/bindings_engine.cpp` lines 219-221 — `registerDebugSubtable` + `registerAsyncSubtable` registration pattern; where `registerTweenSubtable` call goes
- Source inspection: `/home/unwn/dev/enjin/src/scripting/bindings.cpp` lines 477-481 — `clearHandlers + clearCoroutines` in `registerAll()`; lines 707-718 — `setActiveScene` cleanup; tween cleanup goes in same locations
- Source inspection: `/home/unwn/dev/enjin/src/platform/sdl/sdl_main.cpp` lines 330-334 — `tickCameraFollow + tickCoroutines` placement; `tickTweens` goes immediately after
- Source inspection: `/home/unwn/dev/enjin/tests/coroutine_async_test.cpp` — AsyncFixture pattern; test structure to mirror for TweenFixture
- Source inspection: `/home/unwn/dev/enjin/tests/CMakeLists.txt` — `coroutine_async_test` entry pattern; tween_test entry follows same form
- API verification: Lua 5.4 C API — `lua_getfield(L, idx, key)`, `lua_setfield(L, idx, key)`, `lua_next(L, tableIdx)`, `lua_rawgeti(L, LUA_REGISTRYINDEX, ref)` — all used in existing codebase; correct usage confirmed
- Source inspection: `/home/unwn/dev/enjin/.planning/REQUIREMENTS.md` — TWEEN-01..03 requirements; "Out of Scope: External tween libraries" confirmed

### Secondary (MEDIUM confidence)

- Source inspection: `/home/unwn/dev/enjin/.planning/phases/50-tween-helpers/50-CONTEXT.md` — user decisions, Claude's discretion areas, existing code insights cited
- Source inspection: `/home/unwn/dev/enjin/.planning/STATE.md` — Phase 49 patterns, decision history for pool design

### Tertiary (LOW confidence)

- None — all claims verified against codebase source directly.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new libraries; all math primitives verified in math.hpp; Lua C API usage verified in existing bindings
- Architecture: HIGH — direct structural clone of Phase 49 coroutine scheduler; all integration points confirmed in source
- Easing functions: HIGH — formulas verified analytically; smoothstep directly present in math.hpp; TWEEN-03 no-FPU constraint satisfied
- Pitfalls: HIGH — GC anchoring and unref patterns verified from existing code; `lua_next` stack management from Lua Reference Manual
- Test pattern: HIGH — `coroutine_async_test.cpp` exists and confirms fixture + tick pattern

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable APIs and project patterns)
