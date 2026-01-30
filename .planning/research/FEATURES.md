# Feature Research

**Domain:** C++ Codebase Migration (enjin → enjin2)
**Researched:** 2026-01-30
**Confidence:** HIGH

## Feature Landscape

### Table Stakes (Users Expect These)

Features essential for successful C++ codebase migration. Missing these = migration fails or produces unmaintainable code.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **Dependency Mapping & Analysis** | Cannot migrate what you don't understand | HIGH | Map all enjin1 → enjin2 dependencies across infrastructure, utilities, and feature code. Must identify circular dependencies early. |
| **API Compatibility Layer** | Enables gradual migration without breaking existing code | MEDIUM | Compatibility headers that alias enjin1 types to enjin2 equivalents, allowing staged migration (e.g., `enjin2_compat.hpp`). |
| **Compilation Isolation** | Prevents accidental enjin1 dependencies during migration | MEDIUM | Separate build targets, include paths, and namespaces. Enjin2 already uses `enjin2` namespace - leverage this strictly. |
| **Functional Parity Testing** | Must verify enjin2 provides identical behavior before deprecation | HIGH | Manual testing validation specified in constraints. Should verify component lifecycle, rendering, scene transitions, Lua scripting. |
| **Build System Migration** | CMake must cleanly support enjin2-only builds | MEDIUM | Remove enjin1 paths from CMakeLists.txt, ensure Adafruit-GFX external dependency resolved cleanly. |
| **Memory Layout Equivalence** | Embedded systems require predictable memory behavior | HIGH | enjin1 uses `std::shared_ptr` for components, enjin2 uses handle-based static allocation. Must ensure same lifetime semantics. |
| **Namespace Separation** | Prevents naming collisions during migration | LOW | Both use separate namespaces (`enjin` vs `enjin2`). Critical to maintain this separation until deprecation. |
| **Header Self-Containment** | Required for clean module boundaries | LOW | All enjin2 headers must compile independently (Google C++ Style Guide). Prevents hidden enjin1 dependencies. |
| **Component Lifecycle Mapping** | Object-Component system is core engine feature | HIGH | enjin1: `Awake()`, `Start()`; enjin2: `awake()`, `start()`. Must map lifecycle hooks correctly. |
| **Scene Graph Porting** | Scene management is fundamental to engine | HIGH | `SceneStateMachine`, scene transitions, transition effects must all migrate cleanly. |

### Differentiators (Competitive Advantage)

Features that set this migration apart from typical "big bang" rewrites.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Strangler Fig Pattern Application** | Incremental replacement without "big bang" risk | MEDIUM | Gradually divert flow from enjin1 to enjin2 via compatibility seams. Reduces migration risk significantly. |
| **Legacy Seams Extraction** | Enables testing in isolation, creates migration footholds | HIGH | Introduce seams at boundaries (component interfaces, canvas abstractions, signal/event system). Allows enjin2 to coexist during migration. |
| **Performance Regression Guardrails** | enjin2 promises non-dynamic memory and 4-bit optimization | MEDIUM | Benchmark framework to ensure migration doesn't regress performance. Example: `eisei_game_benchmark.cpp`. |
| **Transitional Architecture Minimalism** | Avoids technical debt accumulation during migration | MEDIUM | Keep compatibility layers thin and temporary. Mark with `// TODO: Remove after migration`. |
| **Feature-First Migration Order** | Delivers value early, reduces risk | MEDIUM | Migrate high-value features first (e.g., Lua scripting integration) to prove enjin2 viability. |
| **Branch by Abstraction** | Parallel development with merge window | HIGH | Create abstraction layer that both enjin1 and enjin2 can implement. Example: canvas interfaces. |
| **Shadow Mode Execution** | Validate correctness without commitment | HIGH | Run both enjin1 and enjin2 implementations in parallel, compare outputs. High confidence builder. |
| **Incremental Dependency Inversion** | Breaks circular dependencies | HIGH | Identify tight coupling, introduce interfaces or forwarding to invert dependencies. Essential for decoupling. |
| **API Stability Guarantees** | Maintains external contract during migration | MEDIUM | Public APIs should remain stable while internals migrate. Use deprecation warnings. |
| **Rollback Capability** | Safe migration requires undo path | MEDIUM | Maintain ability to revert to enjin1 until deprecation complete. Git branches, feature flags. |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems during C++ migration.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| **"Big Bang" Complete Rewrite** | Seems faster to just replace everything | Extremely high risk, impossible to validate behavior, likely to fail mid-project | Strangler Fig pattern with incremental migration |
| **Feature Parity with enjin1 Bugs** | "If enjin1 does it, enjin2 must too" | enjin1 has known bugs (e.g., hardcoded 128x128 effects, missing canvas dependency warnings) | Fix bugs during migration, don't replicate broken behavior |
| **Shared Namespace During Migration** | "Just merge namespaces temporarily" | Breaks name lookup rules, makes dependency tracking impossible, leads to subtle bugs | Keep namespaces strict, use compatibility headers with `using` declarations only |
| **Copy-Paste Implementation** | Fast to just copy enjin1 code to enjin2 | enjin1 uses `std::shared_ptr`, dynamic allocation - violates enjin2 design constraints | Reimplement using enjin2 patterns (static allocation, handle-based) |
| **Extensive Polymorphism** | Make enjin2 classes inherit from enjin1 to "get reuse" | Creates tight coupling, prevents enjin2 from being deleted, violates Liskov Substitution | Use interface extraction, prefer composition over inheritance |
| **Automated Mass Refactoring** | "Let tools do bulk find/replace" | C++ refactoring tools are error-prone, may change semantics, hard to review | Manual, careful migration with peer review of each change |
| **Temporary Global State** | "Just add a global for this migration phase" | enjin2 explicitly avoids global state (unlike enjin1), undermines architecture goals | Pass state explicitly, use dependency injection |
| **Dynamic Allocation Quick Fixes** | "Use `new` temporarily to make it work" | Violates non-dynamic memory constraint, defeats enjin2 value proposition | Use fixed-size pools, arena allocators, or constexpr where possible |
| **Parallel Binary Incompatibility** | Build both enjin1 and enjin2 into same executable during migration | Linker conflicts, symbol clashes, impossible to verify which code runs | Separate executables, separate tests, use feature flags at source level |
| **Transitional Code Permanence** | "We'll clean up this shim layer later" | Technical debt rarely paid off, becomes permanent enjin1 dependency | Mark transitional code clearly, plan deletion milestones, track deprecation timeline |

