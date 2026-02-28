# Phase 27: Fix onRender Pixel4 Bug - Research

**Researched:** 2026-02-26
**Domain:** C++ template dispatch / virtual method override in Scene rendering
**Confidence:** HIGH

## Summary

The bug is a one-line omission in `Scene::render<PixelType>()` (in `/home/unwn/dev/enjin/include/enjin2/core/scene.hpp`, lines 115-126). The method uses `if constexpr` to guard `onRender()` calls — but only dispatches `onRender(canvas)` for `uint8_t`. When `PixelType == Pixel4`, the else-branch runs a comment saying "skip for now", silently doing nothing. The `virtual void onRender(ICanvas<Pixel4>&)` override in derived scenes is therefore never called during `Scene::render<Pixel4>()`.

The fix is minimal and surgical: add a matching `else if constexpr (std::is_same_v<PixelType, Pixel4>)` branch that calls `onRender(canvas)` with the `ICanvas<Pixel4>&` argument. No other code paths need changing. Both virtual `onRender` overloads already exist in `Scene`'s `protected` section (lines 293 and 301), so the method signature infrastructure is already correct.

The rendering pipeline (drawable sorting via `renderObjects`) already works for Pixel4 — only the scene-level `onRender` hook is skipped. This means the fix enables the scene background/overlay drawing hook without touching any object or component rendering logic.

**Primary recommendation:** Add the `Pixel4` branch to `Scene::render<PixelType>()` in `scene.hpp`, add a regression test `scene_render_test.cpp` to `tests/`, and wire it into `tests/CMakeLists.txt`.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| RENDER-01 | Scene-derived `onRender(ICanvas<Pixel4>&)` override is called during `Scene::render()` when using Pixel4 canvas | Root cause identified: missing `if constexpr` branch for Pixel4 in `Scene::render<PixelType>()`. Fix is one-line addition to `scene.hpp`. Test infrastructure uses custom `main()`-based asserts — consistent with existing test files. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++17 `if constexpr` | Language feature | Compile-time branch on PixelType | Already used in the same function for uint8_t dispatch |
| `std::is_same_v<T, U>` | `<type_traits>` | Type identity check | Already `#include`-d in `scene.hpp` |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Project test harness | Internal | `ASSERT(cond, msg)` + `main()` returning exit code | Used by all tests in `tests/` directory |
| CTest | CMake built-in | Run tests via `ctest` | Already enabled via `enable_testing()` in root CMakeLists |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `if constexpr` branch | Virtual dispatch with templated method | Virtual + template requires CRTP or type erasure — far more invasive; `if constexpr` is the established pattern in this codebase |
| Adding a `Pixel4` explicit instantiation to `scene.cpp` | Leave template fully in-header | Scene is already header-only (template methods); explicit instantiation in scene.cpp only covers `uint8_t`, and `Pixel4` is not listed — adding it would be wrong approach; fix is in the dispatch logic |

**Installation:** No new dependencies.

## Architecture Patterns

### Recommended Project Structure
```
include/enjin2/core/
└── scene.hpp         # Bug is here — add Pixel4 branch in render<PixelType>()

tests/
├── CMakeLists.txt    # Add scene_render_test target + ctest registration
└── scene_render_test.cpp   # New test: derives a scene, calls render<Pixel4>, checks pixels
```

### Pattern 1: `if constexpr` Pixel-Type Dispatch in `Scene::render<PixelType>()`

**What:** The existing dispatch pattern already works for `uint8_t`. The `Pixel4` path is simply absent.

**Current (broken) code** (`include/enjin2/core/scene.hpp`, lines 115-126):
```cpp
template<typename PixelType>
void render(ICanvas<PixelType>& canvas) {
    if (!active) return;

    if constexpr (std::is_same_v<PixelType, uint8_t>) {
        onRender(canvas);
    } else {
        // For non-uint8_t canvases, skip scene-specific rendering for now
        // In a full implementation, you'd convert or provide templated onRender
    }
    renderObjects(canvas);
}
```

**Fixed code:**
```cpp
template<typename PixelType>
void render(ICanvas<PixelType>& canvas) {
    if (!active) return;

    if constexpr (std::is_same_v<PixelType, Pixel4>) {
        onRender(canvas);
    } else if constexpr (std::is_same_v<PixelType, uint8_t>) {
        onRender(canvas);
    }
    renderObjects(canvas);
}
```

Note: the two branches can be simplified to a single type-list check, but the two-branch form mirrors the existing project style and is maximally readable.

**When to use:** This exact pattern applies whenever a new PixelType needs scene-level `onRender` dispatch wired in.

### Pattern 2: Project Test Convention

**What:** All tests in `tests/` are standalone executables with a `main()` that returns `failures == 0 ? 0 : 1`. They use a local `ASSERT(cond, msg)` macro. CTest registers each via `add_test(NAME <name> COMMAND <name>)`.

**Example (from `sprite_test.cpp`):**
```cpp
static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

int main() {
    test_foo();
    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
```

**When to use:** All new tests added to `tests/` must follow this pattern.

### Pattern 3: Minimal Derived Scene for Testing

