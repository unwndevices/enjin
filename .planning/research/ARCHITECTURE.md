# Architecture Research

**Domain:** 2D game engine — v1.7 new feature integration
**Researched:** 2026-03-01
**Confidence:** HIGH (full codebase read; all claims grounded in live source files)

---

## Standard Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Lua Script Layer                                  │
│  engine.debug.*  engine.ui.*  engine.camera.follow()                         │
│  engine.tween.*  engine.async.*  engine.scene.persist()                      │
├───────────────────────────────────────┬─────────────────────────────────────┤
│           LuaBindings                 │         LuaEventBus                   │
│  (split across bindings_*.cpp)        │   (scene-scoped, re-entrant safe)     │
│  [NEW: m_debugCanvas, m_tweens[],     │                                       │
│   m_coroutines[], +persist ptr]       │                                       │
├───────────────────────────────────────┴─────────────────────────────────────┤
│                            Component Layer                                   │
│  C_Camera  C_Timer  C_StateMachine  C_Sprite  C_Tilemap                      │
│  [MODIFIED C_Camera: setFollowTarget()]                                      │
│  [NO new C++ UI components — engine.ui.* is stateless draw API]             │
├─────────────────────────────────────────────────────────────────────────────┤
│                       Scene / Object Layer                                   │
│  SceneStateMachine → Scene → ObjectCollection → Object → Component[]         │
│  [NEW PersistentObjectRegistry — SSM-owned, survives scene transitions]      │
├─────────────────────────────────────────────────────────────────────────────┤
│                       Graphics / Canvas Layer                                │
│  ICanvas<Pixel4>  LayerCompositor  Primitives<Pixel4>                        │
│  [DEBUG DRAW: dedicated high-index layer; no new canvas type]               │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component / Module | Responsibility | Location |
|--------------------|---------------|----------|
| `LuaBindings` | Registers engine.* Lua tables; owns LuaStore, LuaEventBus, camera ptr, tween pool, coroutine scheduler | `include/enjin2/scripting/bindings.hpp` + split `.cpp` files |
| `C_Camera` | World-to-screen offset, lerp follow, shake, bounds clamping; **[NEW]** follow-target tracking | `include/enjin2/components/camera.hpp` + `.cpp` |
| `LuaStore` | Fixed-capacity (16-key) JSON-backed KV persistence | embedded in `bindings.hpp` / `bindings_store.cpp` |
| `LuaEventBus` | Scene-scoped pub/sub, re-entrant-safe emit, hot-reload cleanup | `include/enjin2/scripting/lua_event_bus.hpp` |
| `Scene` | Object lifecycle, render pipeline, camera offset application | `include/enjin2/core/scene.hpp` |
| `SceneStateMachine` | Deferred transitions, scene registry; **[NEW]** persistent object migration | `include/enjin2/core/scene_state_machine.hpp` |
| `PersistentObjectRegistry` | **[NEW]** Fixed-capacity Object* array surviving scene transitions; SSM-owned | new `enjin2_core` class |

---

## Recommended Project Structure

Current split of `enjin2_lua` sources (already refactored; the "monolithic bindings.cpp" problem is partially solved):

```
src/scripting/
├── bindings.cpp                  # ScriptProxy metatable, ComponentProxy dispatch,
│                                 # registerAll(), LuaBindings constructor/getBindings
├── bindings_draw.cpp             # Drawing primitives, canvas ops
├── bindings_engine.cpp           # registerEngineTable(): all engine.* sub-table wiring
├── bindings_input_sprites.cpp    # Input polling + sprite pool
├── bindings_layers_text.cpp      # Layer system + text rendering
├── bindings_math.cpp             # Vec2/Point/Rect metatables + math globals
├── bindings_physics.cpp          # engine.physics.*
├── bindings_sprite_load.cpp      # Sprite asset loading
├── bindings_store.cpp            # LuaStore impl + engine.store.* bindings
├── bindings_system.cpp           # LuaScriptSystem glue
├── lua_engine.cpp
├── lua_event_bus.cpp
└── lua_platform.cpp

[ADD for v1.7:]
├── bindings_debug.cpp            # engine.debug.* — rect/circle/line overlay
├── bindings_tween.cpp            # engine.tween.* + TweenSlot pool tick
├── bindings_async.cpp            # engine.async.* + LuaCoroutineScheduler tick
└── bindings_ui.cpp               # engine.ui.* — stateless draw-only widgets
```

The remaining size issue in `bindings.cpp` (~1390 lines) is addressed by splitting proxy metatables:

