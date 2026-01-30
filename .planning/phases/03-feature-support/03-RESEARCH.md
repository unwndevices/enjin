# Phase 3: Feature Support - Research

**Researched:** 2026-01-30
**Domain:** C++ abstraction layers, CMake build configuration, graphics API abstraction
**Confidence:** HIGH

## Summary

Phase 3 focuses on enabling feature migration through abstraction layers without introducing enjin1 dependencies into enjin2. The core challenge is maintaining strict compilation independence while providing interfaces that both enjin1 and enjin2 can implement. CMake's compile-time selection mechanisms (options, generator expressions, target_compile_definitions) provide the infrastructure for building with either implementation without runtime mixing. The abstraction layer design uses pure virtual interfaces (I-prefixed types) following C++ best practices, with virtual destructors to enable proper polymorphic deletion through base pointers. Canvas abstraction follows the existing ICanvas template pattern in enjin2, requiring state management decisions (color, font, transformation, clipping) and resource handling approaches (opaque handles vs wrapped types). The Strangler Fig pattern from Phase 2 provides the seam structure at component/scene boundaries.

**Primary recommendation:** Use CMake compile-time options with generator expressions to select enjin1 or enjin2 builds; extend ICanvas interface with minimal methods needed for current migration; ensure all abstraction interfaces have virtual destructors and pure virtual methods.

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| CMake | 3.16+ | Build configuration and compile-time selection | Provides generator expressions for conditional compilation, target_compile_definitions for preprocessor macros, and option() for user-selectable build variants |
| C++17 | Standard | Language features required for enjin2 | Provides virtual functions, override specifier, unique_ptr, constexpr, and template metaprogramming needed for abstraction layers |

### Supporting

| Library | Version | Purpose | When to Use |
|-----------|---------|---------|---------------|
| CMake Generator Expressions | 3.8+ | Conditional compilation based on build type, compiler ID, or options | Use in target_include_directories, target_link_libraries, target_compile_definitions for compile-time selection |
| CMake option() command | 3.0+ | Boolean user-selectable build options | Use for toggling between enjin1 and enjin2 builds via -DUSE_ENJIN1=ON/OFF |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|--------------|-----------|----------|
| Runtime implementation switching (Strategy pattern) | Compile-time only (CMake options) | Runtime switching adds vtable overhead and complex initialization; compile-time selection has zero runtime cost, simpler initialization |
| Multiple inheritance | Single inheritance interfaces | Multiple inheritance introduces diamond problem, complex object layout; single inheritance interfaces are simpler, vtable costs minimal |
| Direct concrete type exposure | Pure virtual interfaces | Direct exposure couples components to specific implementation; interfaces enable swapping implementations for migration |

**Installation:**
No additional installation required - uses existing CMake 3.16+ and C++17.

## Architecture Patterns

### Recommended Project Structure

```
enjin2/include/enjin2/
├── abstract/              # NEW: Pure abstraction interfaces
│   ├── icanvas.hpp    # Abstract canvas interface
│   ├── icomponent.hpp # Abstract component interface
│   └── iscene.hpp    # Abstract scene interface
├── graphics/
│   └── canvas.hpp     # Existing concrete implementations (Canvas4, Canvas8, ICanvas)
├── core/
│   ├── component.hpp   # Existing core Component
│   └── scene.hpp      # Existing core Scene
├── seams/                 # Existing from Phase 2
│   ├── component_seam.hpp
│   └── scene_seam.hpp
└── compat/                # Existing from Phase 2
    ├── types.hpp
    ├── component.hpp
    └── scene.hpp
```

### Pattern 1: Pure Virtual Interface with I-Prefix

**What:** Abstract base classes with only pure virtual functions, no data members, I-prefixed naming.

**When to use:** When creating abstraction layers that multiple implementations will support during migration.

