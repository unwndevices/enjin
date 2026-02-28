# Lua Embedding Design Research
_enjin2 — compiled 2026-02-25_

---

## 1. Reference Engines Survey

### PICO-8

The simplest model in existence. Three global callbacks, no object system, no scene manager — just tables.

```lua
function _init()  end   -- once on startup
function _update() end  -- 30 or 60 fps
function _draw()  end   -- each visible frame
```

No standard library. Sprites are a 128×8 sprite sheet accessed by index via `spr()`. Game objects are plain Lua tables; the developer invents their own ECS pattern every time. Memory is a hard cap (2 MB Lua heap, 8 KB cart RAM). GC is automatic and untunable.

**What's good**: Ultra-low surface area. Any programmer can read the full API in 20 minutes.
**What's bad**: No structure means every game re-solves entity management. GC is uncontrollable. No way to isolate scenes.

---

### Playdate SDK

Hybrid architecture: the developer chooses Lua _or_ C, not both simultaneously (though both can coexist in a project).

```lua
-- Lua path
function playdate.update() end          -- called each frame

-- C path (in C)
playdate->system->setUpdateCallback(myUpdate, nil);
```

Lua games get a `playdate.graphics.sprite` class with its own update/draw lifecycle. Sprites self-register and the engine drives them — the developer doesn't call `sprite:update()` manually.

The documented GC behavior is honest: "wildly variable from frame-to-frame." Playdate's solution is to let you tune collection timing and recommend C for deterministic sections. Hardware RAM is ~256 KB.

**What's good**: The sprite self-registration pattern is elegant — you create sprites and they just work. Honest about GC cost.
**What's bad**: Language binary choice means Lua-only games hit a ceiling. No formal scene abstraction.

---

### LÖVE2D

The gold standard for Lua game frameworks. Callback-driven, LuaJIT, module namespacing.

```lua
function love.load()        end   -- once
function love.update(dt)    end   -- every frame, delta-time provided
function love.draw()        end   -- every frame, render only
function love.keypressed(k) end   -- event callbacks optional
function love.quit()        end   -- cleanup
```

All engine functionality is under `love.*` sub-namespaces (`love.graphics`, `love.audio`, `love.input`, `love.filesystem`). No built-in entity/scene system — that's left to the game. Objects are Lua tables or classes built with metatables.

The callback split between `update` and `draw` is critical: it enforces that logic and rendering are separate, making it obvious where each kind of work belongs.

**What's good**: The cleanest callback design. `love.*` namespace is discoverable and consistent. `dt` is always present — no frame-rate ambiguity.
**What's bad**: Zero entity system means every project re-implements one. GC still possible to spike.

---

### Defold

Component-based scripting. Scripts are components attached to game objects; each component has its own `self` and full lifecycle.

```lua
function init(self)                          end
function update(self, dt)                    end
function fixed_update(self, dt)              end   -- physics frame
function late_update(self, dt)               end
function final(self)                         end   -- destroy cleanup
function on_message(self, msg_id, msg, sender) end
function on_input(self, action_id, action)   end
function on_reload(self)                     end   -- hot reload
```

Communication is message-passing: `msg.post("#sprite", "play_animation", { id = hash("walk") })`. Components never call each other directly. Object reuse is the idiom for GC control: mutate existing tables instead of creating new ones.

**What's good**: `on_message` is a great decoupling pattern. `fixed_update` vs `update` separates physics from logic cleanly. Hot reload is excellent for iteration.
**What's bad**: Message routing via URL strings (`"#sprite"`, `"/level/hero#weapon"`) is opaque and error-prone. Learning curve is steep.

---

### Solarus

Zelda-engine. Lua is the primary scripting language; C++ does physics/rendering.

```lua
sol.main.on_started()   -- program starts
sol.main.on_update()    -- every frame
sol.main.on_draw()      -- render hook
-- per-entity: enemies/goblin.lua, npcs/merchant.lua
```