```
[OPTIONAL split of existing bindings.cpp:]
├── bindings_proxy.cpp            # ScriptProxy + ObjectProxy + ComponentProxy metatables
└── bindings_register.cpp         # registerAll() orchestration only
```

### Structure Rationale

- **One bindings_*.cpp per engine.X sub-table**: Matches the established split strategy. Registration wired in `registerEngineTable()` in `bindings_engine.cpp`.
- **No new C++ component for UI**: `engine.ui.*` is stateless draw functions. Existing `FillUpGauge` and `Label` use `std::string`/`std::vector` and `ICanvas<uint8_t>`, making them incompatible with the Pixel4 zero-alloc pipeline.
- **C_Tween not a new component**: Tween state lives in `LuaBindings::m_tweens[]`. Component-owned tween would burn one of the 16 component slots per object with no benefit over a centralized pool.
- **PersistentObjectRegistry in enjin2_core**: Owned by `SceneStateMachine`, not by `LuaBindings`. Scene transitions happen at the C++ level, not just from Lua.

---

## Architectural Patterns

### Pattern 1: engine.* Sub-table Registration

**What:** Each feature domain gets an `engine.X` sub-table populated via `lua_newtable` + `luaBindFunctions`. Registration happens inside `registerEngineTable()` in `bindings_engine.cpp`.

**When to use:** Any new Lua-visible API. Always.

**Trade-offs:** Sub-table isolation prevents naming collisions. Minor stack overhead per nested access is negligible at this scale.

**Example (new `engine.debug` sub-table):**
```cpp
// In bindings_engine.cpp :: registerEngineTable()
static const LuaFuncDef kDebugFuncs[] = {
    {"rect",   lua_engine_debug_rect},
    {"circle", lua_engine_debug_circle},
    {"line",   lua_engine_debug_line},
    {"clear",  lua_engine_debug_clear},
};
lua_newtable(L);
luaBindFunctions(L, -1, kDebugFuncs, ENJIN_ARRAY_LEN(kDebugFuncs));
lua_setfield(L, -2, "debug");
```

### Pattern 2: Pointer-to-Pointer Registry for Injected State

**What:** Engine subsystem pointers (`m_ssm`, `m_activeScene`, `m_activeCamera`) are stored as `void**` lightuserdata in the Lua registry under keys like `"enjin_ssm"`. Static binding functions retrieve them at call time.

**When to use:** Any new C++ subsystem that Lua bindings need to reach. Used for: existing SSM, active scene, active camera. New: persistent registry pointer.

**Trade-offs:** Pointer-to-pointer allows the host to swap the pointed-at object without re-running `registerAll()`. One indirection per call; negligible cost.

**Example (persistent registry):**
```cpp
// In LuaBindings::registerAll():
lua_pushlightuserdata(L, &m_persistentRegistry);
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_persist_reg");

// In static binding function:
static PersistentObjectRegistry* getPersistReg(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_persist_reg");
    auto** pp = static_cast<PersistentObjectRegistry**>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return (pp && *pp) ? *pp : nullptr;
}
```

### Pattern 3: Fixed-Capacity State Arrays (Zero Alloc)

**What:** All mutable state for a new feature is stored as a fixed-size static array inside `LuaBindings`. Lua refs (`int`) are stored in parallel arrays. The sentinel `LUA_NOREF` marks empty slots.

**When to use:** Tween slot pool, coroutine registry. Mandatory given the zero-dynamic-allocation constraint.

**Tween pool example:**
```cpp
static constexpr int MAX_TWEENS = 8;
struct TweenSlot {
    float from{0}, to{0}, duration{0}, elapsed{0};
    int   callbackRef{LUA_NOREF};
    int   targetRef{LUA_NOREF};    // Lua registry ref to ObjectProxy
    char  property[16]{};          // "x", "y", "visible", etc.
    uint8_t easing{0};             // 0=linear, 1=easeIn, 2=easeOut, 3=easeInOut
    bool  active{false};
};
TweenSlot m_tweens[MAX_TWEENS];
```

### Pattern 4: Debug Draw via Dedicated Layer

**What:** Debug primitives are drawn onto a fixed layer (layer index N-1, highest available). The debug layer is cleared before each render pass. `Primitives<Pixel4>` provides all needed draw functions. No new canvas type required.

**When to use:** `engine.debug.rect()`, `engine.debug.circle()`, `engine.debug.line()`.

**Trade-offs:** Uses one of 8 available layers. On ESP32 the debug layer must be compiled out (`#ifndef NDEBUG` or a CMake option `ENJIN2_DEBUG_DRAW`). On platforms where it exists, the debug layer is composited on top by `LayerCompositor`.

