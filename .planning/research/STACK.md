# Stack Research

**Domain:** Zero-alloc 2D game engine — debug draw, save/load, persistent objects, camera follow helpers, coroutine/async, tween helpers, UI component bindings, bindings.cpp refactoring (v1.7)
**Researched:** 2026-03-01
**Confidence:** HIGH (derived from direct codebase analysis + LuaJIT C API verification)

---

## Scope

This document covers **only stack additions and API-level decisions for v1.7 Developer Experience & New Capability**. It does not re-research validated capabilities from v1.0–v1.6 (Lua scripting, ScriptProxy, ComponentProxy, C_Timer, C_StateMachine, EventBus, C_Tilemap, C_Camera, physics bindings, etc.).

---

## What Already Exists (Critical Integration Context)

Reading the live codebase reveals these constraints and integration points relevant to v1.7:

| Existing Element | Implication for v1.7 |
|------------------|----------------------|
| `Primitives<TPixel>` — drawLine, drawRect, drawCircle, fillRect, drawTriangle in `graphics/primitives.hpp` | Debug draw needs NO new C++ drawing code — all shapes already implemented. Bindings only. |
| `C_Camera::getScreenOffset()` + `Scene::renderObjects()` camera offset pipeline | Debug draw must accept a camera offset or draw in screen-space to stay aligned with world objects |
| `LuaStore` with `saveToFile(path)` / `loadFromFile(path)` — 16-slot fixed KV store, handwritten JSON | LuaStore IS the save/load mechanism. v1.7 work is exposing richer serialization (e.g. `engine.store.saveAll()` trigger) and documenting it as the canonical save system — not a new library |
| `LuaStore::saveToFile()` is `#ifdef VCV_RACK` only — ESP32 returns false | Save/load is desktop/SDL-only until NVS support added. This is an existing known limitation — document clearly, do NOT add ESP32 NVS in v1.7 |
| `engine.store.*` binding already wired in `bindings_engine.cpp:101` | Persistent objects across scenes reuse the same store. "Persistent objects" = naming + tagging + store state preserved via LuaStore, not a new object lifecycle mechanism |
| `engine.camera.*` has `setPosition`, `getPosition`, `lookAt(x,y,speed)`, `shake`, `setBounds`, `clearBounds` | `camera.follow(target)` helper is the missing piece — convenience wrapper around `lookAt()` that takes an ObjectProxy and reads its C_Position each frame |
| LuaJIT 2.1 (Lua 5.1 API) with `lua_newthread`, `lua_resume`, `lua_yield`, `lua_status` in C API | Coroutine C API is available. `coroutine` library is part of LuaJIT standard libs. Desktop opens `luaL_openlibs` (includes coroutine). ESP32 does NOT open the coroutine library — must add `luaopen_coroutine` in embedded path if coroutines are required there |
| `bindings.cpp` is 1390 lines; total scripting cpp files are 3408 lines across 9 files | bindings.cpp is the main monolith to split. The pattern for splitting already exists: `bindings_engine.cpp` (904 lines), `bindings_draw.cpp` (363 lines), etc. |
| `bind_helpers.hpp` provides `LuaFuncDef` + `luaBindFunctions()` + `ENJIN_ARRAY_LEN` | All new binding files follow this pattern — no new helper infrastructure needed |
| `math::lerp()` exposed via `engine.math.lerp` | Tween helpers build on top of lerp — implemented as a C++ `C_Tween` component or pure Lua scheduler, not a new math library |
| `ScriptProxy` `__index` / `__newindex` dispatch pattern | UI component bindings (`engine.ui.*`) follow the same sub-table registration pattern already used by engine.camera, engine.physics, engine.collision |

---

## Recommended Stack

### Core Technologies

All v1.7 features are pure C++ + existing LuaJIT — no new external dependencies are required.

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| LuaJIT 2.1 (Lua 5.1 API) | bundled in repo | Coroutine/async, tween scheduling | `lua_newthread` / `lua_resume` / `lua_yield` are in the bundled LuaJIT. Coroutines are a first-class Lua feature — no third-party scheduler library needed |
| `Primitives<Pixel4>` (existing) | already in codebase | Debug draw shapes | drawLine, drawRect, drawCircle already exist in `graphics/primitives.hpp`. Zero new code for the drawing layer |
| `LuaStore` (existing) | already in codebase | Save/load serialization, persistent objects | Already exposes `saveToFile` / `loadFromFile` with a minimal handwritten JSON writer. Only bindings changes needed |
| `C_Camera` (existing) | already in codebase | Camera follow helpers | `lookAt(x, y, lerpSpeed)` already exists. `follow(target)` is a per-frame call to `lookAt` with target object's position |

