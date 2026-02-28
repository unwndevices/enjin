---
phase: 27-fix-onrender-pixel4-bug
plan: 01
subsystem: core/rendering
tags: [scene, pixel4, rendering, bugfix, regression-test]
dependency_graph:
  requires: []
  provides: [RENDER-01]
  affects: [scene.hpp, scene_render_test.cpp]
tech_stack:
  added: []
  patterns: ["if constexpr dispatch", "ASSERT macro test harness"]
key_files:
  created:
    - tests/scene_render_test.cpp
  modified:
    - include/enjin2/core/scene.hpp
    - tests/CMakeLists.txt
decisions:
  - "Two-branch if constexpr form retained (Pixel4 then uint8_t) — collapsing to single onRender(canvas) without if constexpr guards would fail overload resolution at compile time"
metrics:
  duration: "80 seconds"
  completed: "2026-02-27"
  tasks_completed: 2
  files_changed: 3
---

# Phase 27 Plan 01: Fix onRender Pixel4 Dispatch Bug Summary

**One-liner:** Added `if constexpr (std::is_same_v<PixelType, Pixel4>)` dispatch branch in `Scene::render()` so derived scenes can draw to Pixel4 canvases via `onRender(ICanvas<Pixel4>&)`.

## What Was Built

Fixed a silent rendering skip in `Scene::render<Pixel4>()` that prevented derived scenes from drawing backgrounds or overlays on Pixel4 canvases. The `else` stub (with comment "skip scene-specific rendering for now") was replaced with an explicit `if constexpr` Pixel4 branch calling `onRender(canvas)`.

Added `tests/scene_render_test.cpp` with 3 tests registered in CTest:
1. `test_onRender_pixel4_called()` — flag-based verification that `onRender(ICanvas<Pixel4>&)` is invoked
2. `test_onRender_pixel4_pixels_appear()` — pixel-output verification (writes `Pixel4(7)` at (0,0), reads back)
3. `test_onRender_uint8_still_works()` — regression guard for `uint8_t` canvas path

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Fix Pixel4 dispatch and add regression test | f8011cf | include/enjin2/core/scene.hpp, tests/scene_render_test.cpp |
| 2 | Register test in CMake and run full suite | 0a3447b | tests/CMakeLists.txt |

## Verification Results

- `grep -n "is_same_v<PixelType, Pixel4>" include/enjin2/core/scene.hpp` — line 123 confirmed
- Old comment "skip scene-specific rendering for now" — GONE
- `ctest --output-on-failure` — 8/8 tests pass: input_test, palette_test, sprite_test, compositor_test, named_objects_test, drawable_decoupling_test, scene_render_test, scene_transition_test

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] CMakeLists.txt added during Task 1 (not Task 2)**

- **Found during:** Task 1 verification
- **Issue:** Task 1 verify step required building `scene_render_test` target, which didn't exist in CMakeLists.txt yet (Task 2 was planned to add it)
- **Fix:** Added `scene_render_test` CMake target before Task 1 verification to unblock the build step; committed as part of Task 2
- **Files modified:** tests/CMakeLists.txt
- **Commit:** 0a3447b

## Self-Check: PASSED

- FOUND: include/enjin2/core/scene.hpp
- FOUND: tests/scene_render_test.cpp
- FOUND: tests/CMakeLists.txt
- FOUND commit: f8011cf
- FOUND commit: 0a3447b
