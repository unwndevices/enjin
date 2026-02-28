# Phase 28: float dt Migration - Research

**Researched:** 2026-02-26
**Domain:** C++ virtual function signature migration, CMake compile flags
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Parameter naming**
- Use `dt` (not `deltaTime`) as the parameter name everywhere
- Applies to: `update(float dt)`, `lateUpdate(float dt)`, and all overrides
- Rename the UI system's existing `float deltaTime` to `float dt` for full consistency
- Migrate ALL engine update signatures — core chain (Object, Component, Scene, SceneStateMachine) plus PostFx, AnimationTrack, and any other engine code using `uint16_t deltaTime`

**Accumulated time variables**
- Convert all internal time accumulators to float seconds (elapsed_time, sceneTime, lastUpdateTime, etc.)
- Animation frame durations become float seconds (e.g. 0.1f = 100ms per frame)
- Single conversion point: SDL/platform layer converts ticks to float seconds once. No `/1000` divisions downstream.

**Lua scripting bridge**
- Lua API stays the same: `dt` and `time` variables remain in seconds (no breaking change for Lua scripts)
- Remove the `/1000.0` conversion in C_LuaScript::update() — dt is already seconds
- advanceAnimation binding switches from milliseconds to seconds for consistency
- No Lua script audit required — dt was already exposed in seconds on the Lua side

### Claude's Discretion

- Whether to cap dt at the platform edge (e.g. max 0.1f) to prevent physics explosions from frame spikes — currently some examples cap at 33ms
- Float vs double precision when passing dt to Lua (Lua numbers are natively double)

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DT-01 | `Object::update()`, `Component::update()`, `Scene::update()`, and `SceneStateMachine` pass `float dt` in seconds (not `uint16_t` milliseconds) | Core chain signatures identified in 7 files: component.hpp, object.hpp, object.cpp, object_collection.hpp, scene.hpp, scene_state_machine.hpp, scene.cpp |
| DT-02 | All concrete Component subclasses compile and run with the new `float dt` signature | 8 concrete component override sites found: C_Animation, C_Canvas (update+lateUpdate), C_Sprite (lateUpdate), C_Planet, C_Probe, C_Satellite, C_LuaScript, C_ImageCache; AnimationTrack also has update() |
| DT-03 | `-Woverride` enabled on all platform builds to catch silent override detachment | CMakeLists.txt has no `-Woverride` yet; needs `target_compile_options` additions to enjin2_core, enjin2_ui, enjin2_lua, and SDL/WASM targets |
</phase_requirements>

---

## Summary

Phase 28 is a pure signature migration: `uint16_t deltaTime` (milliseconds) becomes `float dt` (seconds) everywhere in the engine update chain. The SDL platform layer (`sdl_main.cpp`) already computes `float dt` correctly — `dt = static_cast<float>(frame_start - prev_ticks) / 1000.0f` with a 4-frame cap. That single conversion point must be preserved and hardened; nothing downstream should divide by 1000.

The migration touches 7 core headers/sources plus 8 concrete component override sites. The largest structural change is in `SceneStateMachine`, which uses `uint16_t transitionTimer` and `uint16_t transitionDuration` as millisecond accumulators — these must become `float` seconds fields. `AnimationTrack` also has its own `uint16_t currentTime`/`duration` accumulator and `update(uint16_t)` that must be converted. Several components (`C_Planet`, `C_Probe`, `C_Satellite`) currently do `deltaTime / 1000.0f` inside their update bodies — after migration these divisions disappear because dt arrives already in seconds.

DT-03 requires adding `-Woverride` to CMake targets. The flag catches the exact class of bug this migration could introduce: if a single override site is missed or has the wrong signature, the compiler produces no error without `-Woverride` — the old `uint16_t` override simply detaches silently and the component stops updating.

**Primary recommendation:** Migrate base signatures first (Component, Object, ObjectCollection, Scene, SceneStateMachine), then update all concrete overrides, then add `-Woverride` and verify zero warnings.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++17 | project standard | Language features (`if constexpr`, structured bindings) | Already established — CMAKE_CXX_STANDARD 17 confirmed |
| CMake 3.16+ | project minimum | Build system, `target_compile_options` for `-Woverride` | Already in use |

