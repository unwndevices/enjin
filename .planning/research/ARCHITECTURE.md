# Architecture Research

**Domain:** Embedded/WASM 2D graphics engine — v1.5 Lua Scripting Foundation
**Researched:** 2026-02-26
**Confidence:** HIGH (direct codebase inspection; all claims verified against live source)

---

## Current System Overview (post-v1.4)

```
┌─────────────────────────────────────────────────────────────────────┐
│                    SDL3 Runner (sdl_main.cpp)                        │
│  g_compositor (LayerCompositor<128,128>)  g_input (InputState)      │
│  g_lua (LuaScriptSystem)  g_rgb_staging[128*128*3]                  │
│  performReload() — full Lua lifecycle in one place                  │
│  Frame: advance+poll input → callFunction("update",dt) → draw       │
│           → compositor.composite() → expand_rgb → SDL_blit         │
└────────────┬────────────────────────────────────────────────────────┘
             │ LuaCanvas*[]  InputState*  bool* (visible array)
┌────────────▼────────────────────────────────────────────────────────┐
│                Scripting Layer (enjin2_lua)                          │
│  LuaScriptSystem → LuaEngine (lua_State + static pool allocator)    │
│  LuaBindings: canvas globals, palette, input polling, sprite pool   │
│               layer system (setLayer/clearLayer/getLayerCount)      │
│  g_currentBindings — static ptr recovered via LUA_REGISTRYINDEX     │
└────────────┬────────────────────────────────────────────────────────┘
             │ ICanvas<Pixel4>&
┌────────────▼────────────────────────────────────────────────────────┐
│                Graphics Layer (enjin2_graphics)                      │
│  LayerCompositor<W,H>  Canvas4<W,H>  Palette  Primitives           │
│  SpriteSheet (header-only)  Sprite  C_Sprite                        │
└────────────┬────────────────────────────────────────────────────────┘
             │
┌────────────▼────────────────────────────────────────────────────────┐
│                  Core Layer (enjin2_core)                            │
│  Object  Component  Scene  SceneStateMachine  Signal                │
│  ObjectCollection  memory.hpp  types.hpp (Pixel4, Point, Rect)      │
└─────────────────────────────────────────────────────────────────────┘
```

**Frame sequence (v1.4, SDL3 runner):**

```
input_advance_frame(&g_input)
input_platform_poll(&g_input)
g_compositor.clearAll()
bindings.setInput(&g_input)
g_lua.callFunction("update", dt)    → Lua update(dt)
g_lua.callFunction("draw")          → Lua draw(), targets layerCanvases[]
g_compositor.composite()            → painters-order, index-15 transparent
expand_canvas_to_rgb()
SDL_UpdateTexture + SDL_RenderTexture
```

---

## V1.5 Feature Integration Map

### Feature 1: Fix onRender Pixel4 Bug

**Location:** `include/enjin2/core/scene.hpp` lines 116-126.

**What is broken:** The `render<PixelType>()` template has an `if constexpr` guard that only calls `onRender(canvas)` when `PixelType == uint8_t`. The `Pixel4` overload is declared virtual but never dispatched. Any derived Scene that overrides `onRender(ICanvas<Pixel4>&)` is silently skipped every frame.

**Fix:** Remove the `if constexpr` guard. Call `onRender(canvas)` unconditionally before `renderObjects(canvas)`. The two virtual overloads (`Pixel4` and `uint8_t`) dispatch correctly via C++ overload resolution.

**Files modified:**
- `include/enjin2/core/scene.hpp` — remove guard (3-line change)

**Module:** enjin2_core. No new files. No CMake change.

---

### Feature 2: float dt Everywhere

**What changes:** Every `update(uint16_t deltaTime)` and `lateUpdate(uint16_t deltaTime)` becomes `update(float dt)` and `lateUpdate(float dt)`. Time unit becomes seconds, not milliseconds.

**Pervasive scope:**
- `include/enjin2/core/component.hpp` — Component::update, lateUpdate signatures
- `include/enjin2/core/object.hpp` — Object::update, lateUpdate signatures
- `src/core/object.cpp` — Object::update, lateUpdate implementations
- `include/enjin2/core/scene.hpp` — Scene::update, onUpdate signatures
- `include/enjin2/core/object_collection.hpp` — ObjectCollection::update, lateUpdate
- `include/enjin2/core/scene_state_machine.hpp` — SceneStateMachine::update, updateTransition, TRANSITION_TIME becomes float
- `include/enjin2/components/lua_script.hpp` — C_LuaScript::update
- `src/components/lua_script.cpp` — C_LuaScript::update (also kills the `/1000.0` cast in update)
- All other concrete Component subclasses: C_Sprite, C_Drawable, C_Animation, etc.
- `src/platform/sdl/sdl_main.cpp` — already uses `float dt` at the runner level; no change needed there