Clean separation: C++ side owns collision, drawing, and physics; Lua side owns logic and behavior. Objects are C++ userdata exposed to Lua with custom metatables. The `sol.*` global table is the entire API surface.

**What's good**: The entity-script convention (one `.lua` per entity type) produces naturally organized game code. Metatables make C++ objects feel native in Lua.
**What's bad**: API is tightly coupled to the action-RPG genre. Harder to repurpose for other game types.

---

## 2. Current enjin2 State

### What exists

- `LuaEngine` — wraps Lua/LuaJIT, uses a static memory pool (`char memoryPool[MEMORY_LIMIT]`), custom allocator, panic handler.
- `LuaBindings` — love2d-style graphics API registered as globals: `rectangle()`, `circle()`, `setColor()`, `point()`, `line()`, etc. 16-slot fixed sprite pool (`newSprite`, `drawSprite`, `updateSprite`, `setFrame`). Input polling (`isButtonHeld`, `isButtonJustPressed`, `isButtonJustReleased`, `getAxis`).
- `C_LuaScript` — component that wraps a script. Exposes `init()`, `update(dt)`, `draw()` callbacks. Can set/get Lua variables. Has error tracking and a performance counter.
- `LuaCanvas` — type-erased wrapper around `ICanvas<Pixel4>` or `Canvas8` that lets Lua bindings work without knowing pixel type.

### What's missing

1. **No scene-level scripting.** Lua scripts are per-component. A Lua script cannot switch scenes, query other objects, or signal the engine.
2. **No event callbacks.** Input is polling-only. There is no `on_button_pressed(btn)` callback model.
3. **No object access from Lua.** A script running inside `C_LuaScript` cannot read the position of its parent Object, cannot find sibling components, cannot reach other Objects in the scene.
4. **No script-to-script communication.** There is no message bus or equivalent.
5. **No scene lifecycle in Lua.** Scripts don't know when the scene activates or deactivates.
6. **GC is unmanaged.** The static memory pool bounds total heap but there is no explicit GC step control.
7. **Error recovery is partial.** `C_LuaScript` tracks errors but the engine has no defined behavior for "what happens after a script error."

### Lessons from eisei usage

eisei built a full 7-scene embedded UI on enjin2 without using any Lua. The reason is clear from the friction analysis:

- Scenes are passive views; external C++ code drives all their methods. Lua scripts have no mechanism to drive scenes the same way.
- SharedContext ties everything together through a grab-bag struct. A Lua script running inside one object could not reach the context.
- There's no event bus so a Lua script can't notify the scene of user actions.
- Dead `// LuaUI now handles...` comments show that Lua integration was attempted and abandoned, likely because the scripting boundary was too narrow.

---

## 3. Design Principles

Derived from the reference engines and eisei friction analysis:

**P1 — Minimum viable API surface.**
Follow PICO-8's lead: keep the Lua API small. Every function you add is a contract you maintain forever. Add things when real game code requires them, not speculatively.

**P2 — Separate update and draw.**
Love2d's split is right. `update(dt)` and `draw()` must remain distinct callbacks. Mixing them causes platform-specific bugs (display drivers, buffered rendering) and makes profiling impossible.

**P3 — Delta-time is always provided.**
`update(dt)` not `update()`. Frame-rate independence is not optional for an engine targeting both ESP32 (slow, variable) and desktop (fast, consistent).

**P4 — Scripts see their Object.**
A script component must be able to read/write the properties of the Object it belongs to. At minimum: position (x, y), drawable properties (visible, layer), and a way to reach sibling components by type name. Without this, every non-trivial behavior requires C++ support code.

**P5 — No raw global state in Lua.**
The eisei SharedContext grab-bag was painful. Lua scripts should access engine state through a structured API (`engine.scene`, `engine.input`, `engine.time`), not through implicitly available globals.

**P6 — Event callbacks alongside polling.**
Polling (`isButtonHeld`) is good for held-state logic. Edge events (`on_button_pressed`) are good for actions. Both should exist. Defold does this well with `on_input`.