No new dependencies are introduced by this phase. This is a pure in-tree C++ signature change.

---

## Architecture Patterns

### Recommended Change Order

The dependency graph for the update chain is:

```
Component (base)
  └── Object (calls component->update())
        └── ObjectCollection (calls object->update())
              └── Scene (calls objects.update())
                    └── SceneStateMachine (calls scene->update())
```

Migrate bottom-up: change `Component` first, then `Object`, then `ObjectCollection`, then `Scene`, then `SceneStateMachine`. After each base class changes, all overrides in that class's subtree will fail to compile (missing override) only if `-Woverride` is active — which is why DT-03 is done last as a verification step, not first.

**Practical approach:** Change all bases and all concrete overrides in a single pass, then add `-Woverride` and verify no warnings remain. This avoids intermediate broken builds.

### Pattern 1: Base Class Virtual Signature Change

**What:** Change the `virtual void update(uint16_t deltaTime)` declaration in `Component` and `Object` to `virtual void update(float dt)`.

**Current state (component.hpp line 71):**
```cpp
// BEFORE
virtual void update(uint16_t deltaTime) {}
virtual void lateUpdate(uint16_t deltaTime) {}
```

**After:**
```cpp
// AFTER
virtual void update(float dt) {}
virtual void lateUpdate(float dt) {}
```

**When to use:** Every virtual `update`/`lateUpdate` in the inheritance hierarchy — base declarations and all concrete overrides.

### Pattern 2: Call Site Forwarding (no conversion)

**What:** Call sites that currently pass `uint16_t deltaTime` to child calls simply pass `float dt` through unchanged.

**Current state (object.cpp line 53-66):**
```cpp
// BEFORE
void Object::update(uint16_t deltaTime) {
    for (size_t i = 0; i < componentCount; ++i) {
        if (components[i] && components[i]->isEnabled()) {
            components[i]->update(deltaTime);
        }
    }
}
```

**After:**
```cpp
// AFTER
void Object::update(float dt) {
    for (size_t i = 0; i < componentCount; ++i) {
        if (components[i] && components[i]->isEnabled()) {
            components[i]->update(dt);
        }
    }
}
```

No conversion, no division. `dt` flows through unchanged.

### Pattern 3: In-body Division Removal

**What:** Components that currently do `deltaTime / 1000.0f` inside their update bodies must remove that division — dt is already in seconds.

**Current state (planet.hpp line 81, satellite.hpp line 109, probe.hpp line 108):**
```cpp
// BEFORE (C_Planet)
void update(uint16_t deltaTime) override {
    animationTime += deltaTime;  // accumulates ms
    currentRotation += rotationSpeed * (deltaTime / 1000.0f);  // convert to seconds
}
```

**After:**
```cpp
// AFTER (C_Planet)
void update(float dt) override {
    animationTime += dt;   // now accumulates seconds (float)
    currentRotation += rotationSpeed * dt;  // dt is already seconds — no division
}
```

Note: `animationTime` is `uint32_t` today — it must become `float` to accumulate seconds correctly (see Pitfalls).

### Pattern 4: Accumulator Type Change

**What:** Internal time accumulators that store milliseconds as `uint32_t` or `uint16_t` must become `float` seconds.

| Accumulator | Location | Current Type | New Type |
|-------------|----------|-------------|---------|
| `animationTime` | `C_Planet`, `C_Probe`, `C_Satellite` | `uint32_t` | `float` |
| `lastUpdateTime` | `C_LuaScript` (lua_script.hpp:43) | `uint32_t` | `float` |
| `elapsed_time` | `PostFx` (postfx.hpp:158) | `uint32_t` | `float` |
| `currentTime` | `AnimationTrack` (animation_track.hpp:29) | `uint16_t` | `float` |
| `duration` | `AnimationTrack` (animation_track.hpp:30) | `uint16_t` | `float` |
| `transitionTimer` | `SceneStateMachine` (scene_state_machine.hpp:51) | `uint16_t` | `float` |
| `transitionDuration` | `SceneStateMachine` (scene_state_machine.hpp:52) | `uint16_t` | `float` |
| `TRANSITION_TIME` constant | `SceneStateMachine` (scene_state_machine.hpp:43) | `uint16_t` (500) | `float` (0.5f) |
| `_accumMs` | `C_Sprite` (sprite.hpp:111) | `float ms` | `float seconds` — rename to `_accumSec` and change math |

