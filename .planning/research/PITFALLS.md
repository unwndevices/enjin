# Pitfalls Research

**Domain:** C++ Codebase Migration (Embedded Game Engine Refactoring)
**Researched:** 2026-01-30
**Confidence:** MEDIUM (Based on codebase analysis and general C++ migration patterns)

## Critical Pitfalls

### Pitfall 1: Incomplete Dependency Mapping

**What goes wrong:**
Moving code from enjin to enjin2 without tracking all dependencies results in subtle bugs that surface only during manual testing. The project has tight coupling across infrastructure, utilities, and feature code - moving one piece without its dependents creates build failures or runtime crashes.

**Why it happens:**
Developers focus on visible dependencies (explicit #include statements) but miss implicit dependencies (shared state, initialization order, platform-specific code). The parallel codebase structure (enjin/ and enjin2/) makes it easy to assume independence where coupling exists.

**How to avoid:**
Create a dependency graph before migration. For each file moved:
1. List all explicit includes
2. List all implicit dependencies (globals, singletons, initialization order)
3. Verify dependents are moved or stubbed
4. Test with dependency injection to surface hidden couplings

**Warning signs:**
- Linker errors only appear after manual testing phase
- Tests pass but manual testing crashes
- Adding one file causes unrelated build failures
- Manual testing reveals issues not caught by compilation

**Phase to address:**
Phase: Infrastructure Migration (initial phase) - create dependency graph before moving any code

---

### Pitfall 2: Premature enjin1 Deletion

**What goes wrong:**
Deleting enjin1 directory before verifying enjin2 works independently leaves no rollback path. When bugs surface in production, the team cannot restore the working state. This is especially risky with manual testing validation only.

**Why it happens:**
Pressure to "finish" the migration leads to deleting enjin1 as soon as enjin2 compiles. The goal state (only enjin2 remains) creates temptation to remove enjin1 early.

**How to avoid:**
Establish deletion criteria checklist:
1. [ ] All enjin2 examples compile and run
2. [ ] Manual testing passes on all target platforms (VCV_RACK, ESP32, WebAssembly)
3. [ ] Performance benchmarks match or exceed enjin1
4. [ ] No enjin1 references in enjin2 build
5. [ ] Critical feature parity verified
6. Archive enjin1 before deletion (git tag + backup)

**Warning signs:**
- Temptation to delete enjin1 "to clean things up"
- Running out of space or disk usage concerns
- Commit messages suggest "removed enjin1" before validation complete

**Phase to address:**
Phase: Final Cleanup - only after validation phase completes

---

### Pitfall 3: Memory Model Incompatibility

**What goes wrong:**
enjin1 uses dynamic allocation (std::shared_ptr, new/delete) while enjin2 uses static allocation for embedded systems. Mixing these patterns without proper abstraction causes memory leaks, allocation failures, or crashes at runtime.

**Why it happens:**
Developers copy utility code from enjin1 without adapting to enjin2's memory constraints. Static allocation is unfamiliar to those coming from desktop development. The temptation to "just make it work" leads to quick fixes that leak memory.

**How to avoid:**
Define memory model guidelines early:
1. No new/delete after initialization phase
2. Use fixed-size arrays for collections
3. Implement pool allocators for temporary buffers
4. Audit all moved code for allocation patterns
5. Add compile-time assertions for max sizes

**Warning signs:**
- Build warnings about deprecated heap usage
- ESP32 build succeeds but WebAssembly build fails
- Memory usage increases over time (indicates leaks)
- Crashes only after extended runtime

**Phase to address:**
Phase: Core Infrastructure Migration - establish memory model before moving feature code

---

### Pitfall 4: Component Lifecycle Drift

**What goes wrong:**
enjin1 and enjin2 have different component lifecycle methods (Awake/Start vs awake/start). Migrating components without updating lifecycle calls results in components never initializing or updating correctly. This is exacerbated by tight coupling - one broken component affects others.

**Why it happens:**
Lifecycle methods look similar (Awake vs awake) so the difference goes unnoticed during copy-paste. The change is subtle (capitalization) but critical (framework calls different method).

**How to avoid:**
1. Create lifecycle migration checklist for each component
2. Add static assertions or compile-time checks for lifecycle method signatures
3. Audit component initialization in Scene code
4. Document lifecycle differences clearly in headers
5. Consider renaming enjin1 methods during migration to force updates

**Warning signs:**
- Components added to objects but never appear or update
- Debugging shows constructors called but Awake/Start never invoked
- Some components work, others don't (inconsistent lifecycle)

**Phase to address:**
Phase: Component System Migration - migrate all components together, not piecemeal

---

### Pitfall 5: Template Code Explosion

**What goes wrong:**
enjin2 uses template-based graphics (ICanvas<Pixel4>, ICanvas<uint8_t>). Moving enjin1 code without template adaptation causes compilation errors or unexpected behavior. Developers create duplicate implementations for each pixel type, inflating code size.

**Why it happens:**
enjin1 had simpler abstractions (non-templated). enjin2's template complexity requires understanding template specialization. Developers work around templates with if statements or type erasure, losing the benefits.

**How to avoid:**
1. Document template specialization patterns
2. Create template migration guide for enjin1 code
3. Use concepts (C++20) or static assertions to catch type mismatches
4. Prefer compile-time polymorphism over runtime type checking
5. Limit template instantiations in build (explicit instantiation for common types)

**Warning signs:**
- Compile time increases dramatically
- Binary size balloons (template bloat)
- Many similar functions with only type differences
- Linker warnings about unused template instantiations

**Phase to address:**
Phase: Graphics Layer Migration - migrate canvas abstractions before feature code

---

### Pitfall 6: Platform-Specific Hardcoding

**What goes wrong:**
enjin2 supports VCV_RACK, ESP32-S3, and WebAssembly. Migrating code with platform-specific assumptions (e.g., hardcoded 128x128 canvas, PSRAM availability) causes failures on other platforms. The migration focuses on one platform, breaking others.

**Why it happens:**
Developers test on their primary platform (likely VCV_RACK for development, ESP32 for production). Platform-specific paths compile fine but fail on other targets. The #ifdef hell hides issues until cross-platform testing.

**How to avoid:**
1. Establish platform abstraction layer for all platform operations
2. Build and test all targets after each migration batch
3. Prefer compile-time errors over runtime crashes (use static assertions)
4. Document platform capabilities as compile-time constants
5. Add CI for each platform (prevent silent breakage)

**Warning signs:**
- Code compiles for ESP32 but VCV_RACK build fails
- Hardcoded dimensions or memory sizes
- Direct hardware API calls without abstraction
- Platform test failures only discovered late

**Phase to address:**
Phase: Platform Abstraction Migration - migrate all platform-specific code early

---

### Pitfall 7: Lua Binding Breakage

**What goes wrong:**
enjin2 integrates LuaJIT for scripting. Migrating enjin1 feature code without updating Lua bindings results in scripts that reference non-existent APIs or crash at runtime. The Lua integration is a key enjin2 feature - breakage here defeats the purpose.

**Why it happens:**
Lua bindings are manually maintained in enjin2/src/scripting/bindings.cpp. Adding new features to enjin2 requires adding corresponding binding functions. This step is easily forgotten during migration.

**How to avoid:**
1. Create binding checklist for each migrated feature
2. Use binding generators or macros to reduce manual work
3. Add Lua script tests for each enjin2 API surface
4. Enforce binding coverage in code review
5. Document Lua API changes in migration tickets

**Warning signs:**
- Lua scripts compile but fail at runtime
- Lua error messages about "attempt to call nil"
- Features work in C++ but not exposed to Lua
- Inconsistent behavior between C++ and Lua code paths

**Phase to address:**
Phase: Scripting Integration - migrate bindings alongside feature code

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|-----------|-------------------|-----------------|----------------|
| Keeping enjin1 includes during migration | Faster initial compiles | Hidden dependencies, unclear migration status | Never - create clean break points |
| Using #ifdef to bypass enjin1 calls | Unblocks migration of specific code | Increases technical debt, hard to remove later | Only in initial migration phase, with TODO |
| Copying enjin1 code without refactoring | Faster feature porting | Duplicated bugs, misses enjin2 improvements | Never - refactor during migration |
| Stubbing missing functions with empty implementations | Allows compilation | Silent failures, runtime crashes | Only with clear assertions to surface missing implementations |
| Skipping manual testing for "simple" code | Faster iteration velocity | Bugs escape to later phases | Never - manual testing is validation approach |

## Integration Gotchas

Common mistakes when connecting to external systems.

| Integration | Common Mistake | Correct Approach |
|-------------|------------------|------------------|
| Adafruit GFX | Direct pixel access bypassing library | Use GFX API, cache fonts in GFX format |
| ESP32 PSRAM | Assume PSRAM always available | Detect PSRAM at runtime, provide fallback path |
| VCV Rack API | Assume rack context always present | Validate rack context in all render paths |
| Emscripten | Use blocking file I/O | Use async fetch, handle Emscripten filesystem quirks |
| LuaJIT | Direct C++ object exposure to Lua | Use userdata with proper metatables, manage lifetime |
| FreeRTOS | Create tasks without considering stack size | Profile stack usage, set conservative limits |

## Performance Traps

Patterns that work at small scale but fail as usage grows.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|------------------|
| First-fit allocator (C_ImageCache) | Fragmentation, allocation failures | Implement buddy allocator or best-fit with coalescing | After ~100 images cached |
| Pixel-by-pixel post-processing | Slow rendering, frame drops | Use SIMD, scanline processing, cache results | > 10K pixels per effect |
| No canvas dirty tracking | Redraw entire scene each frame | Track dirty regions, redraw only changed | > 50 objects per scene |
| Linear search in collections | O(n) lookups, frame hitching | Use hash maps or sorted arrays for frequent lookups | > 100 objects/components |
| String allocation in tight loops | Memory churn, GC pauses (Lua) | Use string pools, pre-allocate buffers | Any string-heavy loops |
| Double buffer copy in effects | Stack overflow on constrained platforms | Use in-place variants, ping-pong buffers | ESP32 stack limit (~8KB) |

## Security Mistakes

Domain-specific security issues beyond general C++ security.

| Mistake | Risk | Prevention |
|---------|------|------------|
| No Lua sandboxing | Malicious scripts can access host APIs | Enforce Lua environment restrictions, whitelist allowed functions |
| Unchecked canvas bounds | Buffer overflow from Lua scripts | Validate all coordinates in binding functions |
| Raw new[] without RAII | Memory leaks, use-after-free | Use unique_ptr, delete in destructor |
| Stack-allocated large buffers in effects | Stack overflow, crashes | Use static/global buffers for large allocations |
| Global mutable state (g_currentBindings) | Race conditions in future threading | Pass bindings through registry, document single-threaded requirement |

## UX Pitfalls

Common user experience mistakes in embedded game engines.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| No visual feedback for missing assets | Blank screens, confusion | Add placeholder rendering, log missing asset warnings |
| Hardcoded canvas sizes | Content scales poorly | Use compile-time canvas dimensions, document scaling limits |
| Inconsistent error handling between C++ and Lua | Different behavior in scripts vs C++ | Unified error handling, propagate Lua errors to C++ |
| Manual memory management exposed to users | Confusion, crashes | Hide allocation behind engine API, provide pre-configured pools |
| No Lua debugging hooks | Scripts fail silently | Add debug print, stack trace, and breakpoint support |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Feature Migration:** Often missing Lua bindings — verify script API coverage matches C++ API
- [ ] **Component Lifecycle:** Often missing Start() call — verify components initialize in correct order
- [ ] **Canvas Operations:** Often missing bounds checking — verify all drawing functions clamp coordinates
- [ ] **Memory Model:** Often missing static allocation — verify no heap usage after initialization
- [ ] **Platform Abstraction:** Often missing ESP32-specific paths — verify all platforms build
- [ ] **Error Handling:** Often missing Lua error propagation — verify script failures surface to user
- [ ] **Testing:** Often missing manual testing — verify all examples run on target hardware
- [ ] **Documentation:** Often missing migration notes — verify API differences documented

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Premature enjin1 deletion | HIGH | Restore from git reflog or backup, re-migrate missing pieces |
| Memory leaks | MEDIUM | Use Valgrind/ASan to identify sources, add RAII wrappers |
| Component lifecycle bugs | LOW | Add lifecycle logging, trace initialization order |
| Template bloat | MEDIUM | Explicitly instantiate templates, reduce generic code |
| Platform breakage | HIGH | Revert platform-specific changes, add abstraction layer |
| Lua binding missing | LOW | Add missing binding function, run script tests |
| Performance regression | MEDIUM | Profile with platform-specific tools, compare to enjin1 baseline |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Incomplete dependency mapping | Phase 1: Infrastructure Migration | Dependency graph complete, all dependents tracked |
| Premature enjin1 deletion | Phase 5: Final Cleanup | Deletion checklist complete, all validation passed |
| Memory model incompatibility | Phase 1: Core Infrastructure | Compile-time assertions, no heap usage after init |
| Component lifecycle drift | Phase 2: Component System | All components use enjin2 lifecycle, tests pass |
| Template code explosion | Phase 2: Graphics Layer | Template instantiations limited, binary size within limits |
| Platform-specific hardcoding | Phase 1: Platform Abstraction | All platforms build and test successfully |
| Lua binding breakage | Phase 3: Scripting Integration | Script coverage tests pass, all APIs exposed |
| Performance regression | Phase 4: Validation Phase | Benchmarks match or exceed enjin1 baseline |

## Sources

- enjin2 codebase analysis: ARCHITECTURE.md, CONCERNS.md, STRUCTURE.md (2026-01-29)
- Herb Sutter: "Elements of Modern C++ Style" - smart pointer usage, RAII patterns
- Refactoring.Guru: Code Smells Catalog - change preventers, couplers
- enjin vs enjin2 code comparison: 12,488 lines vs 25,578 lines
- Embedded systems best practices: Barr Group "10 Tips for Embedded Software Development"
- Known issues in codebase: hardcoded 128x128 canvas, missing canvas dependencies, TODO comments

---
*Pitfalls research for: C++ Codebase Migration (enjin to enjin2)*
*Researched: 2026-01-30*