**What:** To test `onRender` dispatch, create a minimal derived scene that sets a flag or writes a pixel in its override, then verify the side effect after calling `render<Pixel4>`.

```cpp
// In test file
struct TestScene : public enjin2::Scene {
    bool onRenderCalled = false;
    enjin2::Pixel4 drawnColor{0};

    TestScene() : Scene(1) {}

protected:
    void onRender(enjin2::ICanvas<enjin2::Pixel4>& canvas) override {
        onRenderCalled = true;
        canvas.setPixel(0, 0, enjin2::Pixel4(7));
        drawnColor = canvas.getPixel(0, 0);
    }
};
```

Then:
```cpp
TestScene scene;
scene.initialize();
scene.activate();
enjin2::Canvas4<16, 16> canvas;
canvas.clear(enjin2::Pixel4(0));
scene.render(canvas);

ASSERT(scene.onRenderCalled, "onRender(ICanvas<Pixel4>&) must be called during render()");
ASSERT(canvas.getPixel(0, 0).value == 7,
       "pixel written in onRender must appear in output canvas");
```

### Anti-Patterns to Avoid

- **Don't touch `scene.cpp`:** The existing explicit instantiations (`render<uint8_t>`, `renderObjects<uint8_t>`) are unrelated to the fix. Do not add a `Pixel4` explicit instantiation — the template is defined in the header and resolved at compile time.
- **Don't make `onRender` a template:** The virtual dispatch mechanism (`virtual void onRender(ICanvas<Pixel4>&)`) is correct. The bug is the dispatch site, not the override signature.
- **Don't collapse the `if constexpr` chain prematurely:** A simple `onRender(canvas)` without type guard will not compile because `onRender` is overloaded — the compiler needs a concrete argument type to resolve the overload. The `if constexpr` branches provide that.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Type dispatch to overloaded virtual methods | Custom CRTP wrapper or type-erased functor | C++17 `if constexpr` + `std::is_same_v` | Already used in the same function; zero overhead, no allocations |
| Test runner | Custom test framework | Project's `ASSERT` macro pattern | Consistent with all 5 existing test files; CTest integration already wired |

**Key insight:** The entire fix is adding ~3 lines inside an already-existing template function. No new abstractions needed.

## Common Pitfalls

### Pitfall 1: Collapsing to a single onRender call without the if constexpr guard

**What goes wrong:** Writing `onRender(canvas)` directly inside `render<PixelType>()` without an `if constexpr` causes a compile error — `onRender` has two overloads (`ICanvas<Pixel4>&` and `ICanvas<uint8_t>&`), and the compiler cannot resolve which one to call when `PixelType` is generic.

**Why it happens:** C++ overload resolution happens at instantiation time for templates. Without `if constexpr`, both overload candidates are considered and neither matches a generic `ICanvas<PixelType>&` parameter when the virtual functions are declared for specific types only.

**How to avoid:** Keep the `if constexpr (std::is_same_v<PixelType, Pixel4>)` guard. Each branch must match a concrete overload signature.

**Warning signs:** Compiler error "no matching function for call to `onRender`" or "ambiguous overload".

### Pitfall 2: Not activating the scene before calling render() in the test

**What goes wrong:** `Scene::render()` returns early if `!active`. A test that creates and initializes a scene but forgets to call `activate()` will see `onRenderCalled == false` even after the fix.

**Why it happens:** The guard `if (!active) return;` is the first line of `render()` (line 117).

**How to avoid:** Test setup must call `scene.initialize()` then `scene.activate()`.

**Warning signs:** `onRenderCalled` is false even after applying the fix — check that `activate()` was called.

### Pitfall 3: Forgetting to register the new test in CMakeLists.txt

**What goes wrong:** `scene_render_test.cpp` compiles (if built manually) but `ctest` does not know about it.

**Why it happens:** `tests/CMakeLists.txt` requires an explicit `add_executable` + `add_test` pair for each test.

**How to avoid:** Add both stanzas to `tests/CMakeLists.txt` using the exact pattern already used for `sprite_test` and `compositor_test`.

### Pitfall 4: Testing only that the flag is set, not that the pixel appears in the output

**What goes wrong:** The success criterion #2 says "pixels written in onRender appear in the rendered output". A test that only checks the flag would satisfy criterion #1 but miss criterion #2. If `onRender` writes to the canvas but `renderObjects` corrupts it later, we'd miss that regression.

**How to avoid:** After `scene.render(canvas)`, read back the pixel from the canvas and assert its value equals what `onRender` wrote.

## Code Examples

Verified patterns from official sources (all from this codebase):

### Existing `if constexpr` dispatch (scene.hpp lines 119-124)
```cpp
// Source: /home/unwn/dev/enjin/include/enjin2/core/scene.hpp
if constexpr (std::is_same_v<PixelType, uint8_t>) {
    onRender(canvas);
} else {
    // For non-uint8_t canvases, skip scene-specific rendering for now
}
```