### New C++ Components (to be authored in v1.7)

| Component | Header Location | Purpose | Implementation Notes |
|-----------|-----------------|---------|----------------------|
| `C_Tween` | `include/enjin2/components/tween.hpp` | Fixed-slot (8 slots) tween engine per object | Stores start/end/duration/elapsed/easing per slot. update(dt) advances all active tweens. Zero heap — static `TweenSlot[8]` array. Easing functions inline (linear, easeIn, easeOut, easeInOut). Exposes via `engine.tween.*` sub-table |
| `DebugDraw` helper | `include/enjin2/scripting/debug_draw.hpp` | Stateless screen-space debug overlay | `engine.debug.*` bindings call `Primitives4::drawRect/Circle/Line` directly on a designated debug layer canvas. Not a component — just a set of static functions + a layer index pointer injected at init |

### Supporting Libraries (existing, referenced in v1.7 bindings)

| Library | Source | Purpose | When Used |
|---------|--------|---------|-----------|
| `bind_helpers.hpp` | `include/enjin2/scripting/bind_helpers.hpp` | `LuaFuncDef` + `luaBindFunctions()` for sub-table registration | All new binding files (bindings_debug.cpp, bindings_tween.cpp, bindings_ui.cpp, bindings_camera_follow.cpp split) |
| `Primitives<Pixel4>` | `include/enjin2/graphics/primitives.hpp` | drawRect, drawCircle, drawLine, fillRect for debug overlay | `engine.debug.rect(x,y,w,h,color)`, `engine.debug.circle(cx,cy,r,color)`, `engine.debug.line(...)` |
| `LuaStore` | `include/enjin2/scripting/bindings.hpp` | Persistent cross-scene state | `engine.store.save(key,val)` / `engine.store.load(key)` already wired; `engine.store.flush()` to trigger file write |
| `lua_newthread` / `lua_resume` / `lua_yield` | LuaJIT C API (bundled) | Coroutine management from C binding | `engine.co.*` sub-table wraps coroutine creation and resume cycle for loading screens / async tasks |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| CMake (existing) | Build system | New bindings files added to `enjin2_lua` target `SOURCES` list. No new CMake changes needed beyond file additions |
| ctest (existing) | Unit tests | New test files follow existing pattern: `tests/tween_test.cpp`, `tests/debug_draw_test.cpp`, `tests/coroutine_test.cpp` |

---

## What NOT to Add

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| External tween library (EnTT, tweeny, cpptween) | All require dynamic allocation or templates that break ESP32/WASM constraints | Implement `C_Tween` with a fixed `TweenSlot[8]` array — same pattern as C_Timer's fixed 8-slot array |
| External JSON library (nlohmann/json, rapidjson) | LuaStore's handwritten JSON writer already covers the save/load use case; adding a full parser increases binary size by 50–150 KB | Keep the existing minimal JSON writer; extend capacity if needed (increase STORE_MAX_KEYS) |
| Lua async frameworks (copas, luasocket) | Desktop/server-oriented, require sockets and `io` library, incompatible with ESP32 and WASM | Use Lua coroutines directly via `coroutine.create` + a C-side `resume` table managed by `engine.co.*` |
| Separate "persistent object" system (DontDestroyOnLoad) | Would require heap allocation or a global object pool separate from scenes — breaks zero-alloc constraint and scene isolation | Use named objects + LuaStore: save object state to store before scene switch, restore on scene load. This is the correct pattern for the target hardware |
| Additional easing libraries | Single-header easing libs (e.g. easing.h) add 20+ functions but the target Lua API only needs linear, ease-in, ease-out, ease-in-out for game UX | Implement 4 inline easing functions directly in `C_Tween` using `t*t`, `t*t*(3-2*t)` etc. |
| LVGL or other embedded GUI frameworks | Full UI framework for a 16-color 128x128 display is extreme overkill; adds thousands of LOC and incompatible allocation patterns | Implement `engine.ui.*` as thin wrappers around existing `Primitives4` + text renderer: progress bars are fillRect + drawRect + text, stat bars are the same |
| ESP32 NVS for save/load | Out-of-scope for v1.7; requires platform-specific ESP-IDF APIs | Keep `saveToFile` as SDL/desktop-only. Document that ESP32 save/load is deferred. The `#ifdef VCV_RACK` guard is already in place |
| Lua 5.4 coroutine.close() | LuaJIT is Lua 5.1 API — `coroutine.close` does not exist | Use `coroutine.status == "dead"` to detect completion; gc handles dead threads |

