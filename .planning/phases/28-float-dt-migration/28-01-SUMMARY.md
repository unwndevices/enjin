---
phase: 28
plan: 01
subsystem: core-update-loop
tags: [dt-migration, api-change, float, animation, update-loop]
dependency_graph:
  requires: []
  provides: [float-dt-api]
  affects: [all-update-consumers]
tech_stack:
  added: []
  patterns: [float-seconds-dt, accumulator-based-periodic-timing]
key_files:
  created: []
  modified:
    - include/enjin2/core/component.hpp
    - include/enjin2/core/object.hpp
    - src/core/object.cpp
    - include/enjin2/core/object_collection.hpp
    - include/enjin2/core/scene.hpp
    - include/enjin2/core/scene_state_machine.hpp
    - include/enjin2/animation/keyframe.hpp
    - include/enjin2/animation/animation_track.hpp
    - include/enjin2/components/animation.hpp
    - include/enjin2/components/canvas.hpp
    - src/components/canvas.cpp
    - include/enjin2/components/sprite.hpp
    - include/enjin2/components/planet.hpp
    - include/enjin2/components/probe.hpp
    - include/enjin2/components/satellite.hpp
    - include/enjin2/components/lua_script.hpp
    - src/components/lua_script.cpp
    - include/enjin2/components/image_cache.hpp
    - src/components/image_cache.cpp
    - include/enjin2/effects/postfx.hpp
    - src/effects/postfx.cpp
    - include/enjin2/ui/system.hpp
    - include/enjin2/ui/systems.hpp
    - include/enjin2/components/button_dial.hpp
    - include/enjin2/components/tickmarks.hpp
    - include/enjin2/components/slider.hpp
    - include/enjin2/components/fill_up_gauge.hpp
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
    - tests/sprite_test.cpp
    - tests/shadow_mode_test.cpp
    - examples/text_demo.cpp
    - examples/ecs_demo.cpp
    - examples/space_ui_demo.cpp
    - examples/graphics_output_demo.cpp
decisions:
  - "float seconds dt replaces uint16_t milliseconds throughout entire update chain"
  - "accumulator pattern (noisePeriodAccum) replaces integer modulo for periodic triggers in PostFx"
  - "SpriteState::accumMs renamed to accumSec in bindings — Lua updateSprite API now expects dt in seconds"
  - "SceneStateMachine TRANSITION_TIME changed from 500 (ms) to 0.5f (seconds)"
  - "animationTime fields in Planet/Probe/Satellite changed from uint32_t to float seconds"
  - "AnimationTrack handleLoopBoundary uses float modulo via subtraction, not integer %"
metrics:
  duration_minutes: 14
  completed_date: "2026-02-26"
  tasks_completed: 2
  files_modified: 36
---

# Phase 28 Plan 01: Float DT Migration — Core Update Chain Summary

Replace the entire engine's `uint16_t deltaTime` (milliseconds) update API with `float dt` (seconds).

## What Was Built

Complete float-seconds delta-time migration across the entire Enjin2 update chain: core Component/Object/Scene/SceneStateMachine virtual chain, all component implementations (Sprite, Animation, Planet, Probe, Satellite, LuaScript, ImageCache, Canvas), animation system (AnimationTrack, keyframe time fields, EasingFunctions), post-processing effects (PostFx), UI system base classes (SystemBase, SystemManager), UI components (ButtonDial, Tickmarks, Slider, FillUpGauge), Lua scripting bindings (SpriteState accumulator), and all test/example call sites.

## Tasks Completed

| Task | Description | Commit |
|------|-------------|--------|
| 1 | Core update chain and component implementations | d3c9299 |
| 2 | Secondary systems, tests, and Lua bindings | ed91cc3 |

## Key Changes

### Core Chain (Task 1)

- `Component::update(float dt)` and `lateUpdate(float dt)` — base virtuals
- `Object::update(float dt)` / `lateUpdate(float dt)` — propagates to components
- `ObjectCollection::update/lateUpdate(float dt)` — iterates objects
- `Scene::update(float dt)` and `onUpdate(float dt)` — scene hook
- `SceneStateMachine::update(float dt)` — `TRANSITION_TIME = 0.5f`, `transitionTimer/Duration` now `float`