**Example:**
```cpp
// Source: Existing ICanvas in enjin2/include/enjin2/graphics/canvas.hpp

template <typename TPixel>
class ICanvas {
public:
    using PixelType = TPixel;
    virtual ~ICanvas() = default;  // VIRTUAL DESTRUCTOR REQUIRED

    // Pure virtual methods
    virtual uint16_t getWidth() const = 0;
    virtual uint16_t getHeight() const = 0;
    virtual void setPixel(int16_t x, int16_t y, TPixel color) = 0;
    virtual TPixel getPixel(int16_t x, int16_t y) const = 0;
    virtual void clear(TPixel color = TPixel(0)) = 0;
    virtual void fill(const Rect &rect, TPixel color) = 0;
};
```

**Key principles:**
- I-prefix clearly distinguishes abstract types from concrete implementations
- Pure virtual functions (`= 0`) enforce implementation requirements
- Virtual destructor enables proper cleanup through base pointers
- Template parameter for pixel type maintains type safety

### Pattern 2: Compile-Time Implementation Selection

**What:** Use CMake options and generator expressions to select enjin1 or enjin2 at compile time.

**When to use:** When components need to use different implementations without runtime overhead.

**Example:**
```cmake
# Source: CMake documentation for target_compile_definitions and option()

# User-selectable option
option(USE_ENJIN1 "Use enjin1 legacy implementation" OFF)

# In enjin2 CMakeLists.txt
if(USE_ENJIN1)
    # Link against enjin1 implementation
    target_link_libraries(enjin2 PRIVATE enjin1)
    target_compile_definitions(enjin2 PRIVATE USE_ENJIN1_BACKEND)
else()
    # Use enjin2 implementation (default)
    target_link_libraries(enjin2 PRIVATE enjin2_core)
endif()

# Conditional compile flags via generator expression
target_compile_definitions(enjin2 PRIVATE
    $<${USE_ENJIN1}:ENJIN1_IMPLEMENTATION>
)
```

**C++ code uses preprocessor:**
```cpp
// Component implementation selection
#ifdef USE_ENJIN1_BACKEND
    #include "enjin/Component.hpp"  // Legacy implementation
#else
    #include "enjin2/core/component.hpp"  // New implementation
#endif
```

### Pattern 3: Seam Interface Routing

**What:** Strangler Fig pattern with compile-time-only implementation switching.

**When to use:** At component and scene boundaries to enable isolated testing during migration.

**Example:**
```cpp
// Source: Existing enjin2/include/enjin2/seams/component_seam.hpp

class ComponentSeam {
public:
    enum class Implementation { LEGACY, NEW };
private:
    Implementation impl;
    Component* newImpl;      // enjin2 concrete implementation
    void* legacyImpl;      // enjin1 opaque pointer

public:
    explicit ComponentSeam(Implementation implementation) : impl(implementation) {}

    void awake() {
        #ifdef USE_ENJIN1_BACKEND
            if (impl == Implementation::LEGACY && legacyImpl != nullptr) {
                // Route to legacy implementation
                // TODO: Implement when enjin1 headers integrated
            }
        #else
            if (impl == Implementation::NEW && newImpl != nullptr) {
                newImpl->awake();
            }
        #endif
    }

    // Additional lifecycle methods...

    void switchToNew(Component* component) {
        newImpl = component;
        impl = Implementation::NEW;
    }
};
```

### Anti-Patterns to Avoid

- **Mixed runtime/compile-time switching:** Don't switch implementations at runtime during migration - causes initialization complexity, undefined behavior during object construction. Stick to compile-time selection only.
- **Non-virtual destructors in base classes:** If you delete derived through base pointer without virtual destructor, you leak resources. Always make base destructors virtual.
- **Exposing concrete types in abstraction headers:** Including implementation headers in abstraction interfaces defeats the purpose. Use forward declarations or pure virtual interfaces.
- **Shared ownership mixing between enjin1 and enjin2:** Don't try to use shared_ptr across implementation boundaries. Phase 2 established scene-based ownership with unique_ptr - maintain that separation.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|-----------|--------------|-------------|------|
| Build flag selection | Complex preprocessor logic throughout codebase | CMake option() with generator expressions | CMake provides standardized cross-platform option handling, integrates with IDE build configuration, and generator expressions enable conditional linking without macro spaghetti |
| Interface implementation routing | Manual switch/if-else in every method | Pure virtual functions with vtable dispatch | Compiler generates optimal vtable dispatch; manual routing adds branch overhead and is error-prone |
| Resource management across implementations | Custom reference counting or manual delete | unique_ptr with scene-based ownership (Phase 2 decision) | unique_ptr provides deterministic destruction timing, no refcount overhead, and matches Phase 2's established memory mapping |
| Canvas state management | Per-state get/set methods everywhere | Stateful canvas with save/restore stacks | C++ RAII with std::optional or state stack objects provides exception safety, automatic cleanup, and clear semantics |