**SDL3 runner alignment:** The runner already computes `float dt` in seconds and passes it to `g_lua.callFunction("update", dt)`. After this change, the same `dt` flows into C++ component updates without any unit conversion. This eliminates the `static_cast<double>(deltaTime) / 1000.0` pattern in `C_LuaScript::update()`.

**Module:** enjin2_core, enjin2_lua (component side). No new files, no CMake change.

---

### Feature 3: Named Objects + Tags

**What changes:** Two independent additions to enjin2_core.

**Named lookup on Object/ObjectCollection:**

Add `std::string name` field to `Object`. Add name-keyed lookup to `ObjectCollection`:

```
Object (modified):
  + std::string name         — empty = unnamed
  + getName() / setName()

ObjectCollection (modified):
  + std::array<..., MAX_OBJECTS> name_index — parallel name array (no map, no heap)
  + findByName(const char* name) -> Object*
  + registerByName(const char* name, Object* obj)
```

Zero-alloc constraint means no `std::unordered_map`. Use a parallel `std::array` of `std::string_view` pointing into the Object's `name` field, or a linear scan over `MAX_OBJECTS=128` slots. Linear scan is O(n=128) — acceptable since name lookup happens on events, not every frame. If performance is critical, a sorted array with binary search is feasible at zero heap cost.

**Tags on Object:**

```cpp
// Object (modified):
static constexpr size_t MAX_TAGS = 8;
std::array<const char*, MAX_TAGS> tags{};   // null-terminated slots
size_t tagCount = 0;

void addTag(const char* tag);
bool hasTag(const char* tag) const;
void removeTag(const char* tag);
```

Tags are string literals (caller owns lifetime). The `const char*` array holds pointers only — zero allocation, compatible with ESP32 flash strings.

`ObjectCollection::findAllWithTag(const char* tag)` returns matches into a caller-provided buffer (no heap):

```cpp
size_t findAllWithTag(const char* tag, Object** results, size_t maxResults);
```

**Lua surface:** `engine.scene.find("name")` calls `ObjectCollection::findByName()` on the active scene. This is the primary consumer of this feature.

**Files modified:**
- `include/enjin2/core/object.hpp` — add name, tags
- `src/core/object.cpp` — initialize name/tags fields
- `include/enjin2/core/object_collection.hpp` — add findByName, findAllWithTag

**Module:** enjin2_core. No new files, no CMake change.

---

### Feature 4: Scene Self-Transitions

**What changes:** Scene gains a non-owning `SceneStateMachine*` pointer injected at activation. The Scene does not own it — pointer is valid only during the active lifetime.

**Current state:** `Scene::activate()` takes no arguments. `SceneStateMachine` holds ownership of all scenes and drives them. A scene cannot request a scene change from within its own logic without an external callback chain.

**Fix:** Add overload to `Scene::activate()`:

```cpp
// Scene (modified):
SceneStateMachine* stateMachine = nullptr;  // non-owning, valid while active

void activate(SceneStateMachine* sm = nullptr) {
    stateMachine = sm;
    // ... existing activate logic ...
}

// Derived scene can then:
void onUpdate(float dt) override {
    if (condition) {
        stateMachine->changeScene(SCENE_ID_NEXT);
    }
}
```

**SceneStateMachine change:** In `completeTransition()`, `startTransition()`, and the `addScene()` flow, pass `this` to `scene->activate(this)`.

**Circular header dependency:** `Scene` currently includes `SceneStateMachine` indirectly via `scene.hpp` including `scene_state_machine.hpp`. To avoid making `scene.hpp` include `scene_state_machine.hpp` (which itself includes `scene.hpp`), use a forward declaration:

```cpp
// In scene.hpp: forward declare
class SceneStateMachine;

// In scene_state_machine.hpp: include scene.hpp as normal
```

This is already the correct direction — `SceneStateMachine` owns `Scene`, not vice versa. The Scene only stores a raw pointer.

**Files modified:**
- `include/enjin2/core/scene.hpp` — forward declare SceneStateMachine, add sm pointer, update activate()
- `include/enjin2/core/scene_state_machine.hpp` — pass `this` in activate() calls

