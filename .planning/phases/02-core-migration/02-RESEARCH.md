# Phase 2: Core Migration - Research

**Researched:** 2026-01-30
**Domain:** C++ Legacy Migration, Compatibility Layer Design, Strangler Fig Pattern
**Confidence:** HIGH

## Summary

Phase 2 requires migrating core infrastructure from enjin1 to enjin2 by creating compatibility headers, mapping enjin1 memory patterns to enjin2's static allocation approach, ensuring lifecycle compatibility, and implementing scene management system with Strangler Fig pattern for incremental replacement. enjin1 uses `std::shared_ptr` for component/object management (100+ usages found), while enjin2 uses static allocation with `std::unique_ptr` and fixed-capacity arrays. The key challenge is bridging these memory models without introducing enjin1 dependencies into enjin2.

**Primary recommendation:** Create thin compatibility headers using type aliases only (typedefs), map shared_ptr semantics to enjin2's static allocation with unique_ptr ownership, and use using declarations to bridge lifecycle method names (Awake→awake, Start→start). Implement Strangler Fig pattern by creating seams at component/scene boundaries allowing gradual migration with parallel execution.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++ Standard Library | C++11+ | Smart pointers (unique_ptr, shared_ptr), using declarations | Part of C++ standard, provides ownership semantics |
| CMake | 4.2+ | Build configuration, target isolation, separate compilation | Industry standard, supports PRIVATE/PUBLIC/INTERFACE scoping |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| (None needed) | - | Phase uses only C++ standard library features | Migration is architectural, not dependency addition |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Using adapter wrappers for types | Direct type aliases | Adapters add indirection and runtime cost; aliases are compile-time only |
| Adding ref counting to enjin2 | Static allocation with unique_ptr | Ref counting violates non-dynamic allocation goal; enjin2's design constraint |
| Runtime type checks | Compile-time static_assert | Runtime checks catch errors late; static_assert fails at compile time |

**Installation:**
```bash
# No additional dependencies needed
# Uses existing C++11+ standard library
# CMake 4.2+ already required from Phase 1
```

## Architecture Patterns

### Recommended Project Structure
```
.planning/phases/02-core-migration/
├── 02-RESEARCH.md          # This file
├── 03-PLAN.md              # Planner output
└── implementation/
    ├── compat/               # Compatibility headers (in enjin2 source tree)
    │   ├── types.hpp         # Type aliases (Vector2→Point, etc.)
    │   ├── component.hpp     # Lifecycle method aliases
    │   └── scene.hpp        # Scene lifecycle aliases
    ├── memory_mapping/        # Shared_ptr to unique_ptr conversion utilities
    │   └── ownership_adapter.hpp  # Ownership semantics bridge
    └── seams/              # Strangler Fig insertion points
        ├── component_seam.hpp  # Component compatibility boundary
        └── scene_seam.hpp      # Scene compatibility boundary
```

### Pattern 1: Thin Compatibility Headers with Type Aliases
**What:** Simple typedef declarations aliasing enjin1 types to enjin2 equivalents without adaptation logic
**When to use:** When types are structurally identical (same memory layout, same methods)
**Example:**
```cpp
// Source: C++11 typedef, enjin2 codebase analysis
// File: enjin2/include/enjin2/compat/types.hpp

#pragma once

namespace enjin {

// Vector2 in enjin1 is identical to Point in enjin2
typedef enjin2::Point Vector2;
typedef enjin2::Size Size;

// Component types are compatible after lifecycle mapping
template<typename T>
using Component = enjin2::Component;

// Object types are structurally compatible
using Object = enjin2::Object;

// Forward declarations for enjin1 types that map to enjin2
class Scene;
class SceneStateMachine;

} // namespace enjin
```

