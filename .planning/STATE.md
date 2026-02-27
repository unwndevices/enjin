---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: Lua Scripting Foundation
status: unknown
last_updated: "2026-02-27T21:01:51.066Z"
progress:
  total_phases: 18
  completed_phases: 17
  total_plans: 43
  completed_plans: 42
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-26)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** v1.5 Lua Scripting Foundation — COMPLETE (Phase 37-03 closes all 10 CONCERNS.md items)

## Current Position

Phase: 38 of 38 (Close v1.5 Scripting Runtime Gaps) — COMPLETE (2 of 2 plans done)
Plan: 02 complete (2 of 2 plans for this phase)
Status: Phase 38-02 complete — SDL input edge callbacks (on_button_pressed/on_button_released); Phase 35 VERIFICATION.md; all 19 ctests pass
Last activity: 2026-02-27 — Phase 38-02 complete: SDL runner wired with 16-button input dispatch; Phase 35 docs closed; v1.5 milestone fully complete

Progress: [████████████████████] 100% (38/38 phases complete)

## Performance Metrics

**Velocity:**
- Total plans completed: 58
- v1.0: 21 plans (Phases 1-6)
- v1.1: 17 plans (Phases 7-15)
- v1.2: 5 plans (Phases 16-18)
- v1.3: 7 plans (Phases 19-22)
- v1.4: 8 plans (Phases 23-26)
- v1.5: 14 plans (Phase 28-01 — float dt migration; Phase 28-02 — -Woverride verification; Phase 29-01 — Object name/tag identity; Phase 29-02 — Scene proxy methods; Phase 30-01 — SSM back-pointer + deferred self-transition; Phase 31-01 — engine.* global table wiring; Phase 31-02 — engine.scene/input/time/log implementations; Phase 31-03 — engine_table_test + SDL time wiring; Phase 32-01 — ScriptProxy userdata + metatable registration; Phase 32-02 — script signature migration + SDL runner nil-self proxy; Phase 37-01 — ObjectProxy + char[256] errorMessage + tag dispatch; Phase 37-02 — ObjectProxy userdata + Object destructor hook; Phase 37-03 — clang-tidy lint target + ERR-SIBLING + INPUT-03-ORDER + GC/zero-alloc audits)

*Updated after each plan completion*

## Accumulated Context

### Decisions

All decisions logged in PROJECT.md Key Decisions table.