**Module:** enjin2_core. No new files, no CMake change.

---

### Feature 5: engine.* Global Table

**What changes:** LuaBindings registers a structured `engine` global table replacing the current flat global registrations for input. Existing drawing globals (`rectangle`, `circle`, etc.) remain as flat globals for backward compatibility.

**Table structure:**

```lua
engine = {
  scene  = { switch(id), id(), find(name) },
  input  = { held(btn), just_pressed(btn), just_released(btn), axis(n) },
  time   = { now(), frame(), delta() },
  lua    = { collect(), memory() },
  log    = function(...)
}
```

**C++ registration pattern** (in `LuaBindings::registerAll()`):

```cpp
// Create engine table
lua_newtable(L);                        // push engine table

// engine.input subtable
lua_newtable(L);
lua_pushcfunction(L, lua_engine_held);
lua_setfield(L, -2, "held");
// ... etc ...
lua_setfield(L, -2, "input");           // engine.input = subtable

// engine.scene subtable
lua_newtable(L);
// ... lua_engine_scene_switch, find, id ...
lua_setfield(L, -2, "scene");

// engine.time subtable
lua_newtable(L);
// ... now, frame, delta ...
lua_setfield(L, -2, "time");

// engine.lua subtable
lua_newtable(L);
lua_pushcfunction(L, lua_engine_lua_collect);
lua_setfield(L, -2, "collect");
lua_pushcfunction(L, lua_engine_lua_memory);
lua_setfield(L, -2, "memory");
lua_setfield(L, -2, "lua");

// engine.log
lua_pushcfunction(L, lua_engine_log);
lua_setfield(L, -2, "log");

lua_setglobal(L, "engine");             // _G.engine = table
```

**Pointer wiring for engine.scene and engine.input:**

`LuaBindings` needs `SceneStateMachine*` and `InputState*` pointers. The `InputState*` is already present (`currentInput`). `SceneStateMachine*` must be added.

`LuaBindings` (modified):
- Add `SceneStateMachine* currentScene = nullptr`
- Add `setSceneStateMachine(SceneStateMachine* sm)` setter
- `engine.scene.switch(id)` calls `currentScene->changeScene(id)`
- `engine.scene.id()` calls `currentScene->getCurrentScene()->getId()`
- `engine.scene.find(name)` calls active scene's `objects.findByName(name)`, returns a ScriptProxy or nil

**Where is setSceneStateMachine() called?** In `performReload()` in `sdl_main.cpp`, after `lua.initialize()`:

```cpp
lua.getBindings().setSceneStateMachine(&g_scene_machine);
```

This requires `g_scene_machine` to be a static global in `sdl_main.cpp`. Currently the runner is stateless on the ECS side — scripts call drawing functions directly with no scene object. A SceneStateMachine is only needed if the host uses it. For the SDL runner that runs flat scripts, a stub is sufficient. For a full ECS app, the host wires the real SceneStateMachine.

**engine.time.now():** Uses `SDL_GetTicks()` (SDL runner) or a static counter (WASM/ESP32). Store accumulated time in `LuaBindings` as `float engineTimeSeconds` updated each frame by the host calling `setTime(float t)`.

**engine.time.delta():** Store `float lastDt` in `LuaBindings`, updated each frame. Scripts that use the `update(dt)` form don't need `delta()` but it's useful in callbacks.

**Files modified:**
- `include/enjin2/scripting/bindings.hpp` — add SceneStateMachine*, engineTime, lastDt, new static methods
- `src/scripting/bindings.cpp` — register engine.* table in registerAll(), implement engine.* handlers
- `src/platform/sdl/sdl_main.cpp` — call setSceneStateMachine() and setTime(dt) in performReload/game loop

**Module:** enjin2_lua (LuaBindings). Optional: enjin2_sdl (sdl_main.cpp wiring).

---

### Feature 6: Self Proxy (ScriptProxy Userdata)

**What changes:** Before calling each Lua lifecycle callback, the engine pushes a `self` userdata (ScriptProxy) as the first argument. Script callbacks become `update(self, dt)`, `draw(self)`, `init(self)`.

**ScriptProxy struct** (new, lives in `enjin2_lua`):