### Pattern 5: C_Sprite Accumulator Migration

**What:** C_Sprite uses a millisecond float accumulator (`_accumMs`) for frame timing. After migration it holds seconds.

**Current state (sprite.hpp line 93-101):**
```cpp
// BEFORE
void lateUpdate(uint16_t deltaTimeMs) override {
    _accumMs += static_cast<float>(deltaTimeMs);
    const float frameMs = 1000.0f / _fps;
    while (_accumMs >= frameMs) {
        _accumMs -= frameMs;
        advanceFrame();
        if (_done) break;
    }
}
```

**After:**
```cpp
// AFTER
void lateUpdate(float dt) override {
    _accumSec += dt;
    const float frameSec = 1.0f / _fps;
    while (_accumSec >= frameSec) {
        _accumSec -= frameSec;
        advanceFrame();
        if (_done) break;
    }
}
```

Note: rename `_accumMs` to `_accumSec` and rename field comment. Frame duration math inverts: `1.0f / _fps` instead of `1000.0f / _fps`.

### Pattern 6: C_LuaScript Bridge — Remove Division

**What:** `C_LuaScript::update()` currently converts ms to seconds before exposing to Lua. After migration, dt arrives in seconds — remove the `/1000.0` conversion entirely.

**Current state (lua_script.cpp line 166-181):**
```cpp
// BEFORE
void C_LuaScript::update(uint16_t deltaTime) {
    lastUpdateTime += deltaTime;
    setScriptVar("dt", static_cast<double>(deltaTime) / 1000.0);  // REMOVE this division
    setScriptVar("time", static_cast<double>(lastUpdateTime) / 1000.0);  // and this
    callScriptFunctionSafe(UPDATE_FUNCTION);
}
```

**After:**
```cpp
// AFTER
void C_LuaScript::update(float dt) {
    lastUpdateTime += dt;   // float seconds accumulator
    setScriptVar("dt", static_cast<double>(dt));       // dt already seconds
    setScriptVar("time", static_cast<double>(lastUpdateTime));  // time already seconds
    callScriptFunctionSafe(UPDATE_FUNCTION);
}
```

Lua scripts continue to receive `dt` in seconds — no breaking change.

### Pattern 7: PostFx Migration

**What:** `PostFx::update()` has its own `elapsed_time` accumulator and per-frame comparisons in milliseconds that need to become seconds.

**Current state (postfx.cpp line 17-36):**
```cpp
// BEFORE
void PostFx::update(uint16_t deltaTime) {
    elapsed_time += deltaTime;  // accumulates ms
    if (elapsed_time >= 150) { ... }        // 150 ms threshold
    if ((elapsed_time % 100) == 0) { ... }  // every 100 ms
}
```

**After:**
```cpp
// AFTER
void PostFx::update(float dt) {
    elapsed_time += dt;            // accumulates seconds now
    if (elapsed_time >= 0.15f) { ... }        // 0.15 seconds
    if (/* periodic trigger in seconds */ ...) { ... }
}
```

Note: The `% 100` modulo on a float is not valid — the periodic noise seed update needs to use a counter-based approach or a separate accumulator. See Pitfalls section.

### Pattern 8: AnimationTrack Migration

**What:** `AnimationTrack<T>` uses `uint16_t currentTime`/`duration` as millisecond accumulators. Keyframe `time` fields are also `uint16_t` milliseconds.

This is the deepest change in the phase. Two sub-tasks:
1. Change `AnimationTrack::update(uint16_t)` to `update(float dt)` and make `currentTime`/`duration` floats.
2. Change `PositionKeyframe::time`, `FloatKeyframe::time`, `ColorKeyframe::time` from `uint16_t` to `float` seconds.

