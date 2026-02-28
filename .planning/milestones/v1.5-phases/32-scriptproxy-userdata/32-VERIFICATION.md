---
phase: 32-scriptproxy-userdata
verified: 2026-02-27T02:53:24Z
status: gaps_found
score: 9/14 must-haves verified
re_verification: false
gaps:
  - truth: "C_LuaScript::callWithProxy() retrieves the stored proxy from the registry and pushes it as the first argument before lua_pcall"
    status: failed
    reason: "lua_script.cpp is not in any CMake target. The callWithProxy() implementation exists in the file but is never compiled into a library. The enjin2_lua target only compiles lua_engine.cpp, lua_platform.cpp, and bindings.cpp."
    artifacts:
      - path: "src/components/lua_script.cpp"
        issue: "File exists with full callWithProxy() implementation but excluded from all CMake targets — code is dead"
      - path: "include/enjin2/components/lua_script.hpp"
        issue: "Header declares interpreter: IScriptInterpreter* but lua_script.cpp uses scriptSystem: LuaScriptSystem* — incompatible types, files cannot compile together"
    missing:
      - "Add lua_script.cpp to a CMake target (e.g., enjin2_lua or a new enjin2_components target)"
      - "Reconcile lua_script.hpp and lua_script.cpp: the header must declare the same fields the .cpp uses (scriptSystem, luaCanvas) or the .cpp must use the interface the header declares"

  - truth: "One proxy userdata is created per C_LuaScript in executeScript() and stored in the Lua registry keyed by lightuserdata(this)"
    status: failed
    reason: "lua_script.cpp is not compiled. The proxy creation block in executeScript() (lines 129-162) is dead code. No built artifact calls luaL_getmetatable('ScriptProxy') or lua_settable(LUA_REGISTRYINDEX) from C_LuaScript."
    artifacts:
      - path: "src/components/lua_script.cpp"
        issue: "executeScript() proxy creation block (lines 129-162) is unreachable — file not in build"
    missing:
      - "Same root fix: add lua_script.cpp to CMake target after resolving header mismatch"

  - truth: "init(self), update(self, dt), and draw(self) all receive the proxy userdata as the first argument"
    status: partial
    reason: "update() and draw() use callWithProxy() (lines 227, 248) which is correct. However: (1) lua_script.cpp is not built so none of this executes; (2) loadScriptFile() at line 91 calls callScriptFunctionSafe(INIT_FUNCTION) — the file-load path does NOT use callWithProxy, meaning init() would not receive the proxy even if the file were compiled."
    artifacts:
      - path: "src/components/lua_script.cpp"
        issue: "loadScriptFile() calls callScriptFunctionSafe(INIT_FUNCTION) instead of callWithProxy(INIT_FUNCTION, 0.0f, false) — init() missing proxy on file-load path"
    missing:
      - "In loadScriptFile(): replace callScriptFunctionSafe(INIT_FUNCTION) with proxy creation block + callWithProxy(INIT_FUNCTION, 0.0f, false) mirroring executeScript()"

  - truth: "C_LuaScript destructor sets proxy->valid = false before calling scriptSystem->shutdown()"
    status: failed
    reason: "lua_script.cpp is not compiled. The destructor implementation at lines 19-35 is dead code."
    artifacts:
      - path: "src/components/lua_script.cpp"
        issue: "Destructor (lines 19-35) with proxy->valid = false exists but is not built"
    missing:
      - "Same root fix: add lua_script.cpp to CMake target"

  - truth: "Proxy is not recreated on every callback — one userdata per script lifetime, reused each frame"
    status: failed
    reason: "lua_script.cpp is not compiled. The registry-keyed design exists in code but is never executed."
    artifacts:
      - path: "src/components/lua_script.cpp"
        issue: "Registry storage pattern (lines 131-162) is dead code"
    missing:
      - "Same root fix: add lua_script.cpp to CMake target"
