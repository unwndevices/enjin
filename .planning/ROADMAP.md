# Roadmap: Enjin Migration

## Overview

Migrate enjin2 to complete independence by mapping dependencies, migrating core infrastructure, enabling feature support through abstraction layers, validating behavior through manual testing, and completing enjin2-only builds. The journey moves from understanding dependencies (Phase 1) through foundational migration (Phase 2) to feature enablement (Phase 3), validation (Phase 4), and final cleanup (Phase 5).

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Dependency Analysis** - Map enjin1→enjin2 dependencies and establish compilation isolation
- [ ] **Phase 2: Core Migration** - Migrate core infrastructure with compatibility layer
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
**Plans**: TBD

Plans:
- [ ] 01-01: TBD
- [ ] 01-02: TBD
- [ ] 01-03: TBD

### Phase 2: Core Migration
**Goal**: Migrate core infrastructure with compatibility layer
**Depends on**: Phase 1
**Requirements**: FND-04, FND-05, FND-06, FND-07, STR-01
**Success Criteria** (what must be TRUE):
  1. Compatibility headers alias enjin1 types to enjin2 equivalents allowing gradual code migration
  2. enjin1 shared_ptr usage maps to enjin2 static allocation patterns with equivalent lifetime semantics
  3. enjin1 component lifecycle (Awake/Start) maps to enjin2 lifecycle (awake/start) with consistent behavior
  4. Scene management system including SceneStateMachine and transitions works in enjin2
  5. Strangler Fig pattern enables incremental replacement via compatibility seams at component and scene boundaries
**Plans**: TBD

Plans:
- [ ] 02-01: TBD
- [ ] 02-02: TBD
- [ ] 02-03: TBD

### Phase 3: Feature Support
**Goal**: Enable feature migration with abstraction layers
**Depends on**: Phase 2
**Requirements**: FND-09, STR-02, STR-04
**Success Criteria** (what must be TRUE):
  1. All enjin2 headers compile independently without enjin1 includes
  2. Legacy seams at component and scene boundaries enable testing in isolation
  3. Canvas abstraction layer enables both enjin1 and enjin2 implementations to target the same interface
**Plans**: TBD

Plans:
- [ ] 03-01: TBD
- [ ] 03-02: TBD
- [ ] 03-03: TBD

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
| 1. Dependency Analysis | 0/0 | Not started | - |
| 2. Core Migration | 0/0 | Not started | - |
| 3. Feature Support | 0/0 | Not started | - |
| 4. Validation | 0/0 | Not started | - |
| 5. Final Cleanup | 0/0 | Not started | - |