```cpp
// include/enjin2/scripting/script_proxy.hpp  (NEW FILE)
namespace enjin2 {

struct ScriptProxy {
    Object* object;           // non-owning; may be null if object destroyed
    C_LuaScript* script;      // owning script component

    // Metatable name registered with luaL_newmetatable
    static constexpr const char* METATABLE = "enjin2.ScriptProxy";

    static void push(lua_State* L, Object* obj, C_LuaScript* script);
    static ScriptProxy* check(lua_State* L, int idx);

    // Metamethods registered on METATABLE
    static int index(lua_State* L);      // __index: field reads
    static int newindex(lua_State* L);   // __newindex: field writes
    static int gc(lua_State* L);         // __gc: invalidate (optional)
};

} // namespace enjin2
```

**Supported self fields:**

| Field | C++ mapping | Read | Write |
|-------|-------------|------|-------|
| `self.x` | `C_Position::x` | yes | yes |
| `self.y` | `C_Position::y` | yes | yes |
| `self.visible` | `C_Drawable::visible` | yes | yes |
| `self.layer` | `C_Drawable::buffer_index` | yes | yes |
| `self.name` | `Object::name` | yes | yes |
| `self.active` | `Object::active` | yes | yes |

`__index` in C++ (`ScriptProxy::index`):

```cpp
int ScriptProxy::index(lua_State* L) {
    ScriptProxy* p = check(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (!p->object) { lua_pushnil(L); return 1; }

    if (strcmp(key, "x") == 0) {
        auto* pos = p->object->getComponent<C_Position>();
        lua_pushnumber(L, pos ? pos->x : 0.0f);
    } else if (strcmp(key, "visible") == 0) {
        auto* d = p->object->getComponent<C_Drawable>();
        lua_pushboolean(L, d ? d->isVisible() : false);
    }
    // ... etc
    return 1;
}
```

**Call site in C_LuaScript::update(float dt):**

```cpp
void C_LuaScript::update(float dt) {
    lua_State* L = scriptSystem->getEngine().getState();
    lua_getglobal(L, "update");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    ScriptProxy::push(L, owner, this);   // push self as arg 1
    lua_pushnumber(L, dt);               // push dt as arg 2
    lua_pcall(L, 2, 0, 0);              // update(self, dt)
}
```

**Where ScriptProxy lives:** `enjin2_lua`. It requires `Object`, `C_Position`, `C_Drawable` — all in `enjin2_core`/`enjin2_graphics`. The `enjin2_lua` target already links `enjin2_graphics` and `enjin2_ui` (which link core), so no new CMake dependency.

**Files created:**
- `include/enjin2/scripting/script_proxy.hpp` — NEW
- `src/scripting/script_proxy.cpp` — NEW (metatable registration and field dispatch)

**Files modified:**
- `src/components/lua_script.cpp` — update/draw call sites push ScriptProxy
- `include/enjin2/components/lua_script.hpp` — update signature changes to float dt
- `CMakeLists.txt` — add `src/scripting/script_proxy.cpp` to enjin2_lua sources

---

### Feature 7: ScriptErrorPolicy on C_LuaScript

**What changes:** Replace the current implicit `scriptError = true; stop calling` behavior with an explicit enum.

**Enum** (add to `lua_script.hpp`):

```cpp
enum class ScriptErrorPolicy : uint8_t {
    Disable,   // disable component on first error, log once (DEFAULT)
    Log,       // log every frame error, keep calling (debug only)
    Panic,     // call platform panic handler
};
```

**C_LuaScript (modified):**

```cpp
class C_LuaScript : public C_Drawable {
    ScriptErrorPolicy errorPolicy = ScriptErrorPolicy::Disable;
    // ...
public:
    void setErrorPolicy(ScriptErrorPolicy p) { errorPolicy = p; }
    ScriptErrorPolicy getErrorPolicy() const { return errorPolicy; }
};
```

**Error handling in callScriptFunctionSafe():**

```cpp
bool C_LuaScript::callScriptFunctionSafe(const char* fn) {
    LuaResult r = scriptSystem->callFunction(fn);
    if (!r.success) {
        switch (errorPolicy) {
            case ScriptErrorPolicy::Disable:
                scriptError = true;   // disables component
                errorMessage = r.error;
                // log once only
                break;
            case ScriptErrorPolicy::Log:
                // log but continue; do NOT set scriptError
                break;
            case ScriptErrorPolicy::Panic:
                // platform-defined panic
                break;
        }
        return false;
    }
    return true;
}
```

**Files modified:**
- `include/enjin2/components/lua_script.hpp` — add enum, policy field, setter/getter
- `src/components/lua_script.cpp` — update callScriptFunctionSafe()