human_verification:
  - test: "Load a Lua script via C_LuaScript::loadScript() and verify init(self) receives a valid ScriptProxy userdata"
    expected: "self is non-nil userdata; self.x, self.y, self.visible read valid component values"
    why_human: "C_LuaScript is not exercised by any current test — requires integration test with an Object+C_LuaScript instance"
  - test: "Verify self.layer is 1-indexed: a component on buffer_index=0 should report self.layer == 1 in Lua"
    expected: "self.layer returns 1 for layer BG, 2 for MID, etc."
    why_human: "No automated test exists for proxy property dispatch through C_LuaScript"
  - test: "Destroy an Object hosting C_LuaScript, then attempt to access self.x in a stored Lua closure — should return nil, not crash"
    expected: "proxy->valid=false prevents dereference; self.x returns nil"
    why_human: "Destructor invalidation path requires runtime object lifetime test"
---

# Phase 32: ScriptProxy Userdata Verification Report

**Phase Goal:** C_LuaScript passes a ScriptProxy userdata as `self` to init/update/draw callbacks — scripts can read and write x, y, visible, layer, active, and name through self; proxy invalidates safely on destruction; all existing scripts migrated to (self, ...) signatures
**Verified:** 2026-02-27T02:53:24Z
**Status:** gaps_found
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | ScriptProxy struct exists in bindings.hpp with C_LuaScript* component and bool valid fields | VERIFIED | `include/enjin2/scripting/bindings.hpp` lines 34-37: `struct ScriptProxy { C_LuaScript* component; bool valid; }` |
| 2 | luaL_newmetatable called once for 'ScriptProxy' in LuaBindings::registerAll() with __index and __newindex set | VERIFIED | `registerProxyMetatable()` called at line 359 of `bindings.cpp`; `luaL_newmetatable(L, PROXY_METATABLE)` at line 409 with `__index` and `__newindex` set |
| 3 | One proxy userdata is created per C_LuaScript in executeScript() and stored in the Lua registry keyed by lightuserdata(this) | FAILED | `lua_script.cpp` is NOT in any CMake target; proxy creation block is dead code |
| 4 | C_LuaScript::callWithProxy() retrieves the stored proxy from the registry and pushes it as the first argument before lua_pcall | FAILED | `lua_script.cpp` not compiled into any built artifact |
| 5 | init(self), update(self, dt), and draw(self) all receive the proxy userdata as the first argument | PARTIAL | update/draw call sites use callWithProxy in lua_script.cpp; but (a) file is not built, (b) loadScriptFile() still calls callScriptFunctionSafe(INIT_FUNCTION) — file-load path missing callWithProxy |
| 6 | self.x and self.y read/write C_Position via owner->getPosition() | VERIFIED | `bindings.cpp` lines 44-50 and 90-100: strcmp dispatch to `pos->getPosition().x`/`y` and `pos->setPosition()` |
| 7 | self.visible reads/writes C_Drawable::is_visible via isVisible()/SetVisibility() | VERIFIED | `bindings.cpp` lines 52-53 and 102-103: `comp->isVisible()` and `comp->SetVisibility()` |
| 8 | self.layer reads/writes C_Drawable::buffer_index via GetBufferIndex()/SetBufferIndex() with 1-indexed Lua conversion | VERIFIED | `bindings.cpp` lines 55-57 and 104-109: `GetBufferIndex() + 1` read, `SetBufferIndex(luaLayer - 1)` write |
| 9 | self.active reads/writes Object::active via isActive()/setActive() | VERIFIED | `bindings.cpp` lines 59-60 and 110-111: `owner->isActive()` and `owner->setActive()` |
| 10 | self.name reads Object::getName() — read-only, __newindex for 'name' is a no-op | VERIFIED | `bindings.cpp` lines 62-70: `owner->getName()` in __index; __newindex has no 'name' branch (silently ignored) |
| 11 | Accessing self.x on an invalidated proxy (valid=false) returns nil without crashing | VERIFIED | `bindings.cpp` lines 30-33: `!proxy->valid` check returns nil before any dereference |
| 12 | C_LuaScript destructor sets proxy->valid = false before calling scriptSystem->shutdown() | FAILED | Destructor implementation in `lua_script.cpp` is dead code — file not in any CMake target |
| 13 | Proxy is not recreated on every callback — one userdata per script lifetime, reused each frame | FAILED | Registry storage design exists in lua_script.cpp but is not compiled |
| 14 | reload_test.lua, layer_demo.lua, pikachu_demo.lua, e2e_parity.lua all have update(self, dt) and draw(self) signatures | VERIFIED | Direct file inspection confirms all four scripts have correct signatures; pikachu_demo.lua uses `updateSprite(sprite, dt)` without `* 1000` |

