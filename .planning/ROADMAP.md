# Roadmap: Enjin Migration

## Overview

Migrate enjin2 to complete independence by mapping dependencies, migrating core infrastructure, enabling feature support through abstraction layers, validating behavior through manual testing, and completing enjin2-only builds. The journey moves from understanding dependencies (Phase 1) through foundational migration (Phase 2) to feature enablement (Phase 3), validation (Phase 4), and final cleanup (Phase 5).

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Dependency Analysis** - Map enjin1→enjin2 dependencies and establish compilation isolation
- [x] **Phase 2: Core Migration** - Migrate core infrastructure with compatibility layer
- [ ] **Phase 3: Feature Support** - Enable feature migration with abstraction layers
- [ ] **Phase 4: Validation** - Validate behavior through manual testing and shadow mode
- [ ] **Phase 5: Final Cleanup** - Complete enjin2-only build system

## Phase Details

### Phase 1: Dependency Analysis
**Goal**: Understand enjin1→enjin2 dependencies and establish compilation isolation
**Depends on**: Nothing (first phase)
**Requirements**: FND-01, FND-02, FND-03
**Success Criteria** (what must be TRUE):
  1. Dependency graph exists showing all enjin1 references in enjin2 across infrastructure, utilities, and features
  2. enjin2 compiles with separate build target and include paths from enjin1
  3. No `namespace enjin` references exist in enjin2 codebase
**Plans**: 3 plans

Plans:
- [x] 01-01-PLAN.md — Generate dependency graph mapping all enjin1→enjin2 dependencies
- [x] 01-02-PLAN.md — Verify no namespace enjin references in enjin2 codebase
- [x] 01-03-PLAN.md — Establish compilation isolation with separate CMake targets
- [x] 02-01-PLAN.md — Create compatibility headers for type aliases
- [x] 02-02-PLAN.md — Implement Strangler Fig seams for incremental migration
- [x] 02-03-PLAN.md — Document memory mapping strategy (shared_ptr → unique_ptr)
- [x] 03-01-PLAN.md — Configure CMake build system for compile-time backend selection
- [x] 03-02-PLAN.md — Create abstraction interfaces (ICanvas, IComponent, IScene)
- [x] 03-03-PLAN.md — Update seams for compile-time routing with interface implementation

### Phase 4: Validation
**Goal**: Validate behavior through manual testing and shadow mode
**Depends on**: Phase 3
**Requirements**: FND-08, STR-03
**Success Criteria** (what must be TRUE):
  1. Manual testing baseline covers component lifecycle, rendering, scene transitions, and Lua scripting
  2. Shadow mode execution runs both enjin1 and enjin2 in parallel with output comparison for behavioral verification
**Plans**: TBD

Plans:
- [ ] 04-01: TBD
- [ ] 04-02: TBD

### Phase 5: Final Cleanup
**Goal**: Complete enjin2-only build system
**Depends on**: Phase 4
**Requirements**: FND-10
**Success Criteria** (what must be TRUE):
  1. CMakeLists.txt supports clean enjin2-only builds without any enjin1 paths or references
**Plans**: TBD

Plans:
- [ ] 05-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5

  | Phase | Plans Complete | Status | Completed |
  |-------|----------------|--------|-----------|
  | 1. Dependency Analysis | 3/3 | Complete | 2026-01-30 |
  | 2. Core Migration | 3/3 | Complete | 2026-01-30 |
  | 3. Feature Support | 3/3 | Complete | 2026-01-30 |
  | 4. Validation | 0/0 | Not started | - |
  | 5. Final Cleanup | 0/0 | Not started | - |
