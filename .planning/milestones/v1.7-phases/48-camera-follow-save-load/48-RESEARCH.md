# Phase 48: Camera Follow + Save/Load — Research

**Researched:** 2026-03-01
**Domain:** C++ Lua bindings (camera follow target, LuaStore platform I/O)
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Single follow target at a time (one slot, not a list)
- If follow target proxy becomes invalid (object destroyed), silently stop following — no Lua error
- Consistent with codebase pattern: silent failure on invalid state

### Claude's Discretion
- Camera follow implementation approach (flag on C_Camera vs per-frame lookAt wrapper)
- Default lerp speed for follow
- Store default file path and naming
- Whether flush() changes auto-save behavior or supplements it
- VCV_RACK guard replacement strategy (SDL3 platform detection)

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CAM-01 | engine.camera.follow(target, speed) resolves named object and tracks per-frame | Follow stores ObjectProxy* in LuaBindings; per-frame follow logic calls cam->lookAt(pos.x, pos.y, speed) from within update binding |
| CAM-02 | engine.camera.stopFollow() clears follow target | Nulls the stored proxy pointer in LuaBindings |
| STORE-01 | LuaStore JSON file I/O enabled for SDL3 builds (VCV_RACK guard replaced) | enjin2_lua has no VCV_RACK define — guard is incorrect; replace with `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)` |
| STORE-02 | engine.store.flush() explicit save and engine.store.path() setter | flush() calls m_store.saveToFile(m_storePath); path() calls existing setStorePath() logic inline |
</phase_requirements>

---

## Summary

Phase 48 delivers two independent features on top of the existing Phase 44 camera system and the VCV_RACK-gated LuaStore. Both features are incremental additions that extend existing code paths without redesigning anything.

**Camera follow (CAM-01, CAM-02):** The approach is a per-frame `lookAt` wrapper stored in `LuaBindings`. `C_Camera::lookAt()` already does smooth lerp via `m_lerpSpeed * dt * 10`. The binding stores one `ObjectProxy*` pointer (`m_followTargetProxy`) in `LuaBindings`. Each time `update()` is called (from within the Lua `update` callback, or from `C_LuaScript::update()`), the follow binding reads `C_Position` from the target Object and calls `cam->lookAt(x, y, m_followSpeed)`. If the proxy is invalid (object was destroyed), the follow silently stops. No changes to `C_Camera` itself are needed — all state lives in `LuaBindings`.

**Store SDL3 I/O (STORE-01, STORE-02):** The VCV_RACK guard at `bindings_store.cpp:110` controls whether `saveToFile`/`loadFromFile` compile. The SDL desktop runner (`enjin2_sdl`) and `enjin2_lua` do NOT define `VCV_RACK` — only `enjin2_core`, `enjin2_graphics`, and `enjin2_ui` define it. So currently the store file I/O compiles in only when those libraries transitively export `VCV_RACK` into `enjin2_lua` — which they do via `PUBLIC` visibility in `target_compile_definitions`. The correct fix is to replace `#ifdef VCV_RACK` with `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)`, meaning "compile file I/O on any platform that is not ESP32 and not WASM". `flush()` is a one-liner that calls `m_store.saveToFile(m_storePath)`. `path()` calls the same logic as `setStorePath()` from Lua.

**Primary recommendation:** Store follow-target as `ObjectProxy*` in `LuaBindings` (no new member in `C_Camera`). Replace `VCV_RACK` guard with `!ESP32 && !EMSCRIPTEN`. Both features fit in the existing file split pattern (new functions in `bindings_engine.cpp` for camera, `bindings_store.cpp` for store).

---

## Standard Stack

