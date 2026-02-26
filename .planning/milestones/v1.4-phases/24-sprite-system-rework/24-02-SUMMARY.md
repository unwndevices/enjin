---
phase: 24-sprite-system-rework
plan: 02
subsystem: components
tags: [sprite, drawable, animation, pixel4, refactor]
dependency_graph:
  requires: [24-01]
  provides: [C_Sprite component, C_Drawable Pixel4 signature]
  affects: [scene.hpp, canvas.cpp, draw.hpp, lua_script, planet, satellite, probe, ecs_demo]
tech_stack:
  added: []
  patterns:
    - Delta-time accumulator for frame animation (carry-over preserved)
    - Pure virtual signature change propagated atomically
    - ENG-01 stub pattern for deferred compositing
key_files:
  created: []
  modified:
    - include/enjin2/components/drawable.hpp
    - include/enjin2/components/sprite.hpp
    - include/enjin2/components/canvas.hpp
    - src/components/canvas.cpp
    - include/enjin2/components/draw.hpp
    - include/enjin2/components/lua_script.hpp
    - src/components/lua_script.cpp
    - include/enjin2/components/planet.hpp
    - include/enjin2/components/satellite.hpp
    - include/enjin2/components/probe.hpp
    - include/enjin2/core/scene.hpp
    - examples/ecs_demo.cpp
key_decisions:
  - C_Drawable::draw() pure virtual changed from ICanvas<uint8_t>& to ICanvas<Pixel4>&
  - C_Canvas::draw() is a stub (ENG-01 deferred to Phase 25); applyBlendMode is also stubbed
  - planet/satellite/probe: removed draw(ICanvas<uint8_t>&) overloads (Pixel4 overloads kept)
  - lua_script: removed draw(ICanvas<uint8_t>&) overload (Pixel4 overload already existed)
  - scene.hpp renderObjects(): Pixel4 path routes to drawables; uint8_t path is a no-op
  - C_Sprite holds SpriteSheet by value; setSheet() resets frame + accumulator
metrics:
  duration_minutes: 4
  completed_date: "2026-02-24"
  tasks_completed: 2
  files_changed: 12
---

# Phase 24 Plan 02: C_Drawable Pixel4 Signature + C_Sprite Rewrite Summary

C_Drawable::draw() signature changed from ICanvas<uint8_t>& to ICanvas<Pixel4>& across all derived classes, and C_Sprite rewritten to use SpriteSheet with a full delta-time frame animation state machine (Once/Loop/PingPong).

## What Was Built

### Task 1: C_Drawable signature change + all derived class updates

**New pure virtual signature:**
```cpp
virtual void draw(ICanvas<Pixel4>& canvas) = 0;
```

**Files modified and what changed:**

| File | Change |
|------|--------|
| `include/enjin2/components/drawable.hpp` | Pure virtual signature: uint8_t -> Pixel4; doc updated |
| `include/enjin2/components/canvas.hpp` | draw() override + applyBlendMode() updated to Pixel4 |
| `src/components/canvas.cpp` | draw() replaced with ENG-01 stub; applyBlendMode() stub added |
| `include/enjin2/components/draw.hpp` | DrawFunction typedef + override updated to Pixel4 |
| `include/enjin2/components/lua_script.hpp` | draw(ICanvas<uint8_t>&) override removed; Pixel4 kept |
| `src/components/lua_script.cpp` | draw(ICanvas<uint8_t>&) implementation removed |
| `include/enjin2/components/planet.hpp` | draw(ICanvas<uint8_t>&) override removed; Pixel4 kept |
| `include/enjin2/components/satellite.hpp` | draw(ICanvas<uint8_t>&) override removed; Pixel4 kept |
| `include/enjin2/components/probe.hpp` | draw(ICanvas<uint8_t>&) override removed; Pixel4 kept |
| `include/enjin2/core/scene.hpp` | renderObjects(): Pixel4 path calls drawables; uint8_t no-op |

**C_Canvas stub approach:**
```cpp
void C_Canvas::draw(ICanvas<Pixel4>& /*target_canvas*/) {
    // C_Canvas renders to its own internal Canvas8 buffer.
    // Compositing Canvas8 -> ICanvas<Pixel4> is deferred to ENG-01 (v2).
    // This override satisfies the C_Drawable pure virtual contract.
}
```

