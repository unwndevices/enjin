---
phase: 38-close-v1.5-scripting-runtime-gaps
verified: 2026-02-27T22:30:00Z
status: passed
score: 7/7 must-haves verified
re_verification: false
---

# Phase 38: Close v1.5 Scripting Runtime Gaps — Verification Report

**Phase Goal:** Close the three open v1.5 runtime requirements: engine.scene.switch(), engine.scene.find(), and loadScriptFile() init(self) proxy. Wire SDL input edge callbacks. Achieve 19/19 ctests passing.
**Verified:** 2026-02-27T22:30:00Z
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | engine.scene.switch(id) reaches SceneStateMachine::switchTo() after post-registerAll injection | VERIFIED | bindings.cpp:387 stores `&m_ssm`; bindings_engine.cpp:95 casts to `SceneStateMachine**` and dereferences; ENG-01 live test in engine_table_test confirms mockSSM.getCurrentScene()->getId() == 2u after Lua call |
| 2 | engine.scene.find('name') returns valid ObjectProxy (not nil) for named Object after setActiveScene() | VERIFIED | bindings.cpp:389 stores `&m_activeScene`; bindings_engine.cpp:110 casts to `Scene**` and dereferences; ENG-02 live test confirms `found == 1.0` via Lua |
| 3 | A script loaded via loadScriptFile() receives a valid self proxy in its init(self) callback | VERIFIED | lua_script.cpp:92-127 proxy creation block identical to executeScript(); callWithProxy(INIT_FUNCTION) at line 127; PROXY-01 test writes /tmp/proxy01_test.lua and confirms `init_self_valid == 1.0` |
| 4 | on_button_pressed and on_button_released Lua globals called in SDL runner on button edge events | VERIFIED | sdl_main.cpp:273-309 — 16-button justPressed/justReleased dispatch loop after setInput() (line 269) and before update lua_getglobal block (line 312); satisfies INPUT-03 ordering |
| 5 | Phase 35 VERIFICATION.md documents GC-01, GC-02, DEP-01, DEP-02, DEP-03 as verified | VERIFIED | `.planning/phases/35-gc-control-component-assertions/35-VERIFICATION.md` exists with VERIFIED status for all 5 requirements; committed at 2729929 |
| 6 | All 19 ctests pass after these changes | VERIFIED | `ctest --output-on-failure` output: "100% tests passed, 0 tests failed out of 19" |
| 7 | ENG-01, ENG-02, PROXY-01 checkboxes are [x] in REQUIREMENTS.md | VERIFIED | REQUIREMENTS.md lines 35-36 show `[x] **ENG-01**` and `[x] **ENG-02**`; line 44 shows `[x] **PROXY-01**`; traceability table maps both to Phase 38 Complete |

**Score:** 7/7 truths verified

---

## Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/scripting/bindings.cpp` | registerAll() stores `&m_ssm` and `&m_activeScene` (pointer-to-pointer) | VERIFIED | Lines 387-390: `lua_pushlightuserdata(L, &m_ssm)` and `lua_pushlightuserdata(L, &m_activeScene)` — committed at ebd4094 |
| `src/scripting/bindings_engine.cpp` | lua_engine_scene_switch and lua_engine_scene_find dereference double pointer from registry | VERIFIED | Lines 95-100: `auto** ssmPP = static_cast<SceneStateMachine**>(...)` + `(*ssmPP)->switchTo(id)`; lines 110-113: `auto** scenePP = static_cast<Scene**>(...)` + `Scene* scene = *scenePP;` |
| `src/components/lua_script.cpp` | loadScriptFile() creates ScriptProxy before calling callWithProxy(INIT_FUNCTION) | VERIFIED | Lines 92-127: full proxy creation block (invalidate-old, create-new, set-metatable, store-in-registry); line 127: `callWithProxy(INIT_FUNCTION, 0.0f, false)` |
| `tests/engine_table_test.cpp` | Live-wiring tests for ENG-01, ENG-02, PROXY-01 present and called from main() | VERIFIED | Lines 295-380: three test functions; lines 396-398: all three called from main(); committed at 18e2c24 |
| `src/platform/sdl/sdl_main.cpp` | Per-frame input edge callback dispatch loop for on_button_pressed and on_button_released | VERIFIED | Lines 273-309: 16-button loop with justPressed/justReleased dispatch, placed after setInput() (line 269) and before update block (line 312); committed at 3965f3e |
| `.planning/phases/35-gc-control-component-assertions/35-VERIFICATION.md` | Verification document for Phase 35 GC-01, GC-02, DEP-01, DEP-02, DEP-03 | VERIFIED | File exists, contains VERIFIED status for all five requirements with code evidence and ctest test names; committed at 2729929 |

---

## Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| bindings.cpp registerAll() | bindings_engine.cpp lua_engine_scene_switch() | LUA_REGISTRYINDEX 'enjin_ssm' key stores `&m_ssm` | WIRED | bindings.cpp:387 `lua_pushlightuserdata(L, &m_ssm)` feeds bindings_engine.cpp:95 `static_cast<SceneStateMachine**>(lua_touserdata(...))` |
| bindings.cpp registerAll() | bindings_engine.cpp lua_engine_scene_find() | LUA_REGISTRYINDEX 'enjin_active_scene' key stores `&m_activeScene` | WIRED | bindings.cpp:389 `lua_pushlightuserdata(L, &m_activeScene)` feeds bindings_engine.cpp:110 `static_cast<Scene**>(lua_touserdata(...))` |
| lua_script.cpp loadScriptFile() | callWithProxy(INIT_FUNCTION) | ScriptProxy userdata created in registry before init call | WIRED | lua_script.cpp:107 `lua_newuserdata(L, sizeof(ScriptProxy))` + registry storage at lines 119-121; `callWithProxy` at line 127 |
| sdl_main.cpp input advance/poll block | on_button_pressed / on_button_released Lua globals | per-frame justPressed/justReleased loop after setInput() call | WIRED | setInput() at line 269; dispatch loop lines 273-309; update block begins at line 312 — correct INPUT-03 ordering |

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| ENG-01 | 38-01-PLAN.md | Lua scripts access engine.scene.switch(id) to request scene transitions | SATISFIED | Pointer-to-pointer fix in bindings.cpp + bindings_engine.cpp; live test `test_engine_scene_live_switch` passes; REQUIREMENTS.md shows [x] |
| ENG-02 | 38-01-PLAN.md | Lua scripts access engine.scene.find(name) to locate named objects (returns proxy or nil) | SATISFIED | Same pointer-to-pointer fix; live test `test_engine_scene_live_find` passes; REQUIREMENTS.md shows [x] |
| PROXY-01 | 38-01-PLAN.md | Every Lua callback receives self as the first argument; init(self) receives valid proxy | SATISFIED | loadScriptFile() proxy creation block; live test `test_proxy01_load_script_file_init_self` writes actual .lua file, loads it, verifies `init_self_valid == 1.0`; REQUIREMENTS.md shows [x] |

No orphaned requirements — ENG-01, ENG-02, PROXY-01 are the only requirement IDs declared across all plans for Phase 38. All three are marked [x] in REQUIREMENTS.md with Phase 38 / Complete in the traceability table.

Note: 38-02-PLAN.md also lists `requirements: [ENG-01, ENG-02, PROXY-01]` in its frontmatter. Its work (SDL input callbacks, Phase 35 docs) supports the overall milestone but doesn't satisfy new requirement IDs beyond those covered by Plan 01.

---

## Anti-Patterns Found

No anti-patterns found in modified files. Scan of `src/scripting/bindings.cpp`, `src/scripting/bindings_engine.cpp`, `src/components/lua_script.cpp`, and `tests/engine_table_test.cpp` returned no TODO/FIXME/PLACEHOLDER/return-stub matches.

---

## Human Verification Required

### 1. SDL input edge callbacks fire in the production game loop

**Test:** Build with ENJIN2_BUILD_SDL=ON. Load a Lua script that defines `on_button_pressed(self, btn)` and logs `btn`. Press a button on a connected gamepad or keyboard. Verify the log prints.
**Expected:** Each button press triggers on_button_pressed once (edge, not held); each release triggers on_button_released once.
**Why human:** sdl_main.cpp is excluded from the ctest build (ENJIN2_BUILD_SDL=OFF). The dispatch loop is verified by code inspection only — runtime behavior in the SDL runner requires a human with a build that includes SDL.

---

## Commits Verified

All commits referenced in SUMMARY files were verified to exist in git history:

| Commit | Description |
|--------|-------------|
| ebd4094 | fix(38-01): pointer-to-pointer registry wiring for ENG-01 and ENG-02 |
| 2c76d81 | fix(38-01): loadScriptFile() creates ScriptProxy before calling init (PROXY-01) |
| 18e2c24 | test(38-01): add live-wiring tests for ENG-01, ENG-02, and PROXY-01 |
| 3965f3e | feat(38-02): add input edge callback dispatch loop to SDL runner |
| 2729929 | docs(38-02): write Phase 35 VERIFICATION.md for GC and component assertion requirements |

---

## Summary

Phase 38 achieves its goal. The three open v1.5 runtime requirements are closed:

- **ENG-01**: The silent no-op bug in engine.scene.switch() is fixed. registerAll() now stores `&m_ssm` (address-of-member), and lua_engine_scene_switch() dereferences the double pointer. Post-registerAll injection via setSceneStateMachine() is live. The ENG-01 live test confirms SceneStateMachine::switchTo() is actually reached.

- **ENG-02**: The same pointer-to-pointer fix applies to `&m_activeScene`. lua_engine_scene_find() now correctly accesses the injected scene. The ENG-02 live test confirms a named Object is found and returns a non-nil ObjectProxy.

- **PROXY-01**: loadScriptFile() now creates a ScriptProxy userdata and stores it in the Lua registry before calling init, exactly mirroring the executeScript() code path. The PROXY-01 live test writes a real .lua file to /tmp and confirms init(self) receives a valid proxy (not nil).

Additional work: SDL runner gained a 16-button input edge dispatch loop satisfying INPUT-03 ordering in the production path. Phase 35 documentation debt is closed with a formal VERIFICATION.md.

19/19 ctests pass. All ENG-01, ENG-02, PROXY-01 checkboxes are [x] in REQUIREMENTS.md. The v1.5 milestone has no remaining open requirement IDs.

---

_Verified: 2026-02-27T22:30:00Z_
_Verifier: Claude (gsd-verifier)_
