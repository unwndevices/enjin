# Phase 57: QoL Features - Research

**Researched:** 2026-03-02
**Domain:** Lua scripting bindings (C++/Lua 5.4 + LuaJIT 5.1) — coroutine scheduler, tween pool, camera follow
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### Camera dead zone
- Dead zone is a **rectangle centered on the camera's current position** (not anchored to target)
- While the target is inside the dead zone: camera **freezes completely** — no lerp target update
- When target exits the dead zone: camera **resumes immediately at normal follow speed** — no ease-in
- Shape is rectangle only (width x height), matching the `setDeadZone(w, h)` signature

### Claude's Discretion
- `tween.await()` with invalid/expired ID: Claude decides whether to resume immediately or error — success criteria only specifies the happy path ("suspends until tween completes, then resumes exactly once")
- `wait_frames(n)` edge cases: n=0, n<0 behavior
- Whether dead zone state is stored on `LuaBindings` alongside `m_followTargetProxy`, or on `C_Camera`
- Dead zone persistence across scene changes (whether it's cleared with `m_followTargetProxy`)

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| QOL-01 | `engine.tween.await(id)` inside a coroutine suspends until tween completes, then resumes exactly once | Polling approach: `CoroutineSlot` gains `waitTweenId` int field; `tickTweens` resumes awaiting coroutines after completion, before clearing the tween slot |
| QOL-02 | `engine.async.wait_frames(n)` yields the coroutine for exactly n frames before resuming | `CoroutineSlot` gains `waitFrames` int field alongside existing `waitRemaining` float; `tickCoroutines` decrements frame counter |
| QOL-03 | `engine.camera.setDeadZone(w, h)` causes camera to freeze while target is inside dead zone boundary, resumes follow when target exits | Two new float fields on `LuaBindings` (`m_deadZoneW`, `m_deadZoneH`); dead zone check in `tickCameraFollow` before `cam->lookAt()` |
</phase_requirements>

---

## Summary

Phase 57 adds three additive features to the existing scripting bindings. All three are pure extensions — they add new fields to existing pool structs and new Lua functions, without touching the happy-path behavior of any existing API.

**QOL-01 (`tween.await`)** uses a polling (not callback-driven) design. Each `CoroutineSlot` needs one new field: `waitTweenId` (int, default 0 = not waiting). When `engine.tween.await(id)` is called inside a coroutine, it stores the tween ID into the coroutine's slot and yields. In `tickTweens`, after a tween completes and before clearing its slot, the code scans the coroutine pool for any slot with `waitTweenId == completedId` and calls `lua_resume` on it. The critical constraint is that this `lua_resume` must NOT happen from within the `done_cb` `lua_pcall` — it must happen in the main `tickTweens` loop body, after property interpolation but before `done_cb`, or after `done_cb`, in the same slot's completion block. Either ordering is safe because `tickTweens` runs at the top level (not inside any `pcall`).

**QOL-02 (`wait_frames`)** is the simplest feature. `CoroutineSlot` gains `waitFrames` (int, default 0). `engine.async.wait_frames(n)` stores n into the calling coroutine's `waitFrames`, then yields. In `tickCoroutines`, the frame-based check fires BEFORE the time-based check: if `waitFrames > 0`, decrement it; if still > 0, skip resume; when it reaches 0, resume normally. The two wait mechanisms must be mutually exclusive per slot — a single yield sets either `waitRemaining > 0` or `waitFrames > 0`, never both.

**QOL-03 (`setDeadZone`)** is a two-field extension to `LuaBindings`. Store `m_deadZoneW` and `m_deadZoneH` (floats) alongside `m_followTargetProxy`. In `tickCameraFollow`, after reading the target's position, compute the displacement between the camera's current position and the target. If `|dx| <= m_deadZoneW / 2` AND `|dy| <= m_deadZoneH / 2`, skip the `cam->lookAt()` call entirely (freeze). Otherwise call `lookAt()` as normal. The dead zone is centered on the camera position, not a world-space rectangle.

**Primary recommendation:** Add one field to `CoroutineSlot`, one field to `CoroutineSlot`, and two fields to `LuaBindings` private section. Add three new Lua binding functions. All logic goes into the existing tick methods. No new files needed — extend `bindings_async.cpp`, `bindings_tween.cpp`, and `bindings_engine.cpp` in place.

---

## Standard Stack

This is a pure C++ extension of existing project code. No new libraries are introduced.

### Core (already present — no new dependencies)
| Component | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| Lua 5.4 / LuaJIT 5.1 | existing | Scripting runtime | Already the project scripting layer |
| `lua_resume` | existing | Coroutine resume primitive | Project-established pattern for coroutine tick |
| `lua_yield` | existing | Coroutine suspend primitive | Used by `engine.async.wait` — same pattern for `wait_frames` |
| `luaL_checkinteger` | existing | Arg validation | Project convention for integer args |
| `lua_pcall` | existing | Protected function call | Used for `done_cb` — must NOT be used to call `lua_resume` |

**Installation:** None. No new packages.

---

## Architecture Patterns

### Pattern 1: Polling-based tween await (QOL-01)

**What:** A coroutine calls `engine.tween.await(id)` and blocks until the tween with that ID finishes. The connection is stored as a field on `CoroutineSlot`, checked during `tickTweens` at completion time.

**Why polling (not callback-driven):** The `done_cb` in `tickTweens` is called with `lua_pcall`. Calling `lua_resume` from within `lua_pcall` is a yield-across-pcall boundary error in Lua. Therefore, coroutine resumption must happen in the main `tickTweens` loop body, not inside the `done_cb` pcall block.

**Struct change — `CoroutineSlot` (bindings.hpp):**
```cpp
struct CoroutineSlot {
    int   threadRef{LUA_NOREF};
    float waitRemaining{0.0f};
    int   waitFrames{0};        // NEW: frames remaining (0 = not frame-waiting)
    int   waitTweenId{0};       // NEW: tween ID we are waiting for (0 = not tween-waiting)
    int   id{0};
    bool  active{false};
};
```

**`clearSlot` update (bindings_async.cpp):**
The existing `clearSlot` template must reset the new fields:
```cpp
slot.waitFrames  = 0;
slot.waitRemaining = 0.0f;
slot.waitTweenId = 0;
```

**New Lua binding `lua_engine_tween_await` (bindings_tween.cpp):**
```cpp
int LuaBindings::lua_engine_tween_await(lua_State* L) {
    if (!lua_isyieldable(L)) {
        luaL_error(L, "engine.tween.await() called outside a coroutine");
        return 0;
    }
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return lua_yield(L, 0);

    int tweenId = static_cast<int>(luaL_checkinteger(L, 1));

    // Check if tween is still active; if not, resume immediately (invalid/expired ID path)
    bool found = false;
    for (int i = 0; i < TWEEN_POOL_SIZE; ++i) {
        if (b->m_tweenPool[i].active && b->m_tweenPool[i].id == tweenId) {
            found = true;
            break;
        }
    }
    if (!found) {
        // Tween already done or never existed — resume immediately (no yield)
        return 0;
    }

    // Find calling coroutine in pool and set waitTweenId
    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        CoroutineSlot& slot = b->m_coroutinePool[i];
        if (!slot.active || slot.threadRef == LUA_NOREF) continue;
        lua_rawgeti(L, LUA_REGISTRYINDEX, slot.threadRef);
        lua_State* co = lua_tothread(L, -1);
        lua_pop(L, 1);
        if (co == L) {
            slot.waitTweenId = tweenId;
            break;
        }
    }
    return lua_yield(L, 0);
}
```

**`tickTweens` completion block update (bindings_tween.cpp):**
In the `if (t >= 1.0f)` block, BEFORE calling `done_cb` (or after — both are safe since `lua_resume` is not inside `lua_pcall`), scan the coroutine pool:
```cpp
if (t >= 1.0f) {
    int completedId = slot.id;  // save before clearTweenSlot zeroes it

    // Resume any coroutines awaiting this tween
    for (int j = 0; j < COROUTINE_POOL_SIZE; ++j) {
        CoroutineSlot& cslot = m_coroutinePool[j];
        if (!cslot.active || cslot.waitTweenId != completedId) continue;
        cslot.waitTweenId = 0;  // clear wait

        // Retrieve and resume the coroutine
        lua_rawgeti(L, LUA_REGISTRYINDEX, cslot.threadRef);
        lua_State* co = lua_tothread(L, -1);
        lua_pop(L, 1);
        if (co) {
            int status;
#if LUA_VERSION_NUM >= 504
            int nres = 0;
            status = lua_resume(co, L, 0, &nres);
            if (nres > 0) lua_pop(co, nres);
#else
            status = lua_resume(co, 0);
            if (lua_gettop(co) > 0) lua_settop(co, 0);
#endif
            if (status == LUA_OK) {
                clearSlot(cslot, L);
            } else if (status != LUA_YIELD) {
                const char* err = lua_tostring(co, -1);
                fprintf(stderr, "[tween.await error] %s\n", err ? err : "(unknown)");
                clearSlot(cslot, L);
            }
            // LUA_YIELD: coroutine re-yielded (e.g. called wait() again) — leave active
        }
    }

    // Fire done_cb if provided (after coroutine resume — order safe)
    if (slot.doneCbRef != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, slot.doneCbRef);
        if (lua_isfunction(L, -1)) {
            if (lua_pcall(L, 0, 0, 0) != 0) {
                const char* err = lua_tostring(L, -1);
                fprintf(stderr, "[tween done_cb error] %s\n", err ? err : "(unknown)");
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }
    }
    clearTweenSlot(slot, L);
}
```

**Note on `clearSlot` vs `clearTweenSlot`:** The coroutine pool uses `clearSlot` (template function in bindings_async.cpp). `tickTweens` does not have direct access to that template because it is a static template in `bindings_async.cpp`. The solution is to inline the clear logic in `tickTweens` or expose `clearSlot` as a `LuaBindings` member. The cleanest approach: move `clearSlot` to a private inline member function in bindings.hpp, or duplicate the four-line clear inline in `tickTweens`. Given the project's existing pattern (two separate static templates for tween and coroutine), the simplest safe choice is to inline the coroutine slot clear in `tickTweens`.

**Registration — add to `registerTweenSubtable`:**
```cpp
{"await", lua_engine_tween_await},
```
Add the declaration to `bindings.hpp` private section:
```cpp
static int lua_engine_tween_await(lua_State* L);
```

---

### Pattern 2: Frame-based yield (QOL-02)

**What:** `engine.async.wait_frames(n)` yields the current coroutine for exactly n frames. n=0 or n<0: resume immediately (no yield, or clamp to 0).

**Struct field already described above** (`waitFrames` on `CoroutineSlot`).

**New Lua binding `lua_engine_async_wait_frames` (bindings_async.cpp):**
```cpp
int LuaBindings::lua_engine_async_wait_frames(lua_State* L) {
    if (!lua_isyieldable(L)) {
        luaL_error(L, "engine.async.wait_frames() called outside a coroutine");
        return 0;
    }
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return lua_yield(L, 0);

    int n = static_cast<int>(luaL_optinteger(L, 1, 0));
    if (n <= 0) return 0;  // resume immediately

    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        CoroutineSlot& slot = b->m_coroutinePool[i];
        if (!slot.active || slot.threadRef == LUA_NOREF) continue;
        lua_rawgeti(L, LUA_REGISTRYINDEX, slot.threadRef);
        lua_State* co = lua_tothread(L, -1);
        lua_pop(L, 1);
        if (co == L) {
            slot.waitFrames = n;
            break;
        }
    }
    return lua_yield(L, 0);
}
```

**`tickCoroutines` update — frame check takes priority over time check:**
```cpp
// In tickCoroutines, slot check:
if (slot.waitFrames > 0) {
    slot.waitFrames--;
    if (slot.waitFrames > 0) continue;  // still waiting
    // waitFrames just hit 0 — fall through to resume
} else if (slot.waitRemaining > 0.001f) {
    slot.waitRemaining -= dt;
    if (slot.waitRemaining > 0.001f) continue;
}
// ... resume logic unchanged
```

The existing `waitRemaining` post-resume decrement (`slot.waitRemaining -= dt` after LUA_YIELD status) must NOT fire when the resume was due to `waitFrames`. Guard it:
```cpp
if (status == LUA_YIELD) {
    if (slot.waitFrames == 0) {
        // Only apply dt credit for time-based waits
        slot.waitRemaining -= dt;
    }
}
```

**Registration — add to `registerAsyncSubtable`:**
```cpp
{"wait_frames", lua_engine_async_wait_frames},
```
Declaration in `bindings.hpp`:
```cpp
static int lua_engine_async_wait_frames(lua_State* L);
```

---

### Pattern 3: Camera dead zone (QOL-03)

**What:** `engine.camera.setDeadZone(w, h)` stores a rectangle. `tickCameraFollow` checks whether the target is inside the dead zone (centered on camera position). If inside: skip `lookAt()`. If outside: call `lookAt()` normally.

**New fields on `LuaBindings` (bindings.hpp, private section, camera follow block):**
```cpp
// -- Camera follow (Phase 48: CAM-01, CAM-02) -----------------------------------
ObjectProxy* m_followTargetProxy{nullptr};  ///< Non-owning; null = not following
float        m_followSpeed{0.1f};           ///< lerp speed passed to lookAt()
float        m_deadZoneW{0.0f};             ///< Dead zone width (0 = disabled)
float        m_deadZoneH{0.0f};             ///< Dead zone height (0 = disabled)
```

**Dead zone persistence:** Per Claude's Discretion, these fields should be cleared alongside `m_followTargetProxy` (on scene change and hot reload), so the dead zone doesn't persist to a new scene. The cleanest approach: clear `m_deadZoneW = 0.0f; m_deadZoneH = 0.0f;` wherever `m_followTargetProxy = nullptr` is set in `setActiveScene()` and `registerAll()`.

**New Lua binding `lua_engine_camera_setDeadZone` (bindings_engine.cpp):**
```cpp
int LuaBindings::lua_engine_camera_setDeadZone(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;
    float w = static_cast<float>(luaL_checknumber(L, 1));
    float h = static_cast<float>(luaL_checknumber(L, 2));
    if (w < 0.0f) w = 0.0f;
    if (h < 0.0f) h = 0.0f;
    b->m_deadZoneW = w;
    b->m_deadZoneH = h;
    return 0;
}
```

**`tickCameraFollow` update (bindings_engine.cpp):**
```cpp
void LuaBindings::tickCameraFollow(float dt) {
    if (!m_followTargetProxy) return;
    if (!m_followTargetProxy->valid || !m_followTargetProxy->object) {
        m_followTargetProxy = nullptr;
        return;
    }

    C_Camera* cam = getActiveCamera();
    if (!cam) return;

    auto* pos = m_followTargetProxy->object->getComponent<C_Position>();
    if (!pos) return;

    float targetX = static_cast<float>(pos->getPosition().x);
    float targetY = static_cast<float>(pos->getPosition().y);

    // Dead zone check: is target within dead zone rectangle centered on camera?
    if (m_deadZoneW > 0.0f && m_deadZoneH > 0.0f) {
        Vec2 camPos = cam->getPosition();
        float dx = targetX - camPos.x;
        float dy = targetY - camPos.y;
        if (dx < 0.0f) dx = -dx;
        if (dy < 0.0f) dy = -dy;
        if (dx <= m_deadZoneW * 0.5f && dy <= m_deadZoneH * 0.5f) {
            return;  // target inside dead zone — freeze camera
        }
    }

    cam->lookAt(targetX, targetY, m_followSpeed);
}
```

**Registration — in the camera sub-table registration (bindings_engine.cpp):**
Find the `kCameraFuncs` array and add:
```cpp
{"setDeadZone", lua_engine_camera_setDeadZone},
```
Also requires the `LuaBindings` member function declaration in `bindings.hpp`:
```cpp
static int lua_engine_camera_setDeadZone(lua_State* L);
```

---

### Recommended File Changes

```
include/enjin2/scripting/bindings.hpp        # struct CoroutineSlot: +waitFrames, +waitTweenId
                                              # LuaBindings private: +m_deadZoneW, +m_deadZoneH
                                              # LuaBindings private: +lua_engine_tween_await
                                              # LuaBindings private: +lua_engine_async_wait_frames
                                              # LuaBindings private: +lua_engine_camera_setDeadZone
src/scripting/bindings_async.cpp              # clearSlot: reset +waitFrames, +waitTweenId
                                              # tickCoroutines: frame-based check
                                              # lua_engine_async_wait_frames: new function
                                              # registerAsyncSubtable: add "wait_frames"
src/scripting/bindings_tween.cpp              # tickTweens: coroutine resume on completion
                                              # lua_engine_tween_await: new function
                                              # registerTweenSubtable: add "await"
src/scripting/bindings_engine.cpp             # tickCameraFollow: dead zone check
                                              # lua_engine_camera_setDeadZone: new function
                                              # registerCameraSubtable (or kCameraFuncs): add "setDeadZone"
tests/qol_test.cpp                            # NEW: integration tests for QOL-01, QOL-02, QOL-03
tests/CMakeLists.txt                          # add qol_test
```

### Anti-Patterns to Avoid

- **Resuming coroutine from inside `lua_pcall`:** If the coroutine resume for `tween.await` happened inside the `done_cb` pcall block, Lua would throw "attempt to yield across a C-call boundary." Resume must happen in the main tick loop, not from within pcall.
- **Using `clearSlot` from `bindings_tween.cpp`:** The `clearSlot` template is static to `bindings_async.cpp`. Do not call it from `bindings_tween.cpp`. Inline the four-line clear operation in `tickTweens` where needed, or refactor `clearSlot` to a non-static inline on `LuaBindings`.
- **Storing dead zone on `C_Camera`:** CONTEXT.md leaves this as discretion, but storing on `LuaBindings` alongside `m_followTargetProxy` is cleaner — it stays in the same cleanup group and doesn't require adding to the camera component API.
- **Not clearing dead zone on scene change:** `m_deadZoneW = m_deadZoneH = 0.0f` must be added to `setActiveScene()` and `registerAll()` cleanup blocks, same as `m_followTargetProxy = nullptr`.
- **Frame count semantics:** `wait_frames(1)` should resume on the NEXT frame, not the same frame. The implementation decrements first then checks: `waitFrames--; if (waitFrames > 0) continue;` — this correctly delays by exactly n frames.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Lua/C compat for lua_resume | Version ifdef | Existing compat block in `tickCoroutines` | Already handles Lua 5.4 vs LuaJIT 5.1 |
| Coroutine identification | New ID scheme | Existing `lua_tothread` + pointer comparison | Established pattern in `engine.async.wait` |
| Frame timer | float accumulator | int decrement counter | Exact frame counting, no float epsilon needed |
| Tween completion signal | Extra callback chain | Poll `completedId` during `tickTweens` | Avoids yield-across-pcall entirely |

**Key insight:** The existing async/tween infrastructure is exactly the right shape for all three features. Extend in-place rather than adding new scheduling abstractions.

---

## Common Pitfalls

### Pitfall 1: Re-entrant resume inside pcall (QOL-01)
**What goes wrong:** If `lua_resume` for a `tween.await`-ing coroutine is called from within the `done_cb` `lua_pcall`, Lua raises "attempt to yield across a C-call boundary." The coroutine errors out.
**Why it happens:** `lua_pcall` sets a C boundary; `lua_resume` called beneath it tries to yield through that boundary.
**How to avoid:** Resume coroutines in the main `tickTweens` loop, before or after the `done_cb` block — not inside it. The coroutine resume scan loop must be at the same nesting level as the property interpolation code, not nested inside the `done_cb` pcall.
**Warning signs:** `[tween.await error] attempt to yield across a C-call boundary` in stderr.

### Pitfall 2: clearSlot scope error (QOL-01)
**What goes wrong:** `tickTweens` calls the `clearSlot` template from `bindings_async.cpp` but can't see it — it's a static template in a different .cpp file.
**Why it happens:** `static` keyword limits template visibility to the translation unit.
**How to avoid:** Either inline the coroutine slot clear in `tickTweens` (4 lines: threadRef, waitRemaining, waitFrames, waitTweenId, id, active), or make `clearSlot` a private inline member of `LuaBindings`. Inlining is the minimal change.

### Pitfall 3: Double-decrement of waitRemaining (QOL-02)
**What goes wrong:** When a coroutine resumes after `wait_frames`, the existing `if (status == LUA_YIELD) { slot.waitRemaining -= dt; }` fires unconditionally and corrupts the time-based wait state if the coroutine immediately calls `engine.async.wait(seconds)` after being frame-woken.
**Why it happens:** The post-resume dt-subtract was added to make time-based waits accurate; it's not conditional on how the resume was triggered.
**How to avoid:** Guard the dt-subtract: only apply when the resume was time-based (i.e., `slot.waitFrames` was 0 when the resume fired, or check `slot.waitRemaining` was the trigger).

### Pitfall 4: Dead zone freezes camera permanently (QOL-03)
**What goes wrong:** Camera stops following and never resumes, even when target moves far away.
**Why it happens:** Dead zone rectangle is centered on the target's position (world-space), not the camera's position. If centered on the target, the target is always inside it.
**How to avoid:** Center the dead zone on `cam->getPosition()` (the camera's current position), not on the target. The check is: is the target's offset from the camera within half-width and half-height?

### Pitfall 5: tween.await on expired ID causes permanent suspension (QOL-01)
**What goes wrong:** Coroutine calls `engine.tween.await(id)` with an ID that already completed. The tween slot is gone. The coroutine sets `waitTweenId` and yields. No tween ever completes with that ID again, so the coroutine is stuck forever.
**How to avoid:** In `lua_engine_tween_await`, check if the tween ID is currently active BEFORE yielding. If not found in the pool, return 0 immediately (no yield) — the coroutine resumes on the same tick. This is the recommended "resume immediately" path for the invalid/expired ID discretion case.

### Pitfall 6: waitFrames and waitRemaining both set simultaneously (QOL-02)
**What goes wrong:** A coroutine calls `wait_frames(5)` then somehow `wait(0.5)` sets `waitRemaining` before the frames expire. The coroutine wakes up either too early or at the wrong time.
**Why it happens:** If `wait_frames` sets `waitFrames = n` and a separate code path sets `waitRemaining`, the tick logic handles whichever clears first.
**How to avoid:** In `lua_engine_async_wait_frames`, clear `slot.waitRemaining = 0.0f` before setting `slot.waitFrames`. In `lua_engine_async_wait`, clear `slot.waitFrames = 0` before setting `slot.waitRemaining`. Mutual exclusion is enforced at the binding layer.

---

## Code Examples

### Full CoroutineSlot struct (updated)
```cpp
// Source: bindings.hpp CoroutineSlot definition
struct CoroutineSlot {
    int   threadRef{LUA_NOREF};   ///< luaL_ref handle anchoring the coroutine thread
    float waitRemaining{0.0f};    ///< seconds until next resume (0 = ready now)
    int   waitFrames{0};          ///< frames remaining for wait_frames (0 = not waiting)
    int   waitTweenId{0};         ///< tween ID this coroutine is awaiting (0 = not waiting)
    int   id{0};                  ///< monotonically increasing cancel ID
    bool  active{false};          ///< slot in use
};
```

### tickCoroutines skip logic (updated, frame check first)
```cpp
// Source: bindings_async.cpp tickCoroutines
if (slot.waitFrames > 0) {
    --slot.waitFrames;
    if (slot.waitFrames > 0) continue;
    // fell through: waitFrames just hit 0, resume this frame
} else if (slot.waitRemaining > 0.001f) {
    slot.waitRemaining -= dt;
    if (slot.waitRemaining > 0.001f) continue;
}
// ... resume via lua_resume
if (status == LUA_YIELD) {
    if (slot.waitFrames == 0) {
        slot.waitRemaining -= dt;  // only credit dt for time-based waits
    }
}
```

### tickTweens coroutine-resume block (in completion handler)
```cpp
// Source: bindings_tween.cpp tickTweens, t >= 1.0f block
if (t >= 1.0f) {
    int completedId = slot.id;

    // Resume awaiting coroutines (BEFORE done_cb pcall — avoids yield-across-pcall)
    for (int j = 0; j < COROUTINE_POOL_SIZE; ++j) {
        CoroutineSlot& cslot = m_coroutinePool[j];
        if (!cslot.active || cslot.waitTweenId != completedId) continue;
        cslot.waitTweenId = 0;
        lua_rawgeti(L, LUA_REGISTRYINDEX, cslot.threadRef);
        lua_State* co = lua_tothread(L, -1);
        lua_pop(L, 1);
        if (co) {
            int status;
#if LUA_VERSION_NUM >= 504
            int nres = 0;
            status = lua_resume(co, L, 0, &nres);
            if (nres > 0) lua_pop(co, nres);
#else
            status = lua_resume(co, 0);
            if (lua_gettop(co) > 0) lua_settop(co, 0);
#endif
            if (status == LUA_OK) {
                // inline clearSlot (clearSlot template not visible here)
                if (cslot.threadRef != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, cslot.threadRef);
                cslot.threadRef     = LUA_NOREF;
                cslot.waitRemaining = 0.0f;
                cslot.waitFrames    = 0;
                cslot.waitTweenId   = 0;
                cslot.id            = 0;
                cslot.active        = false;
            } else if (status != LUA_YIELD) {
                const char* err = lua_tostring(co, -1);
                fprintf(stderr, "[tween.await error] %s\n", err ? err : "(unknown)");
                if (cslot.threadRef != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, cslot.threadRef);
                cslot.threadRef     = LUA_NOREF;
                cslot.waitRemaining = 0.0f;
                cslot.waitFrames    = 0;
                cslot.waitTweenId   = 0;
                cslot.id            = 0;
                cslot.active        = false;
            }
            // LUA_YIELD: coroutine suspended again (e.g. chained wait()) — leave active
        }
    }

    // done_cb via pcall (safe: we're not inside pcall at this point)
    if (slot.doneCbRef != LUA_NOREF) { /* ... existing code ... */ }
    clearTweenSlot(slot, L);
}
```

### Dead zone check in tickCameraFollow
```cpp
// Source: bindings_engine.cpp tickCameraFollow
float targetX = static_cast<float>(pos->getPosition().x);
float targetY = static_cast<float>(pos->getPosition().y);

if (m_deadZoneW > 0.0f && m_deadZoneH > 0.0f) {
    Vec2 camPos = cam->getPosition();
    float dx = targetX - camPos.x;
    float dy = targetY - camPos.y;
    if (dx < 0.0f) dx = -dx;
    if (dy < 0.0f) dy = -dy;
    if (dx <= m_deadZoneW * 0.5f && dy <= m_deadZoneH * 0.5f) {
        return;  // inside dead zone — freeze
    }
}

cam->lookAt(targetX, targetY, m_followSpeed);
```

### Lua usage examples
```lua
-- QOL-01: tween.await
engine.async.start(function()
    local id = engine.tween.to(sprite, {x = 200}, 1.0, "easeOut")
    engine.tween.await(id)           -- suspends here
    print("tween done, sprite at x=200")
end)

-- QOL-02: wait_frames
engine.async.start(function()
    engine.async.wait_frames(3)      -- yields for exactly 3 frames
    print("3 frames have passed")
end)

-- QOL-03: camera dead zone
engine.camera.follow(player_proxy, 0.1)
engine.camera.setDeadZone(32, 24)   -- 32x24 pixel dead zone
```

---

## Test Architecture

### New test file: `tests/qol_test.cpp`

Pattern follows `tween_test.cpp` and `coroutine_async_test.cpp`:
- `struct QoLFixture` — `LuaEngine + LuaBindings`, no canvas, includes `tickCoroutines` and `tickTweens` calls
- For dead zone tests: requires `Scene`, `Object` with `C_LuaScript`, `C_Camera`, `C_Position` (same as `camera_follow_test.cpp`)

**Tests to write:**

| Test | Requirement | Type |
|------|-------------|------|
| `test_tween_await_suspends_and_resumes` | QOL-01 | integration — tick until tween completes, verify coroutine ran after |
| `test_tween_await_invalid_id_resumes_immediately` | QOL-01 | unit — await(0) or await(999) returns without yielding |
| `test_tween_await_resumes_exactly_once` | QOL-01 | integration — two coroutines await the same tween; both resume |
| `test_wait_frames_yields_exactly_n` | QOL-02 | integration — tick n-1 times, not resumed; tick once more, resumed |
| `test_wait_frames_zero_resumes_immediately` | QOL-02 | unit — wait_frames(0) does not yield |
| `test_wait_frames_negative_resumes_immediately` | QOL-02 | unit — wait_frames(-1) does not yield |
| `test_dead_zone_freezes_camera` | QOL-03 | integration — target inside dead zone, camera position unchanged |
| `test_dead_zone_resumes_on_exit` | QOL-03 | integration — target exits dead zone, camera moves again |
| `test_dead_zone_zero_disables` | QOL-03 | unit — setDeadZone(0,0) does not freeze camera |

**Note on `tickBoth` helper:** Tests for QOL-01 need both `tickCoroutines` and `tickTweens` in correct order. Add a helper:
```cpp
void tickBoth(float dt) {
    bindings.tickCoroutines(dt);
    bindings.tickTweens(dt);
}
```

**CMakeLists.txt entry:**
```cmake
# QoL features test (Phase 57: QOL-01, QOL-02, QOL-03)
add_executable(qol_test
    qol_test.cpp
)
target_include_directories(qol_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(qol_test PRIVATE
    enjin2
    enjin2_lua
)
add_test(NAME qol_test COMMAND qol_test)
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| float-based tween wait | polling `waitTweenId` int field | Phase 57 | No float epsilon needed; ID exact match |
| No frame counting in async | `waitFrames` int alongside `waitRemaining` float | Phase 57 | Both wait modes coexist; frame-exact timing |
| Camera always follows | Dead zone halts `lookAt()` call | Phase 57 | Standard platformer camera feel |

---

## Open Questions

1. **Should `clearSlot` be refactored to a member function?**
   - What we know: `clearSlot` is a static template in `bindings_async.cpp`, not visible to `bindings_tween.cpp`
   - What's unclear: Whether the plan calls for inlining the clear, making it a private inline member, or a shared static helper in `bindings_internal.hpp`
   - Recommendation: Inline the clear in `tickTweens` — 6 lines, no header change required, consistent with project's "minimal header churn" pattern. The planner should explicitly specify this choice.

2. **Ordering of coroutine resume vs done_cb in tickTweens completion block**
   - What we know: Both are safe from a yield-across-pcall perspective (both are in the main loop, not inside pcall). Coroutine resume first is shown in the code examples above.
   - What's unclear: If the coroutine's first action after resume is `engine.tween.to(...)` with a `done_cb`, and that done_cb fires in the SAME `tickTweens` call... this is theoretically possible if a 0-duration tween is started. In practice it cannot happen: the new tween gets a new slot and will only be processed in a subsequent tick (the outer `for` loop has already passed that slot index, or it's past the current slot index in the scan). No re-entrancy issue.
   - Recommendation: Coroutine resume before done_cb. This is the order shown in the code examples and is safe.

---

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection — `src/scripting/bindings_async.cpp` (full file, lines 1-244)
- Direct codebase inspection — `src/scripting/bindings_tween.cpp` (full file, lines 1-275)
- Direct codebase inspection — `src/scripting/bindings_engine.cpp` (lines 960-1042, tickCameraFollow and camera bindings)
- Direct codebase inspection — `include/enjin2/scripting/bindings.hpp` (lines 448-494, CoroutineSlot and TweenSlot definitions)
- Direct codebase inspection — `src/platform/sdl/sdl_main.cpp` (lines 330-335, tick ordering)
- Direct codebase inspection — `include/enjin2/components/camera.hpp` (full file, C_Camera API)

### Secondary (MEDIUM confidence)
- Lua 5.4 reference manual (training knowledge, consistent with in-code compat guards): `lua_resume` from within `lua_pcall` causes yield-across-C-call-boundary error — this is well-established Lua semantics documented in the Lua manual section on continuations and coroutines.

### Tertiary (LOW confidence — not needed, all findings from primary source)
- N/A

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all code is direct codebase inspection, no external libraries
- Architecture: HIGH — implementation patterns derived from existing code in same files; pitfalls are verified by reading `tickTweens` and `tickCoroutines` call sites
- Pitfalls: HIGH — re-entrant resume pitfall confirmed by `tickTweens` using `lua_pcall` for `done_cb` (line 236); `clearSlot` visibility confirmed by `static` template in `bindings_async.cpp` (line 40)

**Research date:** 2026-03-02
**Valid until:** 2026-04-02 (stable internal codebase; no external dependencies)