## Feature Dependencies

```
[Dependency Mapping & Analysis]
    └──requires──> [Compilation Isolation]
                   └──requires──> [Namespace Separation]

[API Compatibility Layer]
    └──enhances──> [Strangler Fig Pattern Application]
    └──requires──> [Header Self-Containment]

[Legacy Seams Extraction]
    └──enables──> [Branch by Abstraction]
    └──enables──> [Shadow Mode Execution]
    └──requires──> [Dependency Mapping & Analysis]

[Strangler Fig Pattern Application]
    └──uses──> [API Compatibility Layer]
    └──uses──> [Legacy Seams Extraction]
    └──enhances──> [Feature-First Migration Order]

[Functional Parity Testing]
    └──requires──> [Shadow Mode Execution]
    └──requires──> [Performance Regression Guardrails]
    └──validates──> ALL_FEATURES

[Incremental Dependency Inversion]
    └──requires──> [Dependency Mapping & Analysis]
    └──enables──> [Component Lifecycle Mapping]
    └──enables──> [Scene Graph Porting]

[Build System Migration]
    └──requires──> [Compilation Isolation]
    └──requires──> [Feature-First Migration Order]
    └──blocks──> enjin1_deletion
```

### Dependency Notes

- **Dependency Mapping & Analysis requires Compilation Isolation**: Cannot analyze dependencies cleanly if enjin1 and enjin2 intermix at build time.
- **API Compatibility Layer enhances Strangler Fig Pattern**: Compatibility headers are the "enabling point" for strangler fig implementation (Martin Fowler's enabling point concept).
- **Legacy Seams enables Branch by Abstraction**: Seams create the abstraction layer that both implementations can target.
- **Shadow Mode Execution validates Functional Parity**: Running both implementations in parallel provides the comparison data for parity testing.
- **Incremental Dependency Inversion enables Core Feature Migration**: Many circular dependencies block component lifecycle and scene graph migration. Inversion breaks these cycles.
- **Feature-First Migration Order uses Strangler Fig**: Strangler fig provides the mechanism for incrementally diverting flow to new features.
- **Build System Migration blocks enjin1 deletion**: Cannot delete enjin1 directory while CMake still references it.
- **Transitional Code Permanence anti-pattern conflicts with Rollback Capability**: If transitional code accumulates without cleanup, rollback becomes impossible.

## MVP Definition

### Launch With (Phase 1: Core Infrastructure Migration)

Minimum viable migration to establish enjin2 independence foundation.

- [x] **Dependency Mapping & Analysis** — Must understand what enjin2 depends on before migrating anything
- [x] **Compilation Isolation** — Separate enjin2 build from enjin1, verify clean build
- [x] **Namespace Separation** — Ensure no `namespace enjin` references in enjin2
- [x] **API Compatibility Layer** — Create basic compatibility headers for critical types (Point, Rect, Component base)
- [x] **Functional Parity Testing** — Establish manual testing baseline for core features

### Add After Validation (Phase 2: Feature Migration)

Features to add once core infrastructure is working and tested.

- [ ] **Legacy Seams Extraction** — Introduce seams at component and scene boundaries
- [ ] **Branch by Abstraction** — Create canvas abstraction layer
- [ ] **Component Lifecycle Mapping** — Port component lifecycle from enjin1 to enjin2
- [ ] **Scene Graph Porting** — Port scene management system
- [ ] **Shadow Mode Execution** — Implement parallel execution testing

### Future Consideration (Phase 3: Completion & Deletion)

Features to defer until migration is nearly complete.

- [ ] **Performance Regression Guardrails** — Benchmark framework for optimization validation
- [ ] **Incremental Dependency Inversion** — Break remaining circular dependencies
- [ ] **Build System Migration** — Remove all enjin1 paths, enable enjin2-only build
- [ ] **Rollback Capability** — Maintain branches until deprecation complete
- [ ] **enjin1 Directory Deletion** — Final step, only after all validation complete

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Dependency Mapping & Analysis | HIGH | HIGH | P1 |
| Compilation Isolation | HIGH | LOW | P1 |
| Namespace Separation | HIGH | LOW | P1 |
| API Compatibility Layer | MEDIUM | MEDIUM | P1 |
| Functional Parity Testing | HIGH | HIGH | P1 |
| Legacy Seams Extraction | MEDIUM | HIGH | P2 |
| Component Lifecycle Mapping | HIGH | HIGH | P2 |
| Scene Graph Porting | HIGH | HIGH | P2 |
| Build System Migration | MEDIUM | MEDIUM | P2 |
| Shadow Mode Execution | MEDIUM | HIGH | P2 |
| Branch by Abstraction | LOW | HIGH | P2 |
| Incremental Dependency Inversion | MEDIUM | HIGH | P3 |
| Performance Regression Guardrails | MEDIUM | MEDIUM | P3 |
| Rollback Capability | MEDIUM | LOW | P3 |
| Strangler Fig Pattern Application | HIGH | MEDIUM | P3 |
| Transitional Architecture Minimalism | MEDIUM | LOW | P3 |
| API Stability Guarantees | MEDIUM | MEDIUM | P3 |
| Feature-First Migration Order | MEDIUM | LOW | P3 |

**Priority key:**
- P1: Must have for migration start (foundation)
- P2: Should have for migration progress (core features)
- P3: Nice to have for migration completion (optimization)

## Competitor Feature Analysis

| Feature | Enjin → Enjin2 Approach | Typical "Big Bang" Rewrite | Google C++ Migration |
|---------|---------------------------|------------------------|-------------------|
| Migration Strategy | Incremental via Strangler Fig | Complete replacement then switch | Branch by Abstraction |
| Dependency Management | Compatibility layers + seams | Copy all dependencies | Forward declarations only |
| Testing Approach | Manual testing + shadow mode | Integration tests after migration | Unit test driven |
| Validation | Functional parity + performance | Feature checklist | Style guide conformance |
| Build Approach | Separate targets, gradual merge | Single build system | Bazel / Blaze |
| Architecture | Strangler fig + transitional architecture | Monolithic replacement | Modular libraries |
| Namespace Handling | Strict separation via compat layers | Merge namespaces | Strict separation required |
| Memory Model | enjin2: static, enjin1: dynamic (mapped) | New memory model | Trivially destructible globals |

**Our Approach:** Strangler Fig with incremental migration, manual validation, compatibility layers. Favors risk reduction over speed.

## Sources

- **Patterns of Legacy Displacement** - Martin Fowler, Ian Cartwright, Rob Horn, James Lewis (HIGH) - Comprehensive patterns for incremental legacy modernization including Strangler Fig, Legacy Seams, Event Interception, Divert the Flow
- **Strangler Fig Application** - Martin Fowler (HIGH) - Core pattern for gradual replacement without "big bang" risk
- **Legacy Seam** - Martin Fowler (HIGH) - How to introduce enabling points for testing and migration in legacy systems
- **Working Effectively with Legacy Code** - Michael Feathers (MEDIUM) - Original source of "seam" concept, though not directly fetched
- **Google C++ Style Guide** (HIGH) - Header self-containment, include ordering, namespace usage guidelines
- **Codebase Analysis Files** - `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/CONCERNS.md` (HIGH) - Specific understanding of enjin/enjin2 structure, coupling, and technical debt
- **Compatibility Layer Analysis** - `enjin/enjin2_compat.hpp`, `enjin2/examples/eisei_game_benchmark.cpp` (HIGH) - Real examples of compatibility approach in this codebase

---
*Feature research for: C++ Codebase Migration (enjin → enjin2)*
*Researched: 2026-01-30*
*Confidence: HIGH*