**Key insight:** CMake's build system abstraction and C++'s virtual function dispatch provide proven, tested mechanisms for compile-time selection and polymorphism. Building custom solutions introduces maintenance burden and potential undefined behavior.

## Common Pitfalls

### Pitfall 1: Header Dependency Bleed-Through

**What goes wrong:** enjin2 headers include enjin1 headers, causing compilation independence failure. A header in enjin2/include/enjin2/graphics/ includes "../enjin/Display.hpp" - now any code including enjin2 graphics pulls in enjin1 dependencies.

**Why it happens:** Include paths are relative, developers add "convenience" includes for quick access, or copy-paste from legacy code without checking dependencies.

**How to avoid:**
- Use forward declarations in headers (`class Enjin1Canvas;`)
- Include full headers only in .cpp implementation files
- Use CMake `target_include_directories(PRIVATE ...)` to enforce strict isolation
- Audit includes with grep for cross-boundary references

**Verification:** Build enjin2 headers alone (without enjin1 source) - any failure indicates dependency leak.

### Pitfall 2: Virtual Function Calls During Construction/Destruction

**What goes wrong:** Virtual function calls from base class constructor call the base implementation, not the derived override. Derived class hasn't been constructed yet.

**Why it happens:** Object initialization order in C++ - base constructs first, derived hasn't overridden vtable entries yet.

**How to avoid:**
- Never call virtual functions from constructors (use non-virtual helpers or init methods called post-construction)
- Never call virtual functions from destructors (use non-virtual cleanup helpers)
- Document that derived classes must call initialization methods explicitly

**Example:**
```cpp
// WRONG: Virtual call in constructor
class Base {
public:
    Base() { init(); }  // Calls Base::init(), not Derived::init()
    virtual void init() = 0;
};
class Derived : public Base {
public:
    void init() override { /* derived-specific init */ }
};

// CORRECT: Two-phase initialization
class Base {
public:
    Base() {}  // No virtual calls
    void initialize() { /* default init */ }  // Non-virtual
    virtual void initDerived() = 0;  // Called explicitly
};
class Derived : public Base {
public:
    void initDerived() override { /* derived-specific init */ }
};
// Usage: Derived d; d.initialize(); d.initDerived();
```

### Pitfall 3: Missing Virtual Destructors

**What goes wrong:** Deleting derived object through base pointer causes resource leaks or undefined behavior because only base destructor runs.

**Why it happens:** Virtual destructor not declared in base, delete through base* uses static destructor type.

**How to avoid:**
- Always declare virtual destructor in interfaces intended for polymorphic deletion
- Make destructors virtual in any class with virtual functions (C++ Core Guidelines R.12)
- Use `override` specifier in derived destructors

**Example:**
```cpp
// WRONG: Resource leak
class ICanvas {
public:
    virtual ~ICanvas() {}  // Non-virtual - delete through ICanvas* leaks
};

// CORRECT: Proper cleanup
class ICanvas {
public:
    virtual ~ICanvas() = default;  // Virtual - delete through ICanvas* calls derived
};
```

### Pitfall 4: State Management Confusion in Canvas Abstraction

**What goes wrong:** Canvas state (color, font, transform, clip) becomes ambiguous - is it part of the abstract interface or concrete implementation? Mixing state in both places causes duplication or lost state.

**Why it happens:** Abstraction layer needs state (to maintain interface) but implementations also need state (to do actual work).