**P7 — Errors must not crash the engine.**
A script error should log, disable the offending script, and continue. This is especially critical for embedded targets where a reboot to recover is disruptive.

**P8 — GC must be controllable.**
For embedded targets, automatic GC at arbitrary times causes frame drops. Expose `lua.collectgarbage()` control and document when to call it (e.g., on scene transitions, not mid-frame).

**P9 — Memory budget is compile-time.**
The current static pool approach is correct. The pool size should be a named constant in `LuaPlatformConfig` and must be documented clearly (what fits in 32 KB vs 254 KB).

**P10 — Scripts should be loadable from multiple sources.**
Embedded targets load from flash (binary). Desktop loads from filesystem. WASM loads from virtual FS. The `C_LuaScript` source abstraction should be explicit, not implicit.

---

## 4. Proposed API Design

### 4.1 Per-Script Lifecycle

Every Lua script gets these callbacks. All are optional — the engine only calls them if they exist.

```lua
function init()        end   -- called once when component activates
function update(dt)    end   -- called every frame, dt in seconds
function draw()        end   -- called every render frame
function destroy()     end   -- called when component is removed

-- input events (called when input changes, not every frame)
function on_button_pressed(btn)  end
function on_button_released(btn) end
function on_axis_changed(axis, value) end
```

This maps cleanly to `C_LuaScript`'s existing `awake/update/draw` but renames to match love2d vocabulary (less confusion for newcomers) and adds `destroy` and input events.

### 4.2 Self Reference — `self`

The engine injects a `self` table before calling each script callback. It contains the script's own Object:

```lua
-- in update(dt):
local x, y = self.x, self.y       -- read position
self.x = x + 50 * dt              -- write position
self.visible = false               -- hide the drawable
self.layer = 2                     -- change render layer
self.sort_order = 5                -- change Z within layer
```

`self` is not a raw Lua table — it's a userdata proxy that maps reads/writes to the C++ component system. This prevents Lua from holding stale pointers after Objects are destroyed.

### 4.3 Engine Global

