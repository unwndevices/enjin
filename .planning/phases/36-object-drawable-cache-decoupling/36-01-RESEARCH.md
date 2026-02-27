# Phase 36: object-drawable-cache-decoupling - Research

**Researched:** 2026-02-27
**Domain:** C++ entity-component architecture / Object decoupling / Rendering pipeline
**Confidence:** HIGH

## Summary

Phase 36 removes the `C_Drawable` cache that currently lives inside `Object`. Object currently maintains `std::array<C_Drawable*, MAX_COMPONENTS> drawables` and `size_t drawableCount`, populated by `dynamic_cast<C_Drawable*>` calls in `addComponent()` and `removeComponent()`. This couples `enjin2_core` (which compiles `object.cpp`) to `C_Drawable`, a type that lives in `enjin2_ui`. The forward declaration in `object.hpp` and the `#include "../../include/enjin2/components/drawable.hpp"` in `object.cpp` create a layering violation: the core layer knows about a UI-layer type.

The consumer of the cache is `Scene::renderObjects()` (in `scene.hpp`), which iterates objects and calls `obj->getDrawable(i)` to collect all drawables for sort-and-render. One secondary consumer is `C_Animation::start()` in `animation.hpp`, which queries `owner->getDrawable(i)` to find color-animatable drawables. Both consumers can be re-pointed to `owner->getComponent<C_Drawable>()` patterns or redesigned so `Object` is not the intermediary.

The target state: `Object` is a generic component container with no knowledge of `C_Drawable`. Rendering discovery moves to `Scene::renderObjects()`, which iterates object components directly using `getComponent<C_Drawable>()` or a similar template scan — the same way `ObjectCollection::findObjectWithComponent<T>()` already works. `C_Animation` similarly uses `getComponent<C_Drawable>()` rather than the cache.

**Primary recommendation:** Remove the drawable cache fields and methods from `Object`, update `Scene::renderObjects()` to scan components via `getComponent<C_Drawable>()`, and update `C_Animation` color-track lookup similarly. Remove `#include drawable.hpp` and the `C_Drawable` forward declaration from object.hpp/object.cpp entirely.

## Standard Stack

This phase is pure C++17 refactoring within the existing enjin2 codebase. No external libraries are added.

### Core Technologies
| Component | Location | Role |
|-----------|----------|------|
| `Object` | `include/enjin2/core/object.hpp`, `src/core/object.cpp` | Generic component container — remove drawable coupling |
| `Scene::renderObjects()` | `include/enjin2/core/scene.hpp` (template, header-only) | Render discovery — move drawable query here |
| `C_Drawable` | `include/enjin2/components/drawable.hpp`, `src/components/drawable.cpp` | Drawable base — unchanged |
| `C_Animation` | `include/enjin2/components/animation.hpp` | Secondary drawable cache consumer — fix |
| `Component::getComponent<T>()` | via `owner->getComponent<T>()` | Already available O(n) scan pattern |
| Tests | `tests/` | ctest suite — must remain green |

## Architecture Patterns

### Current Architecture (to be removed)

```
Object (core layer)
  |-- components[]          generic slot storage
  |-- position*             C_Position cache (keep: same layer)
  |-- drawables[]           C_Drawable* cache (REMOVE)
  |-- drawableCount         (REMOVE)
  |-- addComponent<T>()     dynamic_cast<C_Drawable*> inside (REMOVE cast)
  |-- removeComponent<T>()  drawable cache maintenance (REMOVE)
  |-- getDrawable(i)        (REMOVE)
  |-- getDrawableCount()    (REMOVE)
  |-- getDrawables()        (REMOVE)

object.cpp
  #include drawable.hpp     (REMOVE)

object.hpp
  class C_Drawable;         forward declaration (REMOVE)
```

### Target Architecture

```
Object (core layer)
  |-- components[]          generic slot storage (unchanged)
  |-- position*             C_Position cache (keep)
  |-- addComponent<T>()     no dynamic_cast for drawables
  |-- removeComponent<T>()  no drawable cache maintenance
  (no drawable fields/methods)

Scene::renderObjects() (scene.hpp, ui layer)
  forEach object:
    scan all components for C_Drawable via getComponent<C_Drawable>()
    — or — iterate componentCount directly with dynamic_cast<C_Drawable*>

C_Animation::start() (animation.hpp, ui layer)
  owner->getComponent<C_Drawable>()   instead of owner->getDrawable(0)
```