The keyframe time change means all keyframe construction calls like `PositionKeyframe(200, pos)` (200ms) become `PositionKeyframe(0.2f, pos)` (0.2 seconds). Search all keyframe construction sites in `include/enjin2/components/animation.hpp` and any user-facing API.

**Current state (animation_track.hpp line 137):**
```cpp
// BEFORE
void update(uint16_t deltaTime) {
    if (reversed) {
        if (currentTime >= deltaTime) {
            currentTime -= deltaTime;
        } else { ... }
    } else {
        currentTime += deltaTime;
        if (currentTime >= duration) { ... }
    }
}
```

**After:**
```cpp
// AFTER
void update(float dt) {
    if (reversed) {
        if (currentTime >= dt) {
            currentTime -= dt;
        } else { ... }
    } else {
        currentTime += dt;
        if (currentTime >= duration) { ... }
    }
}
```

### Pattern 9: SceneStateMachine Transition Timer

**What:** `SceneStateMachine::updateTransition()` and the `transitionTimer`/`transitionDuration` fields need to become `float` seconds.

**Current state (scene_state_machine.hpp line 325-327):**
```cpp
// BEFORE
void updateTransition(uint16_t deltaTime) {
    transitionTimer += deltaTime;
    transitionProgress = static_cast<float>(transitionTimer) / static_cast<float>(transitionDuration);
}
```

**After:**
```cpp
// AFTER
void updateTransition(float dt) {
    transitionTimer += dt;
    transitionProgress = transitionTimer / transitionDuration;  // both float now
}
```

Also update `TRANSITION_TIME` constant: `static constexpr float TRANSITION_TIME = 0.5f;` (was 500ms).
The `changeScene()` method signature that takes `uint16_t duration` should also change to `float duration`.

### Pattern 10: Adding -Woverride to CMakeLists.txt

**What:** DT-03 requires `-Woverride` on all platform builds.

**Current state:** No `-Woverride` flag in CMakeLists.txt. The `target_compile_options` calls only exist for the WASM and examples benchmarks.

**After (add to each relevant target):**
```cmake
# Add to enjin2_core, enjin2_ui, enjin2_lua, enjin2_sdl targets
target_compile_options(<target> PRIVATE -Woverride)
```

Platform coverage required:
- `enjin2_core` — SDL desktop build
- `enjin2_graphics`, `enjin2_ui` — component library
- `enjin2_lua` — Lua binding library
- `enjin2_sdl` — SDL runner executable
- `enjin2_wasm` — already has `target_compile_options` block

Note: `-Woverride` is a Clang/GCC flag. It is equivalent to GCC's `-Wsuggest-override`. Both are valid for this project's platforms. Emscripten uses Clang so WASM is covered.

### Anti-Patterns to Avoid

- **Integer cast to float at call sites:** If any call site does `obj->update(static_cast<float>(someUint16))`, that suggests a leftover integer source — find and fix the source instead.
- **Floating point modulo for periodic triggers:** `(elapsed_time % 0.1f)` is not valid. Use a sub-accumulator or counter instead.
- **Accumulating uint32_t animationTime in seconds:** A `uint32_t` cannot accumulate fractional seconds. Change the field type to `float`.
- **Changing keyframe constructor time arguments in existing tests:** Any test that passes `uint16_t` milliseconds to a keyframe constructor will break after keyframe `time` becomes `float`. Verify tests compile.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Periodic event every N seconds | Custom modulo logic on float | Separate float sub-accumulator | Float modulo is not integer modulo; sub-accumulator is the standard game pattern |
| dt capping | Custom per-component cap | Single cap at platform edge in sdl_main.cpp | Already present: `if (dt > max_dt) dt = max_dt;` — no additional capping needed downstream |

---

## Common Pitfalls

### Pitfall 1: Silent Override Detachment Without -Woverride

**What goes wrong:** If a concrete component still has `void update(uint16_t deltaTime)` after the base changes to `float dt`, it no longer overrides anything — it compiles cleanly as an unrelated overload, and the component silently stops updating every frame.

**Why it happens:** C++ does not require that `override` be spelled out. Without `-Woverride`, there is zero compiler feedback that an intended virtual override has detached.