**How to avoid:**
- Decide on state management approach upfront: stateless (pass all parameters), stateful (canvas holds state), or hybrid (state object passed to drawing methods)
- Document state ownership in interface comments
- Provide save/restore methods for stateful abstraction

**Example approaches:**

```cpp
// Approach 1: Stateless interface - all parameters explicit
template <typename TPixel>
class ICanvas {
public:
    virtual void drawText(const char* text, int16_t x, int16_t y, 
                    const Color& color, const Font& font) = 0;
};

// Approach 2: Stateful interface - canvas manages state
template <typename TPixel>
class ICanvas {
public:
    virtual void setColor(const Color& color) = 0;
    virtual void setFont(const Font& font) = 0;
    virtual void drawText(const char* text, int16_t x, int16_t y) = 0;
    
    // Optional: state save/restore for complex operations
    virtual void pushState() = 0;
    virtual void popState() = 0;
};
```

**Decision guidance:** Phase 3 CONTEXT.md delegates state management to Claude's discretion. Recommendation: Use stateful approach for performance (fewer parameters) but document clearly that implementations must maintain state consistently. State saving/restoring (push/pop) provides safe isolation for temporary changes.

### Pitfall 5: Mixing Compile-Time and Runtime Selection

**What goes wrong:** Components try to detect implementation at runtime via config files or global state, then switch behavior. This defeats the "compile-time only" constraint and introduces complexity.

**Why it happens:** Desire for single binary that supports both implementations, or gradual migration within same build.

**How to avoid:**
- Build separate binaries for enjin1 and enjin2 (two build directories)
- Use CMake configuration types or build flags, not runtime checks
- Document that migration requires separate builds per CONTEXT.md decision

**Consequence:** Attempting runtime mixing causes:
- Increased code complexity (every method checks flags)
- Vtable overhead for indirection layer
- Undefined behavior during construction if implementation type changes

### Pitfall 6: Opaque Pointers Without Type Safety

**What goes wrong:** Using `void*` for legacy implementation storage (as in component_seam.hpp) loses type safety, requires casts, risks undefined behavior.

**Why it happens:** Seam can't include enjin1 headers (compilation independence), so must use incomplete type.

**How to avoid:**
- Wrap opaque pointer in type-safe helper class with templated accessor
- Use std::variant or std::any if C++17 available and type can be determined
- Document clearly what type the void* actually holds

**Example:**
```cpp
// RISKY: Raw void*
class ComponentSeam {
    void* legacyImpl;
public:
    void doWork() {
        static_cast<Enjin1Component*>(legacyImpl)->doWork();  // Unsafe cast
    }
};

// SAFER: Type-safe wrapper
class ComponentSeam {
    struct OpaqueImpl {
        void* ptr;
        void (*deleter)(void*);
        ~OpaqueImpl() { if (ptr && deleter) deleter(ptr); }
    };
    OpaqueImpl legacyImpl;
public:
    template<typename T>
    void setLegacyImpl(T* impl, void (*del)(T*)) {
        legacyImpl.ptr = impl;
        legacyImpl.deleter = reinterpret_cast<void(*)(void*)>(del);
    }
    
    template<typename T, typename Func>
    void withLegacyImpl(Func&& func) {
        T* impl = static_cast<T*>(legacyImpl.ptr);
        func(impl);
    }
};
```

**Warning signs:** static_cast or reinterpret_cast scattered through seam code, void* used as universal storage, no compile-time type checking.

## Code Examples

### Minimal Abstraction Interface

```cpp
// Source: Based on ICanvas pattern in enjin2/include/enjin2/graphics/canvas.hpp
// Extend for full rendering API as needed during migration

template <typename TPixel>
class ICanvas {
public:
    using PixelType = TPixel;
    virtual ~ICanvas() = default;

    // Core methods (already defined in enjin2's ICanvas)
    virtual uint16_t getWidth() const = 0;
    virtual uint16_t getHeight() const = 0;
    virtual void setPixel(int16_t x, int16_t y, TPixel color) = 0;
    virtual TPixel getPixel(int16_t x, int16_t y) const = 0;
    virtual void clear(TPixel color = TPixel(0)) = 0;
    virtual void fill(const Rect &rect, TPixel color) = 0;
    
    // Extend as needed for migrated features:
    // virtual void drawText(...) = 0;
    // virtual void drawImage(...) = 0;
    // virtual void setTransform(...) = 0;
};
```

