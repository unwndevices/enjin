# Coding Conventions

**Analysis Date:** 2025-02-12

## Naming Patterns

**Files:**
- Headers: `.hpp` (e.g., `object.hpp`, `canvas.hpp`)
- Implementation: `.cpp` (e.g., `object.cpp`, `canvas.cpp`)
- Directory structure: `include/enjin2/` for headers, `src/` for implementations, mirroring structure.

**Functions:**
- Lifecycle methods: `camelCase` (e.g., `awake()`, `start()`, `update()`, `lateUpdate()`)
- Component API: `PascalCase` (e.g., `SetSortOrder()`, `GetSortOrder()`, `AddComponent()`)
- Internal helpers: `camelCase` (e.g., `initializeComponentCache()`)
- Booleans: `camelCase` with `is` or `has` prefix (e.g., `isActive()`, `hasComponent()`, `isQueuedForRemoval()`)

**Variables:**
- Member variables: `camelCase` (e.g., `componentCount`, `drawableCount`) or `snake_case` (e.g., `queued_for_removal`)
- Parameters: `camelCase` (e.g., `deltaTime`)
- Constants: `SCREAMING_SNAKE_CASE` (e.g., `MAX_COMPONENTS`)

**Types:**
- Classes: `PascalCase` (e.g., `Object`, `Component`, `Scene`)
- Components: `C_` prefix followed by `PascalCase` (e.g., `C_Position`, `C_Drawable`)
- Enums: `PascalCase` for both type and values (e.g., `DrawLayer::Background`, `Anchor::TOP_LEFT`)

## Code Style

**Formatting:**
- Indentation: 4 spaces
- Braces: K&R style (opening brace on the same line)
- Namespaces: `namespace enjin2 { ... }` with closing comment `// namespace enjin2`

**Modern C++:**
- Standard: C++17 (as per `CMakeLists.txt`)
- Use of `std::unique_ptr` for component management
- Use of `std::array` for fixed-size collections
- Use of `override` for virtual function overrides

## Import Organization

**Order:**
1. Standard library headers (`<memory>`, `<vector>`)
2. Library headers with relative paths (e.g., `#include "../../include/enjin2/core/object.hpp"`)
3. Component-specific headers

## Error Handling

**Patterns:**
- Use of `nullptr` for failed allocations or lookups
- `static_assert` for template type constraints
- Avoidance of exceptions in core logic for embedded compatibility

## Logging

**Framework:**
- `std::cout` and `printf` for examples and tests

---

*Convention analysis: 2025-02-12*