**Integration:** `LuaBindings` gains `LuaCanvas* m_debugCanvas`. Set by the host alongside other layer canvases (extend `setLayers()` or add `setDebugCanvas()`). Debug draw functions write directly to `m_debugCanvas` via `LuaCanvas::drawRect` etc. (reuse existing `LuaCanvas` drawing methods).

**Null guard:** When `m_debugCanvas == nullptr` (release builds, ESP32), all `engine.debug.*` functions are no-ops that return immediately.

### Pattern 5: Lua Coroutines for Async Patterns

**What:** Lua's native `coroutine.create()` + `lua_resume()` / `lua_yield()` provide the async mechanism. A `LuaCoroutineScheduler` (8-slot fixed array of `int` Lua registry refs) ticks coroutines each frame.

**When to use:** Loading screens, cutscene sequencing, delayed sequences without nested C_Timer chains.

**Trade-offs:** `lua_newthread` allocates on the Lua heap (not C++ heap), satisfying zero-C++-allocation. Dead threads are collected by Lua GC when refs are released. The coroutine count cap (8) must be a compile-time constant.

**Critical lifecycle rule:** All coroutine refs must be `luaL_unref`'d on scene change and hot-reload, matching the `LuaEventBus::clearHandlers()` pattern exactly. This is called from `setActiveScene()` and `executeScript()`.

**Yield mechanism:** `engine.async.wait(seconds)` records the resume timestamp and returns immediately if the scheduled time has not elapsed:
```cpp
// Per-frame in bindings.tickCoroutines(dt):
for each active slot:
    if slot.resumeAt <= m_totalTime:
        status = lua_resume(thread, 0)  // resume with no args
        if status == LUA_YIELD: slot is still running (wait again)
        if status == LUA_OK:   slot is complete — unref + mark inactive
        if status == error:    log + mark inactive
```

---

## Data Flow

### Frame Update Flow (with v1.7 additions)

```
SDL/WASM host per frame:
    input_advance_frame()
    input_platform_poll()
    bindings.setInput(&inputState)
    bindings.setTimeState(dt, totalTime, frame)

    [NEW] bindings.tickTweens(dt)         // update TweenSlot pool, write to ObjectProxies
    [NEW] bindings.tickCoroutines(dt)     // resume yielded async coroutines

    ssm.update(dt)
      └── currentScene.update(dt)
            └── objects.forEach → obj.update(dt)
                  └── C_Camera.update(dt)       [lerp + shake + [NEW] follow target]
                  └── C_Timer.update(dt)
                  └── C_StateMachine.update(dt)
                  └── C_LuaScript.update(dt) → callWithProxy(update)

    // Debug layer cleared before render
    [NEW] if m_debugCanvas: m_debugCanvas->clear(TRANSPARENT)

    ssm.render(layers[0])
      └── currentScene.render(canvas)
            └── Scene::renderObjects() with cam offset

    LayerCompositor.composite(layers, output)   // existing
    // Debug layer is one of the layers — composited automatically on top
    blit output to GPU texture
```

### Persistent Objects Lifecycle

```
Host startup:
    SceneStateMachine created; PersistentObjectRegistry m_persistent is a value member.

Lua: engine.scene.persist(proxy)
    Validate ObjectProxy.
    Scene::removeObject(obj) → returns unique_ptr.
    m_persistent.add(std::move(ptr)).
    m_persistent.injectInto(currentScene.getObjects()) → adds to external[] non-owning array.

SceneStateMachine::applyDeferredTransition(targetId):
    m_persistent.withdrawFrom(currentScene.getObjects())   // remove from external[]
    currentScene.deactivate()                              // owned objects destroyed; persistent objects safe (SSM owns unique_ptr)
    currentScene = targetScene
    m_persistent.injectInto(currentScene.getObjects())     // add to new scene's external[]
    currentScene.initialize() if needed
    currentScene.activate()                                // start() called on persisted objects too

Lua: engine.scene.unpersist(proxy)
    m_persistent.withdrawFrom(currentScene.getObjects())
    unique_ptr = m_persistent.remove(obj)
    currentScene.addObject(std::move(ptr))                 // returns to scene ownership
```

### Camera Follow Target Data Flow

```
Lua: engine.camera.follow(proxy, speed)
    Validate ObjectProxy.valid.
    Store Object* in C_Camera::m_followTarget; store speed in m_followSpeed.

Per frame C_Camera::update(dt):
    if m_followTarget != nullptr:
        pos = m_followTarget->getComponent<C_Position>()->getPosition()
        lookAt(pos.x + halfCanvasW, pos.y + halfCanvasH, m_followSpeed)
        [if proxy->valid == false: clear m_followTarget]

Lua: engine.camera.stopFollow()
    C_Camera::clearFollowTarget()
```