**Module:** enjin2_lua (component side). No new files, no CMake change.

---

### Feature 8: Input Event Callbacks

**What changes:** After each frame's `input_advance_frame` + `input_platform_poll`, if any button transitions, C_LuaScript calls `on_button_pressed(self, btn)` or `on_button_released(self, btn)` on the Lua script.

**Data flow:**

```
input_advance_frame(&g_input)
input_platform_poll(&g_input)
     ↓
for each bit 0-15:
    if g_input.justPressed(btn):
        for each active C_LuaScript:
            callCallback("on_button_pressed", btn)
    if g_input.justReleased(btn):
        for each active C_LuaScript:
            callCallback("on_button_released", btn)
```

**Who drives the dispatch?** Two options:

*Option A (recommended) — LuaBindings drives it:*
Add `dispatchInputEvents()` to `LuaBindings`, called by the host after polling. `LuaBindings` already holds `currentInput`. It checks edge state and calls Lua callbacks directly on the shared global Lua state.

```cpp
// LuaBindings:
void dispatchInputEvents() {
    if (!currentInput) return;
    lua_State* L = engine->getState();
    for (int btn = 0; btn < 16; ++btn) {
        if (currentInput->justPressed(btn)) {
            lua_getglobal(L, "on_button_pressed");
            if (lua_isfunction(L, -1)) {
                lua_pushinteger(L, btn);
                lua_pcall(L, 1, 0, 0);
            } else {
                lua_pop(L, 1);
            }
        }
        if (currentInput->justReleased(btn)) {
            lua_getglobal(L, "on_button_released");
            if (lua_isfunction(L, -1)) {
                lua_pushinteger(L, btn);
                lua_pcall(L, 1, 0, 0);
            } else {
                lua_pop(L, 1);
            }
        }
    }
}
```

*Option B — C_LuaScript drives it:*
Each C_LuaScript queries InputState in its `update()` and dispatches locally. Requires each C_LuaScript to hold an `InputState*`.

**Recommendation: Option A.** The current architecture has a single shared Lua state (one `LuaScriptSystem` per runner). Dispatching from `LuaBindings` is consistent with how `engine.input` functions already work and avoids per-component InputState pointers.

**sdl_main.cpp addition:**

```cpp
// After input_platform_poll, before update/draw:
g_lua.getBindings().dispatchInputEvents();
g_lua.getBindings().setInput(&g_input);
```

**ScriptProxy injection for callbacks:** If callbacks follow the `self`-first convention, `dispatchInputEvents()` needs access to the ScriptProxy for the active script. For the flat-script model (single global Lua state, no C_LuaScript hierarchy), callbacks are called as `on_button_pressed(btn)` without `self`. For the component model, C_LuaScript should drive dispatch instead.

For v1.5, the flat-script model is the primary target. Callbacks are plain globals: `function on_button_pressed(btn) ... end`.

**Files modified:**
- `include/enjin2/scripting/bindings.hpp` — add `dispatchInputEvents()` declaration
- `src/scripting/bindings.cpp` — implement `dispatchInputEvents()`
- `src/platform/sdl/sdl_main.cpp` — call `dispatchInputEvents()` in game loop

**Module:** enjin2_lua. No new files, no CMake change.

---

### Feature 9: GC Control (engine.lua.collect / memory)

**What changes:** Expose two functions under `engine.lua`:

```lua
engine.lua.collect()    -- runs a full GC cycle: lua_gc(L, LUA_GCCOLLECT, 0)
engine.lua.memory()     -- returns Lua heap usage in KB: lua_gc(L, LUA_GCCOUNT, 0)
```

**Implementation** (in `bindings.cpp`, registered under `engine.lua` subtable):

```cpp
static int lua_engine_lua_collect(lua_State* L) {
    lua_gc(L, LUA_GCCOLLECT, 0);
    return 0;
}

static int lua_engine_lua_memory(lua_State* L) {
    int kb = lua_gc(L, LUA_GCCOUNT, 0);
    lua_pushinteger(L, kb);
    return 1;
}
```

**Integration:** Registered as part of `engine.lua` subtable in `registerAll()` alongside `engine.scene`, `engine.input`, `engine.time`.

**Memory pool note:** `LuaEngine` uses a static pool allocator (`luaAllocator`). `lua_gc(L, LUA_GCCOUNT, 0)` returns the count from Lua's internal tracking, which reflects allocations made through the custom allocator. The value is meaningful and accurate.

