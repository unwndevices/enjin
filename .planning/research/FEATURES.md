# Feature Research

**Domain:** 2D embedded game engine — v1.7 developer experience and new capability features
**Researched:** 2026-03-01
**Confidence:** HIGH (features are well-understood standard 2D engine patterns; enjin2 codebase confirmed in depth)

---

## Existing Baseline (Already Built — Do Not Rebuild)

The following are already present in enjin2. These inform complexity estimates and dependency analysis but are **not** v1.7 deliverables.

| Already Built | Where |
|---------------|-------|
| C_Camera (lerp, shake, bounds, lookAt) | `include/enjin2/components/camera.hpp` |
| engine.camera.* Lua bindings (setPosition, lookAt, shake, setBounds) | `src/scripting/bindings_engine.cpp` |
| LuaStore in-memory KV (save/load key-value) | `src/scripting/bindings_store.cpp` |
| LuaStore JSON file I/O (VCV_RACK / desktop) | `src/scripting/bindings_store.cpp` |
| Primitives (drawLine, drawRect, drawCircle, fillRect) | `include/enjin2/graphics/primitives.hpp` |
| math::lerp, math::smoothstep, math::clamp | `include/enjin2/core/math.hpp` |
| AnimationComponent (duration, play, pause, ping-pong) | `include/enjin2/ui/components.hpp` |
| FillUpGauge component (unidirectional + bidirectional) | `include/enjin2/components/fill_up_gauge.hpp` |
| Label component (text, wrapping, alignment, border) | `include/enjin2/components/label.hpp` |
| C_Timer (after/every/cancel) | `include/enjin2/components/timer.hpp` |
| EventBus (on/off/emit) | `src/scripting/lua_event_bus.cpp` |
| SceneStateMachine (scene switch, deferred transitions) | `include/enjin2/core/scene_state_machine.hpp` |

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features any developer expects in a "developer experience" milestone. Missing these = milestone feels incomplete.

| Feature | Why Expected | Complexity | Dependencies | Notes |
|---------|--------------|------------|--------------|-------|
| **Debug draw bindings** (engine.debug.*) | Every 2D engine exposes drawing primitives for dev inspection; visualizing AABB/collision shapes is the first thing scripted when debugging movement bugs | LOW | Primitives already exist (drawLine, drawRect, drawCircle); needs Lua binding layer and canvas access | No new C++ needed — only bindings wiring. Expose rect, circle, line, cross-hair. Color param uses existing 4-bit palette. Draw to currentCanvas directly. |
| **Camera follow helper** (engine.camera.follow) | C_Camera.lookAt() already exists; a `follow(target_name)` shorthand that resolves by object name is the obvious DX improvement developers expect | LOW | Depends on: C_Camera (done), engine.scene.find() (done), ObjectProxy (done) | C++ lookup via findByName + C_Camera.lookAt() per-frame. Single-frame polling model. Follow can be called in update(); camera lerp handles smoothing. No new component needed. |
| **Save/load serialization — SDL3 path** | LuaStore exists but only auto-persists to JSON under VCV_RACK macro. Developers expect save/load to "just work" on SDL3 desktop | LOW | Depends on: LuaStore JSON I/O (done), ENJIN2_BUILD_SDL symbol (done) | One-line preprocessor guard change in `bindings_store.cpp` enables the existing file I/O for SDL3 builds. engine.store.setPath() / engine.store.flush() already work. |
| **Persistent objects across scenes** | Developers using engine.scene.switch() expect some objects (audio, game manager state, player score) to survive scene transitions | MEDIUM | Depends on: SceneStateMachine (done), ObjectCollection (done), Object lifecycle | Industry standard is DontDestroyOnLoad (Unity). Mark an Object as "persistent" so SceneStateMachine skips destroying it on switch. CRITICAL constraint: zero dynamic allocation means persistent pool must be fixed-size (e.g., 4 slots). |
| **Coroutine/async pattern for Lua** | Loading screens, cutscenes, and sequenced animations require waiting across frames without callback hell. Lua 5.1+ has built-in coroutines. Exposing a helper that advances registered coroutines each frame is table stakes for any Lua game engine. | MEDIUM | Depends on: LuaEngine (done), C_Timer (done) | Lua coroutines are cooperative. Engine registers N-slot scheduler; engine.async.start(fn) + engine.wait(seconds). No new C++ threads — pure cooperative. Max 8 active coroutines fits zero-alloc constraint. |
| **Tween helpers** | Animating UI values (health bars, opacity, position) between two numbers over time is universally expected. Without tweens, every script reimplements the same lerp+timer pattern. | MEDIUM | Depends on: C_Timer (done), math::lerp (done), EventBus optional | Tween = start value, end value, duration, easing function, update callback, completion callback. Store up to 8 active tweens. Easing: linear, quadIn, quadOut, quadInOut, cubicInOut. API: engine.tween.to(table, {k=v}, duration, easing, done_cb). |
| **UI component bindings** (engine.ui.*) | FillUpGauge and Label C++ components exist but have zero Lua bindings. Developers need to create and update progress bars and stat bars from scripts. | MEDIUM | Depends on: FillUpGauge (done), Label (done), ComponentProxy (done), C_Position (done) | Expose: engine.ui.newGauge(x, y, w, h, color, mode), engine.ui.setGauge(id, value), engine.ui.newLabel(x, y, w, h, text, color), engine.ui.setLabel(id, text). Fixed pool (8 gauges, 8 labels). Follows sprite pool pattern. |