### Tween Data Flow

```
Lua: local id = engine.tween.to(proxy, "x", 100, 0.5, "easeOut")
    C++: find free TweenSlot in m_tweens[]
         store: from = current proxy.x, to = 100, duration = 0.5
         store: easing = easeOut, targetRef = luaL_ref(proxy), property = "x"
         return slot index as handle

Per frame bindings.tickTweens(dt):
    for each active TweenSlot:
        elapsed += dt
        t = clamp(elapsed / duration, 0.0, 1.0)
        value = applyEasing(easing, t) * (to - from) + from
        lua_rawgeti(L, LUA_REGISTRYINDEX, targetRef)  // push ObjectProxy
        lua_pushnumber(L, value)
        lua_setfield(L, -2, property)                  // proxy.__newindex dispatch
        lua_pop(L, 1)
        if elapsed >= duration:
            if callbackRef != LUA_NOREF: fire callback, unref
            luaL_unref(targetRef)
            slot.active = false

Lua: engine.tween.cancel(id)
    m_tweens[id].active = false; unref targetRef + callbackRef
```

### Save/Load Serialization Data Flow (Extended)

```
Existing (already works on SDL3):
    engine.store.save(key, value)   → LuaStore::setX() + LuaStore::saveToFile() [#ifdef VCV_RACK]
    engine.store.load(key)          → LuaStore::get() → Lua value

v1.7 change: remove VCV_RACK guard, replace with proper platform detection:
    SDL3 / desktop  → std::fstream (already working)
    ESP32           → NVS stub (returns false; deferred implementation)
    WASM            → localStorage stub via emscripten_run_script or compile-time no-op

No API change — only the file I/O guard logic changes.
```

---

## Integration Points (New vs. Modified)

### Explicit New/Modified Table

| Item | New or Modified | Integration Point | File(s) |
|------|----------------|-------------------|---------|
| Debug draw bindings | **New** | `registerEngineTable()` adds `"debug"` sub-table; `LuaBindings` gains `m_debugCanvas` | `bindings_debug.cpp` (new) + `bindings.hpp` (add field) |
| Debug canvas wiring | **Modified** | `setLayers()` or new `setDebugCanvas()` in SDL main loop | `sdl_main.cpp`, `bindings.hpp` |
| Camera follow | **Modified** | `C_Camera::update()` gains follow-target logic; `engine.camera` sub-table gains `follow`/`stopFollow` | `camera.cpp`, `bindings_engine.cpp` |
| Save/load platform guard | **Modified** | Remove `#ifdef VCV_RACK` in `bindings_store.cpp`; add SDL/ESP32/WASM branches | `bindings_store.cpp` |
| Persistent objects | **New** | `PersistentObjectRegistry` class; `SceneStateMachine::applyDeferredTransition()` modified; `engine.scene.persist/unpersist` added | `include/enjin2/core/persistent_registry.hpp` (new), `scene_state_machine.hpp` (mod), `bindings_engine.cpp` (mod) |
| ObjectCollection external array | **Modified** | Non-owning `Object* m_external[]` array; `update()`/`lateUpdate()`/`forEach()` iterate it | `object_collection.hpp` + `object.hpp` if needed |
| Overflow tests | **New** | ctest suites: EventBus 16-channel/8-subscriber saturation, sprite pool 16-slot, component array | `tests/overflow_*_test.cpp` (new) |
| Null safety audit | **Modified** | Systematic pass through all `getBindings(L)` sites; add null guards where missing | all `bindings_*.cpp` |
| `bindings.cpp` split (optional) | **Modified** | Move ScriptProxy/ObjectProxy/ComponentProxy metatables to `bindings_proxy.cpp` | `bindings.cpp` → `bindings_proxy.cpp` (new) |
| Coroutines/async | **New** | `LuaCoroutineScheduler` embedded in `LuaBindings`; `tickCoroutines()` in SDL main loop; `engine.async` sub-table | `bindings_async.cpp` (new), `bindings.hpp` (add scheduler fields), `bindings_engine.cpp` (add sub-table) |
| Tween helpers | **New** | `m_tweens[]` pool in `LuaBindings`; `tickTweens(dt)` in SDL main loop; `engine.tween` sub-table | `bindings_tween.cpp` (new), `bindings.hpp` (add TweenSlot array), `bindings_engine.cpp` |
| UI component bindings | **New** | Stateless draw functions in `engine.ui` sub-table; no new C++ components | `bindings_ui.cpp` (new), `bindings_engine.cpp` |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| `LuaBindings` ↔ `C_Camera` | Direct pointer `m_activeCamera` (set by host) | Camera follow target is `Object*`; camera reads `C_Position` from target each frame; ObjectProxy invalidation must clear the follow ptr |
| `LuaBindings` ↔ debug layer | `m_debugCanvas*` pointer (set by host alongside other layers) | Null when `ENJIN2_DEBUG_DRAW` is off; all debug functions null-check before drawing |
| `LuaBindings` ↔ `PersistentObjectRegistry` | Pointer-to-pointer in Lua registry `"enjin_persist_reg"` | Registry owned by SceneStateMachine, not by LuaBindings |
| `LuaBindings` tween ↔ ObjectProxy | `lua_rawgeti` + `lua_setfield` dispatch — same path as `self.x = value` in Lua | Tween slot holds `int targetRef` (luaL_ref to ObjectProxy); stale check via `proxy->valid` |
| `LuaCoroutineScheduler` ↔ Lua state | `luaL_ref` thread refs cleared on scene change in `setActiveScene()` and in `executeScript()` (hot-reload) | Mirrors `LuaEventBus::clearHandlers()` exactly |
| `SceneStateMachine` ↔ `PersistentObjectRegistry` | Value member `m_persistent`; `withdrawFrom/injectInto` called in `applyDeferredTransition()` | No Lua involvement — pure C++ transition boundary |
| `LuaBindings` ↔ `LuaStore` | Direct member (not pointer); `m_storePath` triggers auto-save | Store platform I/O guard change is internal to `bindings_store.cpp` |

