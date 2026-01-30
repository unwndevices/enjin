# Project Research Summary

**Project:** enjin → enjin2 Migration
**Domain:** C++ Codebase Migration and Refactoring (Embedded Game Engine)
**Researched:** 2025-01-30
**Confidence:** HIGH

## Executive Summary

This project is a C++ embedded game engine migration from enjin (dynamic allocation, std::shared_ptr, 12,488 lines) to enjin2 (static allocation, handle-based, 25,578 lines) across multiple platforms (VCV_RACK, ESP32-S3, WebAssembly). Expert practice for this domain emphasizes incremental migration via Strangler Fig pattern combined with Branch by Abstraction—creating a transitional interface layer that allows both implementations to coexist while gradually diverting flow to the new architecture. A "big bang" rewrite is explicitly rejected across all research sources due to high risk and inability to validate behavior mid-project.

The recommended approach uses Clang Tooling (LLVM 23.x) for dependency analysis and automated refactoring, IWYU for header dependency cleanup, and CppDepend for architecture visualization. Migration proceeds through six phases: establishing abstraction layers, migrating core infrastructure (types, memory, object system, graphics), migrating utilities, migrating features by category, decoupling enjin1 dependencies, and final cleanup. Key risks include incomplete dependency mapping (which causes subtle runtime bugs), memory model incompatibility (dynamic vs static allocation), and premature enjin1 deletion (eliminates rollback path). These are mitigated through dependency graph creation before any code movement, strict memory model guidelines with compile-time assertions, and a comprehensive deletion checklist validated across all platforms.

Research consensus strongly favors feature-first migration order with shadow mode execution (running both implementations in parallel for output comparison) and strict namespace separation during transition. Manual testing is the specified validation approach—automated testing is not required per project constraints. Transitional architecture should be minimal and clearly marked for removal, with API stability guarantees maintained for external contracts while internals migrate incrementally.

## Key Findings

### Recommended Stack

**Core technologies:**
- **Clang Tooling (LLVM 23.x):** Static analysis, refactoring, dependency analysis — Industry standard, comprehensive AST-based analysis with modern C++ (C++23) support and automated fixes
- **Include-What-You-Use (IWYU 0.25):** #include dependency analysis — Automatically identifies missing/unnecessary includes, reduces header coupling, integrates with CMake
- **CppDepend (2026.x):** Architecture visualization, dependency analysis — Dependency matrices, codebase-wide visualization, AI-assisted insights, enforces coding standards
- **CMake 3.30+:** Build system management — Generates compile_commands.json required by all Clang tools, supports modular builds for incremental migration

**Build integration:** compile_commands.json is critical—enables accurate analysis for all Clang tools by capturing project-specific defines, include paths, and build flags. Tools without this integration cannot accurately parse the codebase.

### Expected Features

**Must have (table stakes — Phase 1):**
- **Dependency Mapping & Analysis** — Cannot migrate what you don't understand; maps all enjin1 → enjin2 dependencies across infrastructure, utilities, and features
- **Compilation Isolation** — Prevents accidental enjin1 dependencies during migration; separate build targets, include paths, and namespaces
- **Namespace Separation** — Prevents naming collisions; both use separate namespaces (`enjin` vs `enjin2`) and must maintain strict separation
- **API Compatibility Layer** — Enables gradual migration without breaking existing code; compatibility headers alias enjin1 types to enjin2 equivalents
- **Functional Parity Testing** — Must verify enjin2 provides identical behavior before deprecation; manual testing validation per project constraints

**Should have (competitive — Phase 2):**
- **Strangler Fig Pattern Application** — Incremental replacement without "big bang" risk; gradually divert flow from enjin1 to enjin2 via compatibility seams
- **Legacy Seams Extraction** — Enables testing in isolation; introduces seams at boundaries to create migration footholds
- **Shadow Mode Execution** — Validates correctness without commitment; runs both enjin1 and enjin2 implementations in parallel
- **Component Lifecycle Mapping** — Maps enjin1 (Awake/Start) to enjin2 (awake/start); critical for object-component system
- **Scene Graph Porting** — Scene management is fundamental; SceneStateMachine, transitions must migrate cleanly

**Defer (v2+ / Phase 3):**
- **Performance Regression Guardrails** — Benchmark framework for optimization validation; enjin2 promises non-dynamic memory and 4-bit optimization
- **Incremental Dependency Inversion** — Breaks circular dependencies; essential for decoupling but can wait until feature migration progresses
- **Build System Migration** — Remove all enjin1 paths; enables enjin2-only build but can wait until features validated

### Architecture Approach