---

## Feature-Specific Stack Decisions

### Debug Draw Bindings (`engine.debug.*`)

**What exists:** `Primitives<Pixel4>` with full shape set. `layerCanvases[]` array in `LuaBindings`. Layer 0 is the bottom layer; a dedicated debug layer (highest index, e.g. layer 3 for 4-layer builds) keeps debug overlays above game content.

**What to build:**
- New `bindings_debug.cpp` — static functions that call `Primitives4::drawRect/drawCircle/drawLine/fillRect` on a canvas retrieved from `layerCanvases[debugLayer]`
- `m_debugLayer` injected into `LuaBindings` (default: `layerCount - 1`)
- Functions: `engine.debug.rect(x,y,w,h,color)`, `engine.debug.circle(cx,cy,r,color)`, `engine.debug.line(x1,y1,x2,y2,color)`, `engine.debug.point(x,y,color)`, `engine.debug.setLayer(n)`

**No new C++ drawing code.** The only new code is in the bindings.

### Save/Load Serialization Helper

**What exists:** `LuaStore` with `saveToFile(path)` / `loadFromFile(path)`. `engine.store.save/load/exists/delete/clear` already wired. The store is automatically flushed on `engine.store.save()` when `m_storePath` is set.

**What to add:**
- `engine.store.flush()` — explicit trigger for `saveToFile(m_storePath)` without a save operation, useful to persist after batch writes
- `engine.store.path()` — returns current store path string for debugging
- Better documentation of the existing API as the canonical serialization mechanism

**No new C++ serialization code.** The minimal JSON writer is sufficient for the target use case (game state persistence, not level data).

### Persistent Objects Across Scenes

**Pattern:** There is no Unity-style `DontDestroyOnLoad`. The zero-alloc scene model means objects are owned by their scene.

**Correct approach:** Persistent state is data stored in `LuaStore` (or a global Lua table) before scene transition. On the new scene's `init()`, the script reads back from the store. This is documented as the canonical pattern.

**What to add (if any):** Possibly `engine.store.saveTable(key, tbl)` and `engine.store.loadTable(key)` as convenience helpers for storing entire Lua tables (the existing table slot mechanism exists in `StoreSlot::TableEntry` but it is 1-level deep — document the limitation clearly).

**No new C++ persistence infrastructure.** This is a documentation + binding convenience task.

### Camera Follow Helpers

**What exists:** `engine.camera.lookAt(x, y, speed)` — per-frame call with lerp speed. `C_Camera::lookAt(x, y, lerpSpeed)` in the C++ component.

**What to add:**
- `engine.camera.follow(name, speed)` — looks up object by name via `m_activeScene->findByName(name)`, reads `C_Position` if available, calls `lookAt(pos.x, pos.y, speed)`. This is the "follow helper" — a named-object wrapper around the existing `lookAt`.
- `engine.camera.followObject(proxy, speed)` — takes an ObjectProxy userdata instead of a name string, avoids the findByName scan.

**No new C++ camera code.** `C_Camera::lookAt` already handles lerp follow. The helpers are 15–20 lines of binding code each.

### Coroutine/Async for Lua

**What exists:** LuaJIT bundles the coroutine library. Desktop opens `luaL_openlibs` (includes `coroutine`). ESP32's `openEmbeddedLibraries` does NOT open the coroutine library — it must be added.

**What to add:**
- Add `luaopen_coroutine` to `LuaPlatform::openEmbeddedLibraries()` in `src/scripting/lua_platform.cpp` for ESP32 path (guard with memory check like the UTF8 library)
- `engine.co.*` sub-table: `engine.co.start(fn)` — creates a coroutine and registers it for resumption each frame, `engine.co.cancel(handle)`, `engine.co.isRunning(handle)`
- C-side: `LuaBindings` holds a fixed array of `luaref` coroutine handles (`int m_coRefs[8]`). `updateCoroutines(L)` called each frame resumes each live coroutine. Dead coroutines (status "dead") are auto-removed via `luaL_unref`.
- Lua-side: Users write `function myTask() engine.co.wait(2.0); doThing() end` where `engine.co.wait(seconds)` yields with a timeout stored in C.