---

## New Components Detail

### 1. Debug Draw (`engine.debug.*`)

**C++ additions:** `LuaBindings` gains one new field:
```cpp
LuaCanvas* m_debugCanvas{nullptr};  // null = no debug layer (release/ESP32)
```

**`setDebugCanvas(LuaCanvas* canvas)` or extend `setLayers()`:**
The SDL host passes a pointer to the highest-indexed layer canvas as the debug canvas. Could also be a dedicated 9th canvas created outside the layer compositor — but using an existing layer slot is simpler and avoids changing `MAX_LUA_LAYERS`.

**Lua API:**
```lua
engine.debug.rect(x, y, w, h, color)
engine.debug.circle(x, y, radius, color)
engine.debug.line(x1, y1, x2, y2, color)
engine.debug.text(x, y, str, color)
engine.debug.clear()          -- also called automatically pre-render
```

**CMake guard:**
```cmake
option(ENJIN2_DEBUG_DRAW "Enable debug draw overlay layer" ON)  # OFF for ESP32 target
```
When `ENJIN2_DEBUG_DRAW=OFF`, `bindings_debug.cpp` registers no-op stubs. `m_debugCanvas` stays null.

### 2. Camera Follow Helpers

**`C_Camera` additions:**
```cpp
void setFollowTarget(Object* target, float lerpSpeed);  // lerpSpeed default 0.1
void clearFollowTarget();
Object* getFollowTarget() const { return m_followTarget; }

private:
    Object* m_followTarget{nullptr};
    float   m_followSpeed{0.1f};
```

`C_Camera::update(float dt)` gains (after existing lerp/shake logic):
```cpp
if (m_followTarget) {
    C_Position* pos = m_followTarget->getComponent<C_Position>();
    if (pos) {
        Point p = pos->getPosition();
        lookAt(p.x + m_canvasW / 2.0f, p.y + m_canvasH / 2.0f, m_followSpeed);
    }
}
```

**`engine.camera` sub-table additions (in `bindings_engine.cpp`):**
```lua
engine.camera.follow(objectProxy, speed)    -- speed optional, default 0.1
engine.camera.stopFollow()
```

**Stale follow target handling:** When `engine.scene.destroy(proxy)` is called, the ObjectProxy's `valid` flag becomes false. `C_Camera::update()` checks: if `m_followTarget` is set but its ObjectProxy ref is stale, clear follow target. Implementation: store `int followProxyRef` alongside `m_followTarget` so `update()` can validate via `proxy->valid`.

### 3. Persistent Objects Across Scenes

**`PersistentObjectRegistry` (new, `enjin2_core`):**
```cpp
class PersistentObjectRegistry {
public:
    static constexpr int MAX_PERSISTENT = 16;

    // SSM takes ownership from Scene's ObjectCollection
    bool add(std::unique_ptr<Object> obj);
    // SSM returns ownership to Scene on unpersist
    std::unique_ptr<Object> remove(Object* obj);
    bool contains(const Object* obj) const;

    // Scene transition hooks (called by SSM)
    void injectInto(ObjectCollection& col);    // adds to col.m_external[]
    void withdrawFrom(ObjectCollection& col);  // removes from col.m_external[]

    int count() const { return m_count; }
    Object* get(int i) { return m_owned[i].get(); }

private:
    std::unique_ptr<Object> m_owned[MAX_PERSISTENT]{};
    int m_count{0};
};
```