**How to avoid:** Add `-Woverride` before or alongside the concrete override changes. Treat zero `-Woverride` warnings as the definition of "migration complete."

**Warning signs:** A component's behavior disappears at runtime without errors; animation stops updating; position stops moving.

### Pitfall 2: Accumulator Type Not Changed (uint32_t holds seconds)

**What goes wrong:** `C_Planet::animationTime` is `uint32_t`. If the update signature changes to `float dt` but `animationTime += dt` remains with `uint32_t animationTime`, then `dt` (e.g., 0.033f) truncates to 0 on every accumulation. The animation never advances.

**Why it happens:** The field type and the accumulation math must both change. Changing only the method signature is insufficient.

**How to avoid:** Change every accumulator field listed in the Accumulator Type Change table above. Verify the field declaration in the header matches the new float-seconds semantics.

### Pitfall 3: Missed Override Sites in Header-Only Components

**What goes wrong:** `C_Planet`, `C_Probe`, `C_Satellite`, `C_Animation`, `C_Canvas` define `update()` directly in the header. These are easy to miss when grepping only `.cpp` files.

**Why it happens:** The engine mixes header-only (inline) implementations with separate .cpp definitions.

**How to avoid:** Grep both `include/` and `src/` for `uint16_t deltaTime`. The full list of override sites from codebase inspection:

| File | Override Methods |
|------|-----------------|
| `include/enjin2/core/component.hpp` | `update(uint16_t)`, `lateUpdate(uint16_t)` — base virtuals |
| `include/enjin2/core/object.hpp` | `update(uint16_t)`, `lateUpdate(uint16_t)` — declarations |
| `src/core/object.cpp` | `update(uint16_t)`, `lateUpdate(uint16_t)` — definitions |
| `include/enjin2/core/object_collection.hpp` | `update(uint16_t)`, `lateUpdate(uint16_t)` |
| `include/enjin2/core/scene.hpp` | `update(uint16_t)`, `onUpdate(uint16_t)` |
| `src/core/scene.cpp` | no separate update — scene.hpp inline only |
| `include/enjin2/core/scene_state_machine.hpp` | `update(uint16_t)`, `updateTransition(uint16_t)` |
| `include/enjin2/effects/postfx.hpp` | `update(uint16_t)` — declaration |
| `src/effects/postfx.cpp` | `PostFx::update(uint16_t)` — definition |
| `include/enjin2/animation/animation_track.hpp` | `update(uint16_t)` — template inline |
| `include/enjin2/components/animation.hpp` | `update(uint16_t)` — inline override |
| `include/enjin2/components/canvas.hpp` | `update(uint16_t)`, `lateUpdate(uint16_t)` — inline overrides |
| `src/components/canvas.cpp` | `lateUpdate(uint16_t)` — definition |
| `include/enjin2/components/sprite.hpp` | `lateUpdate(uint16_t)` — inline override |
| `include/enjin2/components/lua_script.hpp` | `update(uint16_t)` — declaration |
| `src/components/lua_script.cpp` | `C_LuaScript::update(uint16_t)` — definition |
| `include/enjin2/components/probe.hpp` | `update(uint16_t)` — inline override |
| `include/enjin2/components/planet.hpp` | `update(uint16_t)` — inline override |
| `include/enjin2/components/satellite.hpp` | `update(uint16_t)` — inline override |
| `include/enjin2/components/image_cache.hpp` | `update(uint16_t)` — declaration |
| `src/components/image_cache.cpp` | `C_ImageCache::update(uint16_t)` — definition (no-op body) |

### Pitfall 4: PostFx Periodic Trigger via Float Modulo

**What goes wrong:** `postfx.cpp` line 32: `if ((elapsed_time % 100) == 0)` — this modulo operation on `uint32_t` is deterministic. After converting `elapsed_time` to `float`, the same expression `fmod(elapsed_time, 0.1f) == 0.0f` is almost never true due to floating-point representation.

**How to avoid:** Replace with a sub-accumulator or a period-based trigger. Example:
```cpp
// Instead of modulo, use accumulator
float noisePeriodAccum = 0.0f;
// In update:
noisePeriodAccum += dt;
if (noisePeriodAccum >= 0.1f) {
    noisePeriodAccum -= 0.1f;
    noise_seed++;
}
```
This requires adding a `noisePeriodAccum` field to `PostFx`.

