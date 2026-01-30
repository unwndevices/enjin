# Requirements: Enjin Migration

**Defined:** 2026-01-30
**Core Value:** enjin2 works independently without any enjin1 dependencies

## v1 Requirements

Requirements for complete migration from enjin to enjin2. Each maps to roadmap phases.

### Foundation

- [x] **FND-01**: Map all enjin1 → enjin2 dependencies across infrastructure, utilities, and feature code
- [x] **FND-02**: Create separate build targets for enjin1 and enjin2 with independent compilation
- [x] **FND-03**: Verify no `namespace enjin` references exist in enjin2 codebase
- [ ] **FND-04**: Create compatibility headers aliasing enjin1 types to enjin2 equivalents
- [ ] **FND-05**: Map enjin1 shared_ptr usage to enjin2 static allocation patterns with equivalent lifetime semantics
- [ ] **FND-06**: Map enjin1 component lifecycle (Awake/Start) to enjin2 lifecycle (awake/start)
- [ ] **FND-07**: Port scene management system including SceneStateMachine and transitions
- [ ] **FND-08**: Establish manual testing baseline for component lifecycle, rendering, scene transitions, and Lua scripting
- [ ] **FND-09**: Ensure all enjin2 headers compile independently without enjin1 includes
- [ ] **FND-10**: Update CMakeLists.txt to support clean enjin2-only builds without enjin1 paths

### Migration Strategy

- [ ] **STR-01**: Implement Strangler Fig pattern with compatibility seams for incremental replacement
- [ ] **STR-02**: Extract legacy seams at component and scene boundaries for testing in isolation
- [ ] **STR-03**: Implement shadow mode execution running both enjin1 and enjin2 in parallel with output comparison
- [ ] **STR-04**: Create canvas abstraction layer enabling both implementations to target same interface

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Performance Optimization

- **PERF-01**: Benchmark framework comparing enjin1 and enjin2 performance
- **PERF-02**: Performance regression guardrails ensuring enjin2 matches or exceeds enjin1 metrics

### Advanced Migration

- **ADV-01**: Incremental dependency inversion to break remaining circular dependencies
- **ADV-02**: API stability guarantees with deprecation warnings for public interfaces
- **ADV-03**: Rollback capability through Git branches until deprecation complete

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| "Big bang" complete rewrite | Extremely high risk, impossible to validate behavior mid-project |
| Feature parity with enjin1 bugs | Fix bugs during migration, don't replicate broken behavior |
| Shared namespace during migration | Breaks name lookup rules, impossible to track dependencies |
| Copy-paste implementation from enjin1 | Violates enjin2 static allocation constraints |
| Automated mass refactoring | C++ refactoring tools are error-prone, manual migration required |
| Temporary global state | Undermines enjin2 architecture goals of no global state |
| Dynamic allocation quick fixes | Violates non-dynamic memory constraint |
| Parallel binary incompatibility | Linker conflicts, impossible to verify which code runs |
| Transitional code permanence | Technical debt accumulation, mark for deletion |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
 | FND-01 | Phase 1 | Complete |
| FND-02 | Phase 1 | Complete |
| FND-03 | Phase 1 | Complete |
| FND-04 | Phase 2 | Pending |
| FND-05 | Phase 2 | Pending |
| FND-06 | Phase 2 | Pending |
| FND-07 | Phase 2 | Pending |
| FND-08 | Phase 4 | Pending |
| FND-09 | Phase 3 | Pending |
| FND-10 | Phase 5 | Pending |
| STR-01 | Phase 2 | Pending |
| STR-02 | Phase 3 | Pending |
| STR-03 | Phase 4 | Pending |
| STR-04 | Phase 3 | Pending |

**Coverage:**
- v1 requirements: 14 total
- Mapped to phases: 14
- Unmapped: 0 ✓

---
*Requirements defined: 2026-01-30*
*Last updated: 2026-01-30 after initial definition*