### Differentiators (Competitive Advantage)

Features that make enjin2 stand out among embedded/Lua game engines.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Coroutine-aware tween** (yield until complete) | Combining tweens + coroutines lets Lua scripts write `engine.tween.await(obj, {x=100}, 0.5)` inside a coroutine — script suspends until tween finishes, eliminating callback nesting | HIGH | Requires tween system + coroutine system to be co-designed. The tween's on_complete callback resumes the suspended coroutine. This is the "async/await" feel for game scripting. |
| **Debug draw is always-on toggle** (engine.debug.enabled) | A boolean that enables/disables all engine.debug.* calls without removing them from scripts — zero cost when disabled | LOW | No new C++ needed — just a bool flag in LuaBindings checked before each debug draw call. Follows the ScriptErrorPolicy precedent. |
| **Camera follow with dead zone** (engine.camera.setDeadZone) | A dead zone radius/rect within which the camera does not follow — character can move slightly without camera movement, reducing jitter on pixel art games | MEDIUM | Extends C_Camera with dead zone rect. If target is within dead zone, no lookAt update. Adds m_deadZone fields to C_Camera. |
| **Tween chaining** (.after(fn)) | Tweens that trigger another tween on completion — enables cutscene-quality sequences without coroutines | LOW | on_complete callback creates the next tween. Matches flux.lua's chain pattern. |
| **engine.store.saveToFile / loadFromFile on SDL3** already implemented | LuaStore JSON I/O is fully implemented under VCV_RACK guard — switching to SDL3 preprocessor path gives save/load on desktop immediately with zero new logic | LOW | Change `#ifdef VCV_RACK` to `#if defined(VCV_RACK) || defined(ENJIN2_BUILD_SDL)`. All file I/O code already exists and tested. Verified in `src/scripting/bindings_store.cpp`. |

### Anti-Features (Commonly Requested, Often Problematic)