**Files modified:**
- `src/scripting/bindings.cpp` — add two static functions, register in engine.lua subtable

**Module:** enjin2_lua. No new files, no CMake change.

---

### Feature 10: Component Dependency Assertions

**What changes:** Add a protected `requires<T>()` helper to `Component` base class. Components call it in `awake()` to assert dependencies exist.

**Implementation** (add to `include/enjin2/core/component.hpp`):

```cpp
class Component {
protected:
    // ...existing members...

    template<typename T>
    void requireComponent() {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
        if (!owner->getComponent<T>()) {
            // Debug: assert loudly
            // Embedded release: disable self and log
#ifdef NDEBUG
            setEnabled(false);
            // platform log: "C_XYZ requires T on Object"
#else
            assert(false && "Component dependency not satisfied");
#endif
        }
    }
};
```

Name `requireComponent<T>()` rather than `requires<T>()` — `requires` is a C++20 keyword.

**Usage in derived components:**

```cpp
void C_Sprite::awake() {
    requireComponent<C_Position>();
}

void C_LuaScript::awake() {
    // No mandatory dependencies — runs standalone
}
```

**Files modified:**
- `include/enjin2/core/component.hpp` — add `requireComponent<T>()` template method

**Module:** enjin2_core. No new files, no CMake change.

---

## Component Responsibilities After v1.5

| Component | Status | What Changes |
|-----------|--------|-------------|
| `Component` | Modified | + `requireComponent<T>()`, `update`/`lateUpdate` → `float dt` |
| `Object` | Modified | + `std::string name`, `tags[]` array |
| `ObjectCollection` | Modified | + `findByName()`, `findAllWithTag()` |
| `Scene` | Modified | + `SceneStateMachine*` in activate(), onRender Pixel4 bug fixed, onUpdate → `float dt` |
| `SceneStateMachine` | Modified | Pass `this` in activate(); update → `float dt` |
| `C_LuaScript` | Modified | + `ScriptErrorPolicy`, `update(float dt)`, ScriptProxy injection in callbacks |
| `LuaEngine` | Unchanged | — |
| `LuaBindings` | Modified | + `engine.*` table, + `SceneStateMachine*` ptr, + `setTime()`, + `dispatchInputEvents()` |
| `LuaScriptSystem` | Unchanged | — |
| `ScriptProxy` | **NEW** | Userdata with `__index`/`__newindex` for Object field access |
| `script_proxy.hpp/.cpp` | **NEW** | `enjin2_lua` target |
| `sdl_main.cpp` | Modified | + `setSceneStateMachine()` call in performReload, + `dispatchInputEvents()`, + `setTime()` |

---

## Data Flow Changes

### float dt Flow (after change)

```
SDL_GetTicks() -> float dt (seconds, at runner)
    ↓
g_lua.callFunction("update", dt)      (Lua: update(self, dt))
g_compositor … (no dt needed)
SceneStateMachine::update(float dt)   (if ECS path used)
    ↓ Scene::update(float dt)
    ↓ ObjectCollection::update(float dt)
    ↓ Object::update(float dt)
    ↓ Component::update(float dt)     (no ms→s conversion anywhere)
```

### engine.scene.switch() Flow

```
Lua: engine.scene.switch(42)
    ↓ lua_engine_scene_switch(L)
    ↓ LuaBindings::currentScene->changeScene(42)
    ↓ SceneStateMachine::changeScene(42)
    ↓ completeTransition() -> Scene::deactivate() / Scene::activate(this)
```

### Input Event Callback Flow

```
input_advance_frame(&g_input)         (prev ← current, current ← 0)
input_platform_poll(&g_input)         (current ← hardware state)
bindings.dispatchInputEvents()
    for btn 0..15:
        if justPressed(btn): lua call on_button_pressed(btn)
        if justReleased(btn): lua call on_button_released(btn)
bindings.setInput(&g_input)
g_lua.callFunction("update", dt)      (polling still available via engine.input.held())
```

### ScriptProxy Read Flow

```
Lua: local x = self.x
    ↓ ScriptProxy::index(L) (C++ __index metamethod)
    ↓ check key == "x"
    ↓ p->object->getComponent<C_Position>()
    ↓ push pos->x as lua_Number
    → returns float to Lua
```

---

## Build Order (Phase Dependencies)