A single `engine` global table (following Solarus's `sol.*` pattern but scoped):

```lua
-- scene management
engine.scene.switch(id)           -- request scene transition
engine.scene.id()                 -- current scene id
engine.scene.add_object(name)     -- instantiate named object prefab

-- time
engine.time.now()                 -- seconds since start (float)
engine.time.frame()               -- frame count (integer)
engine.time.delta()               -- last dt (if not using update(dt) form)

-- input (polling, same as current but namespaced)
engine.input.held(btn)
engine.input.just_pressed(btn)
engine.input.just_released(btn)
engine.input.axis(n)

-- memory
engine.lua.collect()              -- explicit GC step
engine.lua.memory()               -- bytes used (for profiling)

-- logging
engine.log(...)                   -- maps to platform print/serial
```

Graphics functions stay as love2d-style globals (`rectangle`, `circle`, etc.) for now — they are the core of the embedded drawing API and renaming them would break existing scripts.

### 4.4 Object Query from Script

```lua
-- find the Position component on self's Object
local pos = self:get("Position")   -- returns component proxy or nil
-- find any object by name in the current scene
local other = engine.scene.find("enemy_01")
-- get a component from another object
local other_pos = other:get("Position")
```

The component name strings map to C++ type names registered at startup. Only safe accessors are exposed (no raw pointer leakage).

### 4.5 Simple Event Bus (optional, phase-gated)

For script-to-script communication without direct references:

```lua
-- emit an event
engine.emit("player_died", { score = 42 })

-- subscribe (in init())
engine.on("player_died", function(data)
    print("score was", data.score)
end)
```

This maps to a C++ subscriber list inside the scene. Events are synchronous within the frame and cleared after delivery.

---

## 5. C++ Side Changes Required

### 5.1 `LuaBindings` — namespace and additions

Current: all functions registered as raw globals (`rectangle`, `circle`, etc.).
Proposed: keep graphics as globals (love2d compat), move engine state under `engine.*` sub-table.

Registration pattern:
```cpp
// graphics stay as globals (love.graphics equivalent)
lua_register(L, "rectangle", lua_rectangle);
lua_register(L, "circle", lua_circle);

// engine table
luaL_newlib(L, engine_lib);            // { scene={}, input={}, time={}, lua={} }
lua_setglobal(L, "engine");
```

### 5.2 `C_LuaScript` — self injection

Before each callback, push the self proxy:
```cpp
// Before calling update(dt):
lua_getglobal(L, "update");
push_self_proxy(L, owner_object);   // userdata with __index/__newindex
lua_pushnumber(L, dt);
lua_call(L, 2, 0);                  // update(self, dt)
```

**Important**: `self` must be the _first_ parameter, making the signature `update(self, dt)` not `update(dt)`. This is the Defold convention and allows proper OO Lua patterns without global `self` pollution.

### 5.3 Self Proxy — `ScriptProxy` userdata

New type: `ScriptProxy` — a userdata that holds a weak reference to an `Object*` plus the owning `C_LuaScript*`.

- `__index`: maps field names ("x", "y", "visible", "layer", "sort_order") to component reads.
- `__newindex`: maps assignments to component writes with validation.
- `__gc`: invalidates the proxy (Object was destroyed).
- `:get(typename)`: returns a ComponentProxy for the named component.

This keeps C++ pointers out of Lua and prevents use-after-free bugs when Objects are destroyed mid-scene.

### 5.4 Error Recovery

Current `C_LuaScript` tracks errors but behavior on error is undefined. Proposed:

```cpp
enum class ScriptErrorPolicy {
    Disable,     // disable component, log once, continue (default)
    Log,         // log every frame, keep running (debug mode)
    Panic,       // call platform panic handler (hard fault recovery)
};
```

Default is `Disable` — a broken script goes quiet, doesn't spam logs, doesn't crash.

### 5.5 GC Control

Expose `lua_gc()` calls through `engine.lua.collect()` and `engine.lua.memory()`. Document the recommended pattern:

```lua
-- in scene deactivate callback:
function on_deactivate()
    engine.lua.collect()  -- do a full GC on scene change, not mid-frame
end
```

---

## 6. Memory Architecture

| Target | Total RAM | Recommended Lua pool | Sprite pool slots |
|--------|-----------|---------------------|-------------------|
| ESP32-S3 8MB PSRAM | 8 MB | 256–512 KB | 16–32 |
| ESP32 520 KB SRAM | ~200 KB Lua-safe | 32–254 KB | 8 |
| Desktop (SDL) | OS-managed | 4 MB | 64 |
| WASM | 16 MB heap | 2 MB | 32 |

The static pool stays. `LuaPlatformConfig::MEMORY_LIMIT` must be set per platform in the build system, not hardcoded in header.

---

## 7. What NOT to Do

**Don't embed a full scene manager in Lua.**
LÖVE2D leaves scene management to libraries (HUMP, Batteries, etc.) and it's consistently the most painful part of starting a project. Enjin2 has a scene system in C++ — Lua should _drive_ it, not reimplement it.

**Don't use a global `self`.**
PICO-8 uses global state for everything and it works at 8KB scale. Enjin2 supports multiple concurrent scripts, so global `self` causes cross-script contamination.

**Don't make input event-only.**
PICO-8's polling-only model is correct for small programs where you check everything every frame. For Enjin2 at larger scale, pure polling means every script checks every button whether it cares or not. Provide both.

**Don't expose raw C pointers or indices.**
The current 16-slot sprite pool uses integer IDs (`newSprite()` returns 1–16). This is fine. Don't expose `Object*` or `Component*` as Lua integers — use proxy userdata.

**Don't add a garbage collector that fights Lua's.**
Some engines add reference counting on top of Lua GC. Don't. Trust Lua GC within the static pool. The pool bounds protect the system; per-object reference counting just adds overhead.

---

## 8. Recommended Implementation Sequence

1. **`engine.*` global table** — wire up `engine.time`, `engine.input`, `engine.log`. No new C++ types, just registration. Makes existing scripts immediately better.

2. **`self` proxy injection** — the biggest single improvement. Rename current lifecycle callbacks from (`awake`/`update`/`draw`) to (`init`/`update`/`draw`), inject `self` as first arg. Breaks existing scripts in a known way.

3. **Error policy on `C_LuaScript`** — `ScriptErrorPolicy::Disable` as default. Immediately improves robustness for embedded.

4. **`on_button_pressed` / `on_button_released` events** — thin layer on top of existing `InputState` edge detection.

5. **`engine.scene.switch(id)`** — allows Lua to request scene transitions. Requires `SceneStateMachine` to be injectable into the Lua environment.

6. **`ComponentProxy` and `self:get(typename)`** — last, because it requires the most C++ side work and is only needed for multi-component objects.

---

## 9. Open Questions

- **Script isolation**: Should each `C_LuaScript` run in its own Lua state, or share the global state? Shared state is more memory-efficient but scripts can accidentally interfere. Separate states require multiple states per scene.
- **Coroutines**: Love2d and Defold both allow coroutines for async-style scripting (e.g., cutscenes). Should enjin2 support `coroutine.wrap` for scripted sequences?
- **Hot reload**: Defold's `on_reload` is very useful for iteration. On desktop SDL builds, inotify-based hot reload is feasible. Worth planning now even if not implementing yet.
- **Script loading from binary**: BinScript already exists in the include tree. Is it a compiled Lua bytecode loader or something else? This needs to be clarified before designing the source abstraction.

---

## Binding File Structure

`bindings.cpp` was split into 7 files by functional area. Each file implements methods of `LuaBindings` (declared in `bindings.hpp`) — so the private static `getBindings(L)` is in scope everywhere without any visibility changes.

### File Map

| File | Contents |
|------|----------|
| `bindings.cpp` | Core: LuaCanvas methods, LuaBindings ctor/registerAll/getBindings, ScriptProxy metatable |
| `bindings_draw.cpp` | Drawing, fast draw, palette |
| `bindings_input_sprites.cpp` | Input polling + sprite pool |
| `bindings_layers_text.cpp` | Layer system + text/fonts + registerFont() |
| `bindings_engine.cpp` | engine.* table: scene, input, time, log, collision |
| `bindings_math.cpp` | Vec2/Point/Rect userdata + math utility globals |
| `bindings_system.cpp` | LuaScriptSystem |

### `bind_helpers.hpp`

`include/enjin2/scripting/bind_helpers.hpp` — included only by `.cpp` files, never by `bindings.hpp`.

```cpp
struct LuaFuncDef { const char* name; lua_CFunction func; };

// Register an array of LuaFuncDef entries into a Lua table at tableIdx.
void luaBindFunctions(lua_State* L, int tableIdx, const LuaFuncDef* defs, int n);

// Register an array of LuaFuncDef entries as Lua globals.
void luaBindGlobals(lua_State* L, const LuaFuncDef* defs, int n);

// Zero-overhead compile-time array length (constexpr template).
#define ENJIN_ARRAY_LEN(arr)  (enjin2::luaArrayLen(arr))
```

`luaBindFunctions` normalizes negative stack indices before the loop so pushes don't shift the target table index.

### File-local Helpers

These are too domain-specific to share — they live only in their respective `.cpp` files:

- **`bindings_draw.cpp`** — `REQUIRE_CANVAS(b, L)` macro: fetches bindings and early-returns 0 if canvas is null.
- **`bindings_layers_text.cpp`** — `clampLayerIdx(lua_idx, layerCount)` inline: converts 1-indexed Lua layer to clamped 0-indexed C++.

### Rules

- `bind_helpers.hpp` is only `#include`d by `.cpp` files — never by `bindings.hpp` — so it doesn't pollute the public header.
- `getBindings(L)` is a private static of `LuaBindings`; it is accessible from all split files because they define methods of `LuaBindings`.