**Primary pattern:** Branch by Abstraction + Strangler Fig. Create an abstraction layer that allows enjin1 and enjin2 to coexist, gradually migrating functionality from enjin1 to enjin2 while both implementations can be used by client code. This enables continuous delivery, reduced risk, and incremental value delivery.

**Major components (migration order):**
1. **Core Types** (Phase 2) — Point, Rect, Pixel4, Color structures; header-only templates, all code depends on these
2. **Memory System** (Phase 2) — Static allocation, buffer management; template-based pools, Object/Component require this foundation
3. **Object/Component System** (Phase 2) — Entity-Component pattern with component arrays; foundation for all features
4. **Graphics/Canvas Layer** (Phase 2) — Canvas abstraction, drawing primitives; ICanvas<PixelType> specializations, required by all rendering
5. **Utilities** (Phase 3) — Drawing helpers, math utilities, noise functions; helpers for components/graphics
6. **Component Library** (Phase 4) — Reusable game logic (C_Position, C_Drawable, C_Sprite); depends on core infrastructure
7. **UI System** (Phase 4) — User interface widgets, layout managers; depends on components/graphics
8. **Animation System** (Phase 4) — Keyframe-based animation; Animation, AnimationTrack, Keyframe; depends on components
9. **Scripting Layer** (Phase 4) — Lua integration; LuaEngine, LuaScript component; depends on components/objects
10. **Effects** (Phase 4) — Post-processing (CRT, blur, glow); depends on graphics/utils

**Data flow critical path:** Type Definitions → Memory Allocation → Scene Graph → Component Lifecycle → Rendering Flow → Script Execution

### Critical Pitfalls

1. **Incomplete Dependency Mapping** — Create dependency graph before moving any code; track explicit includes, implicit dependencies (globals, singletons, init order), and verify dependents are moved or stubbed. Moving code without tracking all dependencies results in subtle bugs that surface only during manual testing.

2. **Premature enjin1 Deletion** — Establish deletion checklist before removing enjin1: all enjin2 examples compile and run on all platforms (VCV_RACK, ESP32, WebAssembly), performance benchmarks match or exceed enjin1, no enjin1 references in enjin2 build. Deleting enjin1 before verification leaves no rollback path.

3. **Memory Model Incompatibility** — Define memory model guidelines early: no new/delete after initialization phase, use fixed-size arrays, implement pool allocators for temporary buffers, audit all moved code for allocation patterns. Mixing enjin1's dynamic allocation with enjin2's static allocation causes leaks, allocation failures, or crashes.

4. **Component Lifecycle Drift** — Create lifecycle migration checklist for each component; add static assertions for lifecycle method signatures; audit component initialization in Scene code. enjin1 (Awake/Start) and enjin2 (awake/start) differences cause components to never initialize or update correctly.

5. **Platform-Specific Hardcoding** — Establish platform abstraction layer for all platform operations; build and test all targets after each migration batch; use compile-time errors over runtime crashes (static assertions). Hardcoded assumptions (128x128 canvas, PSRAM availability) cause failures on other platforms.

## Implications for Roadmap

Based on combined research, suggested phase structure aligns with architecture recommendations while addressing feature priorities and avoiding critical pitfalls:

### Phase 1: Dependency Analysis & Infrastructure Foundation
**Rationale:** Must understand what enjin2 depends on before migrating anything (PITFALLS: Incomplete Dependency Mapping). Dependency Mapping & Analysis (P1 feature) is the foundation—cannot proceed without mapping all enjin1 → enjin2 dependencies across infrastructure, utilities, and feature code. This establishes the critical path for all subsequent phases.

**Delivers:** Dependency graph, Clang tooling setup, compile_commands.json generation, CMake configuration for enjin2 standalone build, namespace separation validation

**Addresses:** Dependency Mapping & Analysis, Compilation Isolation, Namespace Separation (all P1 table stakes)

**Avoids:** Incomplete Dependency Mapping, Memory Model Incompatibility, Platform-Specific Hardcoding

**Uses:** Clang Tooling (clang-tidy, IWYU), CppDepend for visualization, CMake 3.30+

**Implements:** Core Types, Memory System, Platform Abstraction (ARCHITECTURE Phase 1-2)

### Phase 2: Core System Migration
**Rationale:** Core infrastructure (Object/Component system, Graphics/Canvas, Scene System) must be migrated before features can follow (ARCHITECTURE critical path order). Component Lifecycle Mapping and Scene Graph Porting (P1 features) are blocked by this foundation. Memory model incompatibility is prevented by establishing static allocation patterns now.

**Delivers:** Working Object/Component system, Canvas abstractions, Scene management, Component Lifecycle implementation, API Compatibility Layer

