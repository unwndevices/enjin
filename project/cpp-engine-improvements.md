# C++ Engine Improvements Research
_enjin2 — compiled 2026-02-25_

---

## Context

This document covers improvements to the C++ side of enjin2 independent of Lua embedding. The goal is to make C++ game and UI development cleaner, less error-prone, and less reliant on workarounds — using eisei's friction points as concrete evidence of where the engine falls short today.

---

## 1. Delta Time Type

**Current state**: every `update(uint16_t deltaTime)` passes milliseconds as an unsigned 16-bit integer.

**Problems**:
- Overflows at ~65 seconds (long pauses, debugger breaks, cold boot on ESP32)
- No sub-millisecond precision — at 120fps, dt = 8ms, and repeated integer rounding corrupts animation accumulators over time
- Forces `deltaTime / 1000.0f` casts everywhere; easy to forget, produces subtle bugs
- Inconsistent with every reference engine (love2d, Defold, Unity all use float seconds)

**Fix**: change the signature to `float dt` (seconds) everywhere — `Object::update`, `Component::update`, `Scene::update`, `Scene::onUpdate`. Breaking change that touches the whole codebase, but gets harder to fix the more game code accumulates on top of it.

**Ergonomic difference**:
```cpp
// Before
position.x += speed * deltaTime / 1000;  // "is this right? did I divide?"

// After
position.x += speed * dt;  // speed is in pixels/second, obvious
```

**Scope**: pervasive signature change across `Object`, all `Component` subclasses, `Scene`, `ObjectCollection`, and all existing game code.

---

## 2. Named Objects and Tags

**Current state**: `ObjectCollection::findObject<T>()` finds by C++ type. There is no name-based lookup.

**Eisei evidence**: the entire `ObjectInstances` struct exists because of this gap — a manually curated bag of raw pointers to every object that might be needed across scenes, threaded through `SharedContext` and passed by reference to every scene constructor.

```cpp
// eisei today — manual bag, tightly coupled to every scene
struct ObjectInstances {
    std::shared_ptr<Planet> main_planet;
    std::vector<std::shared_ptr<Satellite>> satellites;
    std::shared_ptr<PopUpUI> popup;
    std::shared_ptr<WarpUI> warpUI;
    // ...
};
```

**Fix A — Named lookup**:
```cpp
// registration (at object creation)
objects.registerByName("planet", planetPtr);
objects.registerByName("popup",  popupPtr);

// retrieval (anywhere that has access to the scene/collection)
Object* planet = objects.findByName("planet");
Object* popup  = objects.findByName("popup");
```

Implementation: `std::string name` field on `Object`, `std::unordered_map<std::string_view, Object*>` in `ObjectCollection`. Names are set at construction, lookup is O(1). No dynamic allocation in the hot path.

**Fix B — Tags**:
```cpp
object->addTag("enemy");
object->addTag("damageable");

// find all
auto enemies = objects.findAllWithTag("enemy");
```

Implementation: a small `std::array<const char*, 8>` of string literals on `Object` (zero allocation, embedded-safe). `findAllWithTag` iterates the collection — not fast, but tag queries happen on events, not every frame.

Together these two replace `ObjectInstances` entirely and eliminate the need for `SharedContext` to exist as a dependency injection workaround.

---

## 3. Scenes Must Drive Their Own Transitions

**Current state**: `Scene` has no reference to `SceneStateMachine`. A scene cannot request a transition to another scene from within its own logic.

**Eisei evidence**: `SceneStateMachine` is owned by `Game`. Every scene transition is driven by `UIManager` calling a method on `Game`, which calls a method on the scene, which calls back to `Game` to switch scenes. The scene is a passive view — it reacts but never acts.

This means any internally motivated transition (calibration complete, save confirmed, screensaver timeout) requires the external driver to poll or receive a callback.

**Fix**: inject a non-owning pointer to `SceneStateMachine` into the scene at activation:

```cpp
void activate(SceneStateMachine* sm) {
    this->stateMachine = sm;
    onActivate();
    // ...
}
```

Then any derived scene (or `LuaScene`) can:
```cpp
void onUpdate(float dt) override {
    if (calibrationComplete) {
        stateMachine->switchTo(SCENE_BASE);
    }
}
```

`Scene` does not own `SceneStateMachine` — just a raw non-owning pointer valid for the scene's active lifetime. No circular ownership.

**Alternative**: expose a `requestTransition(uint32_t sceneId)` virtual method on `Scene` that the `SceneStateMachine` wires up via a `std::function` callback at activation. Either approach works; the callback version avoids the Scene header depending on the StateMachine header.