| Anti-Feature | Why Requested | Why Problematic | Alternative |
|--------------|---------------|-----------------|-------------|
| **Full async/threading** for Lua | "True async" loading feels cleaner | ESP32 and WASM have no pthreads; true async requires platform-specific threading violating zero-alloc and portability constraints | Use cooperative coroutines (coroutine.yield) — identical DX, zero platform dependency |
| **Unlimited persistent objects** | "I want everything to persist" | Fixed static arrays are the foundation of the zero-alloc constraint; unlimited requires heap allocation | Fixed-size persistent pool (4 slots is enough for audio manager, game manager, etc.) — mirrors ESP32 constraint honestly |
| **JSON save data with arbitrary nesting** | Full game state serialization feels comprehensive | LuaStore already caps table depth at 1 level (flat KV + 1-level tables); arbitrary nesting requires recursive malloc, violates zero-alloc | Document the flat-table design. Use multiple top-level keys to simulate structure. For large save data on SDL3, expose raw JSON file path via engine.store.setPath(). |
| **Lua `require()` for tween/coroutine libs** | Developers familiar with flux.lua or tween.lua expect `require` | ESP32 has no filesystem; WASM sandboxes module loading; `require()` only works reliably on SDL3 | Embed easing functions and coroutine scheduler natively in engine.tween.* and engine.async.* — works everywhere |
| **UI widget system with layout engine** | "Real" UI needs auto-layout | A layout engine on 128x128 pixels on ESP32 is overkill; significant C++ complexity and static memory | Fixed-position gauges and labels via engine.ui.*. Developers position manually — appropriate for pixel art HUDs. |
| **Coroutine-per-object** (unlimited coroutines) | Each object wanting its own coroutine feels natural | Each active coroutine is a lua_State thread that holds stack memory — unbounded coroutines would exhaust Lua's memory on ESP32 | Fixed 8-slot coroutine pool. Scripts that need more can use C_Timer chaining (already available). |
| **Debug draw with camera offset auto-applied** | "I want to draw in world space" | Adds a conditional offset pass in every engine.debug.* call; complex for shapes that span screen/world coordinates | Debug draw operates in screen space (no camera offset) — consistent, predictable, works for HUD-style debug info |

---

## Feature Dependencies

```
[Debug draw bindings]
    └──requires──> [Primitives (drawRect, drawLine, drawCircle)] (DONE)
    └──requires──> [LuaBindings canvas access] (DONE)
    └──enhances──> [Debug draw enabled toggle] (new flag in LuaBindings)

[Camera follow helper]
    └──requires──> [C_Camera.lookAt()] (DONE)
    └──requires──> [engine.scene.find()] (DONE)
    └──enhances──> [Camera follow with dead zone] (new C_Camera fields, deferred)

[Save/load serialization — SDL3 path]
    └──requires──> [LuaStore JSON I/O] (DONE, under VCV_RACK guard)
    └──requires──> [ENJIN2_BUILD_SDL preprocessor symbol] (DONE)

[Save/load serialization — ESP32 path]
    └──requires──> [ESP32 NVS / Preferences API] (new stub needed)

[Persistent objects across scenes]
    └──requires──> [SceneStateMachine scene switching] (DONE)
    └──requires──> [Object lifecycle (awake/start/update)] (DONE)
    └──conflicts──> [Unlimited persistent objects] (anti-feature)

[Coroutine/async Lua]
    └──requires──> [Lua 5.1 coroutine API] (available — Lua built-in)
    └──requires──> [C_Timer or per-frame tick] (DONE)
    └──enables──> [Coroutine-aware tween await]

[Tween helpers]
    └──requires──> [math::lerp] (DONE)
    └──requires──> [C_Timer] (DONE — for per-frame update)
    └──enables──> [Coroutine-aware tween await]

[Coroutine-aware tween await]
    └──requires──> [Coroutine/async Lua] (v1.7)
    └──requires──> [Tween helpers] (v1.7)

[UI component bindings — engine.ui.*]
    └──requires──> [FillUpGauge C++ component] (DONE — needs Canvas4 adaptation)
    └──requires──> [Label C++ component] (DONE — needs std::string removal)
    └──requires──> [C_Position] (DONE)
    └──requires──> [LuaBindings pool pattern] (established by sprite pool)
```

### Dependency Notes