**Score:** 9/14 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `include/enjin2/scripting/bindings.hpp` | Forward declaration of C_LuaScript; ScriptProxy struct with component and valid fields | VERIFIED | Lines 27 and 34-37 confirmed |
| `src/scripting/bindings.cpp` | registerProxyMetatable() called from registerAll(); lua_proxy_index and lua_proxy_newindex static functions; all 6 properties dispatched | VERIFIED | All present and substantive |
| `include/enjin2/components/lua_script.hpp` | callWithProxy() private method declaration | VERIFIED | Lines 223-224 confirmed |
| `src/components/lua_script.cpp` | Proxy creation in executeScript(); proxy invalidation in destructor; callWithProxy() implementation | ORPHANED | File exists with complete implementation but is NOT included in any CMake target — code is unreachable |
| `scripts/reload_test.lua` | update(self, dt) and draw(self) signatures | VERIFIED | Lines 20, 25 |
| `scripts/layer_demo.lua` | update(self, dt) and draw(self) signatures | VERIFIED | Lines 4, 8 |
| `scripts/pikachu_demo.lua` | update(self, dt) and draw(self); updateSprite(sprite, dt) | VERIFIED | Lines 10, 16; dt without * 1000 confirmed |
| `scripts/e2e_parity.lua` | update(self, dt) and draw(self) signatures | VERIFIED | Lines 45, 50 |
| `src/platform/sdl/sdl_main.cpp` | SDL runner pushes nil as self before dt for update; nil as self for draw | VERIFIED | Lines 276-293 confirmed |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| LuaBindings::registerAll() | ScriptProxy metatable | luaL_newmetatable(L, "ScriptProxy") | WIRED | `bindings.cpp` line 409 — called at end of registerAll() (line 359) |
| C_LuaScript::executeScript() | Lua registry | lua_newuserdata + lua_pushlightuserdata(this) + lua_settable(LUA_REGISTRYINDEX) | NOT_WIRED | lua_script.cpp not compiled — link exists in source but never executes |
| C_LuaScript::callWithProxy() | lua_pcall | lua_pushlightuserdata(this) + lua_gettable(REGISTRYINDEX) then push dt if needed | NOT_WIRED | lua_script.cpp not compiled |
| C_LuaScript::~C_LuaScript() | proxy->valid = false | retrieve proxy from registry; write false BEFORE scriptSystem->shutdown() | NOT_WIRED | lua_script.cpp not compiled |
| SDL runner g_lua.callFunction('update', dt) | Lua update(self, dt) | lua_pushnil then lua_pushnumber(dt) then lua_pcall(L, 2, 0, 0) | WIRED | sdl_main.cpp lines 273-286 confirmed |
| pikachu_demo.lua update(self, dt) | updateSprite(sprite, dt) | dt without * 1000 | WIRED | pikachu_demo.lua line 12 confirmed |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| PROXY-01 | 32-01 | Every Lua callback receives `self` as the first argument: init(self), update(self, dt), draw(self) | BLOCKED | C_LuaScript callbacks with proxy dispatch not compiled; metatable registered but no integration path to C_LuaScript |
| PROXY-02 | 32-01 | Scripts can read/write self.x, self.y, self.visible, self.layer mapped to C++ component properties | PARTIAL | __index/__newindex dispatch code is correct and compiled into enjin2_lua; but it is unreachable without a C_LuaScript proxy being pushed, which never happens |
| PROXY-03 | 32-01 | ScriptProxy uses a validity mechanism to prevent dangling pointer access after Object destruction | PARTIAL | valid=false check in __index/__newindex is compiled; destructor invalidation is not (lua_script.cpp excluded from build) |
| PROXY-04 | 32-02 | All existing Lua scripts migrated to new (self, ...) callback signature atomically | SATISFIED | All four scripts verified; SDL runner updated; 9/9 tests pass |