### Animation System (Task 1)

- `PositionKeyframe`, `FloatKeyframe`, `ColorKeyframe` — `time` fields: `uint16_t` → `float`
- `AnimationTrack<T>` — `currentTime`, `duration` to `float`; `update(float dt)`, `evaluateAtTime(float)`, `interpolateBetween(float)` — no integer casts
- `handleLoopBoundary` — float modulo via subtraction pattern instead of `%`
- `C_Animation` — all `addKeyframe` methods, `createOrbitAnimation`, `createPulseAnimation`, `createFadeAnimation` use `float` time

### Component-Level (Task 1)

- `C_Sprite` — `lateUpdate(float dt)`, `_accumMs` → `_accumSec`, frame timing via `1.0f / _fps`
- `C_Planet`, `C_Probe`, `C_Satellite` — `animationTime` `uint32_t` → `float`, `update(float dt)`, all `/1000.0f` divisions removed, `animationTime` used directly in `sin()` calls
- `C_LuaScript` — `lastUpdateTime` `uint32_t` → `float`, `update(float dt)`, Lua gets `dt` directly without `/1000.0` conversion
- `C_Canvas`, `C_ImageCache` — trivial signature renames

### Secondary Systems (Task 2)

- `PostFx::update(float dt)` — `elapsed_time` now `float`; `noisePeriodAccum` sub-accumulator replaces `elapsed_time % 100` integer modulo pattern
- `SystemBase::update(float dt)`, `SystemManager::update(float dt)` — parameter renamed
- UI component `onUpdate(float dt)` stubs updated
- `LuaBindings::SpriteState::accumMs` → `accumSec`, `lua_updateSprite` accepts dt in seconds, `frameSec = 1.0f / fps` replaces `frameMs = 1000.0f / fps`

### Tests and Examples

- `tests/sprite_test.cpp` — `lateUpdate(100)` → `lateUpdate(0.1f)`
- `tests/shadow_mode_test.cpp` — `DELTA_TIME` `uint16_t 16` → `float 0.016f`
- All example files (`text_demo`, `ecs_demo`, `space_ui_demo`, `graphics_output_demo`) — main loops and scene overrides updated

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed compile errors in example files**
- **Found during:** Task 2 verification build (`build_22_sdl_lua`)
- **Issue:** `examples/text_demo.cpp`, `examples/ecs_demo.cpp`, `examples/space_ui_demo.cpp`, `examples/graphics_output_demo.cpp` all had `onUpdate(uint16_t)` / `update(uint16_t)` override methods that no longer matched the base class after Task 1 changes
- **Fix:** Updated all four example files — scene override signatures, main loop deltaTime calculation, local `sceneTime`/`updateParameters` types
- **Files modified:** `examples/text_demo.cpp`, `examples/ecs_demo.cpp`, `examples/space_ui_demo.cpp`, `examples/graphics_output_demo.cpp`
- **Commits:** `2ee4e59`, `0a7408e`

**2. [Rule 1 - Bug] PostFx integer modulo on float time replaced with accumulator**
- **Found during:** Task 2 design review
- **Issue:** `elapsed_time % 100` would be undefined behavior on `float`
- **Fix:** Added `noisePeriodAccum` float sub-accumulator field and subtract pattern
- **Files modified:** `include/enjin2/effects/postfx.hpp`, `src/effects/postfx.cpp`
- **Commit:** `ed91cc3`

## Verification Results

- `build_test_sprite` — 100% clean: 31/31 sprite tests pass
- `build_22_sdl_lua` — 100% clean: all targets built without errors
- `shadow_mode_test` — passes (60-frame simulation with float dt)
- Zero remaining `uint16_t deltaTime` or `uint16_t dt` signatures in engine source, tests, or examples

## Self-Check: PASSED

Files exist:
- `.planning/phases/28-float-dt-migration/28-01-SUMMARY.md` — this file

Commits exist:
- `d3c9299` — Task 1 core chain migration
- `ed91cc3` — Task 2 remaining systems
- `2ee4e59` — Rule 3 fix examples (ecs_demo, text_demo)
- `0a7408e` — Rule 3 fix examples (space_ui, graphics_output)