### CMake Build Configuration

```cmake
# In root CMakeLists.txt

cmake_minimum_required(VERSION 3.16)
project(EnjinMigration CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build variant selection
option(USE_ENJIN1 "Use enjin1 legacy backend" OFF)
option(BUILD_TESTS "Build test suite" ON)

# Configure based on selection
if(USE_ENJIN1)
    message(STATUS "Building with enjin1 backend")
    # When enjin1 has CMakeLists.txt, uncomment:
    # add_subdirectory(enjin)
    # target_link_libraries(enjin2 PUBLIC enjin1)
    target_compile_definitions(enjin2 PUBLIC USE_ENJIN1_BACKEND=1)
else()
    message(STATUS "Building with enjin2 backend")
    add_subdirectory(enjin2)
    target_compile_definitions(enjin2 PUBLIC USE_ENJIN1_BACKEND=0)
endif()

# Conditional linking via generator expressions
target_link_libraries(main_app PRIVATE
    $<$<BOOL:${USE_ENJIN1}>:enjin1:enjin2>
)
```

### Compile-Time Seam Implementation

```cpp
// Source: Extended from existing component_seam.hpp

#ifdef USE_ENJIN1_BACKEND
    #include "enjin/Component.hpp"
    using LegacyComponent = enjin::Component;
#else
    #include "enjin2/core/component.hpp"
    using NewComponent = enjin2::Component;
#endif

class ComponentSeam {
public:
    enum class Implementation { LEGACY, NEW };
    
private:
    Implementation impl_;
    union {
        NewComponent* newImpl;
        LegacyComponent* legacyImpl;
    } storage;
    
public:
    explicit ComponentSeam(Implementation impl) : impl_(impl) {
        storage.newImpl = nullptr;
        storage.legacyImpl = nullptr;
    }
    
    void awake() {
        #ifdef USE_ENJIN1_BACKEND
            if (impl_ == Implementation::LEGACY && storage.legacyImpl) {
                // Route to enjin1 Awake lifecycle
                // storage.legacyImpl->Awake(); // When enjin1 integrated
            }
        #else
            if (impl_ == Implementation::NEW && storage.newImpl) {
                storage.newImpl->awake();
            }
        #endif
    }
    
    // Similar for start(), update(), etc.
    
    void switchToNew(NewComponent* comp) {
        storage.newImpl = comp;
        impl_ = Implementation::NEW;
    }
    
    Implementation getImplementation() const { return impl_; }
};
```

### Factory Pattern for Compile-Time Selection