No orphaned requirements found — all four PROXY IDs declared in plan frontmatter match REQUIREMENTS.md.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/components/lua_script.cpp` | 1-319 | File entirely absent from CMake targets | Blocker | callWithProxy(), executeScript() proxy block, destructor invalidation all dead code |
| `src/components/lua_script.cpp` | 91 | `callScriptFunctionSafe(INIT_FUNCTION)` in loadScriptFile() | Blocker | init() would not receive proxy even if file were compiled — file-load path missed during migration |
| `include/enjin2/components/lua_script.hpp` | 29, 201 | Declares `interpreter: IScriptInterpreter*` and `initializeInterpreter()` | Warning | Header and .cpp implement different designs; files would not compile together without reconciliation |

### Human Verification Required

#### 1. Proxy Dispatch Integration Test

**Test:** Create an Object with a C_Position and a C_LuaScript component; load a script with `function init(self) result = self.x end`; verify `result` equals the position x value.
**Expected:** `result` is a valid integer matching C_Position.x
**Why human:** No unit test exists that instantiates C_LuaScript through the component/object system. Requires build-level fix first (see gaps).

#### 2. Proxy Invalidation Under Destruction

**Test:** Load a script that captures `self` in a closure. Destroy the Object. Access `self.x` from the captured closure.
**Expected:** Returns `nil` without crash (valid=false guards all reads).
**Why human:** Requires runtime lifetime test; automated grep cannot verify the temporal ordering of valid=false vs lua_close.

#### 3. Layer 1-Indexing End-to-End

**Test:** In a Lua script, write `self.layer = 1` and verify the C++ component's `buffer_index` is 0 (LAYER_BG).
**Expected:** `buffer_index == 0` after `self.layer = 1`
**Why human:** Requires C_LuaScript integration test with a Drawable component.

### Gaps Summary

The root cause of all four failed truths is a single structural issue: **`src/components/lua_script.cpp` is not compiled into any CMake target**. The `enjin2_lua` library contains only `lua_engine.cpp`, `lua_platform.cpp`, and `bindings.cpp`. The `C_LuaScript` component implementation — including all proxy creation, callWithProxy() dispatch, registry storage, and destructor invalidation — lives in an orphaned file.

The SUMMARY for plan 32-01 documented this explicitly: "lua_script.cpp is not in any CMake target so this doesn't affect builds." This was treated as acceptable pre-existing tech debt, but it means PROXY-01, PROXY-02, and PROXY-03 are not satisfied end-to-end in the shipped artifact: the metatable exists, but no C_LuaScript can ever push a proxy through it.

Additionally, a separate gap exists independently of the CMake issue: `loadScriptFile()` at line 91 still calls `callScriptFunctionSafe(INIT_FUNCTION)` instead of the proxy-aware `callWithProxy(INIT_FUNCTION, 0.0f, false)`. This would need fixing regardless.

**What IS working:**
- ScriptProxy metatable is registered in `enjin2_lua` (bindings.cpp) — PROXY-02's dispatch logic is correct
- The invalidation check (`proxy->valid == false` → return nil) is compiled and correct
- PROXY-04 is fully satisfied: all four Lua scripts have correct signatures and the SDL runner correctly pushes nil-self
- Build is clean, all 9 tests pass

---

_Verified: 2026-02-27T02:53:24Z_
_Verifier: Claude (gsd-verifier)_