### Task 2: C_Sprite rewrite

**New C_Sprite API surface:**
```cpp
C_Sprite(Object* owner, uint8_t width, uint8_t height);
void setSheet(const SpriteSheet& sheet);   // resets frame+accumulator
void setFPS(float fps);
void setMode(AnimMode mode);
void setFrame(uint8_t index);              // clamped to [0, frameCount-1]
uint8_t getFrame() const;
bool isDone() const;                       // true when Once mode completes
void draw(ICanvas<Pixel4>& canvas) override;
void lateUpdate(uint16_t deltaTimeMs) override;
bool continueToDraw() const override;
```

**Animation state machine (advanceFrame):**
- `Once`: advances until last frame, sets `_done = true`, further lateUpdate calls no-op
- `Loop`: wraps back to frame 0 after last frame
- `PingPong`: reverses direction at terminal frames; steps one back/forward to avoid bouncing

**Delta-time accumulator:**
```cpp
_accumMs += static_cast<float>(deltaTimeMs);
while (_accumMs >= frameMs) {
    _accumMs -= frameMs;  // preserve sub-frame carry-over
    advanceFrame();
    if (_done) break;
}
```

## Build Status

Zero `error:` lines from `cmake --build build_24_check`.

## Surprises Found During grep Scan

1. **`include/enjin2/core/scene.hpp`** was not in the plan file list but contained a `renderObjects()` template that called `drawables[i]->draw(canvas)` for `uint8_t` only — this would have broken at instantiation with the new signature. Fixed by routing the Pixel4 path to drawables and making the uint8_t path a no-op.

2. **`examples/ecs_demo.cpp`** was not in the plan file list but contained `RectangleDrawable::draw(ICanvas<uint8_t>&)` marked `override`. Fixed to use `ICanvas<Pixel4>&` with `Pixel4(grayscale & 0x0F)`.

3. **`planet.hpp`, `satellite.hpp`, `probe.hpp`** already had `draw(ICanvas<Pixel4>&)` overloads — removing the `uint8_t` overloads was sufficient (no pixel value conversion needed).

4. **`lua_script.hpp`** already had both `draw(ICanvas<Pixel4>&)` and `draw(ICanvas<uint8_t>&)` declared — only the `uint8_t` overload needed removal.

5. **`button_dial.hpp`, `label.hpp`, `slider.hpp`, `tickmarks.hpp`, `fill_up_gauge.hpp`** — these extend `Component` (not `C_Drawable`) so their `draw(ICanvas<uint8_t>&)` methods are non-virtual and do NOT need updating.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] scene.hpp renderObjects() preventing compilation**
- **Found during:** Task 1 build verification
- **Issue:** `renderObjects<uint8_t>` called `drawables[i]->draw(canvas)` which no longer compiles since C_Drawable::draw() requires Pixel4, not uint8_t
- **Fix:** Updated to route Pixel4 canvas to drawables; uint8_t path is a no-op with comment about ENG-01 deferral
- **Files modified:** `include/enjin2/core/scene.hpp`
- **Commit:** b74dc2a

**2. [Rule 1 - Bug] examples/ecs_demo.cpp RectangleDrawable with stale override signature**
- **Found during:** Task 1 build verification
- **Issue:** `RectangleDrawable::draw(ICanvas<uint8_t>&)` marked override would make class abstract
- **Fix:** Updated to `draw(ICanvas<Pixel4>&)` with `Pixel4(grayscale & 0x0F)` conversion
- **Files modified:** `examples/ecs_demo.cpp`
- **Commit:** b74dc2a

## Self-Check

### Files Exist
- `include/enjin2/components/drawable.hpp` — FOUND
- `include/enjin2/components/sprite.hpp` — FOUND
- `include/enjin2/components/canvas.hpp` — FOUND
- `src/components/canvas.cpp` — FOUND
- `include/enjin2/components/draw.hpp` — FOUND

### Commits Exist
- b74dc2a — FOUND (Task 1: drawable signature + all derived classes)
- sprite.hpp Task 2 content captured in d4032e2 (plan 24-03 ran concurrently)

## Self-Check: PASSED