**`ObjectCollection` modification:**
```cpp
// New non-owning external array (for persistent objects borrowed from SSM)
static constexpr size_t MAX_EXTERNAL = 16;
Object* m_external[MAX_EXTERNAL]{};
size_t  m_externalCount{0};

bool injectExternal(Object* obj);    // add to m_external[]
bool withdrawExternal(Object* obj);  // remove from m_external[]
// update(), lateUpdate(), forEach() iterate m_external[] alongside owned m_objects[]
```

**Lua API extensions to `engine.scene`:**
```lua
engine.scene.persist(proxy)        -- SSM takes ownership; object survives scene switch
engine.scene.unpersist(proxy)      -- return to current scene's owned collection
engine.scene.find("name")          -- checks BOTH currentScene AND persistentRegistry
```

`engine.scene.find()` modification: after checking `currentScene->findByName()`, also call `m_persistent.findByName()` if not found.

### 4. Coroutine/Async Pattern

**`LuaCoroutineScheduler` (embedded in `LuaBindings`):**
```cpp
static constexpr int MAX_COROUTINES = 8;
struct CoroutineSlot {
    int   threadRef{LUA_NOREF};  // luaL_ref to Lua thread (lua_State*)
    float resumeAt{0.0f};        // total time when to resume
    bool  active{false};
};
CoroutineSlot m_coroutines[MAX_COROUTINES];

void tickCoroutines();    // called each frame before ssm.update()
void clearCoroutines();   // called from setActiveScene() and executeScript() (hot-reload)
```

**`tickCoroutines()` implementation:**
```cpp
void LuaBindings::tickCoroutines() {
    lua_State* L = engine->getState();
    for (int i = 0; i < MAX_COROUTINES; ++i) {
        if (!m_coroutines[i].active) continue;
        if (m_timeState.totalTime < m_coroutines[i].resumeAt) continue;
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_coroutines[i].threadRef);
        lua_State* thread = lua_tothread(L, -1);
        lua_pop(L, 1);
        int status = lua_resume(thread, L, 0);
        if (status == LUA_OK) {
            // coroutine finished
            luaL_unref(L, LUA_REGISTRYINDEX, m_coroutines[i].threadRef);
            m_coroutines[i] = {};
        } else if (status != LUA_YIELD) {
            printf("[async] coroutine error: %s\n", lua_tostring(thread, -1));
            luaL_unref(L, LUA_REGISTRYINDEX, m_coroutines[i].threadRef);
            m_coroutines[i] = {};
        }
        // LUA_YIELD: leave active, will check resumeAt again next frame
    }
}
```

**`engine.async.wait(seconds)` implementation:**
```lua
-- In Lua (registered C function stores resumeAt, then yields):
engine.async.wait(1.5)   -- stores totalTime + 1.5 in slot.resumeAt, calls lua_yield
engine.async.wait_frames(30)  -- stores frame + 30 in slot, yields
```

The C binding calls `lua_yield(L, 0)` directly — Lua resumes on the next `tickCoroutines()` call where `resumeAt` is satisfied.

**Lua API:**
```lua
local co = coroutine.create(function()
    engine.async.wait(2.0)
    engine.log("two seconds passed")
    engine.async.wait(1.0)
    engine.log("one more second")
end)
local id = engine.async.start(co)   -- registers in scheduler, returns slot id
engine.async.cancel(id)             -- unref + mark inactive
```

### 5. Tween Helpers

**Lua API:**
```lua
-- Tween object property over time
local id = engine.tween.to(proxy, "x", 100, 0.5)           -- linear
local id = engine.tween.to(proxy, "x", 100, 0.5, "easeOut")-- with easing

-- Value-only tween (no object target) — useful for custom blending
local id = engine.tween.value(0, 1, 0.3, function(v)
    engine.graphics.setColor(math.floor(v * 14))
end)

engine.tween.cancel(id)
engine.tween.cancelAll()
```

**Supported easing (compile-time static functions):**
- `linear`, `easeIn` (t²), `easeOut` (1-(1-t)²), `easeInOut` (cubic Hermite)

**`tickTweens(float dt)`** iterates `m_tweens[]`, updates `elapsed`, computes eased value, writes to ObjectProxy via `lua_setfield` (same dispatch path as `self.x = value` from Lua scripts), fires `callbackRef` on completion.