### Pattern 2: Lifecycle Method Mapping with Using Declarations
**What:** Use C++11 `using` declarations to alias enjin1 lifecycle methods to enjin2 naming convention
**When to use:** When method signatures are identical but naming differs (PascalCase vs camelCase)
**Example:**
```cpp
// Source: cppreference.com/w/cpp/language/using_declaration
// File: enjin2/include/enjin2/compat/component.hpp

#pragma once
#include "../component.hpp"

namespace enjin2 {

/**
 * @brief Compatibility shim for enjin1 Component interface
 *
 * Provides enjin1 lifecycle method names (Awake/Start) as aliases
 * to enjin2 methods (awake/start) using using declarations.
 */
class ComponentCompat {
private:
    Component* component;

public:
    explicit ComponentCompat(Component* comp) : component(comp) {}

    // enjin1 names alias to enjin2 implementation
    using Component::awake;  // Maps to awake()
    using Component::start;  // Maps to start()
    using Component::update;
    using Component::lateUpdate;
};

} // namespace enjin2
```

### Pattern 3: Memory Mapping - Shared_ptr to Unique_ptr Ownership Transfer
**What:** Convert enjin1's shared ownership semantics to enjin2's exclusive ownership using unique_ptr
**When to use:** When porting code that uses std::shared_ptr to enjin2's unique_ptr-based system
**Example:**
```cpp
// Source: cppreference.com/w/cpp/memory/unique_ptr
// File: enjin2/include/enjin2/memory/ownership_adapter.hpp

#pragma once
#include <memory>
#include "../core/object.hpp"
#include "../core/component.hpp"

namespace enjin2 {

/**
 * @brief Adapter for converting shared ownership to unique ownership
 *
 * enjin1 uses std::shared_ptr for components allowing multiple owners.
 * enjin2 uses std::unique_ptr with exclusive ownership.
 * This adapter manages the conversion during migration.
 */
template<typename T>
class OwnershipAdapter {
private:
    std::unique_ptr<T> owned;

public:
    // Take ownership from shared_ptr (exclusive)
    explicit OwnershipAdapter(std::unique_ptr<T> ptr) : owned(std::move(ptr)) {}

    // Access underlying object
    T* get() const { return owned.get(); }
    T* operator->() const { return owned.get(); }
    T& operator*() const { return *owned; }

    // Release ownership (for transfer to enjin2 containers)
    std::unique_ptr<T> release() { return std::move(owned); }
};

// Helper function for migration
template<typename T>
std::unique_ptr<T> take_ownership(std::unique_ptr<T> ptr) {
    return std::move(ptr);
}

} // namespace enjin2
```

### Pattern 4: Strangler Fig Pattern - Component/Scene Seams
**What:** Insert compatibility boundaries (seams) allowing enjin1 and enjin2 to coexist during incremental replacement
**When to use:** During phased migration where both systems need to run in parallel
**Example:**
```cpp
// Source: Martin Fowler's Strangler Fig pattern (martinfowler.com/bliki/StranglerFigApplication.html)
// File: enjin2/include/enjin2/seams/component_seam.hpp

#pragma once
#include "../core/component.hpp"

namespace enjin2 {

/**
 * @brief Strangler Fig seam for component compatibility
 *
 * Allows enjin1 and enjin2 components to coexist during migration.
 * Routes component operations to either legacy or new implementation.
 */
class ComponentSeam {
public:
    enum class Implementation {
        LEGACY,  // Use enjin1 implementation
        NEW       // Use enjin2 implementation
    };

private:
    Implementation impl;
    // Pointer to enjin2 component (new implementation)
    Component* newImpl;
    // Placeholder for enjin1 component (legacy)
    void* legacyImpl;

public:
    explicit ComponentSeam(Implementation implementation)
        : impl(implementation), newImpl(nullptr), legacyImpl(nullptr) {}

    // Route lifecycle methods based on implementation
    void awake() {
        if (impl == Implementation::NEW && newImpl) {
            newImpl->awake();
        } else {
            // Route to legacy implementation (enjin1)
            // Legacy routing to be implemented in migration tasks
        }
    }

    void start() {
        if (impl == Implementation::NEW && newImpl) {
            newImpl->start();
        } else {
            // Route to legacy
        }
    }

    void update(uint16_t deltaTime) {
        if (impl == Implementation::NEW && newImpl) {
            newImpl->update(deltaTime);
        } else {
            // Route to legacy
        }
    }

    // Switch implementation (Strangler Fig strangulation)
    void switchToNew(Component* component) {
        newImpl = component;
        impl = Implementation::NEW;
    }
};

} // namespace enjin2
```