### Pattern 1: Component Type Scan in renderObjects()

`Object` already exposes `getComponentCount()` and has `components[]` as private. The cleanest approach that avoids adding a new public API to `Object` uses `getComponent<C_Drawable>()` in a loop — but this only returns the first match. Since an object can have multiple drawables, a `getComponents<T>(T** out, size_t max)` method OR iterating via `dynamic_cast` in `renderObjects()` directly is needed.

**Option A — Add `getComponents<T>()` to Object (recommended):**
`Object` gains a generic `getComponents<T>(T** out, size_t max) -> size_t` method that does the linear scan. This is generic (not drawable-specific), adds zero coupling to `C_Drawable`, and makes the cache removal complete.

```cpp
// In object.hpp — generic, no C_Drawable coupling
template<typename T>
size_t getComponents(T** out, size_t maxOut) const {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
    size_t found = 0;
    for (size_t i = 0; i < componentCount && found < maxOut; ++i) {
        if (auto c = dynamic_cast<T*>(components[i].get())) {
            out[found++] = c;
        }
    }
    return found;
}
```

Then `Scene::renderObjects()` calls:
```cpp
C_Drawable* objDrawables[MAX_COMPONENTS];
size_t n = obj->getComponents<C_Drawable>(objDrawables, MAX_COMPONENTS);
for (size_t d = 0; d < n; ++d) { ... }
```

**Option B — Dynamic cast loop inside Scene (no new Object API):**
`renderObjects()` accesses components directly via the existing per-component `dynamic_cast`. This requires exposing `components[]` or iterating via `getComponent<T>()` in a while-loop (awkward). Option A is cleaner.

**Option C — Keep getDrawable(i) but move cache to Scene:**
A per-scene drawable cache rebuilt each frame avoids the per-Object overhead. But this adds complexity to Scene and still involves dynamic_cast at rebuild time. Option A is simpler and more idiomatic.

**Recommendation: Option A.** Add `getComponents<T>()` as a generic template on `Object`. This removes all drawable-specific knowledge from `Object` and provides a reusable API. Scene and C_Animation both call it.

### Pattern 2: C_Position Cache Consistency

The `C_Position* position` cache in Object is also a cached component pointer and is in the same file. This phase should NOT remove the position cache — it is to the same layer (`C_Position` is also in `enjin2_ui`). However, since this phase is about removing `C_Drawable` coupling specifically, the position cache is out of scope and should be left untouched to minimize risk.

### Pattern 3: Removing Forward Declaration from object.hpp

After removing the drawable cache:
- `class C_Drawable;` forward declaration in `object.hpp` → DELETE
- `#include "../../include/enjin2/components/drawable.hpp"` in `object.cpp` → DELETE
- `dynamic_cast<C_Drawable*>` in `addComponent()` body in `object.hpp` → DELETE
- Drawable cache maintenance in `removeComponent()` body in `object.hpp` → DELETE
- `initializeComponentCache()` in `object.cpp` — remove drawable portion; keep position rebuild

### Anti-Patterns to Avoid

- **Keep getDrawable() but delegate to getComponents<C_Drawable>():** This hides the problem and keeps C_Drawable in Object's API surface. Remove it entirely.
- **Move drawable cache to a wrapper/subclass of Object:** Introduces inheritance for a cross-cutting concern. Use the generic `getComponents<T>()` approach instead.
- **Dynamic rebuild every frame in Object:** The old cache existed for performance (avoid per-frame dynamic_cast per drawable). Moving the scan to `renderObjects()` is acceptable — it was already O(n_objects * n_drawables_per_object); now it becomes O(n_objects * n_components_per_object). With MAX_COMPONENTS = 16 and typical object counts, this is negligible.
- **Removing C_Drawable from scene.hpp:** `Scene::renderObjects()` must still know `C_Drawable` — that include stays. Only `object.hpp` and `object.cpp` lose the dependency.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead |
|---------|-------------|-------------|
| Multi-component lookup | Custom typed cache inside Object | `getComponents<T>()` template scan |
| Drawable sort | Custom sort logic | `std::sort` already in renderObjects() — unchanged |
| Component iteration | Raw pointer arithmetic | Range-based loop over `components[0..componentCount]` |