**`engine.co.wait(seconds)`** requires a C-side yield interceptor: before `lua_resume`, store a "wake time" per slot. `updateCoroutines` skips resume if `totalTime < wakeTime`. This is entirely self-contained in `LuaBindings`.

**No external co-routine scheduler library.** LuaJIT's native coroutine C API is sufficient.

### Tween Helpers

**What to build:**
- `C_Tween` component: 8 `TweenSlot` fixed array. Each slot: `from`, `to`, `duration`, `elapsed`, `easing (enum: Linear/EaseIn/EaseOut/EaseInOut/EasePingPong)`, `active`, `callbackRef (int)`. `update(dt)` advances all active slots, fires `callbackRef` on completion via `lua_rawgeti`+`lua_pcall`.
- `engine.tween.*` sub-table: `engine.tween.to(self, property, targetVal, duration, easing)` — starts a tween on a named property. For MVP: `x` and `y` properties of the script's `C_Position`. Returns a handle integer.
- `engine.tween.cancel(handle)`, `engine.tween.isRunning(handle)`

**Easing functions (inline, no library):**
```cpp
static float ease_in(float t)       { return t * t; }
static float ease_out(float t)      { return t * (2.0f - t); }
static float ease_inout(float t)    { return t < 0.5f ? 2*t*t : -1+(4-2*t)*t; }
static float smoothstep(float t)    { return t * t * (3.0f - 2.0f * t); }
```

These 4 functions + linear are sufficient for all game UI tween use cases.

### UI Component Bindings (`engine.ui.*`)

**What exists:** `include/enjin2/ui/` has `component.hpp`, `system.hpp`, `theme.hpp`, `widget.hpp`. These are C++ UI classes. The Lua bindings for them do not yet exist.

**What to add:**
- New `bindings_ui.cpp` with `engine.ui.*` sub-table
- MVP bindings: `engine.ui.progressBar(x,y,w,h,value,max,fgColor,bgColor)` — draws filled rect (progress portion) + outline rect (border). Pure draw call, no state.
- `engine.ui.statBar(x,y,w,h,value,max,fgColor,bgColor,label)` — progress bar + text label
- `engine.ui.label(x,y,text,color,size)` — thin wrapper around `drawText`
- `engine.ui.panel(x,y,w,h,bgColor,borderColor)` — fillRect + drawRect

These are stateless draw calls. They do NOT wrap the C++ UI component classes — those are heavyweight and the Lua use case is immediate-mode draw-style UI for HUDs and overlays.

**If stateful UI is needed (later):** A `C_UIBar` component would own the value state and update itself. Deferred to post-v1.7.

### Bindings.cpp Refactoring

**Current state:** `bindings.cpp` is 1390 lines. It contains:
- `ScriptProxy` metatable implementation (LuaProxy `__index`/`__newindex`)
- `ObjectProxy` push/metatable helpers
- `ComponentProxy` metatables for all component types (C_Position_Proxy, C_Timer_Proxy, C_StateMachine_Proxy)
- `LuaStore` class implementation
- `LuaCanvas` method implementations
- `LuaBindings::registerAll()` coordinator
- `LuaBindings::resetSpritePool()`

**Split target:**
- `bindings.cpp` → retains only `registerAll()` + `LuaCanvas` methods (the registry coordinator)
- New `bindings_proxy.cpp` → ScriptProxy metatable, ObjectProxy metatable, ComponentProxy metatables
- New `bindings_store_impl.cpp` (or extend `bindings_store.cpp`) → `LuaStore` class methods
- `bindings_debug.cpp` → `engine.debug.*`
- `bindings_tween.cpp` → `engine.tween.*` + `C_Tween` update dispatch
- `bindings_ui.cpp` → `engine.ui.*`

The existing pattern (`#include "../../include/enjin2/scripting/bindings.hpp"` + `namespace enjin2`) is the template for all new files.

**No changes to `bindings.hpp`** except adding new static method declarations for the new sub-tables.