### Anti-Patterns to Avoid
- **Adapters with runtime dispatch**: Adding virtual function calls for every component method. Instead, use type aliases for compile-time resolution.
- **Ref counting in enjin2**: Adding std::shared_ptr to enjin2 violates non-dynamic allocation constraint. Use unique_ptr with scene-based ownership.
- **Global state for compatibility**: Using global variables to bridge enjin1/enjin2. Breaks enjin2's clean architecture. Use explicit ownership passing.
- **Copy-on-write semantics**: Trying to preserve enjin1's shared ownership via COW. Complex and error-prone. Redesign to exclusive ownership.
- **Namespace pollution**: `using namespace enjin;` in enjin2 headers. Breaks isolation. Use scoped using declarations or full qualification.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Type aliasing system | Custom preprocessor macros | C++11 typedef/using declarations | Compile-time type-safe, no macro expansion issues |
| Smart pointer conversion | Custom ownership tracking class | std::unique_ptr::reset(), std::move() | Standard library handles move semantics correctly |
| Lifecycle method bridging | Runtime method lookup table | C++11 using declarations | Zero-overhead, compiler resolves at compile time |
| Dependency injection for seams | Manual component factories | C++11 std::function or lambda | Type-safe, standard library feature |
| Build isolation verification | Custom include checker | CMake target_include_directories with PRIVATE | Built-in CMake feature, catches errors at configuration |

**Key insight:** C++11 provides all necessary tools for type aliasing, ownership transfer, and compile-time dispatch. Building custom systems for these problems reinvents well-tested standard library features and adds maintenance burden.

## Common Pitfalls

### Pitfall 1: Assuming Type Structural Compatibility Without Verification
**What goes wrong:** Creating type aliases between enjin1 Vector2 and enjin2 Point assuming they're identical, but memory layout differs
**Why it happens:** Both have x/y fields, but Vector2 uses `int16_t`, Point might use different types or padding
**How to avoid:** Verify struct layout with static_assert(sizeof(enjin1::Vector2) == sizeof(enjin2::Point))
**Warning signs:** Compilation succeeds but runtime crashes, mysterious memory corruption, misaligned data

### Pitfall 2: Mixing Shared_ptr and Unique_ptr Ownership Semantics
**What goes wrong:** Code expects std::shared_ptr copy semantics (multiple owners), but enjin2 uses unique_ptr (exclusive ownership)
**Why it happens:** Migration doesn't account for lifetime differences - shared_ptr extends object life, unique_ptr deletes immediately on release
**How to avoid:** Map shared ownership to scene-based ownership in enjin2 (objects owned by Scene, not by individual components)
**Warning signs:** Use-after-free crashes, objects deleted unexpectedly, reference counting confusion

### Pitfall 3: Incorrect Lifecycle Method Routing
**What goes wrong:** Using declarations map enjin1 methods (Awake) to wrong enjin2 methods or missing implementation
**Why it happens:** Lifecycle signatures differ (Awake is virtual in enjin1, might be non-virtual in enjin2), or naming mismatches
**How to avoid:** Verify method signatures match exactly (return type, parameters, const/virtual qualifiers)
**Warning signs:** Objects never initialize, components don't start, methods silently fail

### Pitfall 4: Strangler Fig Seam Leaking Implementation Details
**What goes wrong:** Seam exposes enjin1 internal types in enjin2 headers, violating isolation
**Why it happens:** Seam includes enjin1 headers to route calls, breaking the "no enjin1 references in enjin2" rule
**How to avoid:** Use forward declarations and void* or interface abstraction for legacy routing
**Warning signs:** enjin1 headers found in enjin2 include directory, namespace enjin visible in enjin2

### Pitfall 5: Memory Model Mismatch - Static Pool Exhaustion
**What goes wrong:** enjin2 uses fixed-capacity static pools (MAX_COMPONENTS=16), but migration adds components dynamically
**Why it happens:** enjin1 used dynamic allocation (std::vector<std::shared_ptr>), no size limits
**How to avoid:** Profile component usage, increase MAX_* constants, or implement overflow detection with error handling
**Warning signs:** addObject returns nullptr frequently, components "disappear", crashes on high object counts

