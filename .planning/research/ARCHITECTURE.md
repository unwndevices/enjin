# Architecture Research

**Domain:** C++ Codebase Migration (enjin to enjin2)
**Researched:** 2026-01-30
**Confidence:** HIGH

## Standard Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    Client Code (Examples/Apps)             │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐  │
│  │           Transitional Interface Layer          │  │
│  │  (Abstraction/Facade - Temporary, removable)     │  │
│  ├─────────────────────────────────────────────────────────┤  │
│  │  ┌─────────────┐  ┌─────────────┐                │  │
│  │  │ enjin1       │  │ enjin2       │                │  │
│  │  │ (Legacy)     │  │ (Target)     │                │  │
│  │  │              │  │              │                │  │
│  │  └─────────────┘  └─────────────┘                │  │
│  │       ↑                     ↑                    │  │
│  │       └───────────┬─────────┘                    │  │
│  │                 ↓                               │  │
│  │          Abstraction Layer                        │  │
│  └─────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### Migration Phases

| Phase | Description | Key Activities | Dependencies |
|-------|-------------|----------------|---------------|
| **Phase 1: Establish Abstraction** | Create interface layer that both versions implement | Define common interfaces, create adapter wrappers | None |
| **Phase 2: Infrastructure Migration** | Move core types, memory management, base classes | Migrate Point, Rect, Object, Component, Scene | Phase 1 |
| **Phase 3: Utilities Migration** | Move helper functions and algorithms | Migrate drawing helpers, math utilities, noise functions | Phase 2 |
| **Phase 4: Feature Migration** | Move components and features (components → UI → animation) | Migrate by feature category, validate each | Phase 3 |
| **Phase 5: Decoupling** | Remove enjin1 dependencies from enjin2 | Update includes, remove using statements, fix namespace issues | Phase 4 |
| **Phase 6: Cleanup** | Delete enjin1 directory, remove transitional code | Delete legacy files, remove abstraction layer | Phase 5 |

### Component Responsibilities

| Component | Responsibility | Typical Implementation | Migration Priority |
|-----------|---------------|------------------------|-------------------|
| **Core Types** | Point, Rect, Pixel4, Color structures | Header-only templates in `core/types.hpp` | Phase 2 (HIGH) |
| **Memory System** | Static allocation, buffer management | Template-based memory pools in `core/memory.hpp` | Phase 2 (HIGH) |
| **Object System** | Entity-Component pattern, component arrays | `Object`, `Component`, `ObjectCollection` | Phase 2 (HIGH) |
| **Scene System** | State management, lifecycle hooks | `Scene`, `SceneStateMachine` | Phase 2 (HIGH) |
| **Graphics Layer** | Canvas abstraction, drawing primitives | `ICanvas<PixelType>`, canvas specializations | Phase 2 (HIGH) |
| **Component Library** | Reusable game logic pieces | C_Position, C_Drawable, C_Sprite, etc. | Phase 4 (MED) |
| **UI System** | User interface widgets | Widget classes, layout managers | Phase 4 (MED) |
| **Animation System** | Keyframe-based animation | `Animation`, `AnimationTrack`, `Keyframe` | Phase 4 (MED) |
| **Scripting Layer** | Lua integration | `LuaEngine`, `LuaScript` component | Phase 4 (MED) |
| **Utilities** | Helper functions | `drawing_helpers`, `polar`, `noise` | Phase 3 (MED) |
| **Effects** | Post-processing | CRT simulation, blur, glow | Phase 4 (LOW) |

## Recommended Migration Strategy

### Primary Pattern: Branch by Abstraction + Strangler Fig

**What:** Create an abstraction layer that allows enjin1 and enjin2 to coexist, gradually migrating functionality from enjin1 to enjin2 while both implementations can be used by client code.

**When to use:**
- Two implementations with overlapping functionality
- Need to maintain system functionality during migration
- Can define common interfaces between implementations
- Want to incrementally deliver working code

**Trade-offs:**
- *Pros:* Continuous delivery possible, reduced risk, incremental value delivery, easier rollback
- *Cons:* Additional abstraction layer (temporary), more complex builds during transition, requires careful dependency management