- **Coroutine + Tween co-design:** Build the tween system first, then add coroutine scheduling. Coroutine-aware await is an enhancement, not required for tween MVP.
- **Save/load SDL3 path is trivial:** It is a one-line preprocessor guard change in `bindings_store.cpp`. Build this early in the milestone.
- **Camera follow is a one-function add:** C++ camera already has lookAt(). The Lua-side helper just calls findByName + lookAt per-frame. No new C++ component.
- **Debug draw is wiring, not invention:** All draw primitives exist. The work is: add LuaBindings entries, add enabled flag, route to currentCanvas.
- **Persistent objects are the hardest architectural piece:** Requires a design decision about where persistent objects live (outside scene's ObjectCollection), and how SceneStateMachine skips them during scene destruction. Must be designed carefully around the zero-alloc static array model.
- **UI components need C++ adaptation:** Label uses std::string and std::vector — these conflict with zero-alloc for embedded targets. Adaptation required before bindings can be written.

---

## MVP Definition

### Launch With (v1.7 scope)

These are the v1.7 deliverables as declared in PROJECT.md, analyzed for complexity ordering.

- [ ] **engine.debug.* bindings** — Wire primitives to Lua. Add `engine.debug.enabled` flag. Rect, circle, line, cross at minimum. (LOW — ~1 phase)
- [ ] **Save/load SDL3 path** — Enable LuaStore JSON I/O for SDL3 builds via preprocessor change. (LOW — part of existing store phase)
- [ ] **Camera follow helper** — Add `engine.camera.follow(name[, speed])` binding. (LOW — ~1 phase, building on C_Camera)
- [ ] **Tween helpers** — 8-slot tween pool with easing; `engine.tween.to(table, {k=v}, dur, ease, done_cb)`. (MEDIUM — ~2 phases)
- [ ] **Coroutine/async Lua** — Register 8-slot coroutine scheduler; `engine.async.start(fn)` and `engine.wait(seconds)`. (MEDIUM — ~2 phases)
- [ ] **Persistent objects across scenes** — Fixed 4-slot pool; `engine.scene.persist(name)` / `engine.scene.unpersist(name)`. (MEDIUM-HIGH — ~2 phases; architectural)
- [ ] **UI component bindings** — engine.ui.newGauge / setGauge / newLabel / setLabel. (MEDIUM — ~1-2 phases, after Label std::string adaptation)

### Add After Validation (v1.7.x)

- [ ] **Camera dead zone** — Extend C_Camera with dead zone rect after follow helper ships and is exercised.
- [ ] **Coroutine-aware tween await** — Co-design enhancement after both coroutine and tween systems prove stable.
- [ ] **Tween chaining** — Add `.after(fn)` syntax after core tween is working.
- [ ] **ESP32 NVS save path** — Stub-complete-then-test; deferred until ESP32 build is verified in dev environment.

### Future Consideration (v2+)

- [ ] **WASM localStorage bridge** — Requires JS interop layer (emscripten val or Module call); significant scope.
- [ ] **UI layout engine** — Not appropriate for pixel art embedded target.
- [ ] **Per-pixel debug overlay** — Very niche, high implementation cost.

---

## Feature Prioritization Matrix

| Feature | Developer Value | Implementation Cost | Priority |
|---------|-----------------|---------------------|----------|
| Debug draw bindings | HIGH (daily dev tool) | LOW (wiring only) | P1 |
| Camera follow helper | HIGH (every scrolling game) | LOW (one function) | P1 |
| Save/load SDL3 path | HIGH (expected to work) | LOW (preprocessor fix) | P1 |
| Tween helpers | HIGH (UI animation) | MEDIUM (pool + easing) | P1 |
| Coroutine/async Lua | HIGH (loading, cutscenes) | MEDIUM (scheduler) | P1 |
| UI component bindings | MEDIUM (gauges/labels) | MEDIUM (pool + Label adaptation) | P2 |
| Persistent objects | MEDIUM (game managers) | HIGH (architecture) | P2 |
| Camera dead zone | MEDIUM (polish) | LOW (C_Camera extend) | P3 |
| Coroutine-aware tween await | HIGH (DX polish) | MEDIUM (co-design) | P2 |
| ESP32 NVS save path | MEDIUM (platform complete) | MEDIUM (new #ifdef path) | P3 |

**Priority key:**
- P1: Must have for v1.7 launch — directly from PROJECT.md target features
- P2: Should have, builds on P1 systems
- P3: Nice to have, future consideration

---

## Implementation Details for Each Feature

### Debug Draw (engine.debug.*)

**What:** Lua-accessible overlay drawing for development inspection.

**Expected API:**
```lua
engine.debug.enabled = true          -- global toggle (false = all no-ops)
engine.debug.rect(x, y, w, h, color) -- outline rectangle
engine.debug.circle(x, y, r, color)  -- outline circle
engine.debug.line(x1, y1, x2, y2, color)
engine.debug.cross(x, y, size, color) -- crosshair (position marker)
engine.debug.text(x, y, str, color)   -- debug text overlay
```

**Implementation notes:**
- Routes to `Primitives<Pixel4>::drawRect/drawCircle/drawLine` on `currentCanvas`
- `enabled` flag in LuaBindings; all bindings check it first (follows ScriptErrorPolicy pattern)
- Screen-space coordinates (no camera offset) — consistent for HUD-style overlays
- No new C++ component; pure binding additions in new `bindings_debug.cpp`
- Complexity: LOW — primitives confirmed in codebase; pattern established

**Confidence:** HIGH

---

### Camera Follow Helper (engine.camera.follow)

**What:** Per-frame camera tracking of a named object — DX improvement over manually calling lookAt() every frame.

**Expected API:**
```lua
engine.camera.follow("player")         -- snap-follow (lerp=1.0)
engine.camera.follow("player", 0.05)   -- smooth follow (lerp=0.05)
engine.camera.unfollow()               -- stop following
```

**Implementation notes:**
- C_Camera already has `lookAt(x, y, lerpSpeed)` and engine.camera bindings already exist
- LuaBindings stores `m_followTargetName[32]` and `m_followLerpSpeed` — applied each frame in camera update
- Follow resolves target via `findByName()` + `C_Position.getPosition()` each frame (no caching to avoid stale pointers)
- `unfollow()` clears the name; camera stays at last position
- Camera lerp handles smoothing — no extra math needed
- Camera update timing: follow update runs after object update() (camera is last; avoids one-frame lag)
- Complexity: LOW

**Confidence:** HIGH

---

### Save/Load Serialization (SDL3 path)

**What:** Enable the existing LuaStore JSON file I/O for SDL3 builds (currently guarded behind VCV_RACK macro).

**Expected behavior:**
```lua
engine.store.setPath("save.json")     -- set file path (SDL3 only)
engine.store.save("score", 1234)      -- auto-writes to file if path set
engine.store.flush()                  -- explicit flush
engine.store.loadFromFile()           -- explicit load on game start
```

**Implementation notes:**
- Code fully exists in `bindings_store.cpp` under `#ifdef VCV_RACK`
- Change guard to `#if defined(VCV_RACK) || defined(ENJIN2_BUILD_SDL)` — verified: `ENJIN2_BUILD_SDL` is defined via CMake
- ESP32 path: `Preferences.putString/getString` — new `#elif defined(ESP32)` block needed; currently stubs return false
- WASM path: no-op returning false (file I/O unavailable in sandboxed WASM) — document this clearly
- LuaStore auto-persist already works when `m_storePath` is set (confirmed in `lua_engine_store_save`)
- Complexity: LOW for SDL3; MEDIUM for ESP32

**Confidence:** HIGH (SDL3); MEDIUM (ESP32 — NVS has heap overhead, needs testing)

---

### Persistent Objects Across Scenes

**What:** Objects flagged as persistent survive `engine.scene.switch()`.

**Expected API:**
```lua
engine.scene.persist("audio_manager")      -- flag object as persistent
engine.scene.unpersist("audio_manager")    -- remove flag
engine.scene.is_persistent("audio_manager") -- query
```

**Implementation notes:**
- enjin2 uses static arrays in ObjectCollection; "persistent" objects need special handling
- Recommended architectural approach: **flag on Object, SSM skips destruction**
  - Add `m_persistent = false` flag to Object
  - `engine.scene.persist(name)` calls `findByName()` and sets the flag
  - On `SceneStateMachine::switchTo()`, iterate ObjectCollection — skip `m_persistent == true` objects in the cleanup pass
  - Persistent objects' `update()` still gets called from their scene (scenes are statically allocated in SSM and stay alive)
- Unlike Unity: no DontDestroyOnLoad duplicate problem — enjin2 scenes are pre-allocated statically, not instantiated on load
- Key invariant: object names must remain globally unique when persistence is active
- Fixed limit: enforce max 4 persistent objects (document clearly)
- Complexity: MEDIUM-HIGH — SSM modification required

**Confidence:** MEDIUM — Design is clear but implementation requires SSM change; architectural choice not yet validated

---

### Coroutine/Async Lua (engine.async.*)

**What:** Per-frame coroutine scheduler that resumes registered Lua coroutines, enabling `engine.wait(seconds)` syntax.

**Expected API:**
```lua
engine.async.start(function()
    engine.wait(1.0)       -- suspend for 1 second
    setColor(7)
    drawText("Done!", 10, 10)
    engine.wait(0.5)
    setColor(0)
end)

-- Cancel all running coroutines
engine.async.cancelAll()
```

**Implementation notes:**
- Lua 5.1+ has `coroutine.create`, `coroutine.resume`, `coroutine.yield` built in — no external library needed
- Engine side: fixed array of 8 lua_State* threads (each created with `lua_newthread`)
- `engine.async.start(fn)`: creates coroutine from fn, stores in slot, immediately resumes to kick off
- `engine.wait(seconds)`: schedules a C_Timer callback to call `coroutine.resume` after delay, then calls `coroutine.yield` to suspend
- Per-frame: LuaBindings update loop calls `coroutine.resume` on any SUSPENDED coroutines with no pending timer (immediate-yield coroutines advance next frame)
- Memory: each Lua thread shares the main lua_State's heap — no extra static allocation beyond thread object refs
- **Key constraint:** `engine.wait()` must only be called from inside a coroutine. `luaL_error` if called from main script flow.
- Hot-reload: all coroutines cleared when Lua state is reset (F5)
- Complexity: MEDIUM

**Confidence:** MEDIUM — Pattern is well-established (Lua cooperative coroutines); enjin2-specific integration (C_Timer + coroutine.yield interlock) needs validation

---

### Tween Helpers (engine.tween.*)

**What:** Fixed-pool tween system that animates Lua table fields from current to target values over time with easing.

**Expected API:**
```lua
local id = engine.tween.to(self, {x=100, y=50}, 0.5, "quadOut", function()
    print("done!")
end)
engine.tween.cancel(id)
engine.tween.cancelAll()
```

**Implementation notes:**
- Fixed pool of 8 TweenSlot structs in LuaBindings (zero allocation)
- Each slot: `lua_ref` to target table, up to 4 field names, start values, end values, duration, elapsed, easing enum, completion callback ref
- Per-frame: LuaBindings::updateTweens(dt) iterates active slots, advances elapsed, applies easing to compute t (0..1), sets table fields via `lua_rawset`
- Easing functions (pure C++, ESP32-safe — no expf or trig):
  - `linear`: t
  - `quadIn`: t*t
  - `quadOut`: t*(2-t)
  - `quadInOut`: t<0.5 ? 2*t*t : -1+(4-2*t)*t
  - `cubicIn`: t*t*t
  - `cubicOut`: (t-1)^3+1
  - Omit elastic/bounce (require expf or sin — costly on ESP32)
- On completion: call callback if set, mark slot inactive
- Complexity: LOW-MEDIUM — math confirmed in codebase; pool pattern established by sprite pool

**Confidence:** HIGH

---

### UI Component Bindings (engine.ui.*)

**What:** Lua bindings for FillUpGauge and Label components, following the sprite pool pattern.

**Expected API:**
```lua
-- Gauges (progress/stat bars)
local hp_bar = engine.ui.newGauge(x, y, w, h, color, "uni")  -- "uni" or "bi"
engine.ui.setGauge(hp_bar, 0.75)    -- 0.0-1.0 for uni, -1.0-1.0 for bi
engine.ui.removeGauge(hp_bar)

-- Labels (text display)
local lbl = engine.ui.newLabel(x, y, w, h, "Hello", fg_color, bg_color)
engine.ui.setLabelText(lbl, "Score: 42")
engine.ui.setLabelColor(lbl, color)
engine.ui.removeLabel(lbl)
```

**Implementation notes:**
- FillUpGauge C++ component exists — needs canvas type adapted from `Canvas<uint8_t>` to `Canvas4<Pixel4>` for LayerCompositor pipeline
- Label C++ component exists — **uses std::string and std::vector (violates zero-alloc)** — requires adaptation: replace `std::string text` with `char text[64]` and fixed line array; remove `std::vector<std::string> lines`
- Pool: 8 slots each for gauges and labels in LuaBindings; 1-indexed IDs following Lua convention
- Draw: pool draw() called in LuaBindings::onRender() pass, draws to currentCanvas
- FillUpGauge internal_canvas was `Canvas<uint8_t, 64, 64>` — change to `Canvas4<64, 64>`
- Complexity: MEDIUM — C++ adaptation of Label is the blocking work

**Confidence:** MEDIUM — C++ components exist and are complete; adaptation complexity depends on Label usage of std::string

---

## Complexity Summary

| Feature | Phase Estimate | Risk |
|---------|----------------|------|
| Debug draw bindings | 1 phase | LOW — wiring only |
| Camera follow helper | 1 phase | LOW — C_Camera fully capable |
| Save/load SDL3 path | 0.5 phase (part of store phase) | LOW — preprocessor fix |
| Tween helpers | 2 phases | LOW-MEDIUM — math is simple, pool pattern known |
| Coroutine/async Lua | 2 phases | MEDIUM — scheduler co-design, edge cases (error in coroutine) |
| UI component bindings | 2 phases | MEDIUM — Label needs std::string removal for embedded |
| Persistent objects | 2 phases | MEDIUM-HIGH — architectural decision in SSM |

---

## Sources

- Codebase analysis: `/home/unwn/dev/enjin/src/scripting/bindings_store.cpp` (LuaStore full implementation confirmed; VCV_RACK guard identified)
- Codebase analysis: `/home/unwn/dev/enjin/include/enjin2/components/camera.hpp` (C_Camera API confirmed)
- Codebase analysis: `/home/unwn/dev/enjin/include/enjin2/graphics/primitives.hpp` (draw primitives confirmed)
- Codebase analysis: `/home/unwn/dev/enjin/include/enjin2/components/fill_up_gauge.hpp` (FillUpGauge confirmed, canvas type noted)
- Codebase analysis: `/home/unwn/dev/enjin/include/enjin2/components/label.hpp` (Label confirmed; std::string/std::vector usage flagged as embedded incompatible)
- Codebase analysis: `/home/unwn/dev/enjin/include/enjin2/core/math.hpp` (lerp, smoothstep confirmed)
- Codebase analysis: `/home/unwn/dev/enjin/src/scripting/bindings_engine.cpp` (engine.camera.* confirmed; no debug/tween/coroutine bindings present)
- [Lua coroutines for game scripting — Jonathan Fischer](https://www.jonathanfischer.net/lua-coroutines/)
- [Coroutine-based async in Lua — Software Patterns Lexicon](https://softwarepatternslexicon.com/lua/concurrency-and-asynchronous-patterns-in-lua/coroutine-based-asynchronous-programming/)
- [flux.lua — lightweight Lua tween library (rxi)](https://github.com/rxi/flux)
- [tween.lua — Lua tweening/easing (kikito)](https://github.com/kikito/tween.lua)
- [ESP32 NVS documentation — Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- [DontDestroyOnLoad pattern — Unity docs](https://docs.unity3d.com/ScriptReference/Object.DontDestroyOnLoad.html)
- [Improved lerp smoothing — Game Developer](https://www.gamedeveloper.com/programming/improved-lerp-smoothing-)
- [Pixel-perfect camera in GameMaker — yal.cc](https://yal.cc/gamemaker-smooth-pixel-perfect-camera/)
- [bump.lua debug utilities — kikito/GitHub](https://github.com/kikito/bump.lua)

---
*Feature research for: enjin2 v1.7 — debug draw, save/load, persistent objects, camera follow, coroutines, tweens, UI components*
*Researched: 2026-03-01*