**Key insight:** The `dynamic_cast` in `renderObjects()` was already happening (indirectly via the cache). The cache was an optimization that also introduced coupling. At the scale of enjin2 (128 objects max, 16 components max), the optimization is not needed and the coupling cost is higher than the performance gain.

## Common Pitfalls

### Pitfall 1: Forgetting C_Animation Uses the Cache

**What goes wrong:** Remove the cache from Object, update Scene, ship — C_Animation::start() still calls `owner->getDrawable(0)` and fails to compile.

**Why it happens:** C_Animation is in `animation.hpp` — not in `scene.hpp` or `object.hpp` — so it's easy to miss in a grep for `getDrawable`.

**How to avoid:** Search for `getDrawable` across ALL headers and source files before writing any code:
```bash
grep -rn "getDrawable\|drawableCount\|getDrawables" include/ src/ tests/
```
The grep result already shows: `include/enjin2/components/animation.hpp:91-92` is a consumer. Fix it.

**Warning signs:** Compile error in `animation.hpp` after removing the API from `object.hpp`.

### Pitfall 2: Leaving the Forward Declaration

**What goes wrong:** Delete `drawables[]` and the cache logic but leave `class C_Drawable;` in `object.hpp`. This is a latent violation — `object.hpp` still knows about `C_Drawable` by name.

**Why it happens:** Forward declarations are easy to forget since they don't cause compile errors when unused.

**How to avoid:** After the refactor, verify `object.hpp` and `object.cpp` have zero mentions of `C_Drawable` or `drawable`.

### Pitfall 3: Breaking the named_objects_test Link

**What goes wrong:** `named_objects_test` links with `--start-group/--end-group` specifically because it uses `Object` directly alongside `C_Drawable` symbols. After removing drawable from Object, the link requirements may change.

**Why it happens:** Phase 29 decision: `--start-group enjin2_core enjin2_graphics enjin2_ui enjin2_input --end-group` was needed because `object.cpp` pulls in `drawable.hpp` typeinfo. After Phase 36, `enjin2_core` no longer depends on `enjin2_ui` symbols at all.

**How to avoid:** After removing drawable from object.cpp, attempt to simplify `named_objects_test` link to just `enjin2_core` (+ any actually needed libs). Verify ctest passes.

### Pitfall 4: initializeComponentCache() Partial Removal

**What goes wrong:** `initializeComponentCache()` in `object.cpp` has both position and drawable rebuild logic. Removing the drawable portion and leaving the method in a half-state that still references `C_Drawable`.

**How to avoid:** After removing the drawable block from `initializeComponentCache()`, verify `object.cpp` compiles without including `drawable.hpp`.

### Pitfall 5: getDrawables() Reinterpret Cast

**What goes wrong:** The current `getDrawables()` uses `reinterpret_cast<const C_Drawable* const*>(drawables.data())`. This cast is removed with the method, but if someone copies this pattern elsewhere it is unsafe.

**Warning signs:** Any new code using `reinterpret_cast` on `Component*` arrays.

## Code Examples

### After Refactor: Object has no drawable knowledge

```cpp
// include/enjin2/core/object.hpp — after Phase 36

#pragma once
#include "types.hpp"
#include <array>
#include <memory>
#include <functional>
#include <type_traits>

namespace enjin2 {

class Component;
class C_Position;
// C_Drawable forward declaration: REMOVED

class Object {
private:
    static constexpr size_t MAX_COMPONENTS = 16;
    std::array<std::unique_ptr<Component>, MAX_COMPONENTS> components;
    size_t componentCount;
    bool awoken;
    bool started;
    bool active;

    C_Position* position;        // kept: same-layer optimization
    // drawables[] array: REMOVED
    // drawableCount: REMOVED

public:
    // addComponent<T>: dynamic_cast<C_Drawable*> block removed
    // removeComponent<T>: drawable cache maintenance removed
    // getDrawable(i): REMOVED
    // getDrawableCount(): REMOVED
    // getDrawables(): REMOVED

    // New generic multi-component accessor (no C_Drawable coupling):
    template<typename T>
    size_t getComponents(T** out, size_t maxOut) const {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
        size_t found = 0;
        for (size_t i = 0; i < componentCount && found < maxOut; ++i) {
            if (auto c = dynamic_cast<T*>(components[i].get())) {
                out[found++] = c;
            }
        }
        return found;
    }
    // ... rest unchanged
};
```