**Stale target guard:** Before each write, check `proxy->valid`. If false, cancel the slot silently.

### 6. UI Component Bindings (`engine.ui.*`)

**Decision: stateless draw functions, not C++ components.**

The existing `FillUpGauge`, `Label`, `Slider` in `include/enjin2/components/` use `std::string`, `std::vector`, internal `Canvas8<>` buffers, and target `ICanvas<uint8_t>`. They are incompatible with the Pixel4 pipeline and violate zero-alloc for embedded.

**`engine.ui.*` implemented as stateless Lua-callable draw functions in `bindings_ui.cpp`:**

```lua
-- Progress bar (horizontal fill from left)
engine.ui.progressBar(x, y, w, h, value, fgColor, bgColor)
-- value: 0.0 to 1.0; uses fillRect + drawRect from LuaCanvas

-- Stat bar (integer current/max, same visual as progressBar)
engine.ui.statBar(x, y, w, h, current, maximum, fgColor, bgColor)

-- Panel (rounded-rect or flat background)
engine.ui.panel(x, y, w, h, bgColor, borderColor)

-- Label (text with background)
engine.ui.label(x, y, w, h, text, textColor, bgColor)
```

Each function calls existing `LuaCanvas` methods (`fillRect`, `drawRect`, `text`). No state, no allocation, no C++ component required.

**Internal developer guide note:** For animated UI (bars that fill over time, blinking indicators), compose with existing tools:
- Use `engine.tween.to()` to animate a Lua variable.
- Call `engine.ui.progressBar()` in `draw()` with the animated value.
- This is deliberately simpler than a stateful UI widget system.

---

## Build Order

Dependencies determine phase ordering. Phases with no arrows between them can be built in parallel.

```
Phase A: bindings.cpp refactoring (split to bindings_proxy.cpp)
  → Reduces file size for all subsequent phases.
  → No new features; isolatable change.
  → Dependencies: none.

Phase B: Overflow tests + null safety
  → Discover existing overflow paths first; fix null guards.
  → Dependencies: none (can run parallel to Phase A).

Phase C: Debug draw bindings
  → Needs bindings structure stable (Phase A).
  → Add ENJIN2_DEBUG_DRAW CMake option.
  → Add m_debugCanvas to LuaBindings (bindings.hpp + sdl_main.cpp).
  → bindings_debug.cpp + engine.debug sub-table in bindings_engine.cpp.
  → Dependencies: Phase A.

Phase D: Camera follow helpers
  → Modify C_Camera (src/components/camera.cpp).
  → Modify bindings_engine.cpp (engine.camera sub-table additions).
  → Dependencies: Phase A.

Phase E: Save/load serialization hardening
  → Remove VCV_RACK guard in bindings_store.cpp; add SDL/ESP32/WASM branches.
  → No API change.
  → Dependencies: Phase B (null safety first).

Phase F: Persistent objects
  → New PersistentObjectRegistry in enjin2_core.
  → Modify ObjectCollection (m_external[]).
  → Modify SceneStateMachine::applyDeferredTransition().
  → Modify engine.scene.persist/unpersist/find in bindings_engine.cpp.
  → Most invasive structural change — build last among core features.
  → Dependencies: Phase B (null safety), Phase E (persistence integration).

Phase G: Coroutines/async
  → LuaCoroutineScheduler embedded in LuaBindings.
  → bindings_async.cpp + engine.async sub-table.
  → tickCoroutines() wired in sdl_main.cpp.
  → clearCoroutines() called from setActiveScene() + executeScript().
  → Dependencies: Phase A.

Phase H: Tween helpers
  → m_tweens[] + TweenSlot in LuaBindings.
  → bindings_tween.cpp + engine.tween sub-table.
  → tickTweens() wired in sdl_main.cpp.
  → Dependencies: Phase D (ObjectProxy write path; camera follow establishes the pattern).

Phase I: UI component bindings
  → bindings_ui.cpp: stateless draw functions only.
  → engine.ui sub-table wired in bindings_engine.cpp.
  → Dependencies: Phase A, Phase C (establishes the stateless draw pattern for bindings).
```

**Parallelizable groups:**
- Phase A + Phase B (fully independent)
- Phase C + Phase D + Phase G (all depend only on A; independent of each other)
- Phase H + Phase I (depend on A; H depends on D implicitly)

**Suggested milestone delivery order:**
1. A + B (foundation)
2. C + D + E (quick wins with clear C++ targets)
3. F (most invasive; all prior phases stable)
4. G + H + I (new Lua capabilities on top of stable base)