**Addresses:** API Compatibility Layer, Component Lifecycle Mapping, Scene Graph Porting, Memory Layout Equivalence (all P1 table stakes)

**Avoids:** Memory Model Incompatibility, Component Lifecycle Drift, Template Code Explosion

**Uses:** clang-rename for symbol updates, clang-tidy with -fix for automated refactoring, clang-format for consistency

**Implements:** Object System, Graphics Layer, Scene System (ARCHITECTURE Phase 2-3)

### Phase 3: Utility & Feature Migration
**Rationale:** With core infrastructure in place, migrate supporting utilities (Phase 3) then features by category (Phase 4) in parallel streams. This delivers incremental value early and validates architecture. Shadow Mode Execution (P2 differentiator) enables high-confidence validation of migrated features against enjin1 reference.

**Delivers:** Migrated utilities (drawing helpers, math, noise), Component library (C_Position, C_Drawable, C_Sprite), UI system widgets, Animation system, Lua bindings for all migrated features

**Addresses:** Legacy Seams Extraction, Shadow Mode Execution, Branch by Abstraction (P2 differentiators); Component Library, UI, Animation, Scripting (P2 features)

**Avoids:** Lua Binding Breakage, Platform-Specific Hardcoding, Template Code Explosion

**Uses:** IWYU for include cleanup during migration, LibTooling for custom analysis if needed, run-clang-tidy.py for validation

**Implements:** Utilities, Component Library, UI System, Animation System, Scripting Layer (ARCHITECTURE Phase 3-4)

### Phase 4: Decoupling & Validation
**Rationale:** After all features migrated, decouple enjin1 dependencies and validate across all platforms. This is the safety net before enjin1 deletion. Build System Migration (P3) and Performance Regression Guardrails (P3) validate that enjin2 is truly standalone and doesn't regress performance.

**Delivers:** enjin2 standalone build, removal of all enjin1 includes/references, cross-platform validation (VCV_RACK, ESP32, WebAssembly), performance benchmarks matching or exceeding enjin1, manual testing of all examples

**Addresses:** Build System Migration, Performance Regression Guardrails (P3 features); Incremental Dependency Inversion (P3 differentiator)

**Avoids:** Premature enjin1 Deletion, integration gotchas (Adafruit GFX, ESP32 PSRAM, Emscripten)

**Uses:** CppDepend to confirm enjin2 is self-contained, run-clang-tidy.py for full codebase analysis, platform-specific CI/CD

**Implements:** Decoupling, Cleanup preparation (ARCHITECTURE Phase 5)

### Phase 5: Final Cleanup
**Rationale:** Only after all validation is complete (Phase 4), delete enjin1 directory and remove transitional architecture. API Stability Guarantees and Rollback Capability (P3) maintain external contract during cleanup. This is the final milestone—enjin2 remains as the sole engine.

**Delivers:** Deleted enjin1 directory, removed abstraction layer/adapters, clean CMakeLists.txt, enjin2-only codebase, updated documentation

**Addresses:** enjin1 Directory Deletion, API Stability Guarantees, Rollback Capability (P3 features); Transitional Architecture Minimalism (P3 differentiator)

**Avoids:** Premature enjin1 Deletion, Transitional Code Permanence

**Uses:** Git tags for archival, manual verification of final state

**Implements:** Final Cleanup (ARCHITECTURE Phase 6)

### Phase Ordering Rationale

- **Why this order:** Follows critical path dependency order from ARCHITECTURE (types → memory → objects → graphics → utilities → features). Cannot skip ahead because each layer depends on previous. Phase 1 establishes understanding; Phase 2 migrates foundation; Phase 3 adds features incrementally; Phase 4 validates independence; Phase 5 removes legacy.
- **Why this grouping:** Infrastructure (Phases 1-2) MUST BE FIRST to establish foundation. Features (Phase 3) CAN BE PARALLEL after core is working—delivers value early. Decoupling (Phase 4) BLOCKS deletion—must complete before cleanup. Cleanup (Phase 5) is FINAL—only after all validation.
- **How this avoids pitfalls:** Dependency mapping before any movement (PITFALL 1). Memory model established early (PITFALL 3). Component lifecycle handled together (PITFALL 4). Deletion criteria enforced before enjin1 removal (PITFALL 2). Platform abstraction prevents hardcoding (PITFALL 6).

### Research Flags

**Phases likely needing deeper research during planning:**
- **Phase 3 (Feature Migration):** Feature-first order decisions require research into which features provide highest value for validation and user testing. Shadow mode execution implementation details (how to compare outputs, what metrics matter) need planning attention.
- **Phase 4 (Decoupling & Validation):** Performance benchmarking approach is unspecified—what metrics, what comparisons, what constitutes "match or exceed enjin1"? Cross-platform validation strategy (VCV_RACK, ESP32, WebAssembly) needs detailed planning.