**Example - Abstraction Layer for Canvas:**
```cpp
// Phase 1: Define common abstraction
template<typename TPixel>
class ICanvasAbstraction {
public:
    virtual void setPixel(int x, int y, TPixel color) = 0;
    virtual TPixel getPixel(int x, int y) const = 0;
    virtual void clear(TPixel color) = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual ~ICanvasAbstraction() = default;
};

// Phase 2: Wrap enjin1 implementation
template<typename TPixel>
class Enjin1CanvasAdapter : public ICanvasAbstraction<TPixel> {
    enjin::Canvas4_128x128* canvas;
public:
    Enjin1CanvasAdapter(enjin::Canvas4_128x128* c) : canvas(c) {}
    void setPixel(int x, int y, TPixel color) override {
        canvas->setPixel(x, y, color);
    }
    // ... implement other methods delegating to enjin1
};

// Phase 3: Wrap enjin2 implementation
template<typename TPixel>
class Enjin2CanvasAdapter : public ICanvasAbstraction<TPixel> {
    enjin2::Canvas4<128, 64>* canvas;
public:
    Enjin2CanvasAdapter(enjin2::Canvas4<128, 64>* c) : canvas(c) {}
    void setPixel(int x, int y, TPixel color) override {
        canvas->setPixel(x, y, color);
    }
    // ... implement other methods delegating to enjin2
};

// Client code uses abstraction
void drawSomething(ICanvasAbstraction<Pixel4>& canvas) {
    canvas.setPixel(10, 10, Pixel4(15));
}
```

### Pattern 2: Adapter Pattern for Interface Translation

**What:** Create adapter classes that translate enjin1 interfaces to enjin2 interfaces, allowing gradual migration of client code.

**When to use:**
- Direct interface compatibility cannot be achieved
- Need to preserve existing client code during migration
- Environments require different implementations simultaneously

**Trade-offs:**
- *Pros:* Minimal changes to client code, can migrate incrementally, preserves working functionality
- *Cons:* Adapter layer adds indirection, performance overhead (temporary), more code to maintain

### Pattern 3: Facade Pattern for Simplified Migration

**What:** Create facade classes in enjin2 that expose enjin1-like interfaces, allowing client code to switch with minimal changes.

**When to use:**
- Enjin1 has complex, tightly-coupled interfaces
- Want to present cleaner, simpler interface in enjin2
- Can afford to update client code in stages

**Trade-offs:**
- *Pros:* Cleaner final interface, opportunity to improve design, can hide complexity
- *Cons:* Requires client code changes, facade may become god object, temporary maintenance burden

## Data Flow

### Migration Data Flow

```
Phase 1: Establish Abstraction
┌────────────────────────────────────────────┐
│ Client Code (Examples, Tests)         │
│     │                                   │
│     │ Uses                              │
│     ↓                                   │
│ ┌──────────────────────────────────────┐ │
│ │ Abstraction Interface (New)       │ │
│ │ (common signatures for both)        │ │
│ └──────────────────────────────────────┘ │
│     │              │                    │
│     │ implements   │ implements        │
│     ↓              ↓                    │
│ ┌──────────┐  ┌──────────┐       │
│ │ enjin1   │  │ enjin2   │       │
│ │ (Legacy) │  │ (Target) │       │
│ └──────────┘  └──────────┘       │
└────────────────────────────────────────────┘

Phase 2-4: Gradual Migration
┌────────────────────────────────────────────┐
│ Client Code                            │
│     │                                   │
│     │ Uses                              │
│     ↓                                   │
│ ┌──────────────────────────────────────┐ │
│ │ Abstraction Interface               │ │
│ └──────────────────────────────────────┘ │
│     │              │                    │
│     │ implements   │ implements        │
│     ↓              ↓                    │
│ ┌──────────┐  ┌──────────┐       │
│ │ enjin1   │  │ enjin2   │       │
│ │ (shrinking)│  │ (growing) │       │
│ └──────────┘  └──────────┘       │
│       ↓              ↑                    │
│  Moving code ────────────────         │
│  (infrastructure → components)         │
└────────────────────────────────────────────┘

Phase 5-6: Final Cleanup
┌────────────────────────────────────────────┐
│ Client Code                            │
│     │                                   │
│     │ Uses directly                      │
│     ↓                                   │
│ ┌──────────────────────────────────────┐ │
│ │         enjin2 (Final)           │ │
│ └──────────────────────────────────────┘ │
│                                       │
│                                   (enjin1 deleted)
└────────────────────────────────────────────┘
```

### Dependency Migration Order

**Critical Path:**
1. Core types (Phase 2) - All code depends on Point, Rect, Pixel
2. Memory system (Phase 2) - Object/Component require static allocation
3. Object/Component system (Phase 2) - Foundation for all features
4. Graphics/Canvas (Phase 2) - Required by all rendering code
5. Utilities (Phase 3) - Helpers for components/graphics
6. Component library (Phase 4) - Game logic depends on core
7. UI system (Phase 4) - Depends on components/graphics
8. Animation system (Phase 4) - Depends on components
9. Scripting (Phase 4) - Depends on components/objects
10. Effects (Phase 4) - Depends on graphics/utils