---

## Anti-Patterns

### Anti-Pattern 1: Tween/Coroutine State in Lua Tables

**What people do:** Store tween or coroutine scheduling state as Lua tables in the global environment.

**Why it's wrong:** Lua tables are GC-managed and do not survive hot-reload (full Lua state destroy/recreate). GC pause on ESP32 with many active tables causes frame spikes.

**Do this instead:** All scheduler state in C++ fixed-capacity arrays inside `LuaBindings`. Use `luaL_ref` for Lua callbacks only.

### Anti-Pattern 2: New C++ Component for Every UI Element

**What people do:** Create `C_ProgressBar extends Component` with update/draw for each widget type.

**Why it's wrong:** Each component requires an Owner Object, occupies one of 16 component slots, and incurs the full awake/start/update lifecycle overhead for what is essentially a stateless draw call.

**Do this instead:** `engine.ui.*` stateless draw functions. For animated state, compose with `engine.tween.to()` driving a Lua variable, read in `draw()`.

### Anti-Pattern 3: Persistent Object Pointer Without Re-injection

**What people do:** Store `Object*` pointers across scene transitions without re-registering them with the new scene's ObjectCollection.

**Why it's wrong:** `Scene::renderObjects()` and `ObjectCollection::update()` iterate only the scene's own collections. Unregistered objects are invisible and frozen.

**Do this instead:** `PersistentObjectRegistry::injectInto(newScene.getObjects())` is called by SSM before `newScene.activate()`. Persistent objects appear in the new scene's `m_external[]` and participate normally in update/render.

### Anti-Pattern 4: Debug Draw Accumulation List

**What people do:** Accumulate debug shapes into a list each frame and render at end of frame.

**Why it's wrong:** Growing list; requires explicit list management; violates zero-alloc.

**Do this instead:** Debug layer cleared at start of each render pass. Scripts call `engine.debug.X()` during their `draw()` callback, which writes directly to the debug canvas. No accumulation; no list.

### Anti-Pattern 5: Forgetting Coroutine/Tween Cleanup on Scene Change

**What people do:** Start coroutines or tweens in one scene, switch scenes, and leave the scheduler populated with stale refs that target destroyed objects.

**Why it's wrong:** The `tickCoroutines()` / `tickTweens()` functions will attempt to resume threads or write to ObjectProxies from destroyed objects. The proxy `valid` flag catches tween staleness, but coroutine threads may access globals that no longer exist.

**Do this instead:** `LuaBindings::clearCoroutines()` and `LuaBindings::cancelAllTweens()` called from `setActiveScene()`. This matches `LuaEventBus::clearHandlers()` and `C_Timer::clearTimers()` — the established scene-change cleanup pattern.

---

## Sources

- `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` — LuaBindings class, LuaStore, LuaCanvas (HIGH confidence)
- `/home/unwn/dev/enjin/src/scripting/bindings_engine.cpp` — registerEngineTable() and all sub-table registrations (HIGH confidence)
- `/home/unwn/dev/enjin/src/scripting/bindings_store.cpp` — LuaStore impl, VCV_RACK platform guard, JSON I/O (HIGH confidence)
- `/home/unwn/dev/enjin/include/enjin2/components/camera.hpp` — C_Camera full API (HIGH confidence)
- `/home/unwn/dev/enjin/include/enjin2/core/scene.hpp` — Scene lifecycle, renderObjects() with camera offset (HIGH confidence)
- `/home/unwn/dev/enjin/include/enjin2/core/scene_state_machine.hpp` — applyDeferredTransition() hook point (HIGH confidence)
- `/home/unwn/dev/enjin/include/enjin2/graphics/primitives.hpp` — Primitives<Pixel4> (reused by debug draw) (HIGH confidence)
- `/home/unwn/dev/enjin/include/enjin2/core/component.hpp` — ComponentProxy invalidation pattern (HIGH confidence)
- `/home/unwn/dev/enjin/include/enjin2/components/fill_up_gauge.hpp` — shows why existing UI components are not reusable for Pixel4 pipeline (HIGH confidence)
- `/home/unwn/dev/enjin/include/enjin2/components/label.hpp` — confirms std::string/std::vector usage (HIGH confidence)
- `/home/unwn/dev/enjin/CMakeLists.txt` — enjin2_lua target_sources structure (HIGH confidence)
- `/home/unwn/dev/enjin/.planning/PROJECT.md` — v1.7 feature list, constraints, key decisions, tech debt (HIGH confidence)

---

*Architecture research for: enjin2 v1.7 Developer Experience and New Capability*
*Researched: 2026-03-01*
