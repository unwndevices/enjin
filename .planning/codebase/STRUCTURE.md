# Codebase Structure

**Analysis Date:** 2025-02-14

## Directory Layout

```
enjin2/
├── include/enjin2/     # Public headers (mirrors src/)
│   ├── abstract/       # Base interfaces and traits
│   ├── animation/      # Animation system headers
│   ├── components/     # ECS component definitions
│   ├── core/           # Fundamental engine headers
│   ├── effects/        # Post-processing effect headers
│   ├── graphics/       # Rendering and canvas headers
│   ├── scripting/      # Lua integration headers
│   ├── ui/             # UI framework headers
│   └── utils/          # Helper utilities
├── src/                # Implementation files
│   ├── animation/      # Keyframe and track implementations
│   ├── bindings/       # JS/TypeScript and Emscripten bindings
│   ├── components/     # Reusable logic components
│   ├── core/           # Scene, Object, and Memory logic
│   ├── effects/        # Visual effects implementation
│   ├── graphics/       # Primitive drawing and canvas logic
│   ├── scripting/      # Lua engine and bindings implementation
│   ├── ui/             # UI system and widget implementation
│   └── utils/          # Math, noise, and polar helpers
├── examples/           # Demo applications and usage samples
├── tests/              # Unit and integration tests
├── luajit/             # Embedded LuaJIT source (vendor)
├── vendor/             # Third-party headers (stb_image, etc.)
├── docs/               # Docusaurus and Doxygen documentation
└── build/              # Build artifacts (generated)
```

## Directory Purposes

**src/core:**
- Purpose: Core engine logic and lifecycle management.
- Contains: `scene.cpp`, `object.cpp`, `memory.cpp`.
- Key files: `include/enjin2/core/scene.hpp` (Base scene), `include/enjin2/core/object.hpp` (Entity container).

**src/graphics:**
- Purpose: Low-level rendering and hardware abstraction.
- Contains: Canvas implementations and primitive drawing.
- Key files: `include/enjin2/graphics/canvas.hpp` (Interface), `src/graphics/primitives.cpp` (Drawing logic).

**src/scripting:**
- Purpose: Lua scripting integration.
- Contains: Lua engine setup and C++ bindings.
- Key files: `src/scripting/lua_engine.cpp` (Main Lua VM wrapper).

**src/ui:**
- Purpose: User Interface framework.
- Contains: Widget hierarchy, layout systems, and UI-specific ECS.
- Key files: `src/ui/system.cpp` (UI update logic).

## Key File Locations

**Entry Points:**
- `examples/comprehensive_demo.cpp`: Main entry for desktop demos.
- `src/bindings/emscripten_bindings.cpp`: Entry for WebAssembly builds.

**Configuration:**
- `CMakeLists.txt`: Main build configuration.
- `library.json`: PlatformIO library configuration.
- `package.json`: NPM package configuration for Web/JS bindings.

**Core Logic:**
- `src/core/scene.cpp`: Scene management implementation.
- `src/core/object.cpp`: Object-Component management implementation.

**Testing:**
- `tests/image_comparison.cpp`: Visual regression tests.
- `tests/shadow_mode_test.cpp`: Functional testing for shadow rendering.

## Naming Conventions

**Files:**
- Headers: `.hpp` (e.g., `scene.hpp`)
- Implementation: `.cpp` (e.g., `scene.cpp`)
- Components: `C_` prefix often used for component classes (e.g., `C_Drawable`, `C_Position`).

**Directories:**
- Lowercase, snake_case or simple names (e.g., `core`, `graphics`).

## Where to Add New Code

**New Feature:**
- Logic: `src/` subfolder corresponding to the feature area.
- Public API: `include/enjin2/` subfolder.
- Tests: `tests/`.

**New Component:**
- Implementation: `src/components/`.
- Header: `include/enjin2/components/`.
- Convention: Class name `C_MyNewComponent` inheriting from `Component`.

**Utilities:**
- Implementation: `src/utils/`.
- Header: `include/enjin2/utils/`.

## Special Directories

**luajit:**
- Purpose: Full source of LuaJIT for embedding.
- Generated: No
- Committed: Yes

**docs:**
- Purpose: Website and API documentation.
- Contains: Docusaurus project and Doxyfile.
- Generated: Documentation site is generated from this.

---

*Structure analysis: 2025-02-14*
