---
phase: 001-write-simple-design-document
plan: 01
type: execute
wave: 1
depends_on: []
files_modified: [DESIGN.md]
autonomous: true

must_haves:
  truths:
    - "Design document exists at project root"
    - "Document contains brief library description"
    - "Document outlines objectives"
    - "Document lists technical specifications"
    - "Document highlights unique elements"
  artifacts:
    - path: "DESIGN.md"
      provides: "Comprehensive design overview"
      min_lines: 50
  key_links: []
---

<objective>
Create a simple design document for the enjin2 library.

Purpose: Provide a clear, concise overview of the library's purpose, objectives, specifications, and unique elements for developers and users.
Output: DESIGN.md file at project root containing the design overview.
</objective>

<execution_context>
@~/.config/opencode/get-shit-done/workflows/execute-plan.md
@~/.config/opencode/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@README.md
@docs/src/architecture.md
</context>

<tasks>

<task type="auto">
  <name>Create DESIGN.md with library overview</name>
  <files>DESIGN.md</files>
  <action>Create DESIGN.md at project root with the following sections:

1. **Brief Description**
   - What enjin2 is: Lightweight C++ game engine for embedded devices
   - Target platforms: ESP32-S3 and desktop
   - Key purpose: Enable game development on resource-constrained hardware

2. **Objectives**
   - Provide static allocation throughout (no dynamic memory)
   - Support Lua and WASM integration for game logic scripting
   - Enable multi-platform development (embedded + desktop)
   - Deliver predictable performance for real-time systems
   - Maintain clean, modular architecture for maintainability

3. **Technical Specifications**
   - Language: C++17
   - Architecture: Component-based entity system
   - Memory management: Static allocation with compile-time limits
   - Rendering: Hardware-abstracted via ICanvas interface
   - Scene management: State machine with transitions
   - Modules: Core, Graphics, UI, Scripting, Utils, Animation, Effects
   - Platform support: ESP32-S3, Linux, Windows
   - Dependencies: Minimal (Lua, Adafruit-GFX, stb_image)
   - Code size: ~28,271 LOC

4. **Unique Elements**
   - Zero dynamic memory allocation (all static)
   - Lua/WASM scripting integration
   - Multi-platform canvas abstraction (DMA/OpenGL/WebGL)
   - Compile-time component limits (predictable resource usage)
   - Signal-based event system
   - Hierarchical scene system with transitions
   - Integrated UI component library

Keep the document concise, practical, and focused on the essential design aspects. Use the same tone as docs/src/architecture.md: one sentence per concept, short paragraphs, clean structure.
</action>
  <verify>cat DESIGN.md && wc -l DESIGN.md</verify>
  <done>DESIGN.md exists with all four required sections (description, objectives, specs, unique elements)</done>
</task>

</tasks>

<verification>
Review that DESIGN.md:
- [ ] Contains all four required sections
- [ ] Is at least 50 lines (sufficient detail)
- [ ] Uses clear, concise language
- [ ] Accurately reflects enjin2's capabilities based on existing documentation
</verification>

<success_criteria>
DESIGN.md created at project root providing a comprehensive design overview of the enjin2 library with description, objectives, specifications, and unique elements.
</success_criteria>

<output>
After completion, create `.planning/quick/001-write-simple-design-document-of-the-libr/001-SUMMARY.md`
</output>