Recent decisions relevant to v1.5:
- [Phase 26]: assertRequires<T>() naming chosen over requires<T>() to avoid C++20 keyword collision
- [Phase 26]: ScriptProxy uses full userdata (not lightuserdata) — lightuserdata has no metatable in Lua 5.1
- [Phase 26]: float dt uses float not double — ESP32-S3 has hardware single-precision FPU; double is soft-float
- [Phase 28]: float seconds dt replaces uint16_t milliseconds throughout entire update chain
- [Phase 28]: Lua updateSprite API now expects dt in seconds (accumSec replaces accumMs)
- [Phase 28]: PostFx uses noisePeriodAccum sub-accumulator instead of integer modulo on float time
- [Phase 28-02]: -Woverride is Clang-specific; applied via $<CXX_COMPILER_ID:Clang,AppleClang> generator expression — GCC enforces override correctness as a hard compiler error natively
- [Phase 29-01]: Tests using Object directly without C_Drawable symbols need --start-group/--end-group to resolve typeinfo for C_Drawable between enjin2_core.a and enjin2_ui.a
- [Phase 29-01]: Zero-heap-allocation name/tag identity stores raw const char* pointers; caller owns string lifetime
- [Phase 29-02]: Scene::findByName and findAllWithTag are one-liner proxies to ObjectCollection — placed after findObjectWithComponent<T>() for consistent find* API grouping
- [Phase 30-01]: hasPendingTransition cleared BEFORE applyDeferredTransition() — switchTo() from onDeactivate() queues for next frame, not re-enters current dispatch
- [Phase 30-01]: Forward declaration only in scene.hpp for SceneStateMachine — no reverse #include to avoid circular include
- [Phase 30-01]: m_ssm is protected (not private) — matches m_ prefix convention; enables subclasses to expose via getSSM()
- [Phase 30-01]: switchTo() last-wins semantics — multiple calls per frame overwrite pendingSceneId (no allocation, simplest correct policy)
- [Phase 31-01]: setters (setSceneStateMachine/setActiveScene/setTimeState) are public so host code can inject without friend declarations
- [Phase 31-01]: Lua registry stores C++ pointers at registerAll() time — not at call time — so Plan 02 replacements need no re-registration
- [Phase 31-01]: engine.lua registered as empty table at Plan 01 stage; Phase 35 adds gc/memory functions
- [Phase 31-01]: Forward declarations only in bindings.hpp; scene.hpp/scene_state_machine.hpp included in bindings.cpp to avoid circular includes
- [Phase 31-02]: lua_engine_scene_find returns lightuserdata (raw Object*) — Phase 32 upgrades to full ScriptProxy with metatable
- [Phase 31-02]: engine.log uses printf exclusively (not std::cout) — embedded target compatibility with ESP32/Emscripten
- [Phase 31-02]: lua_typename fallback in engine.log prevents nullptr deref when lua_tostring returns nullptr for boolean/table/nil types
- [Phase 31-03]: s_totalTime and s_frameCount declared inside ENJIN2_BUILD_LUA guard — avoids unused-variable warnings in non-Lua builds
- [Phase 31-03]: Hot-reload resets s_totalTime and s_frameCount alongside prev_ticks — prevents time-jump artifacts after F5 script reload
- [Phase 32-01]: ScriptProxy property dispatch uses strcmp chain (6 properties); self.layer is 1-indexed; name is read-only; callWithProxy() retrieves proxy from registry before lua_pcall
- [Phase 32-02]: Option A chosen for SDL runner — push nil as self before dt; ensures pikachu_demo.lua receives correct dt value; lua_L avoids shadowing outer scope L
- [Phase 32-02]: pikachu_demo dt * 1000 removed — Phase 28 confirmed complete (accumSec present); all scripts use (self, dt) and (self) signatures atomically
- [bindings-split]: bindings.cpp split into 7 files by functional area — bindings.cpp (core), bindings_draw.cpp, bindings_input_sprites.cpp, bindings_layers_text.cpp, bindings_engine.cpp, bindings_math.cpp, bindings_system.cpp; bind_helpers.hpp adds LuaFuncDef + luaBindFunctions for zero-overhead table registration
- [Phase 33-01]: lua_script.hpp reconciled to LuaScriptSystem/LuaCanvas; IScriptInterpreter/IScriptGraphics fully removed
- [Phase 33-01]: LuaCanvas gains ICanvas<Pixel4>* constructor — C_LuaScript::draw() abstract interface now wrappable without template width/height
- [Phase 33-01]: GetWidth()/GetHeight() PascalCase correction in lua_script.cpp — C_Drawable accessor convention enforced
- [Phase 33-02]: Policy dispatch moved INTO callWithProxy() — single error capture point dispatches on errorPolicy field (Disable/Log/Panic)
- [Phase 33-02]: ERR-04 (Panic) tested by field value only — std::abort() kills test process; live Panic invocation excluded from automated tests
- [Phase 34-01]: callWithProxyAndBtn() mirrors callWithProxy() error handling exactly — ScriptErrorPolicy dispatch applies uniformly to input callbacks
- [Phase 34-01]: dispatchInputCallbacks() called BEFORE lastUpdateTime and callWithProxy(UPDATE_FUNCTION) in update() — satisfies INPUT-03 ordering requirement
- [Phase 34-01]: After dispatchInputCallbacks(), update() does NOT re-check scriptError mid-frame — Disable policy error on input callback means update still runs same frame; caught on next frame by top-of-update guard
- [Phase 34-01]: LuaBindings::getInput() inline const accessor — zero overhead path to currentInput from lua_script.cpp
- [Phase 35-01]: assertRequires<T>() uses assert(false&&"message") in debug, printf+setEnabled(false) in release — no process abort on ESP32
- [Phase 35-01]: LUA_GCSTEP used for collect() not LUA_GCCOLLECT — incremental avoids stop-the-world frame spike
- [Phase 35-01]: component.hpp includes object.hpp (not circular: object.hpp only forward-declares Component)
- [Phase 35-01]: DEP-03 test case gated #ifdef NDEBUG — debug path would abort test process via assert(false)
- [Phase 36-01]: getComponents<T>() uses caller-provides-buffer pattern (T** out, size_t maxOut) — zero heap allocation; O(n) dynamic_cast scan; static_assert(is_base_of<Component,T>) for compile-time safety
- [Phase 36-01]: Only C_Position fast cache retained in Object; C_Drawable cache eliminated removing enjin2_core->enjin2_ui layering violation; scene.hpp/animation.hpp callers deferred to Plan 02
- [Phase 36-02]: named_objects_test --start-group no longer required after C_Drawable cache removal from object.cpp in Plan 01 — simplified to plain link
- [Phase 36-02]: scene.hpp renderObjects() and animation.hpp start() color track now use getComponents<C_Drawable>() and getComponent<C_Drawable>() respectively — enjin2_core->enjin2_ui layering violation fully eliminated
- [Phase 37-01]: Stale proxy guard split: !proxy returns nil (C++ defensive guard); stale valid/component calls luaL_error (Lua-visible error for destroyed object access)
- [Phase 37-01]: Tag method dispatch in __index: strcmp key returns lua_pushcfunction — addTag/hasTag/clearTags now callable as self:method() from Lua
- [Phase 37-01]: char[256]{} errorMessage replaces std::string — zero heap allocation on error paths; getErrorMessage() returns const char*
- [Phase 37-01]: addComponent overflow: assert(false&&"...") in debug (fast failure); fprintf(stderr)+return nullptr in release (no abort on embedded targets)
- [Phase 37-02]: ObjectProxy impl functions are file-scope statics in bindings.cpp — NOT static class member declarations; class member decls caused linker error (class vs file-scope symbol mismatch)
- [Phase 37-02]: proxy.enable returns nil (not error) when object has no C_LuaScript — allows proxy usage on non-scripted objects without Lua errors
- [Phase 37-02]: proxy.position read returns value-semantics snapshot table {x,y} (not live reference) — no GC overhead, appropriate for embedded targets
- [Phase 37-02]: ObjectProxy in standalone object_proxy.hpp with only Object forward-declaration avoids circular include between object.hpp and bindings.hpp
- [Phase 37-03]: CLANG_TIDY option OFF by default — CI explicitly opts in with -DCLANG_TIDY=ON; no dev friction on configure
- [Phase 37-03]: --warnings-as-errors=* on clang-tidy CLI not in .clang-tidy WarningsAsErrors field — local dev advisory, CI hard-fails
- [Phase 37-03]: test_input03_order_call_sequence() named differently from test_input03_callbacks_fire_before_update() — avoids duplicate symbol; uses call_order/'PU' complementing callback_order/'callback_then_update' pattern
- [Phase 27]: Two-branch if constexpr retained for Pixel4/uint8_t dispatch — collapsing would fail overload resolution at compile time
- [Phase 38]: pointer-to-pointer registry: &m_ssm and &m_activeScene instead of value snapshots — same pattern as m_timeState
- [Phase 38]: loadScriptFile() proxy block is exact copy of executeScript() block — ensures identical init(self) behavior on file-load and string-load paths
- [Phase 38]: ENG-01 live test: addScene<MinimalScene>(2u) required before switchTo(2) — switchTo only queues for registered scene IDs; verified via mockSSM.update()
- [Phase 38-02]: SDL runner dispatch loop uses for-loop condition 'btn < 16 && lua_ok' plus inner break — satisfies INPUT-03 ordering in SDL production runner