```cpp
// enjin2/include/enjin2/seams/component_factory.hpp

// Factory creates appropriate implementation based on compile-time flag
class ComponentFactory {
public:
    template<typename... Args>
    static ComponentSeam* createComponent(Args&&... args) {
        #ifdef USE_ENJIN1_BACKEND
            // Create enjin1 component (placeholder until enjin1 headers integrated)
            // return new ComponentSeam(ComponentSeam::Implementation::LEGACY);
            #error "enjin1 backend not yet integrated"
        #else
            // Create enjin2 component
            auto* newComp = new enjin2::Component(std::forward<Args>(args)...);
            auto* seam = new ComponentSeam(ComponentSeam::Implementation::NEW);
            seam->switchToNew(newComp);
            return seam;
        #endif
    }
};

// Usage
auto* component = ComponentFactory::createComponent();
component->awake();
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|----------------|---------------|--------|
| Manual preprocessor macros in headers | CMake compile definitions with generator expressions | CMake 3.0+ | Build configuration centralized, cross-platform, IDE integration |
| Runtime implementation switching (Strategy pattern) | Compile-time selection via CMake options | Phase 3 decision | Zero runtime overhead, simpler initialization, no vtable for switching layer |
| Mixed public/private includes for sharing | Strict PRIVATE includes with forward declarations | Phase 2 CMakeLists.txt | Guarantees compilation independence, prevents accidental dependencies |
| Shared ownership across implementations | Scene-based unique_ptr ownership (Phase 2) | Phase 2 | Deterministic destruction, no refcount complexity |

**Deprecated/outdated:**
- **Runtime build detection via global state:** Use CMake options instead - cleaner, compile-time enforced
- **Mixing enjin1 and enjin2 headers in same file:** Use forward declarations in headers, include in .cpp only
- **Manual memory management (new/delete everywhere):** Use unique_ptr as established in Phase 2

## Open Questions

1. **Canvas state management granularity**
   - What we know: CONTEXT.md delegates state approach (color, font, transform, clipping) to Claude's discretion. Canvas API has many state-dependent operations.
   - What's unclear: Should state be per-canvas object, per-thread, or passed as parameters? Does enjin1 have specific state management expectations?
   - Recommendation: Document state ownership in ICanvas interface clearly; provide save/restore methods if stateful approach chosen; start with simple per-canvas state and extend as needed.

2. **Error handling across enjin1/enjin2 boundary**
   - What we know: CONTEXT.md delegates error handling strategy to Claude's discretion. Different implementations may have different error conditions.
   - What's unclear: Should errors be standardized (enum, exception class) or propagate implementation-specific? How to handle enjin1 error codes when using enjin2 semantics?
   - Recommendation: Define error type in abstraction interface (enum class CanvasError); use std::optional<T> or Result<T> pattern for fallible operations; document enjin1-specific error codes in seam layer.

3. **Resource lifecycle in canvas abstraction**
   - What we know: enjin2's Canvas4/Canvas8 manage their own pixel buffers. enjin1 likely has different resource handling.
   - What's unclear: Should ICanvas include resource management methods (loadImage, loadFont) or are those outside abstraction scope? Who owns font/image lifetime?
   - Recommendation: Keep ICanvas minimal (drawing only) per CONTEXT.md "minimal shim" guidance; create separate IResourceManager interface if resource management needs abstraction; document that implementations own their resources.

## Sources

### Primary (HIGH confidence)

- **CMake 4.2.3 Documentation** - target_compile_definitions, generator expressions, conditional compilation, option() command
  - URL: https://cmake.org/cmake/help/latest/command/target_compile_definitions.html
  - URL: https://cmake.org/cmake/help/latest/manual/cmake-generator-expressions.7.html
  - URL: https://cmake.org/cmake/help/latest/command/option.html
  - What was checked: Compile-time build flag mechanisms, conditional linking patterns

- **cppreference.com - C++ virtual functions and classes** - Virtual destructors, pure virtual functions, override specifier, construction/destruction rules
  - URL: https://en.cppreference.com/w/cpp/language/virtual
  - URL: https://en.cppreference.com/w/cpp/language/class
  - What was checked: Virtual function dispatch, polymorphism safety, object initialization order

- **enjin2 source code** - Existing ICanvas interface, component/scene implementations, seam patterns from Phase 2
  - Files: enjin2/include/enjin2/graphics/canvas.hpp, enjin2/include/enjin2/seams/component_seam.hpp, enjin2/include/enjin2/seams/scene_seam.hpp
  - What was checked: Current abstraction patterns, existing seam infrastructure

### Secondary (MEDIUM confidence)

- **CMake compile features documentation** - Conditional compilation options, language standard selection
  - URL: https://cmake.org/cmake/help/latest/manual/cmake-compile-features.7.html
  - What was checked: C++11/17 features for abstraction (std::unique_ptr, std::optional, override specifier)

### Tertiary (LOW confidence)

None - All findings verified with official documentation or existing codebase.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Based on official CMake documentation and established C++ practices
- Architecture: HIGH - Verified against existing enjin2 code and cppreference documentation
- Pitfalls: HIGH - Based on C++ language specification and common migration anti-patterns

**Research date:** 2026-01-30
**Valid until:** 2026-02-28 (30 days - C++17 and CMake 3.16 are stable; enjin2 API structure is established)