### Null Safety Improvements

**Pattern:** The existing null guard is:
```cpp
LuaBindings* b = LuaBindings::getBindings(L);
if (!b) return 0;
```

All new binding functions must follow this. Additionally, all pointer chains (`b->m_activeCamera`, `b->m_activeScene`) must be individually null-checked before dereferencing. The existing `engine.camera.*` bindings use a local helper `getCameraFromBindings(L)` that returns `nullptr` if `m_activeCamera` is null — the same helper pattern applies to all new features.

---

## Installation

No new package installations required. All v1.7 changes are:

1. New `.cpp` files added to `src/scripting/`
2. New `.hpp` files added to `include/enjin2/components/` (for `C_Tween`) and `include/enjin2/scripting/` (for debug draw helpers)
3. New CMake `target_sources` entries for new `.cpp` files in the `enjin2_lua` target

```cmake
# In CMakeLists.txt, add to enjin2_lua SOURCES:
src/scripting/bindings_debug.cpp
src/scripting/bindings_tween.cpp
src/scripting/bindings_ui.cpp
src/scripting/bindings_proxy.cpp
```

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| Native Lua coroutines via `lua_newthread` | External scheduler (copas, luvit) | Never for this codebase — external schedulers require socket/io libraries incompatible with ESP32 and WASM |
| `C_Tween` component with 8 fixed slots | EnTT tweeny or cpptween | If the project ever drops the zero-alloc constraint and targets desktop-only |
| `LuaStore` for persistent objects | Separate `PersistentObjectRegistry` | If objects needed to be C++ objects surviving scene destruction — current scene model (Scene owns objects via ObjectCollection) makes this impossible without a heap-allocated global pool |
| Stateless `engine.ui.*` draw calls | Wrapping C++ `ui/component.hpp` | The existing C++ UI system uses dynamic allocation and is designed for a different use pattern; expose it in a later milestone when the Lua game complexity demands retained-mode UI |
| Inline 4 easing functions | Single-header easing library | Only if more than 8 easing modes are required and the binary size increase is acceptable |

---

## Version Compatibility

| Component | Lua Version | Notes |
|-----------|-------------|-------|
| `lua_newthread` / `lua_resume` / `lua_yield` | Lua 5.1 (LuaJIT 2.1) | Available in bundled LuaJIT. `lua_resume` signature changed in 5.4 — do NOT use the 5.4 variant |
| `coroutine` library | Lua 5.1 | Available in LuaJIT. Desktop: `luaL_openlibs` already includes it. ESP32: add `luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1)` to `openEmbeddedLibraries` |
| `LuaStore::saveToFile` | N/A | `#ifdef VCV_RACK` guard — returns false on ESP32 silently. v1.7 must document this in the API reference |
| `C_Tween` | C++17 | `if constexpr` is already used in the codebase (scene.hpp:124); same standard is in use |

---

## Sources

- Direct codebase analysis:
  - `/home/unwn/dev/enjin/src/scripting/bindings.cpp` — 1390-line monolith, split targets identified
  - `/home/unwn/dev/enjin/src/scripting/bindings_engine.cpp` — 904 lines, engine sub-table registration pattern
  - `/home/unwn/dev/enjin/include/enjin2/components/camera.hpp` — C_Camera API (setPosition, lookAt, shake, setBounds)
  - `/home/unwn/dev/enjin/include/enjin2/graphics/primitives.hpp` — full shape set available for debug draw
  - `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` — LuaStore, LuaBindings member layout
  - `/home/unwn/dev/enjin/src/scripting/bindings_store.cpp` — LuaStore JSON save/load implementation
  - `/home/unwn/dev/enjin/src/scripting/lua_platform.cpp` — ESP32 library open list, coroutine not yet included
  - `/home/unwn/dev/enjin/luajit/src/lua.h` — `lua_newthread`, `lua_resume`, `lua_yield`, `lua_status` confirmed present
  - `/home/unwn/dev/enjin/luajit/src/lj_ffdef.h` — `coroutine_create`, `coroutine_yield`, etc. confirmed in LuaJIT
  - `/home/unwn/dev/enjin/.planning/PROJECT.md` — v1.7 feature list and constraints

---

*Stack research for: enjin2 v1.7 Developer Experience & New Capability*
*Researched: 2026-03-01*