```
Phase A: onRender Pixel4 bug fix       ← independent, 3-line correctness fix, do first
    |
Phase B: float dt signature change     ← pervasive, must be done as one atomic change
    |                                    breaks all component subclasses; do before any
    |                                    new components are added that use old signature
    ↓
Phase C: Named objects + tags          ← enjin2_core only; no Lua dependency
    |
Phase D: Scene self-transitions        ← requires Scene header change; no Lua dependency
    |
Phase E: engine.* table (partial)      ← engine.input, engine.time, engine.lua (no scene yet)
    |                                    depends on D for engine.scene.switch
    ↓
Phase F: ScriptProxy userdata          ← requires Object (Phase C for name field), requires
    |                                    C_Position/C_Drawable to be stable
    |
Phase G: ScriptErrorPolicy             ← isolated C_LuaScript change; can be done any time
    |                                    after float dt lands (Phase B)
    |
Phase H: Input event callbacks         ← requires LuaBindings (Phase E)
    |
Phase I: GC control                    ← slots into engine.lua subtable (Phase E)
    |
Phase J: Component dependency asserts  ← enjin2_core only; can be done after Phase B
```

**Rationale for this order:**

- **Phase A first:** Correctness bug. Zero risk. Clears an existing silent failure before adding new code that depends on onRender working.
- **Phase B second:** The `float dt` change is the most pervasive. Doing it early means all subsequent new components are written with the correct signature. Deferring it means a bigger rename pass later.
- **Phases C and D (C++ foundations) before Lua features:** `engine.scene.find()` requires named objects. `engine.scene.switch()` requires scene self-transitions. The C++ foundations enable the Lua features cleanly.
- **Phase E (engine.* table) after D:** Can register `engine.input` and `engine.lua` early, but `engine.scene` subtable needs `SceneStateMachine*` wired — wait until D is done.
- **Phase F (ScriptProxy) after C:** `self.name` field requires `Object::name` from Phase C. `self.x`/`self.y` require C_Position which already exists.
- **Phases G–J:** Relatively independent once Phases A–F are in.

---

## Integration Points Summary

### enjin2_core (no CMake change)

| File | Change Type | What |
|------|-------------|------|
| `include/enjin2/core/component.hpp` | Modified | float dt signatures, `requireComponent<T>()` |
| `include/enjin2/core/object.hpp` | Modified | name field, tags array |
| `src/core/object.cpp` | Modified | initialize name/tags |
| `include/enjin2/core/object_collection.hpp` | Modified | `findByName()`, `findAllWithTag()` |
| `include/enjin2/core/scene.hpp` | Modified | onRender fix, float dt, SceneStateMachine* inject |
| `src/core/scene.cpp` | Modified | float dt in template specializations |
| `include/enjin2/core/scene_state_machine.hpp` | Modified | float dt, pass `this` in activate() |

### enjin2_lua (CMake change: add script_proxy.cpp)

| File | Change Type | What |
|------|-------------|------|
| `include/enjin2/components/lua_script.hpp` | Modified | ScriptErrorPolicy enum + field, float dt |
| `src/components/lua_script.cpp` | Modified | float dt call sites, ScriptProxy injection, error policy dispatch |
| `include/enjin2/scripting/bindings.hpp` | Modified | SceneStateMachine* ptr, setTime(), dispatchInputEvents(), engine.* statics |
| `src/scripting/bindings.cpp` | Modified | engine.* table registration, dispatchInputEvents(), GC functions |
| `include/enjin2/scripting/script_proxy.hpp` | **NEW** | ScriptProxy struct, metatable name |
| `src/scripting/script_proxy.cpp` | **NEW** | metatable registration, __index/__newindex |
| `CMakeLists.txt` | Modified | add `src/scripting/script_proxy.cpp` to enjin2_lua |

### enjin2_sdl (sdl_main.cpp only)

| File | Change Type | What |
|------|-------------|------|
| `src/platform/sdl/sdl_main.cpp` | Modified | setSceneStateMachine(), setTime(dt) in loop, dispatchInputEvents() |

---

## Architecture Anti-Patterns to Avoid

### Anti-Pattern 1: Global self in Lua

**What people do:** Set a Lua global `self = <proxy>` before each callback.

**Why it's wrong:** All scripts share a single Lua state. Concurrent scripts (if ever) corrupt each other's `self`. The current design uses `g_currentBindings` as a static pointer already — adding another global creates a second global-state coupling.

**Do this instead:** `self` is a function argument — the first parameter to every lifecycle callback. `update(self, dt)`, not `update(dt)` with a global `self`. This matches Defold's convention and is safe with multiple scripts in the same state.

### Anti-Pattern 2: ScriptProxy Holding Shared Pointers