### Pitfall 5: AnimationTrack Keyframe Time API Break

**What goes wrong:** The `addPositionKeyframe(uint16_t time, ...)` API on `C_Animation` (animation.hpp line 140) takes `uint16_t` milliseconds. If `Keyframe::time` becomes `float` seconds, callers providing integer milliseconds will silently pass the wrong value — `200` becomes `200.0f` (200 seconds, not 0.2 seconds).

**How to avoid:** Change `addKeyframe` signatures that take `uint16_t time` to `float time` at the same time as the internal type change. Update all keyframe construction call sites in `animation.hpp` accordingly (the `createOrbitAnimation`, `createPulseAnimation`, `createFadeAnimation` factory methods all use `uint16_t` time arguments). Examples in `examples/` are out of scope per user decisions.

### Pitfall 6: C_Sprite _accumMs Rename

**What goes wrong:** `C_Sprite::_accumMs` is named for milliseconds. After migration it holds seconds. Leaving it named `_accumMs` with the old `frameMs` variable and `1000.0f / _fps` formula will produce wrong frame timing (100x too fast at 1000 fps effective frame rate).

**How to avoid:** Rename `_accumMs` to `_accumSec`, and change `frameMs` to `frameSec` with `1.0f / _fps` formula. The sprite_test.cpp exercises `lateUpdate` — update the test to call with float seconds.

### Pitfall 7: SceneStateMachine changeScene() Duration Parameter

**What goes wrong:** `changeScene(uint32_t sceneId, TransitionType, uint16_t duration)` — the `duration` parameter is in milliseconds. After migration, callers would need to pass `0.5f` instead of `500`. If the signature stays `uint16_t`, the internal arithmetic will be wrong when mixed with `float transitionDuration`.

**How to avoid:** Change `duration` parameter to `float` (in seconds) and update `TRANSITION_TIME` constant to `0.5f`.

---

## Code Examples

### SDL Platform Layer (Conversion Point — Keep Unchanged)

The SDL main loop already computes float dt correctly. This is the source of truth:

```cpp
// sdl_main.cpp line 246-247 — DO NOT CHANGE this logic
float dt = static_cast<float>(frame_start - prev_ticks) / 1000.0f;
if (dt > max_dt) dt = max_dt;
prev_ticks = frame_start;
```