### After Refactor: Scene::renderObjects() — no longer uses Object cache

```cpp
// inside scene.hpp renderObjects() — after Phase 36
template<typename PixelType>
void renderObjects(ICanvas<PixelType>& canvas) {
    static constexpr size_t MAX_DRAWABLES = 256;
    static constexpr size_t OBJ_MAX_DRAW = 16; // MAX_COMPONENTS
    C_Drawable* drawables[MAX_DRAWABLES];
    size_t drawableCount = 0;

    objects.forEach([&](Object* obj) {
        if (!obj || !obj->isActive()) return;

        // Collect drawables from this object's components directly
        C_Drawable* objDrawables[OBJ_MAX_DRAW];
        size_t n = obj->getComponents<C_Drawable>(objDrawables, OBJ_MAX_DRAW);
        for (size_t i = 0; i < n && drawableCount < MAX_DRAWABLES; ++i) {
            if (objDrawables[i]->isVisible()) {
                drawables[drawableCount++] = objDrawables[i];
            }
        }
    });

    std::sort(drawables, drawables + drawableCount,
              [](const C_Drawable* a, const C_Drawable* b) {
                  return a->shouldDrawBefore(*b);
              });

    for (size_t i = 0; i < drawableCount; ++i) {
        if constexpr (std::is_same_v<PixelType, Pixel4>) {
            drawables[i]->draw(canvas);
        } else {
            (void)i;
        }
    }
}
```

### After Refactor: C_Animation — use getComponent instead of cache

```cpp
// animation.hpp::start() color track connection — after Phase 36
if (hasColorTrack && updateColor) {
    colorConnection = std::make_unique<SignalConnection<Pixel4>>(
        colorTrack.connectOnUpdate([this](Pixel4 color) {
            // Use getComponent<C_Drawable>() — first drawable only (existing behavior)
            auto drawable = owner->getComponent<C_Drawable>();
            if (drawable) {
                // color update logic (break was already here — first drawable only)
            }
        })
    );
}
```

### After Refactor: object.cpp — no drawable include

```cpp
// src/core/object.cpp — after Phase 36
#include "../../include/enjin2/core/object.hpp"
#include "../../include/enjin2/core/component.hpp"
#include "../../include/enjin2/components/position.hpp"
// #include drawable.hpp: REMOVED

// Object::Object() constructor initializer list: removes drawableCount(0), drawables.fill(nullptr)
// initializeComponentCache(): removes drawable rebuild block
// addComponent<T>() body: removes dynamic_cast<C_Drawable*> block
// removeComponent<T>() body: removes drawable cache update block
```

## State of the Art

| Old Approach | Current Approach | Impact |
|--------------|------------------|--------|
| Object caches drawables via dynamic_cast | Object is a generic container; Scene scans components | Clean layer separation; enjin2_core no longer depends on enjin2_ui types |
| getDrawable(i) / getDrawableCount() public API | getComponents<T>() generic template | Works for any component type, not just drawables |
| initializeComponentCache() rebuilds drawable cache | Only position cache rebuilt (or method simplified) | Removes cross-layer rebuild logic from core |

**Note:** The `C_Position` cache in Object is similar in structure but is NOT removed in this phase. `C_Position` is also in `enjin2_ui`, so it's a similar layering issue — however the phase scope is specifically drawable-cache-decoupling. Position cache removal would be a separate phase if desired.

## Open Questions

1. **Should C_Position cache also be removed?**
   - What we know: `C_Position` is in `include/enjin2/components/position.hpp` and is referenced from `object.cpp`. This is the same layering violation pattern.
   - What's unclear: Whether the phase goal is "remove all component caches from Object" or specifically "remove drawable cache."
   - Recommendation: Phase 36 is named `object-drawable-cache-decoupling`. Keep the position cache and leave it to a future phase if needed. Doing both in one phase increases risk. Confirm the scope is drawable-only before writing plans.

