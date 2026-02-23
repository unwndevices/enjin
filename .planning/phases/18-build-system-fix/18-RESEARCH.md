# Phase 18: Build System Fix - Research

**Researched:** 2026-02-23
**Domain:** CMake build system / Emscripten WASM / conditional compilation
**Confidence:** HIGH

## Summary

Phase 18 fixes a CMake configuration bug where the WASM build target unconditionally depends on `enjin2_lua`, regardless of the `ENJIN2_BUILD_LUA` option. When configured with `-DENJIN2_BUILD_WASM=ON -DENJIN2_BUILD_LUA=OFF`, the `enjin2_lua` target is never created (the `if(ENJIN2_BUILD_LUA)` block is skipped), yet `enjin2_wasm` still links against it — causing a CMake configuration error because the target does not exist.

There are two related defects: (1) the `CMakeLists.txt` WASM block hardcodes `target_link_libraries(enjin2_wasm PRIVATE ... enjin2_lua)` unconditionally and also hardcodes `target_include_directories(enjin2_wasm PRIVATE luajit/src)` unconditionally; (2) `src/bindings/emscripten_bindings.cpp` `#include`s Lua scripting headers (`lua_engine.hpp`, `bindings.hpp`) unconditionally, which will produce compile errors when the Lua library is disabled.

The fix is purely a CMake + source file change. No new libraries are introduced. The scope is small and well-defined: two locations, one file each.

**Primary recommendation:** Guard WASM's Lua link and include directives with `if(ENJIN2_BUILD_LUA)` blocks in CMakeLists.txt, and guard Lua-specific includes and binding code in emscripten_bindings.cpp with `#ifdef ENJIN2_BUILD_LUA` (or a CMake-injected definition).

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BLDS-01 | WASM build succeeds with `ENJIN2_BUILD_LUA=OFF` | Both defect locations identified; fix patterns documented below |
</phase_requirements>

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| CMake | 3.16+ (project minimum) | Build configuration | Already the project build system |
| Emscripten | Project-defined | WASM compilation | Already gated by `if(EMSCRIPTEN)` elsewhere |

### Supporting
| Mechanism | Purpose | When to Use |
|-----------|---------|-------------|
| CMake generator expressions `$<BOOL:...>` | Conditional link targets without `if()` blocks | Inline in `target_link_libraries` |
| CMake `if(ENJIN2_BUILD_LUA)` blocks | Coarse-grained feature guards | Wrapping `target_include_directories` and `target_link_libraries` for Lua components |
| C preprocessor `#ifdef` | Source-level feature guard | Gating Lua includes in emscripten_bindings.cpp |
| `target_compile_definitions` | Passing CMake option values into C++ source | Injecting `ENJIN2_BUILD_LUA` macro into enjin2_wasm target |

### Alternatives Considered
| Standard | Alternative | Tradeoff |
|----------|-------------|----------|
| `if(ENJIN2_BUILD_LUA)` wrapping Lua link in WASM block | Generator expression `$<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>` | Generator expression is already used for the main `enjin2` interface target (line 169); consistent to use it in WASM block too — avoids nested `if()` blocks |
| `#ifdef ENJIN2_BUILD_LUA` in source | Separate emscripten_bindings_lua.cpp / emscripten_bindings_nolua.cpp | Separate files add complexity; a single guarded file is simpler |

## Architecture Patterns

### Defect Location 1: CMakeLists.txt WASM block (lines 179-229)

**Current broken state:**
```cmake
# WebAssembly build
if(ENJIN2_BUILD_WASM)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "WebAssembly build requires Emscripten toolchain")
    endif()

    add_executable(enjin2_wasm)
    target_sources(enjin2_wasm PRIVATE
        src/bindings/emscripten_bindings.cpp
    )
    target_include_directories(enjin2_wasm PRIVATE luajit/src)   # <-- UNCONDITIONAL Lua include
    target_link_libraries(enjin2_wasm PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_ui
        enjin2_lua                                                # <-- UNCONDITIONAL Lua link (target may not exist)
    )
    ...
endif()
```

**Fixed pattern (Option A — generator expression, consistent with line 169):**
```cmake
    target_include_directories(enjin2_wasm PRIVATE
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:luajit/src>
    )
    target_link_libraries(enjin2_wasm PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_ui
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>
    )
```

**Fixed pattern (Option B — explicit if block):**
```cmake
    target_link_libraries(enjin2_wasm PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_ui
    )
    if(ENJIN2_BUILD_LUA)
        target_include_directories(enjin2_wasm PRIVATE luajit/src)
        target_link_libraries(enjin2_wasm PRIVATE enjin2_lua)
    endif()
```

Option A (generator expression) is preferred because the project already uses that pattern for the main `enjin2` interface target (line 169: `$<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>`), keeping the style consistent.

Additionally, the compile definition must be injected so the source file can use `#ifdef`:
```cmake
    target_compile_definitions(enjin2_wasm PRIVATE
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:ENJIN2_BUILD_LUA=1>
    )
```