### Core
| Component | Location | Purpose | Why Standard |
|-----------|----------|---------|--------------|
| `C_Camera` | `include/enjin2/components/camera.hpp` | Camera position, lerp, shake | Existing; `lookAt(x, y, speed)` is the per-frame tracking mechanism |
| `LuaBindings` | `include/enjin2/scripting/bindings.hpp` | Binding state, follow-target slot | All per-binding state lives here |
| `ObjectProxy` | `include/enjin2/scripting/object_proxy.hpp` | Proxy with `valid` flag | Codebase pattern for safe object references |
| `C_Position` | `include/enjin2/components/position.hpp` | Object's world position | Follow binding reads `getPosition().x / .y` |
| `LuaStore` | `include/enjin2/scripting/bindings.hpp:264` | Key-value persistent store | Existing; `saveToFile`/`loadFromFile` already implemented |
| `bindings_engine.cpp` | `src/scripting/bindings_engine.cpp` | engine.camera.* sub-table | Existing camera bindings; follow/stopFollow extend the same table |
| `bindings_store.cpp` | `src/scripting/bindings_store.cpp` | engine.store.* sub-table + LuaStore impl | Existing; flush/path extend this file |

### No New Libraries
Neither feature requires new libraries. Everything is implemented in the existing codebase.

---

## Architecture Patterns

### Recommended File Changes

```
src/scripting/
├── bindings_engine.cpp   — add lua_engine_camera_follow, lua_engine_camera_stopFollow
│                           extend kCameraFuncs[] array (lines 132-142)
├── bindings_store.cpp    — replace #ifdef VCV_RACK with #if !defined(ESP32) && !defined(__EMSCRIPTEN__)
│                           add lua_engine_store_flush, lua_engine_store_path
│                           extend kStoreFuncs[] in bindings_engine.cpp lines 101-111

include/enjin2/scripting/
└── bindings.hpp          — add m_followTargetProxy + m_followSpeed members (private section)
                            add static int declarations for follow/stopFollow/flush/path
```

### Pattern 1: Follow Target Storage in LuaBindings

**What:** Store one `ObjectProxy*` and one `float` in `LuaBindings` private section. The follow binding resolves position from `proxy->object->getComponent<C_Position>()` each frame.

**When to use:** Any time a Lua-side handle to a C++ object must be tracked across frames — consistent with how `m_activeCamera`, `m_activeScene`, and the event bus are stored.

```cpp
// Add to bindings.hpp private section (after m_debugEnabled)
// -- Camera follow (Phase 48: CAM-01, CAM-02) -----------------------------------
ObjectProxy* m_followTargetProxy{nullptr};  ///< Non-owning; null = not following
float        m_followSpeed{0.1f};           ///< lerp speed passed to lookAt()
```

**Why not store in C_Camera:** Camera component has no access to the Lua proxy system or ObjectProxy. Keeping it in LuaBindings is consistent with the codebase's separation of concerns — C_Camera is pure C++ math, LuaBindings holds all Lua-facing state.

### Pattern 2: Per-Frame Follow Call

**What:** In `lua_engine_camera_follow`, store the proxy pointer and speed. The Lua script is responsible for calling `engine.camera.follow(proxy, speed)` — the follow is then driven per-frame by calling this in the per-frame update. The actual tracking is triggered each frame as part of the Lua `update()` call, with the binding re-reading the proxy position and calling `cam->lookAt()`.