`max_dt` is already computed as `4.0f / static_cast<float>(fps)` — a 4-frame ceiling. This dt cap is at the platform edge (Claude's Discretion satisfied: cap already exists, no per-component capping needed).

### Complete -Woverride CMake Addition

```cmake
# Apply to each library and executable that contains virtual update overrides
foreach(tgt enjin2_core enjin2_graphics enjin2_ui)
    target_compile_options(${tgt} PRIVATE -Woverride)
endforeach()

# If enjin2_lua is built:
if(ENJIN2_BUILD_LUA)
    target_compile_options(enjin2_lua PRIVATE -Woverride)
endif()

# SDL runner:
if(ENJIN2_BUILD_SDL)
    target_compile_options(enjin2_sdl PRIVATE -Woverride)
endif()

# WASM — add to existing target_compile_options block:
# target_compile_options(enjin2_wasm PRIVATE -Woverride ...)
```

Note: `-Woverride` requires Clang 3.5+ or GCC 5+. Both are well above project minimums. Emscripten uses Clang — compatible.

---

## Open Questions

1. **advanceAnimation binding and milliseconds**
   - What we know: CONTEXT.md says "advanceAnimation binding switches from milliseconds to seconds for consistency"
   - What's unclear: The `advanceAnimation` function was not found in the current codebase grep. It may be in `src/scripting/bindings.cpp` or Lua-side only.
   - Recommendation: Search `src/scripting/bindings.cpp` and `include/enjin2/scripting/bindings.hpp` during planning. If found, add a task for it.

2. **Float vs double for Lua dt (Claude's Discretion)**
   - What we know: `setScriptVar("dt", static_cast<double>(deltaTime) / 1000.0)` uses double. Lua numbers are natively double. ESP32-S3 has hardware single-precision FPU but soft-float double.
   - What's unclear: Whether the existing code path already casts to double (it does — `static_cast<double>`) and whether the cast cost matters.
   - Recommendation: Keep `static_cast<double>(dt)` — the cast from float to double is lossless and costs nothing meaningful. Consistent with existing pattern, no change needed.

3. **ButtonDial::onUpdate(float deltaTime) anomaly**
   - What we know: `button_dial.hpp` declares `void onUpdate(float deltaTime) override {}` (line 60) — it already uses float. However it also references `Position*` (not `C_Position*`) which suggests it may be from a different subsystem or is dead/untested code.
   - What's unclear: Whether `ButtonDial` is part of the engine's Component hierarchy or a separate UI-layer class. Its base is `Component` but it overrides `onUpdate` (not `update`) — this does not match the `Component` base interface.
   - Recommendation: Treat `ButtonDial` as out-of-scope or dead code unless it compiles as part of the build. Check if it appears in any CMakeLists target.

---

## Sources

### Primary (HIGH confidence)

- Live codebase inspection (2026-02-26):
  - `include/enjin2/core/component.hpp` — `update(uint16_t)`, `lateUpdate(uint16_t)` base virtuals confirmed
  - `include/enjin2/core/object.hpp` — `update(uint16_t)` declarations confirmed
  - `src/core/object.cpp` — `update(uint16_t)` and `lateUpdate(uint16_t)` definitions confirmed
  - `include/enjin2/core/object_collection.hpp` — `update(uint16_t)`, `lateUpdate(uint16_t)` confirmed
  - `include/enjin2/core/scene.hpp` — `update(uint16_t)`, `onUpdate(uint16_t)` confirmed
  - `include/enjin2/core/scene_state_machine.hpp` — `update(uint16_t)`, `updateTransition(uint16_t)`, `transitionTimer`/`transitionDuration` as `uint16_t` confirmed
  - `include/enjin2/animation/animation_track.hpp` — `update(uint16_t)`, `currentTime`/`duration` as `uint16_t` confirmed
  - `include/enjin2/components/animation.hpp` — `update(uint16_t)` override, `addPositionKeyframe(uint16_t time)` confirmed
  - `include/enjin2/components/sprite.hpp` — `lateUpdate(uint16_t deltaTimeMs)`, `_accumMs` float field confirmed
  - `include/enjin2/components/planet.hpp` — `update(uint16_t)`, `animationTime uint32_t`, `deltaTime / 1000.0f` division confirmed
  - `include/enjin2/components/probe.hpp` — `update(uint16_t)`, `animationTime uint32_t`, `deltaTime / 1000.0f` division confirmed
  - `include/enjin2/components/satellite.hpp` — `update(uint16_t)`, `animationTime uint32_t`, `deltaTime / 1000.0f` division confirmed
  - `include/enjin2/components/lua_script.hpp` — `update(uint16_t)` declaration, `lastUpdateTime uint32_t` confirmed
  - `src/components/lua_script.cpp` — `/ 1000.0` division on both dt and time confirmed (lines 176-177)
  - `include/enjin2/effects/postfx.hpp` — `update(uint16_t)`, `elapsed_time uint32_t` confirmed
  - `src/effects/postfx.cpp` — `elapsed_time += deltaTime` (ms accumulation), `% 100` modulo confirmed
  - `src/platform/sdl/sdl_main.cpp` — float dt already computed as `(frame_start - prev_ticks) / 1000.0f`, cap already present
  - `include/enjin2/ui/system.hpp` — `SystemBase::update(float deltaTime)` already uses float
  - `CMakeLists.txt` — no `-Woverride` flag present, confirmed C++17 standard

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — C++17/CMake confirmed from project files
- Architecture: HIGH — all override sites confirmed by direct codebase grep; no speculation
- Pitfalls: HIGH — all pitfalls derived from actual code inspection, not assumption

**Research date:** 2026-02-26
**Valid until:** 90 days (stable C++ codebase, no fast-moving dependencies)