**Parallel Streams:**
- Infrastructure (core, memory, graphics) - MUST BE FIRST
- Features (components, UI, animation, scripting) - CAN BE PARALLEL after infrastructure
- Cleanup (decoupling, deletion) - AFTER all features migrated

### Key Data Flows

1. **Type Definitions:** Point, Rect, Pixel4, Color → All systems
2. **Memory Allocation:** Static pools → Object/Component instantiation
3. **Scene Graph:** Scene → Object → Components → Rendering
4. **Component Lifecycle:** Object::addComponent → Component::awake/start → Component::update
5. **Rendering Flow:** Scene::render → Collect drawables → Sort → Draw to Canvas
6. **Script Execution:** LuaEngine::load → LuaScript component → Component::update

## Build Order Implications

### Phase Dependencies

| Phase | Can Start After | Must Complete Before | Blocking |
|-------|----------------|---------------------|----------|
| Phase 1 | N/A (first) | Phase 2 | None |
| Phase 2 | Phase 1 | Phase 3 | Infrastructure |
| Phase 3 | Phase 2 | Phase 4 | Utilities |
| Phase 4 | Phase 2 & 3 | Phase 5 | Feature implementation |
| Phase 5 | Phase 4 | Phase 6 | Dependency resolution |
| Phase 6 | Phase 5 | N/A (final) | N/A |

### CMake Build Configuration Strategy

**Phase 1-2 (Dual Build):**
```cmake
# Build both enjin1 and enjin2, client code links to abstraction
add_subdirectory(enjin)
add_subdirectory(enjin2)
add_library(enjin_abstraction INTERFACE)
target_link_libraries(enjin_abstraction enjin enjin2)
```

**Phase 3-4 (Transition Build):**
```cmake
# enjin2 can optionally build without enjin1
option(ENJIN2_STANDALONE "Build enjin2 without enjin1" OFF)

if(NOT ENJIN2_STANDALONE)
    add_subdirectory(enjin)
endif()
add_subdirectory(enjin2)

# Examples can choose which implementation to use
target_compile_definitions(basic_drawing PRIVATE
    ENJIN2_STANDALONE=${ENJIN2_STANDALONE}
)
```

**Phase 5-6 (Standalone Build):**
```cmake
# enjin2 only, enjin1 removed
add_subdirectory(enjin2)

# Remove all enjin1 references from CMakeLists.txt
# No abstraction layer needed
```

### Incremental Validation

**After Each Phase:**
1. Build all examples - Ensure compilation succeeds
2. Run basic_drawing example - Core functionality test
3. Run lua_scripting example - Script integration test
4. Run vcvrack integration (if applicable) - Platform validation
5. Manual testing of migrated features - Functional validation

**Build Flags for Migration:**
```cmake
# Phase 1-4: Allow dual implementation
option(ENJIN_USE_ENJIN1_IMPLEMENTATION "Use enjin1 implementation for testing" ON)
option(ENJIN_USE_ENJIN2_IMPLEMENTATION "Use enjin2 implementation for testing" ON)

# Phase 5-6: enjin2 only
set(ENJIN_USE_ENJIN1_IMPLEMENTATION OFF CACHE)
set(ENJIN_USE_ENJIN2_IMPLEMENTATION ON CACHE)
```

## Anti-Patterns

### Anti-Pattern 1: Big Bang Rewrite

**What people do:** Attempt to migrate entire codebase at once, delete enjin1 before enjin2 is complete, expect system to work after large rewrite.

**Why it's wrong:** High risk of introducing bugs, no working system during migration, difficult to rollback, hard to validate correctness, extends timeline significantly.

**Instead:** Use gradual migration (Strangler Fig + Branch by Abstraction), migrate incrementally by feature category, keep enjin1 available as fallback until enjin2 is fully validated.

### Anti-Pattern 2: Copy-Paste Without Adaptation

**What people do:** Copy code from enjin1 to enjin2 without adapting to enjin2's architecture (e.g., keep std::shared_ptr where enjin2 uses static allocation).

**Why it's wrong:** Loses benefits of enjin2's design (non-dynamic memory), creates inconsistent architecture within enjin2, technical debt from day one, defeats purpose of migration.

**Instead:** Adapt code to enjin2's patterns during migration (static allocation, template-based design, namespace enjin2), refactor while moving, not just copying.

### Anti-Pattern 3: No Abstraction Layer

**What people do:** Directly include enjin1 headers in enjin2 to use shared code, create tight coupling during migration.