### Fixed dispatch (target state)
```cpp
// Source: /home/unwn/dev/enjin/include/enjin2/core/scene.hpp (after fix)
if constexpr (std::is_same_v<PixelType, Pixel4>) {
    onRender(canvas);
} else if constexpr (std::is_same_v<PixelType, uint8_t>) {
    onRender(canvas);
}
```

### Both virtual onRender overloads already declared correctly
```cpp
// Source: /home/unwn/dev/enjin/include/enjin2/core/scene.hpp lines 293-301
virtual void onRender(ICanvas<Pixel4>& canvas) {}
virtual void onRender(ICanvas<uint8_t>& canvas) {}
```

### Canvas4 available for Pixel4 testing
```cpp
// Source: /home/unwn/dev/enjin/include/enjin2/graphics/canvas.hpp
template <uint16_t WIDTH, uint16_t HEIGHT>
class Canvas4 : public ICanvas<Pixel4> { ... };

// Usage in test:
Canvas4<16, 16> canvas;
scene.render(canvas);  // ICanvas<Pixel4>& overload
```

### Test registration pattern (tests/CMakeLists.txt)
```cmake
# Source: /home/unwn/dev/enjin/tests/CMakeLists.txt (sprite_test pattern)
add_executable(scene_render_test
    scene_render_test.cpp
)
target_include_directories(scene_render_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(scene_render_test PRIVATE
    enjin2
)
add_test(NAME scene_render_test COMMAND scene_render_test)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Silent no-op for non-uint8_t pixel types | Full dispatch for all registered pixel types | Phase 27 (this fix) | Derived scenes can now use Pixel4 canvas in onRender, unblocking all rendering test infrastructure for v1.5 |

**Deprecated/outdated:**
- The comment "In a full implementation, you'd convert or provide templated onRender" in scene.hpp: replaced by the explicit Pixel4 dispatch branch.

## Open Questions

1. **Should `Scene::render` dispatch to `onRender` for unknown PixelTypes at compile time?**
   - What we know: Currently only `uint8_t` and `Pixel4` are registered pixel types in the project. Both have virtual `onRender` overloads.
   - What's unclear: Whether a future PixelType (e.g., RGB565) would need its own `onRender` overload and matching dispatch branch.
   - Recommendation: No action for Phase 27. Fix only the Pixel4 path. Future pixel types will require their own branches when introduced. A static_assert or comment can document this pattern.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Custom `ASSERT` macro + `main()` exit code |
| Config file | None — pure CMake CTest |
| Quick run command | `cd build_25_verify && ctest --output-on-failure` |
| Full suite command | `cd build_25_verify && cmake --build . && ctest --output-on-failure` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| RENDER-01 | `onRender(ICanvas<Pixel4>&)` is called during `Scene::render<Pixel4>()` | unit | `cd build_25_verify && cmake --build . && ./tests/scene_render_test` | Wave 0 |
| RENDER-01 | Pixel written in `onRender` appears in output canvas | unit | (same test, separate ASSERT) | Wave 0 |
| RENDER-01 | Existing tests still pass (no regression) | regression | `cd build_25_verify && ctest --output-on-failure` | Exists (4 tests) |

### Sampling Rate
- **Per task commit:** `cd build_25_verify && cmake --build . && ctest --output-on-failure`
- **Per wave merge:** Same as above
- **Phase gate:** Full suite green (all 5 tests, including new `scene_render_test`)

### Wave 0 Gaps
- [ ] `tests/scene_render_test.cpp` — covers RENDER-01 (does not yet exist)
- [ ] Register in `tests/CMakeLists.txt` as `add_executable` + `add_test`

*(Existing test infrastructure — input_test, palette_test, sprite_test, compositor_test — already covers all other requirements. No new fixtures or framework install needed.)*

## Sources

### Primary (HIGH confidence)
- Codebase direct inspection: `/home/unwn/dev/enjin/include/enjin2/core/scene.hpp` — confirmed bug location at lines 119-124
- Codebase direct inspection: `/home/unwn/dev/enjin/tests/CMakeLists.txt` — confirmed test registration pattern
- Codebase direct inspection: `/home/unwn/dev/enjin/tests/sprite_test.cpp`, `compositor_test.cpp` — confirmed test harness pattern
- Codebase direct inspection: `/home/unwn/dev/enjin/src/core/scene.cpp` — confirmed only `uint8_t` explicit instantiations exist; no `Pixel4` instantiation (correct — fix belongs in header dispatch)
- Build verification: `cd build_25_verify && ctest` — all 4 existing tests pass (baseline confirmed)

### Secondary (MEDIUM confidence)
- None needed — all findings are from direct codebase inspection.

### Tertiary (LOW confidence)
- None.

## Metadata

**Confidence breakdown:**
- Bug root cause: HIGH — confirmed by reading the exact if constexpr block in scene.hpp
- Fix approach: HIGH — `if constexpr` dispatch is already proven by the uint8_t branch in the same function
- Test pattern: HIGH — confirmed by reading 3 existing test files and CMakeLists.txt
- Existing test baseline: HIGH — confirmed by running ctest (4/4 pass)

**Research date:** 2026-02-26
**Valid until:** 2026-03-28 (stable codebase, 30-day window)