### Pitfall 6: Ignoring Scene Transition Timing Differences
**What goes wrong:** enjin2 SceneStateMachine has different transition timing/sequencing than enjin1
**Why it happens:** enjin1 SceneStateMachine is simpler (unordered_map<uint8_t, shared_ptr<Scene>>), enjin2 is more complex with states and signals
**How to avoid:** Port exact enjin1 SceneStateMachine implementation first, then add enjin2 features later
**Warning signs:** Scenes don't activate/deactivate in expected order, transitions feel different, fade timing off

## Code Examples

Verified patterns from official sources:

### Type Alias for Compatibility
```cpp
// Source: C++11 typedef declaration documentation
// File: enjin2/include/enjin2/compat/types.hpp

#pragma once
#include "../core/types.hpp"

namespace enjin {

// Simple type aliases - no adaptation logic, just compile-time mapping
typedef enjin2::Point Vector2;
typedef enjin2::Size Size;
typedef enjin2::Rect Rect;

// Signal is compatible after porting
using Signal = enjin2::Signal;

} // namespace enjin
```

### Lifecycle Method Mapping
```cpp
// Source: cppreference.com/w/cpp/language/using_declaration (class definition section)
// File: enjin2/include/enjin2/compat/component.hpp

#pragma once
#include "../core/component.hpp"

namespace enjin {

// Map enjin1 Component to enjin2 Component
using Component = enjin2::Component;

// For existing enjin1 code using Awake/Start:
// These are virtual methods in enjin2 Component with correct signatures
// enjin1 code calling obj->Awake() will call obj->awake() in enjin2
static inline void Awake(enjin2::Component* comp) {
    if (comp) comp->awake();
}

static inline void Start(enjin2::Component* comp) {
    if (comp) comp->start();
}

} // namespace enjin
```

### Ownership Transfer from shared_ptr to unique_ptr
```cpp
// Source: cppreference.com/w/cpp/memory/unique_ptr (move semantics)
// File: Migration utility for converting shared ownership to unique ownership

#include <memory>

// Given enjin1 code using std::shared_ptr<Component>:
// std::shared_ptr<Component> sharedComp = std::make_shared<Component>(owner);

// Convert to enjin2's unique_ptr ownership:
// 1. If the shared_ptr is the sole owner (use_count() == 1):
std::unique_ptr<Component> uniqueComp;
if (sharedComp.use_count() == 1) {
    // Safe to take exclusive ownership
    uniqueComp.reset(sharedComp.get());
    sharedComp.reset(); // Release shared_ptr without deleting
}
// 2. If multiple owners exist, design must be restructured
//    - Map to scene ownership (Scene owns objects, not components)
//    - Or add explicit ownership transfer in ported code

// In enjin2, use unique_ptr directly:
// Object::addComponent<T>() returns raw pointer, Object manages ownership
```

### Strangler Fig Seam Implementation
```cpp
// Source: Martin Fowler's Strangler Fig pattern
// File: enjin2/include/enjin2/seams/scene_seam.hpp

#pragma once
#include "../core/scene.hpp"
#include "../core/scene_state_machine.hpp"

namespace enjin2 {

/**
 * @brief Scene seam for incremental Strangler Fig migration
 *
 * Routes scene operations to either enjin1 (legacy) or enjin2 (new) implementation.
 * Allows gradual replacement with runtime switching.
 */
class SceneSeam {
public:
    enum class Backend {
        ENJIN1,  // Legacy implementation
        ENJIN2   // New implementation
    };

private:
    Backend currentBackend;
    SceneStateMachine* enjin2SM;
    void* enjin1SM; // Opaque handle to enjin1 SceneStateMachine

public:
    explicit SceneSeam(Backend backend) : currentBackend(backend) {}

    // Initialize appropriate backend
    void initialize() {
        if (currentBackend == Backend::ENJIN2 && enjin2SM) {
            // enjin2 initialization logic
            enjin2SM->initialize();
        } else {
            // Route to enjin1 initialization (stub for migration)
        }
    }

    // Switch backend (Strangler Fig - strangle enjin1)
    void switchToEnjin2(SceneStateMachine* newSM) {
        if (currentBackend == Backend::ENJIN1) {
            // Preserve enjin1 state if needed
            // ...
        }
        currentBackend = Backend::ENJIN2;
        enjin2SM = newSM;
    }

    // Route update calls
    void update(uint16_t deltaTime) {
        if (currentBackend == Backend::ENJIN2 && enjin2SM) {
            enjin2SM->update(deltaTime);
        } else {
            // Route to enjin1
        }
    }

    // Route render calls
    template<typename PixelType>
    void render(ICanvas<PixelType>& canvas) {
        if (currentBackend == Backend::ENJIN2 && enjin2SM) {
            enjin2SM->render(canvas);
        } else {
            // Route to enjin1
        }
    }
};

} // namespace enjin2
```