**Phases with standard patterns (skip research-phase):**
- **Phase 1 (Dependency Analysis):** Clang tooling workflow is well-documented in STACK.md, migration workflow defined with commands. Pattern: use clang-tidy, IWYU, CppDepend in sequence.
- **Phase 2 (Core System Migration):** Component lifecycle mapping and scene graph porting follow established patterns from object-component systems. Memory model guidelines are clear (static allocation, no new/delete after init).
- **Phase 5 (Final Cleanup):** Standard deletion cleanup, git archiving, removing transitional code—well-understood patterns.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Verified from official LLVM/Clang documentation, IWYU official site, CppDepend features. All tools are industry standards with active development. |
| Features | HIGH | Based on Martin Fowler's Strangler Fig and Legacy Seams patterns (official), Google C++ Style Guide (official), and codebase analysis (ARCHITECTURE.md, CONCERNS.md). Feature dependencies mapped clearly. |
| Architecture | HIGH | Derived from Martin Fowler's Strangler Fig and Branch by Abstraction patterns (official), Refactoring.Guru patterns (verified), and practical codebase structure. 6-phase migration flow is well-established. |
| Pitfalls | MEDIUM | Based on codebase analysis (ARCHITECTURE.md, CONCERNS.md), embedded systems best practices (Barr Group), and Herb Sutter's modern C++ guidance. Some pitfall severity estimates require validation during implementation. |

**Overall confidence:** HIGH

### Gaps to Address

- **Performance benchmarking specifics:** What metrics constitute "match or exceed enjin1"? How to measure? What's acceptable variance? Gap to address: Define benchmark suite during Phase 4 planning.
- **Shadow mode execution implementation:** How to run both implementations in parallel? What outputs to compare? How to detect differences automatically? Gap to address: Design comparison framework during Phase 3 planning.
- **Manual testing scope:** What manual testing is required per project constraints? Which features? On which platforms? Gap to address: Define manual testing checklist during requirements phase.
- **Lua binding coverage:** What enjin2 APIs must be exposed to Lua? How to verify completeness? Gap to address: Create binding checklist during Phase 3 planning.

These gaps are not showstoppers—they require attention during detailed phase planning but do not invalidate the overall research-based approach.

## Sources

### Primary (HIGH confidence)
- Martin Fowler — [Strangler Fig Pattern](https://martinfowler.com/bliki/StranglerFigApplication.html) — Core pattern for gradual replacement without "big bang" risk
- Martin Fowler — [Branch by Abstraction](https://martinfowler.com/bliki/BranchByAbstraction.html) — Pattern for parallel development with merge window
- Thoughtworks — [Patterns of Legacy Displacement](https://martinfowler.com/articles/patterns-legacy-displacement/) — Comprehensive patterns including Legacy Seams, Event Interception, Divert the Flow
- [Clang Tools Documentation](https://clang.llvm.org/docs/ClangTools.html) — Overview of Clang tooling ecosystem (official LLVM docs)
- [Clang-tidy Documentation](https://clang.llvm.org/extra/clang-tidy/) — Clang-tidy with check categories, automated fixes (official LLVM docs)
- [IWYU Documentation](https://include-what-you-use.org/) — Include-What-You-Use for header dependency analysis (official docs)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/) — Modern C++ best practices (official ISO C++ docs)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) — Header self-containment, include ordering, namespace usage

### Secondary (MEDIUM confidence)
- [CppDepend 2026.1](https://www.cppdepend.com/) — Dependency analysis, AI assistant, codebase visualization (verified official site)
- Refactoring.Guru — [Adapter Pattern](https://refactoring.guru/design-patterns/adapter), [Facade Pattern](https://refactoring.guru/design-patterns/facade) — Pattern implementation guidance
- Barr Group — "10 Tips for Embedded Software Development" — Embedded systems best practices
- Codebase analysis files — ARCHITECTURE.md, CONCERNS.md, STRUCTURE.md (2026-01-29) — Specific enjin/enjin2 structure, coupling, technical debt

### Tertiary (LOW confidence)
- Compatibility layer analysis — enjin/enjin2_compat.hpp, enjin2/examples/eisei_game_benchmark.cpp — Real examples in codebase, needs validation for broader applicability
- Known issues in codebase — Hardcoded 128x128 canvas, missing canvas dependencies, TODO comments — Identified concerns, need verification during implementation

---
*Research completed: 2025-01-30*
*Ready for roadmap: yes*