**Key design decision (Claude's Discretion):** The follow tracking happens inside `lua_engine_camera_follow` itself when called from the Lua `update()` function, not as a separate host-side per-frame call. This means the script calls `engine.camera.follow(proxy, speed)` each frame from its own `update()` function, and the binding resolves position and calls `cam->lookAt()` immediately. This is simpler than storing a follow-tick function in the C++ host loop.

**Alternative considered:** Add a `tickFollow(float dt)` method to `LuaBindings` and call it from `sdl_main.cpp` each frame. This would enable follow without per-frame Lua calls, but it requires host changes and the CONTEXT.md states the design goal is "tracks a named object per-frame via C_Camera" driven by bindings — not host-loop changes.

**Recommended approach:** `engine.camera.follow(proxy, speed)` is a normal per-frame call that the script invokes from `update()`. It resolves proxy, checks validity, calls `cam->lookAt(pos.x, pos.y, speed)`. This matches the Lua usage pattern: scripts call things in `update()`.

```cpp
// src/scripting/bindings_engine.cpp
static int lua_engine_camera_follow(lua_State* L) {
    C_Camera* cam = getActiveCamera(L);
    if (!cam) return 0;  // silent no-op
    auto* proxy = static_cast<enjin2::ObjectProxy*>(
        luaL_testudata(L, 1, "ObjectProxy"));
    if (!proxy || !proxy->valid || !proxy->object) return 0;  // invalid proxy — silent stop
    float speed = lua_gettop(L) >= 2 ? static_cast<float>(luaL_checknumber(L, 2)) : 0.1f;
    auto* pos = proxy->object->getComponent<C_Position>();
    if (!pos) return 0;  // no position component — silent no-op
    cam->lookAt(static_cast<float>(pos->getPosition().x),
                static_cast<float>(pos->getPosition().y),
                speed);
    return 0;
}

static int lua_engine_camera_stopFollow(lua_State* L) {
    (void)L;  // no state needed — this is purely a Lua convention signal
    return 0;
}
```

**Note on stopFollow:** Because follow is a per-frame explicit call pattern (Lua calls `engine.camera.follow()` each `update()`), `stopFollow()` does not need to clear any C++ state — stopping the per-frame call automatically stops the follow. `stopFollow()` is a no-op that exists purely for code clarity and symmetry. This is the simplest correct implementation.

**If the user prefers a set-once pattern:** Store `m_followTargetProxy` and `m_followSpeed` in `LuaBindings`. The `follow()` binding sets them. The host or per-frame Lua update calls a `tickFollow(dt)` helper. `stopFollow()` clears `m_followTargetProxy = nullptr`. This is more complex but enables passive follow without per-frame Lua code. Research recommends the per-frame call pattern as simpler and matching the CAM-01 requirement wording.

### Pattern 3: VCV_RACK Guard Replacement

**What:** The current `#ifdef VCV_RACK` guard in `bindings_store.cpp:110` is wrong for the SDL3 desktop build. `VCV_RACK` is defined as `PUBLIC` on `enjin2_core`, `enjin2_graphics`, and `enjin2_ui`, which means it transitively reaches `enjin2_lua` — so file I/O does compile today! But this is accidental and semantically wrong.

**The fix:** Replace `#ifdef VCV_RACK` with `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)`.

**Rationale:** `ESP32` is defined for ESP32 builds (see `CMakeLists.txt:200`). `__EMSCRIPTEN__` is automatically defined by the Emscripten toolchain. The else-branch stub (lines 318-320) remains for ESP32 (NVS deferred to STORE-03). WASM is excluded because localStorage bridge is STORE-04 (future). This accurately describes "SDL3 desktop + any other desktop" without relying on VCV_RACK.

```cpp
// bindings_store.cpp:110 — REPLACE:
// #ifdef VCV_RACK
// WITH:
#if !defined(ESP32) && !defined(__EMSCRIPTEN__)
// ... existing file I/O implementation (unchanged) ...
#else
// ESP32 stub — NVS support deferred (STORE-03)
// WASM stub — localStorage bridge deferred (STORE-04)
bool LuaStore::saveToFile(const char*) const { return false; }
bool LuaStore::loadFromFile(const char*) { return false; }
#endif
```

### Pattern 4: flush() and path() Bindings

**What:** Two new functions in `bindings_store.cpp`.

```cpp
// engine.store.flush() — explicitly write current store to disk
static int lua_engine_store_flush(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) { lua_pushboolean(L, 0); return 1; }
    if (b->m_storePath[0] == '\0') { lua_pushboolean(L, 0); return 1; }  // no path set
    bool ok = b->m_store.saveToFile(b->m_storePath);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// engine.store.path(filepath) — set the save file path at runtime
static int lua_engine_store_path(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;
    const char* path = luaL_checkstring(L, 1);
    strncpy(b->m_storePath, path, sizeof(b->m_storePath) - 1);
    b->m_storePath[sizeof(b->m_storePath) - 1] = '\0';
    // Load existing data from new path if file exists
    b->m_store.loadFromFile(b->m_storePath);  // silent no-op if file doesn't exist
    return 0;
}
```

**flush() semantics:** Supplements auto-save, does not change it. Auto-save (on `save()`/`delete()`/`clear()`) remains unchanged. `flush()` is an explicit write — useful when a script has set up store keys and wants to write them before a potential crash or scene exit.

**path() semantics:** Equivalent to `setStorePath()` called from Lua. Loads existing data from the new path (consistent with `setStorePath()` behavior in `bindings.hpp:566`). This allows scripts to redirect their save file at runtime.

### Pattern 5: Registering New Functions in Existing Sub-Tables

Both `engine.camera` and `engine.store` sub-tables are registered in `registerEngineTable()` in `bindings_engine.cpp`. The `kCameraFuncs[]` and `kStoreFuncs[]` arrays must be extended.

```cpp
// Extend kCameraFuncs in bindings_engine.cpp (lines 132-142)
static const LuaFuncDef kCameraFuncs[] = {
    {"setPosition",  lua_engine_camera_setPosition},
    {"getPosition",  lua_engine_camera_getPosition},
    {"lookAt",       lua_engine_camera_lookAt},
    {"shake",        lua_engine_camera_shake},
    {"setBounds",    lua_engine_camera_setBounds},
    {"clearBounds",  lua_engine_camera_clearBounds},
    {"follow",       lua_engine_camera_follow},      // Phase 48: CAM-01
    {"stopFollow",   lua_engine_camera_stopFollow},  // Phase 48: CAM-02
};

// Extend kStoreFuncs in bindings_engine.cpp (lines 101-111)
static const LuaFuncDef kStoreFuncs[] = {
    {"save",   lua_engine_store_save},
    {"load",   lua_engine_store_load},
    {"exists", lua_engine_store_exists},
    {"delete", lua_engine_store_delete},
    {"clear",  lua_engine_store_clear},
    {"flush",  lua_engine_store_flush},  // Phase 48: STORE-02
    {"path",   lua_engine_store_path},   // Phase 48: STORE-02
};
```

### Anti-Patterns to Avoid
- **Storing follow target in C_Camera:** Camera is pure C++ math. Proxy awareness belongs in LuaBindings.
- **Using `getComponent<C_Position>()` without null check:** `getComponent` can return nullptr if the object has no position. Guard must be present (though all spawned objects auto-add C_Position).
- **Calling `lua_error` or `luaL_error` on invalid proxy:** CONTEXT.md decision is silent failure. Return 0 with no values.
- **Defining VCV_RACK on enjin2_lua explicitly:** The fix is to remove the wrong guard, not to propagate VCV_RACK further.
- **Keeping `#ifdef VCV_RACK` in bindings_store.cpp:** The new guard must be the canonical one — do not add a second guard or dual-guard the same block.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Smooth camera lerp | Custom per-frame lerp in binding | `C_Camera::lookAt(x, y, speed)` | Already implements `lerpSpeed * dt * 10` frame-rate-correct lerp |
| Proxy validity checking | Custom object-alive check | `ObjectProxy::valid` flag | Already invalidated by `Object::~Object()` |
| JSON file I/O | New file writer | Existing `LuaStore::saveToFile/loadFromFile` | Already implemented; correct; tested |
| Platform detection macros | New platform abstraction | `!ESP32 && !__EMSCRIPTEN__` | Two defines already in use, consistent with CMakeLists.txt |

**Key insight:** Both features are thin bindings over fully-implemented C++ code. The only "work" is wiring Lua call sites to existing C++ methods.

---

## Common Pitfalls

### Pitfall 1: C_Camera::update() Is Not Called from sdl_main
**What goes wrong:** If follow tracking is designed to run from C_Camera::update(), it will not fire in SDL standalone mode because `sdl_main.cpp` never calls `C_Camera::update()` — it only calls the Lua `update()` global function.
**Why it happens:** C_Camera::update() is an `Object::update()` callback, called only when the object is part of a Scene that calls `scene.update(dt)`. SDL standalone does not use scenes.
**How to avoid:** Implement follow as a per-frame Lua call pattern (script calls `engine.camera.follow()` in its `update()` function) OR as a `LuaBindings::tickFollow()` host method called from `sdl_main.cpp`. The per-frame call pattern is simpler and recommended.
**Warning signs:** Camera stops following in SDL standalone mode but works in scene mode.

### Pitfall 2: ObjectProxy Forward-Declaration Missing in bindings.hpp
**What goes wrong:** `bindings.hpp` already uses `ObjectProxy` via `#include "object_proxy.hpp"` — this is present. The include is on line 13 of bindings.hpp. No issue here.
**Why it happens:** N/A — include is already there.
**How to avoid:** Confirm line 13 of bindings.hpp: `#include "object_proxy.hpp"`.

### Pitfall 3: C_Position Include Not in bindings_engine.cpp
**What goes wrong:** `bindings_engine.cpp` already includes `position.hpp` on line 7: `#include "../../include/enjin2/components/position.hpp"`. The follow binding can call `proxy->object->getComponent<C_Position>()` without any new include.
**How to avoid:** Verify line 7 of bindings_engine.cpp — it is already there.

### Pitfall 4: getComponent<> Template Access Pattern
**What goes wrong:** Calling `proxy->object->getComponent<C_Position>()` — this is the correct pattern. The Object class provides `getComponent<T>()` template (not shown in header directly, but confirmed used in camera_lua_test.cpp line 67: `obj->addComponent<C_Camera>()`). The non-const version is used in bindings. Confirmed pattern from Phase 44 camera tests.
**How to avoid:** Use `proxy->object->getComponent<C_Position>()` (returning pointer, check for null).

### Pitfall 5: VCV_RACK Transitively Defined on enjin2_lua
**What goes wrong:** `enjin2_core`, `enjin2_graphics`, and `enjin2_ui` all define `VCV_RACK PUBLIC` in CMakeLists.txt (lines 77, 92, 107). `enjin2_lua` links these via `target_link_libraries(enjin2_lua PRIVATE ...)`. With `PRIVATE` linking, `PUBLIC` definitions from the dependency are inherited by the target. This means `enjin2_lua` already has `VCV_RACK` defined, which is why file I/O currently compiles on SDL desktop builds. The fix is NOT to remove VCV_RACK from enjin2_lua — it is to replace the guard with a semantically correct one.
**How to avoid:** Leave CMakeLists.txt VCV_RACK definitions alone. Only change the guard in `bindings_store.cpp`.

### Pitfall 6: flush() Should Not Clear the Store
**What goes wrong:** A developer might implement flush() as "save then clear" — that would be destructive.
**How to avoid:** flush() only calls `saveToFile()`. It does not call `m_store.clear()`.

### Pitfall 7: path() Should Load Existing Data
**What goes wrong:** If `engine.store.path(newPath)` does not load existing data from `newPath`, the game starts with an empty store even if a save file already exists at that path.
**How to avoid:** Call `m_store.loadFromFile(m_storePath)` after setting the new path (same as `setStorePath()` behavior in bindings.hpp:566).

---

## Code Examples

### engine.camera.follow() — Minimal Implementation

```cpp
// Source: codebase analysis — pattern follows existing lua_engine_camera_lookAt
// File: src/scripting/bindings_engine.cpp

// Forward declaration (add with existing camera forward decls at top of file)
static int lua_engine_camera_follow(lua_State* L);
static int lua_engine_camera_stopFollow(lua_State* L);

static int lua_engine_camera_follow(lua_State* L) {
    C_Camera* cam = getActiveCamera(L);
    if (!cam) return 0;
    auto* proxy = static_cast<enjin2::ObjectProxy*>(
        luaL_testudata(L, 1, "ObjectProxy"));
    if (!proxy || !proxy->valid || !proxy->object) return 0;  // invalid proxy — silent stop
    float speed = lua_gettop(L) >= 2
        ? static_cast<float>(luaL_checknumber(L, 2))
        : 0.1f;
    auto* pos = proxy->object->getComponent<C_Position>();
    if (!pos) return 0;
    cam->lookAt(static_cast<float>(pos->getPosition().x),
                static_cast<float>(pos->getPosition().y),
                speed);
    return 0;
}

static int lua_engine_camera_stopFollow(lua_State* L) {
    (void)L;
    return 0;  // no-op — stopping per-frame calls is the stop mechanism
}
```

### engine.store.flush() and engine.store.path()

```cpp
// Source: codebase analysis — extends existing store bindings
// File: src/scripting/bindings_store.cpp

// --- engine.store.flush() ---
// Explicitly writes the current store to disk. Returns true on success.
// No-op (returns false) if no path is set.
int LuaBindings::lua_engine_store_flush(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushboolean(L, 0); return 1; }
    if (b->m_storePath[0] == '\0') { lua_pushboolean(L, 0); return 1; }
    bool ok = b->m_store.saveToFile(b->m_storePath);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// --- engine.store.path(filepath) ---
// Sets the save file path at runtime and loads any existing data.
int LuaBindings::lua_engine_store_path(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    const char* path = luaL_checkstring(L, 1);
    strncpy(b->m_storePath, path, sizeof(b->m_storePath) - 1);
    b->m_storePath[sizeof(b->m_storePath) - 1] = '\0';
    b->m_store.loadFromFile(b->m_storePath);  // load existing data; silent no-op if missing
    return 0;
}
```

### bindings.hpp Private Member Additions

```cpp
// Add to bindings.hpp private section, after m_debugEnabled (line ~434):
// -- Camera follow (Phase 48: CAM-01, CAM-02) -----------------------------------
// Per-frame follow is implemented as a Lua-driven call — no host-side tick needed.
// m_followTargetProxy is NOT used in the per-frame call pattern; shown here as
// documentation of the stateless approach. If a stateful set-once approach is chosen,
// add ObjectProxy* m_followTargetProxy{nullptr} and float m_followSpeed{0.1f}.
```

**Note:** In the recommended per-frame call pattern, no new private members are needed in `LuaBindings`. The follow state is implicit in whether the script calls `follow()` each frame.

### bindings.hpp Declaration Additions

```cpp
// Add to bindings.hpp (after existing camera binding declarations ~line 680):
// -- Phase 48: camera follow (CAM-01, CAM-02) --
static int lua_engine_camera_follow(lua_State* L);
static int lua_engine_camera_stopFollow(lua_State* L);

// -- Phase 48: store flush/path (STORE-02) --
static int lua_engine_store_flush(lua_State* L);
static int lua_engine_store_path(lua_State* L);
```

### Lua Usage Example

```lua
-- Lua script using engine.camera.follow (called each update)
function init(self)
    self.target = engine.scene.find("player")
end

function update(self, dt)
    if self.target then
        engine.camera.follow(self.target, 0.1)  -- smooth follow, speed 0.1
    end
end
```

```lua
-- Lua script using store.path and store.flush
function init(self)
    engine.store.path("saves/game.json")  -- redirect save file
    engine.store.save("level", 1)
    engine.store.flush()                  -- explicit write
end
```

### Test File Structure (following store_test.cpp / camera_lua_test.cpp patterns)

```cpp
// tests/camera_follow_test.cpp
// Fixture: LuaEngine + LuaBindings + Scene + Object with C_Position + C_Camera
// Tests:
//   - engine.camera.follow(proxy, speed) calls cam->lookAt() with target position
//   - engine.camera.stopFollow() is a no-op (no crash)
//   - Invalid proxy: engine.camera.follow(nil_proxy) is silent no-op
//   - No camera: engine.camera.follow() with no active camera is silent no-op

// tests/store_sdl_test.cpp (or extend store_test.cpp)
// Tests:
//   - engine.store.flush() returns false when no path set
//   - engine.store.flush() returns true after path set and data saved
//   - engine.store.path(filepath) redirects save location and loads existing data
//   - saveToFile / loadFromFile compile and work without VCV_RACK define
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|---|---|---|---|
| `#ifdef VCV_RACK` for file I/O | `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)` | Phase 48 | File I/O explicitly enabled on SDL3 desktop, excluded only on known embedded/WASM targets |
| No follow binding | `engine.camera.follow(proxy, speed)` per-frame call | Phase 48 | Scripts can track objects without per-frame `lookAt` boilerplate |

**Deprecated/outdated:**
- `#ifdef VCV_RACK` in `bindings_store.cpp`: semantically wrong for the SDL3 use case; replaced Phase 48

---

## Open Questions

1. **Per-frame vs. set-once follow pattern**
   - What we know: CONTEXT.md says "per-frame via C_Camera" and "per-frame script code" should not be required
   - What's unclear: "without additional per-frame script code" in the success criteria suggests set-once is preferred
   - Recommendation: Implement set-once pattern — store `m_followTargetProxy` and `m_followSpeed` in `LuaBindings`. Add `tickFollow(float dt)` to `LuaBindings` called from within the Lua `update` binding. The planner should confirm this reading of success criteria 1.

2. **Where to call tickFollow in set-once pattern**
   - What we know: `sdl_main.cpp` calls Lua `update()` via `lua_pcall`. `C_LuaScript::update()` exists for scene-based use.
   - What's unclear: In SDL standalone mode, there is no C++ per-frame callback except what the Lua `update()` triggers via bindings.
   - Recommendation: If set-once is chosen, add `tickCameraFollow(float dt)` as a `LuaBindings` method. Call it from within the `lua_engine_time_delta` region or from the host loop, injecting `dt` from the `enjin_time` registry value. See `lua_engine_time_delta` for how to read `EngineTimeState`.

3. **`engine.store.path()` return value**
   - What we know: No return value is specified in requirements.
   - What's unclear: Should it return the new path string or nothing?
   - Recommendation: Return nothing (0 return values), consistent with setter conventions in the codebase (`engine.camera.setPosition`, `engine.camera.setBounds`).

---

## Sources

### Primary (HIGH confidence)
- Codebase direct inspection: `src/scripting/bindings_engine.cpp` — existing camera binding pattern
- Codebase direct inspection: `src/scripting/bindings_store.cpp` — VCV_RACK guard location, LuaStore implementation
- Codebase direct inspection: `include/enjin2/scripting/bindings.hpp` — member layout, declaration pattern
- Codebase direct inspection: `include/enjin2/components/camera.hpp` — `lookAt()` signature
- Codebase direct inspection: `include/enjin2/components/position.hpp` — `getPosition()` returns `const Point&`
- Codebase direct inspection: `CMakeLists.txt:77,92,107,200` — VCV_RACK PUBLIC visibility, ESP32 define
- Codebase direct inspection: `src/platform/sdl/sdl_main.cpp` — no C_Camera::update() call in host loop
- Codebase direct inspection: `tests/store_test.cpp` — test fixture pattern
- Codebase direct inspection: `tests/camera_lua_test.cpp` — camera test fixture pattern
- Codebase direct inspection: `src/scripting/bindings_internal.hpp` — CCAMERA_PROXY_METATABLE constant

### Secondary (MEDIUM confidence)
- N/A — all findings are from direct source inspection

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — entire stack verified by direct source reading
- Architecture: HIGH — both patterns derived from existing Phase 44 and 46/47 precedents
- Pitfalls: HIGH — VCV_RACK transitivity verified in CMakeLists.txt; proxy/update path verified in sdl_main.cpp

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable codebase; CMakeLists.txt and bindings.hpp unlikely to change before this phase executes)