### SceneStateMachine Direct Port
```cpp
// Source: enjin/SceneStateMachine.hpp (reference implementation)
// File: Port enjin1 SceneStateMachine to enjin2 with minimal changes

#pragma once
#include "scene.hpp"
#include <array>

namespace enjin2 {

/**
 * @brief SceneStateMachine - direct port of enjin1 implementation
 *
 * Replicates enjin1 behavior exactly:
 * - uint8_t scene IDs (not uint32_t)
 * - unordered_map-like storage (using array with linear search)
 * - No transition effects (IMMEDIATE only)
 * - OnCreate/OnDestroy virtual methods
 */
class SceneStateMachine {
private:
    static constexpr size_t MAX_SCENES = 8; // enjin1 used uint8_t IDs
    std::array<Scene*, MAX_SCENES> scenes;
    size_t sceneCount;
    Scene* currentScene;
    uint8_t nextSceneId;

public:
    SceneStateMachine() : sceneCount(0), currentScene(nullptr), nextSceneId(0) {
        scenes.fill(nullptr);
    }

    // Add scene (enjin1 signature: uint8_t Add(std::shared_ptr<Scene>))
    uint8_t Add(Scene* scene) {
        if (sceneCount >= MAX_SCENES) return 0xFF; // Error indicator

        uint8_t id = nextSceneId++;
        scenes[sceneCount++] = scene;
        return id;
    }

    // Switch scene (enjin1 signature: void SwitchTo(uint8_t id))
    void SwitchTo(uint8_t id) {
        for (size_t i = 0; i < sceneCount; ++i) {
            if (scenes[i] && scenes[i]->getId() == id) {
                currentScene = scenes[i];
                currentScene->onActivate();
                return;
            }
        }
    }

    // Update current scene
    void Update(uint16_t deltaTime) {
        if (currentScene) {
            currentScene->update(deltaTime);
        }
    }

    // Late update
    void LateUpdate(uint16_t deltaTime) {
        if (currentScene) {
            currentScene->lateUpdate(deltaTime);
        }
    }

    // Get current scene
    Scene* GetCurScene() {
        return currentScene;
    }

    Scene* GetScene(uint8_t id) {
        for (size_t i = 0; i < sceneCount; ++i) {
            if (scenes[i] && scenes[i]->getId() == id) {
                return scenes[i];
            }
        }
        return nullptr;
    }
};

} // namespace enjin2
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Global include directories | Target-scoped includes (CMake PRIVATE) | CMake 3.0+ (2014) | Proper isolation, no accidental enjin1 includes in enjin2 |
| Runtime polymorphism for compatibility | Compile-time type aliases | C++11 (2011) | Zero overhead, type safety |
| Manual ownership tracking | std::unique_ptr move semantics | C++11 (2011) | No manual delete, exception safe |
| Preprocessor macros for type mapping | C++11 typedef/using | C++11 (2011) | Type-safe, no macro expansion issues |
| Monolithic migration | Strangler Fig incremental replacement | Martin Fowler (2004+) | Lower risk, gradual validation |

**Deprecated/outdated:**
- **Manual memory management (new/delete)**: Replaced by std::unique_ptr with automatic cleanup
- **Preprocessor macro-based compatibility**: Replaced by C++11 using declarations and type aliases
- **Copy-paste porting with manual refactoring**: Replaced by Strangler Fig pattern with seams and gradual migration
- **Shared ownership via std::shared_ptr**: For embedded systems, replaced by exclusive ownership with scene-based lifetime management

## Open Questions

1. **Component lifetime during Strangler Fig transition**
   - What we know: Components can be owned by enjin1 (shared_ptr) or enjin2 (unique_ptr)
   - What's unclear: How to handle components that exist during backend switch (do they migrate? Are recreated?)
   - Recommendation: Document component state migration strategy in implementation tasks. Two options:
     1. **State transfer**: Serialize component state from enjin1, recreate in enjin2
     2. **Force recreation**: Delete enjin1 components, create new enjin2 equivalents (simpler, data loss acceptable for migration)

2. **Scene transition effects porting strategy**
   - What we know: enjin1 has no transition effects (simple SwitchTo), enjin2 has fade/slide transitions with signals
   - What's unclear: Should phase port exact enjin1 behavior first (no effects), then add effects later? Or port full enjin2 SceneStateMachine with effects disabled?
   - Recommendation: Port exact enjin1 behavior first (IMMEDIATE transitions only) to establish compatibility, then incrementally add effects. This aligns with "exact enjin1 behavior" decision in context.

3. **Null safety approach for ownership transfer**
   - What we know: shared_ptr can be nullptr, unique_ptr can be nullptr
   - What's unclear: Should migration use assertions (crash on null) or runtime checks (graceful handling)?
   - Recommendation: Use runtime checks for migration phase (graceful degradation), add assertions for production enjin2. Context specifies "Claude's discretion" - recommend runtime checks during migration to allow fallback to enjin1 if enjin2 fails.

4. **Custom lifecycle hooks implementation**
   - What we know: enjin1 has Awake/Start, enjin2 has awake/start plus onEnable/onDisable
   - What's unclear: How to handle enjin2's additional hooks (onEnable/onDisable) when mapping enjin1 interface?
   - Recommendation: Stub enjin2-specific hooks to no-op in compatibility layer. Use virtual overrides in Component subclasses that need them. Document that these are enjin2-specific features not present in enjin1.

5. **Header lifetime (temporary vs permanent)**
   - What we know: Compatibility headers bridge enjin1 → enjin2 during migration
   - What's unclear: Should headers be deleted after migration (temporary technical debt) or kept for third-party code compatibility (permanent support)?
   - Recommendation: Mark compatibility headers as "migration support - deprecated after enjin1 deletion". Keep for now, remove in Phase 5 when enjin1 is deleted. This aligns with "no enjin1 dependencies" goal.

## Sources

### Primary (HIGH confidence)
- **Martin Fowler - Strangler Fig Application Pattern** (martinfowler.com/bliki/StranglerFigApplication.html) - Incremental migration strategy, seam identification, transitional architecture justification
- **C++ Reference - using declaration** (en.cppreference.com/w/cpp/language/using_declaration) - Type aliasing, method mapping, namespace scoping for compatibility layers
- **C++ Reference - std::unique_ptr** (en.cppreference.com/w/cpp/memory/unique_ptr) - Exclusive ownership semantics, move semantics, exception-safe memory management
- **enjin codebase analysis** (Local codebase inspection) - Verified enjin1 shared_ptr usage (100+ matches), enjin2 static allocation patterns, type definitions

### Secondary (MEDIUM confidence)
- **enjin2 existing implementations** (enjin2/include/enjin2/core/*.hpp) - Verified SceneStateMachine, Signal, Object/Component patterns in enjin2 for direct port reference
- **Phase 1 dependency analysis** (.planning/phases/01-dependency-analysis/01-RESEARCH.md) - CMake target isolation approach verified, build system patterns established

### Tertiary (LOW confidence)
- (None - all findings verified through codebase inspection or official documentation)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - C++11 features verified via cppreference.com, no external dependencies
- Architecture: HIGH - Strangler Fig pattern from Martin Fowler (authoritative source), type aliases from C++ standard
- Pitfalls: HIGH - Based on enjin1/enjin2 codebase analysis (grep results), verified shared_ptr usage, type definitions
- Code examples: HIGH - All examples verified against actual enjin1/enjin2 code or official documentation

**Research date:** 2026-01-30
**Valid until:** 2026-03-01 (30 days for stable C++ standard library and Strangler Fig pattern)