**What people do:** `ScriptProxy` holds a `std::shared_ptr<Object>` to prevent use-after-free.

**Why it's wrong:** The engine has a zero-dynamic-allocation constraint. `Object` is owned by `ObjectCollection` via `std::unique_ptr`. Introducing `shared_ptr` breaks the ownership model and requires the GC overhead the static pool is designed to avoid.

**Do this instead:** `ScriptProxy` holds a raw `Object*`. On `__index`/`__newindex`, null-check the pointer. When an Object is removed from `ObjectCollection`, any Lua code holding a stale ScriptProxy will get nil reads rather than a crash, because the null check guards every access. Optionally use a generation counter for stricter invalidation.

### Anti-Pattern 3: Registering engine.* as Closures with Upvalues

**What people do:** Register `engine.scene.switch` as a Lua closure capturing a C++ `std::function` or lambda upvalue.

**Why it's wrong:** `lua_pushcclosure` with upvalues still technically works but complicates debugging, prevents the bindings from being cleanly re-registered on hot reload, and adds complexity for no benefit when `LuaBindings*` is already recoverable from the registry.

**Do this instead:** All static `lua_CFunction` callbacks recover `LuaBindings*` via `getBindings(L)` (the registry lookup pattern already used by all existing bindings). This pattern is idiomatic in the codebase and survives hot reload correctly.

### Anti-Pattern 4: Dispatching Input Events Through Component Iterator

**What people do:** For `on_button_pressed`, iterate all Objects in the scene, find each C_LuaScript, call the callback.

**Why it's wrong:** Requires the Lua system to know about the Scene/ObjectCollection hierarchy. The current architecture's Lua layer is intentionally decoupled from the ECS — it communicates only through `LuaBindings` and the shared Lua state. Iterating components from inside `LuaBindings` inverts the dependency direction.

**Do this instead:** Dispatch globally from `LuaBindings::dispatchInputEvents()` by calling the Lua global `on_button_pressed`. Individual C++ components (C_LuaScript) can override this if they need per-component dispatch, but the flat-script model uses the global function.

### Anti-Pattern 5: Making requireComponent Assert in All Builds

**What people do:** Use `static_assert` or unconditional `assert()` for component dependency checks.

**Why it's wrong:** Embedded targets (ESP32) often lack a usable assert handler. A hard assert in release firmware causes a silent reset with no diagnostics. The behavior needs to differ between debug and embedded release builds.

**Do this instead:** `#ifdef NDEBUG` — assert in debug (detects programmer errors at dev time), disable-self + log in release (graceful degradation at runtime on embedded hardware).

---

## Memory Budget (v1.5 additions)

| Item | Size | Platform |
|------|------|----------|
| `Object::name` (std::string) | ~32 bytes per object (SSO) | All |
| `Object::tags` (8 const char*) | 64 bytes per object | All |
| `ScriptProxy` userdata per call | ~24 bytes on Lua stack (transient) | Scripting |
| `LuaBindings` new fields (sm ptr, time, lastDt) | ~16 bytes total | Scripting |
| `engine.*` table in Lua state | ~400 bytes in Lua heap | Scripting |
| Total new static overhead per object | ~96 bytes | All |

With `MAX_OBJECTS = 128`, the new per-object fields add approximately 12 KB to the static footprint. Acceptable on all targets. The Lua heap additions are within the existing pool budget.

---

## Sources

- Direct codebase inspection: all files verified 2026-02-26
  - `include/enjin2/core/object.hpp`, `component.hpp`, `scene.hpp`, `scene_state_machine.hpp`
  - `include/enjin2/core/object_collection.hpp`
  - `include/enjin2/scripting/bindings.hpp`, `lua_engine.hpp`
  - `src/scripting/bindings.cpp`, `src/components/lua_script.cpp`
  - `src/platform/sdl/sdl_main.cpp`
  - `include/enjin2/input/input_state.hpp`
  - `CMakeLists.txt`
- `project/lua-embedding-design.md` — reference engine survey, design principles, proposed API
- `project/cpp-engine-improvements.md` — float dt rationale, named objects design, scene self-transition design
- `.planning/research/ARCHITECTURE.md` (v1.4) — v1.4 integration patterns (layer compositor, hot reload)
- `.planning/codebase/ARCHITECTURE.md` — existing layer/component map

---

*Architecture research for: enjin2 v1.5 — Lua Scripting Foundation*
*Researched: 2026-02-26*