2. **Does the `named_objects_test` link line need updating?**
   - What we know: `named_objects_test` links with `--start-group enjin2_core enjin2_graphics enjin2_ui enjin2_input --end-group` due to typeinfo for C_Drawable pulled through object.cpp.
   - What's unclear: After removing `drawable.hpp` from `object.cpp`, whether `named_objects_test` still needs `enjin2_ui` in its link.
   - Recommendation: After the refactor, attempt to simplify the link. The test itself includes `drawable.hpp` directly (line 1 of named_objects_test.cpp), so `enjin2_ui` is still required. The `--start-group` wrapper may still be needed or may be droppable — test after the change.

3. **Is there a test for the drawable cache behavior that needs updating?**
   - What we know: `shadow_mode_test.cpp` creates a `C_Rectangle : public C_Drawable` and uses it. The `gc_assert_test.cpp` includes `drawable.hpp`. Neither directly tests `getDrawable(i)` on Object.
   - What's unclear: Whether a dedicated test for the new `getComponents<T>()` API is warranted.
   - Recommendation: Add a small test in a new `drawable_decoupling_test.cpp` (or add cases to `named_objects_test.cpp`) that verifies: (a) Object with C_Drawable subclass components returns them via `getComponents<C_Drawable>()`, and (b) Object no longer exposes `getDrawable(i)` (compile-time — no test needed).

## Impact Assessment

### Files Changed

| File | Change Type | Description |
|------|-------------|-------------|
| `include/enjin2/core/object.hpp` | Modify | Remove `C_Drawable` forward decl, `drawables[]`, `drawableCount`, `getDrawable()`, `getDrawableCount()`, `getDrawables()`. Remove dynamic_cast blocks in `addComponent()` and `removeComponent()`. Add `getComponents<T>()`. |
| `src/core/object.cpp` | Modify | Remove `#include drawable.hpp`. Remove drawable portions of constructor, `initializeComponentCache()`. |
| `include/enjin2/core/scene.hpp` | Modify | Update `renderObjects()` to use `obj->getComponents<C_Drawable>()` instead of `obj->getDrawable(i)`. |
| `include/enjin2/components/animation.hpp` | Modify | Replace `owner->getDrawable(i)` with `owner->getComponent<C_Drawable>()`. |
| `tests/CMakeLists.txt` | Possibly modify | Update `named_objects_test` link line if `--start-group` no longer needed. |
| `tests/named_objects_test.cpp` | No change expected | Already tests Object without relying on drawable cache. |

### Files NOT Changed

| File | Reason |
|------|--------|
| `include/enjin2/components/drawable.hpp` | C_Drawable itself unchanged |
| `src/components/drawable.cpp` | C_Drawable implementation unchanged |
| `include/enjin2/core/scene.hpp` (include of drawable) | Scene still needs `#include "../components/drawable.hpp"` — it uses `C_Drawable*` in renderObjects() |
| All C_Drawable subclasses (C_Sprite, C_LuaScript, C_Canvas, etc.) | Unchanged — they don't use the Object cache |

## Sources

### Primary (HIGH confidence)
- Direct codebase read of `include/enjin2/core/object.hpp` — confirmed fields and methods
- Direct codebase read of `src/core/object.cpp` — confirmed includes and implementation
- Direct codebase read of `include/enjin2/core/scene.hpp` — confirmed renderObjects() consumer pattern
- Direct codebase read of `include/enjin2/components/animation.hpp` — confirmed secondary consumer
- `grep -rn "getDrawable|drawableCount"` across codebase — confirmed all consumers

### Secondary (MEDIUM confidence)
- Phase decisions in STATE.md: `[Phase 29-01]: Tests using Object directly without C_Drawable symbols need --start-group/--end-group` — explains the link line in named_objects_test and predicts it may change

## Metadata

**Confidence breakdown:**
- What to remove: HIGH — all drawable cache fields, methods, and includes are clearly identified
- How to replace: HIGH — `getComponents<T>()` pattern is idiomatic, zero new dependencies
- Test impact: HIGH — all affected files identified; test changes are minimal
- Scope boundary (position cache): MEDIUM — question of whether to also remove position cache is not answered by current phase name alone

**Research date:** 2026-02-27
**Valid until:** Stable (no external dependencies; pure refactoring of known code)