**Why it's wrong:** Impossible to delete enjin1 later, mixed namespaces confuse dependency graph, circular dependencies likely, unclear what's migrated vs what's legacy.

**Instead:** Create abstraction layer for shared interfaces, use adapters/facades as needed, keep clear boundary between enjin1 and enjin2 code.

### Anti-Pattern 4: Premature Deletion

**What people do:** Delete enjin1 code as soon as feature is migrated to enjin2, without validation.

**Why it's wrong:** Cannot rollback if enjin2 has bugs, lose reference implementation, may need enjin1 code for comparison, increases risk significantly.

**Instead:** Keep enjin1 until entire migration is validated and all examples pass tests, delete only in final cleanup phase.

### Anti-Pattern 5: Namespace Pollution

**What people do:** Add `using namespace enjin;` to enjin2 files to use enjin1 types during migration.

**Why it's wrong:** Unclear which namespace types belong to, makes decoupling difficult, causes linker errors later, hides dependency problems.

**Instead:** Use fully qualified names (enjin::Point, enjin2::Point), create type aliases in abstraction layer, migrate to enjin2 namespace explicitly.

## Integration Points

### Internal Boundaries During Migration

| Boundary | Communication | Notes |
|----------|---------------|-------|
| enjin1 ↔ enjin2 | Through abstraction layer only | No direct includes between implementations |
| Client Code → Abstraction | Uses abstraction interfaces | Client code unaware of which implementation |
| enjin2 → Dependencies | Only enjin2 deps during migration | Gradually remove enjin1 deps |
| Examples → Engine | Link to abstraction library | Build config chooses implementation |

### External Integrations

| Integration | Impact on Migration | Approach |
|-------------|-------------------|-----------|
| VCV Rack | Desktop builds, Adafruit GFX dependency | Migrate VCV-specific code last, maintain compatibility layer |
| ESP32-S3 | Embedded builds, FreeRTOS | Migrate early for platform validation, PSRAM code in enjin2 only |
| Emscripten/WebAssembly | WASM builds, JavaScript bindings | Migrate early for web deployment validation, bindings enjin2 only |
| LuaJIT | Scripting integration | Already in enjin2, no migration needed |
| Adafruit GFX | Font rendering | Create abstraction for font interface, migrate to enjin2's canvas system |

### Transitional Architecture Components

**Phase 1-4 Components (Temporary):**
- `IComponentAbstraction<T>` - Interface for components
- `Enjin1ComponentAdapter<T>` - Adapter for enjin1 components
- `Enjin2ComponentAdapter<T>` - Adapter for enjin2 components
- `CanvasAbstraction<TPixel>` - Interface for canvas operations
- `SceneFactory` - Factory to create scenes from either implementation

**Removed in Phase 5-6:**
- All adapter classes
- Abstraction interfaces (if not needed for plugin API)
- Factory classes
- Transitional build configuration options

## Scalability Considerations

| Scale | Architecture Adjustments |
|-------|--------------------------|
| 0-1000 lines migrated | Build both, manual testing sufficient |
| 1000-5000 lines migrated | Automate build per phase, test suite for migrated features |
| 5000-15000 lines migrated | CI/CD for all platforms, comprehensive integration tests |
| 15000+ lines migrated | Full migration coverage, performance regression testing |

### Scalability Priorities

1. **First bottleneck:** Build time increases as both enjin1 and enjin2 are compiled
   - *Fix:* Conditional compilation, parallel builds, precompiled headers

2. **Second bottleneck:** Testing complexity as both implementations must be validated
   - *Fix:* Automated test suite, shared test cases via abstraction

3. **Third bottleneck:** Dependency management during transition
   - *Fix:* Clear CMake phase structure, explicit dependency declaration

## Sources

- Martin Fowler - [Strangler Fig Pattern](https://martinfowler.com/bliki/StranglerFigApplication.html) (HIGH)
- Martin Fowler - [Branch by Abstraction](https://martinfowler.com/bliki/BranchByAbstraction.html) (HIGH)
- Refactoring.Guru - [Adapter Pattern](https://refactoring.guru/design-patterns/adapter) (HIGH)
- Refactoring.Guru - [Facade Pattern](https://refactoring.guru/design-patterns/facade) (HIGH)
- Thoughtworks - [Patterns of Legacy Displacement](https://martinfowler.com/articles/patterns-legacy-displacement/) (HIGH)
- cppreference.com - [C++ Namespaces](https://en.cppreference.com/w/cpp/language/namespace) (HIGH)

---

*Architecture research for: C++ Codebase Migration (enjin to enjin2)*
*Researched: 2026-01-30*