### Pending Todos

None.

### Quick Tasks Completed

| # | Description | Date | Commit | Status | Directory |
|---|-------------|------|--------|--------|-----------|
| 001 | Write simple design document of the library | 2026-02-03 | 24dd586 | | [001-write-simple-design-document](./quick/001-write-simple-design-document-of-the-libr/) |
| 2 | Aseprite-to-enjin asset conversion tooling | 2026-02-26 | fb6c875 | | [2-aseprite-to-enjin-asset-conversion-tooli](./quick/2-aseprite-to-enjin-asset-conversion-tooli/) |
| 3 | Aseprite Lua export plugin for enjin C header format | 2026-02-26 | 5a124e9 | | [3-aseprite-lua-plugin-for-enjin-export](./quick/3-aseprite-lua-plugin-for-enjin-export/) |
| 4 | Update .gitignore with build artifact patterns | 2026-02-26 | 4c1ec72 | | [4-update-gitignore](./quick/4-update-gitignore/) |
| 5 | Check conformity to plan direction and standards | 2026-02-27 | 9e859fd | Verified | [5-check-conformity-to-plan-direction-and-s](./quick/5-check-conformity-to-plan-direction-and-s/) |
| 6 | clang-tidy static analysis config for C++17 embedded engine | 2026-02-27 | c143f2f | Complete | [6-if-not-present-already-lets-setup-clang-](./quick/6-if-not-present-already-lets-setup-clang-/) |
| Phase 33-scripterrorpolicy P01 | 2 | 2 tasks | 4 files |
| Phase 33-scripterrorpolicy P02 | 3 | 2 tasks | 4 files |
| Phase 34 P01 | 3 | 5 tasks | 5 files |
| Phase 35 P01 | 4 | 3 tasks | 5 files |
| Phase 36-object-drawable-cache-decoupling P01 | 3 | 2 tasks | 2 files |
| Phase 36-object-drawable-cache-decoupling P02 | 2 | 2 tasks | 4 files |
| Phase 27 P01 | 80 | 2 tasks | 3 files |
| Phase 38 P01 | 12 | 3 tasks | 4 files |
| Phase 38 P02 | 2 | 2 tasks | 2 files |

### Blockers/Concerns

- [Phase 32-01 RESOLVED] ScriptProxy uses bool valid flag (not generation token) — simpler, sufficient for single proxy per C_LuaScript lifetime
- [Phase 31-01 RESOLVED] engine.* global table now registered; module-level access no longer a concern for Phase 31-02 onward
- [Phase 31-03 RESOLVED] engine_table_test passes; module-level access verified — Phase 32 unblocked
- [Phase 25 spec] ESP32 PSRAM availability for 4-layer stack — may require compile-time layer count reduction to 2

### Roadmap Evolution

- Phase 36 added: object-drawable-cache-decoupling
- Phase 37 added: address prominent codebase concerns

### Technical Debt (carried)

- Full Emscripten toolchain build not verified (code inspection conclusive)
- getPaletteRGB() snapshot semantics — callers must re-invoke after palette mutation

## Session Continuity

Last session: 2026-02-27
Stopped at: Completed 38-01-PLAN.md — pointer-to-pointer registry fix (ENG-01/ENG-02), loadScriptFile proxy (PROXY-01), 3 live-wiring tests; all 19 ctests pass
Resume file: None