### Defect Location 2: src/bindings/emscripten_bindings.cpp (lines 1-6, 22-33, 34-95+)

**Current broken state:**
```cpp
#include "../../include/enjin2/scripting/lua_engine.hpp"   // <-- always included
#include "../../include/enjin2/scripting/bindings.hpp"     // <-- always included
```

Plus `LuaScriptSystem`, `LuaCanvas`, `LuaEngine`, `LuaBindings`, `LuaResult` types used throughout — all Lua-specific.

**Fixed pattern:**
```cpp
#include <emscripten/bind.h>
#include "../../include/enjin2/graphics/canvas.hpp"
#include "../../include/enjin2/core/types.hpp"

#ifdef ENJIN2_BUILD_LUA
#include "../../include/enjin2/scripting/lua_engine.hpp"
#include "../../include/enjin2/scripting/bindings.hpp"
#endif

using namespace emscripten;
using namespace enjin2;

// ... non-Lua bindings (Pixel4, Canvas4, helper functions) ...

#ifdef ENJIN2_BUILD_LUA
// Lua-specific bindings: LuaEngine, LuaCanvas, LuaScriptSystem, LuaBindings, forceSymbolLinking ...
#endif
```

The `forceSymbolLinking()` function and all Lua class bindings inside `EMSCRIPTEN_BINDINGS` must be guarded. The non-Lua bindings (Pixel4, Canvas4, canvas helper functions) should remain unconditional.

### Anti-Patterns to Avoid
- **Removing the WASM block's LuaJIT include path without the generator expression guard:** If `ENJIN2_BUILD_LUA=ON` and `ENJIN2_BUILD_WASM=ON`, LuaJIT headers must still be found; the fix must remain conditional, not a deletion.
- **Guarding the entire EMSCRIPTEN_BINDINGS macro:** Only Lua-specific bindings need guarding. The macro itself and non-Lua bindings (Pixel4, Canvas4) must remain active regardless of Lua.
- **Using `cmake_dependent_option`:** While it could enforce mutual exclusion, it is not needed here — the goal is to allow `WASM=ON LUA=OFF` to work, not to forbid it.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Conditional target linking | Custom scripting or wrapper targets | CMake generator expressions or `if()` blocks | CMake has native support; custom wrappers add complexity |
| Source-level feature detection | Runtime checks | Preprocessor guards with CMake-injected definitions | Compile-time gating is the right tool; runtime checks are wrong for missing libraries |

## Common Pitfalls

### Pitfall 1: Generator expression on a non-existent target
**What goes wrong:** `$<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>` still fails if CMake evaluates the generator expression at configure time against a target that doesn't exist.
**Why it happens:** CMake validates link targets at generate time; however, because the expression is conditional (`$<BOOL:OFF>:enjin2_lua>` evaluates to empty string when OFF), CMake never attempts to resolve `enjin2_lua` when `ENJIN2_BUILD_LUA=OFF`. This is safe.
**How to avoid:** Confirm that `ENJIN2_BUILD_LUA` is a proper CMake boolean option (it is: declared via `option()` on line 34), so `$<BOOL:${ENJIN2_BUILD_LUA}>` correctly evaluates to 0 when OFF.
**Warning signs:** CMake error "Target enjin2_lua not found" even with generator expression — this would indicate the expression is not being evaluated lazily, which would be a CMake bug in versions older than 3.16; the project requires 3.16+ so this is safe.

### Pitfall 2: Forgetting the include path guard
**What goes wrong:** Linking is fixed but `luajit/src` is still in `target_include_directories` unconditionally. If LuaJIT sources don't exist under WASM+LUA-OFF, the directory may not exist, causing a warning or error.
**Why it happens:** The include directory and the link library are in separate CMake calls; fixing one without the other leaves a dangling include path.
**How to avoid:** Guard both `target_include_directories(enjin2_wasm PRIVATE luajit/src)` and `target_link_libraries(...enjin2_lua)` together.

### Pitfall 3: emscripten_bindings.cpp compiles Lua types without the guard
**What goes wrong:** CMake config succeeds, but the build fails with "lua_engine.hpp not found" or "LuaScriptSystem undeclared" because the .cpp still includes Lua headers unconditionally.
**Why it happens:** CMake fixes are necessary but not sufficient — the source file must also conditionally exclude Lua-specific code.
**How to avoid:** Add `target_compile_definitions(enjin2_wasm PRIVATE $<$<BOOL:${ENJIN2_BUILD_LUA}>:ENJIN2_BUILD_LUA=1>)` in CMakeLists.txt and wrap Lua includes/uses in `#ifdef ENJIN2_BUILD_LUA` in the .cpp.

### Pitfall 4: Breaking the WASM+LUA-ON path
**What goes wrong:** Overly aggressive guards break the existing configuration where both WASM and LUA are ON.
**Why it happens:** Generator expressions or `if()` blocks that invert the logic or omit restoration of the include path.
**How to avoid:** After fixing, verify that `ENJIN2_BUILD_WASM=ON ENJIN2_BUILD_LUA=ON` still configures and builds correctly (same as before). The success criteria are specifically for the LUA=OFF path; the LUA=ON path must remain unbroken.