---

## 4. Persistent Objects Across Scenes

**Current state**: `ObjectCollection` is owned by `Scene`. Objects are destroyed when their scene is destroyed (or deactivated if using shared scenes). There is no first-class mechanism for an object to persist across scene transitions.

**Eisei evidence**: planets and satellites are created once and must persist across all 7 scenes. This forced the `SharedContext` pattern — objects are owned externally, passed by reference to every scene that needs them, and the scene doesn't truly own what it renders.

**Fix — Persistent layer on SceneStateMachine**:

A second `ObjectCollection` that lives on `SceneStateMachine` (or the top-level application). Objects in this layer are updated and rendered every frame regardless of which scene is active. No ownership transfer, no external bag.

```cpp
// at app startup:
auto planet = sceneManager.persistentObjects().addObject<Planet>(...);
sceneManager.persistentObjects().registerByName("planet", planet);

// any scene can read it, but doesn't own it:
auto planet = sceneManager.persistentObjects().findByName("planet");
```

Rendering: the persistent layer renders after the active scene's layer (or before — configurable by layer value). The active scene's own objects composite on top.

This maps directly to Unity's `DontDestroyOnLoad` pattern and what every multi-scene game needs eventually.

---

## 5. Component Signals

**Current state**: component-to-component communication uses `owner->getComponent<T>()` and polling. If `C_Animation` finishes, the owner object must poll `isFinished()` every frame and maintain a `bool alreadyHandled` flag to avoid double-firing.

**Fix**: the `Signal` type already exists on `Scene`. Use it on components too.

```cpp
// C_Animation gains:
Signal<C_Animation*> onFinished;
Signal<C_Animation*, int> onFrameChanged;  // frame index

// setup at object creation:
anim->onFinished.connect([&](C_Animation*) {
    sprite->setFrame(0);
    stateMachine->setState("idle");
});
```

No polling. No `alreadyHandled` booleans. The signal fires once when the animation completes.

Other useful component signals:
- `C_Timer::onFired` — fires when the countdown reaches zero
- `C_Sprite::onAnimationComplete` — wraps per-mode completion (Once mode done)
- `C_Button::onPressed`, `C_Button::onReleased` — UI events without polling input

---

## 6. Component Dependency Assertions

**Current state**: adding a component that requires another component (e.g. `C_Sprite` needs `C_Position`) silently succeeds even if the dependency is absent. The crash happens later, at runtime, inside `draw()`, with no useful context.

**Fix**: a `requires<T>()` assertion called in `awake()`:

```cpp
void C_Sprite::awake() {
    requires<C_Position>();   // asserts owner->getComponent<C_Position>() != nullptr
    requires<C_Drawable>();   // with a clear log: "C_Sprite requires C_Position on Object"
}
```

Implementation: a protected method on `Component` that calls `getComponent<T>()` and either asserts (debug builds) or logs and self-disables (release/embedded builds). Fails loudly at construction, not silently mid-frame.

---

## 7. `C_Timer` Component

**Eisei evidence**: `PopupUI` has `autoHideMs`. `DatumManagerScene` has `debounceDeadline_` and `debounceActive_`. The screensaver has an activity timeout. All three independently hand-roll the same pattern: store a timestamp, check elapsed time each frame, fire logic.

**Fix**:

```cpp
auto timer = obj->addComponent<C_Timer>();

// one-shot
timer->after(2.0f, [&]() { popup->setVisible(false); });

// repeating
timer->every(0.5f, [&]() { blinkState = !blinkState; });

// cancellable
auto handle = timer->after(30.0f, screensaverCallback);
handle.cancel();  // if user presses a button
```

Times in seconds (consistent with float dt change). Internally an accumulator updated in `update(float dt)`. The component lives on the object it drives — no global timer registry, no external state.

For embedded targets where `std::function` heap-allocates: use a fixed-capacity array of `TimerEntry` structs (8 slots) instead of a `std::vector`. Zero allocation after setup.

---

## 8. `C_StateMachine` Component

**Eisei evidence**:
- `ScreensaverScene` hardcodes three variant classes (`ScreensaverStarfield`, `ScreensaverBounce`, `ScreensaverTubes`) and switches between them manually. Not polymorphic, not data-driven.
- `SavePromptUI` exposes 8 public methods to manage a 4-state machine. Callers must invoke them in correct order with no enforcement.
- `CalibrationScene` steps through states via a `calibration_step` integer with no guard rails.

**Fix**:

```cpp
auto sm = obj->addComponent<C_StateMachine>();

sm->addState("idle",    onEnterIdle,  onUpdateIdle,  onExitIdle);
sm->addState("active",  onEnterAct,   onUpdateAct,   nullptr);
sm->addState("done",    onEnterDone,  nullptr,        nullptr);

// conditional auto-transitions (checked in update):
sm->addTransition("idle",   "active", [&]() { return triggered; });
sm->addTransition("active", "done",   [&]() { return confirmed; });

sm->setState("idle");   // initial state
```

States are `std::function` callbacks for enter, update, exit. Transitions are predicates evaluated in `update(float dt)`. The component calls `onExit` → `onEnter` when a transition fires.

The screensaver becomes three states instead of three classes. `SavePromptUI`'s 8-method surface collapses to state entry/exit callbacks. Calibration is one state per step with forward-only transitions.

---

## 9. Bug: `onRender` Never Called for Pixel4 Canvas

**Location**: `scene.hpp:116-126`.

```cpp
template<typename PixelType>
void render(ICanvas<PixelType>& canvas) {
    if constexpr (std::is_same_v<PixelType, uint8_t>) {
        onRender(canvas);   // ← only fires for uint8_t canvas
    }
    // ...
}
```

`onRender(ICanvas<Pixel4>&)` is declared as a virtual override point for derived scenes. But `Pixel4` is not `uint8_t`, so the guard prevents it from ever being called. Every derived scene that overrides `onRender(ICanvas<Pixel4>&)` is silently skipped.

**Fix**: remove the guard, call `onRender(canvas)` unconditionally before `renderObjects(canvas)`. The two overloads (`Pixel4` and `uint8_t`) handle dispatch via overload resolution.

This should be fixed before adding anything else — it's a correctness issue, not a design issue.

---

## 10. Integer Layer System

**Current state**: render layer is an enum with 5 fixed values: `Background`, `Entities`, `Foreground`, `Overlay`, `UI`.

**Problem**: games immediately need more granularity. "This enemy is behind that enemy." "This particle is above the player but below the UI." "This tooltip is above the popup." Five fixed layers cannot express this without `sort_order` hacks that break the semantic meaning of layers.

**Fix**: change layer from enum to `int16_t`. Establish conventions by constant instead of by enum:

```cpp
namespace Layer {
    constexpr int16_t Background = 0;
    constexpr int16_t Entities   = 100;
    constexpr int16_t Foreground = 200;
    constexpr int16_t Overlay    = 300;
    constexpr int16_t UI         = 400;
}

// games use any value:
drawable->setLayer(Layer::Entities + 5);   // just above normal entities
drawable->setLayer(Layer::UI - 10);        // just below main UI
```

`shouldDrawBefore()` already compares layer + sort_order. The only change is the type of the layer field. `sort_order` (also `int16_t`) handles ties within a layer, giving 16-bit precision at each granularity level.

No semantic loss — the constants preserve the named layers. Unlimited slots in between.

---

## Priority Table

| Change | Effort | Impact | Notes |
|--------|--------|--------|-------|
| Fix `onRender` Pixel4 bug | Minimal | Correctness | Do first — it's broken |
| `float dt` everywhere | High (pervasive) | Very high | Harder to fix later |
| Named objects + tags | Low | Very high | Kills `SharedContext`/`ObjectInstances` |
| Scene → StateMachine reference | Very low | High | Scenes drive their own flow |
| Component signals | Medium | High | Eliminates polling flags |
| `C_Timer` | Low | High for UI work | Kills debounce/timeout boilerplate |
| `C_StateMachine` | Medium | High for behavior work | Kills hardcoded state patterns |
| Component dependency assertions | Very low | Medium | Better errors at construction |
| Persistent object layer | Medium | High for multi-scene | Kills shared-pointer bags |
| Integer layers | Low | Medium | Flexibility without breaking change |

---

## Relationship to Lua Changes

Most of these C++ improvements directly benefit the Lua layer as well:

- `float dt` → `update(self, dt)` in Lua also gets seconds, consistent with love2d
- Named objects → `engine.scene.find("planet")` is the Lua surface of the same registry
- Scene self-transitions → `engine.scene.switch(id)` maps to the same mechanism
- Component signals → Lua `on_animation_finished` callbacks use the same signal infrastructure
- Persistent objects → accessible from both Lua scenes and C++ scenes via the same layer
- `C_StateMachine` → could be driven from Lua callbacks in a later phase

The C++ improvements are not preliminary work for Lua — they stand alone and make C++ game/UI code better. Lua bindings then expose the same underlying systems.