## Code Examples

### CMakeLists.txt — Fixed WASM block (minimal diff)

```cmake
# WebAssembly build
if(ENJIN2_BUILD_WASM)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "WebAssembly build requires Emscripten toolchain")
    endif()

    add_executable(enjin2_wasm)
    target_sources(enjin2_wasm PRIVATE
        src/bindings/emscripten_bindings.cpp
    )
    # Guard Lua include path — only needed when Lua is enabled
    target_include_directories(enjin2_wasm PRIVATE
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:luajit/src>
    )
    target_compile_definitions(enjin2_wasm PRIVATE
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:ENJIN2_BUILD_LUA=1>
    )
    target_link_libraries(enjin2_wasm PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_ui
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>
    )
    # ... rest of Emscripten options unchanged ...
endif()
```

### emscripten_bindings.cpp — Fixed includes and guarded Lua section

```cpp
#include <emscripten/bind.h>
#ifdef ENJIN2_BUILD_LUA
#include "../../include/enjin2/scripting/lua_engine.hpp"
#include "../../include/enjin2/scripting/bindings.hpp"
#endif
#include "../../include/enjin2/graphics/canvas.hpp"
#include "../../include/enjin2/core/types.hpp"

using namespace emscripten;
using namespace enjin2;

#ifdef ENJIN2_BUILD_LUA
static void forceSymbolLinking() {
    LuaScriptSystem dummy1;
    Canvas4<128, 128> dummy2;
    LuaCanvas dummy3(&dummy2);
    (void)dummy1; (void)dummy2; (void)dummy3;
}
#endif

EMSCRIPTEN_BINDINGS(enjin2_test) {
#ifdef ENJIN2_BUILD_LUA
    forceSymbolLinking();
#endif

    // ... non-Lua bindings remain unconditional: Pixel4, Canvas4, helper functions ...

#ifdef ENJIN2_BUILD_LUA
    // LuaResult, LuaEngine, LuaCanvas, LuaBindings, LuaScriptSystem bindings
    // debugLuaBindings, setupGlobalLuaFunctions
#endif
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Unconditional `enjin2_lua` link in WASM block | Conditional via generator expression | Phase 18 | WASM+LUA-OFF configuration works |
| Unconditional Lua headers in emscripten_bindings.cpp | `#ifdef ENJIN2_BUILD_LUA` guards | Phase 18 | Source compiles without Lua |

## Open Questions

1. **Does `ENJIN2_BUILD_LUA=OFF` need to keep any Lua-free WASM functionality?**
   - What we know: `emscripten_bindings.cpp` exports Pixel4, Canvas4 bindings and canvas helper functions that have no Lua dependency.
   - What's unclear: Whether the project intends WASM+LUA-OFF to be a useful configuration (e.g., pure graphics WASM without scripting) or just a configuration that must not error.
   - Recommendation: The success criteria only require "CMake configuration succeeds and build completes" — so minimal non-Lua WASM output (Pixel4, Canvas4 bindings) is sufficient. The planner does not need to expose new JS API surface.

2. **Is Emscripten available in the dev environment for build verification?**
   - What we know: The project has Emscripten support and uses `if(EMSCRIPTEN)` guards; the WASM build requires the Emscripten toolchain.
   - What's unclear: Whether the dev environment has `emcc` installed and can actually execute a WASM build.
   - Recommendation: The fix can be verified at the CMake configuration level even without Emscripten present — CMake will reject configuration before reaching the EMSCRIPTEN guard if the toolchain is missing. However, "build completes" in the success criteria implies actual compilation must be verified. Plan should note this dependency.

## Validation Architecture

> `workflow.nyquist_validation` is not set in `.planning/config.json` — skipping this section.

## Sources

### Primary (HIGH confidence)
- Direct code inspection of `/home/unwn/dev/enjin/CMakeLists.txt` lines 85-229 — defects confirmed by reading the file
- Direct code inspection of `/home/unwn/dev/enjin/src/bindings/emscripten_bindings.cpp` lines 1-6 — unconditional Lua includes confirmed
- CMake documentation (training knowledge, HIGH confidence for `option()`, `target_link_libraries`, generator expressions — these are stable CMake 3.16+ features)

### Secondary (MEDIUM confidence)
- CMake generator expression behavior for conditional targets: well-established pattern in CMake ecosystem; consistent with how line 169 of CMakeLists.txt already uses this pattern

### Tertiary (LOW confidence)
- None — the fix is entirely determined by reading the existing source files

## Metadata

**Confidence breakdown:**
- Bug identification: HIGH — defects are visible in source; no ambiguity
- Fix pattern: HIGH — generator expressions are the established CMake pattern; already used in this file
- Source-level guard: HIGH — `#ifdef` with CMake-injected definition is standard practice
- Scope completeness: MEDIUM — open question about Emscripten availability for build verification

**Research date:** 2026-02-23
**Valid until:** Stable indefinitely (pure CMake/C++ change, no external dependencies)
