# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-01-30)

**Core value:** enjin2 works independently without any enjin1 dependencies
**Current focus:** Phase 6: Create library docs, using doxygen + Docusaurus

## Current Position

Phase: 6 of 6 (Create library docs, using doxygen + Docusaurus)
Plan: 6 of 6 in current phase
Status: Gap closure - MDX formatting errors block build
Last activity: 2026-02-01 — Partially completed 06-06: API navigation configured, build blocked by MDX errors

Progress: [████████████░] 95% (Phase 5 complete, Phase 6: 95% complete - MDX formatting issues remain)

## Performance Metrics

**Velocity:**
- Total plans completed: 21
- Average duration: 6.1 min
- Total execution time: 2.1 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01-dependency-analysis | 3 | 3 | 5 min |
| 02-core-migration | 3 | 3 | 4 min |
| 03-feature-support | 3 | 3 | 3 min |
| 04-validation | 4 | 4 | 8.5 min |
| 05-final-cleanup | 1 | 1 | 7 min |
| 06-create-library-docs | 5 | 5 | 9.2 min |

**Recent Trend:**
 - Last 3 plans: 11 min, 11 min, 3 min
 - Trend: Stable

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Production vs test/examples separation: Analysis limited to src/ and include/ directories, excluding tests/ and examples/ (01-02)
- Verification methodology: Multi-pattern grep + AST-level analysis using clang-tidy for comprehensive dependency checking (01-02)
- Namespace verification confirmed: Zero enjin1 namespace references in enjin2 production code (01-02)
- Include directory scoping: enjin2 uses PRIVATE include directories for strict isolation from enjin1 (01-03)
- Build isolation verification: Multi-method verification using .d files, symbol tables, and deliberate include tests (01-03)
- External dependency handling: Adafruit-GFX-Library documented separately from enjin1 isolation assessment (01-03)
- Include directory scoping: enjin2 uses PRIVATE include directories for strict isolation from enjin1 (01-03)
- Build isolation verification: Multi-method verification using .d files, symbol tables, and deliberate include tests (01-03)
- External dependency handling: Adafruit-GFX-Library documented separately from enjin1 isolation assessment (01-03)
- Compatibility namespace: Use `namespace enjin` for compatibility layer wrapping enjin2 types (02-01)
- Lifecycle mapping: PascalCase wrappers (Awake, Start, Update) map to camelCase enjin2 methods (awake, start, update) (02-01)
- Seam API alignment: Seams must only expose methods that exist in underlying implementations (e.g., SceneSeam doesn't have initialize() because SceneStateMachine doesn't) (02-02)
- Memory mapping: shared_ptr to unique_ptr conversion requires scene-based ownership and runtime null checks during migration (02-03)
- Backend selection: CMake option USE_ENJIN1 controls compile-time backend with USE_ENJIN1_BACKEND macro for conditional compilation (03-01)
- INTERFACE library scope: INTERFACE targets must use target_compile_definitions with INTERFACE scope to propagate to dependent targets (03-01)
- Template abstraction: ICanvas and IScene templated on pixel type, IComponent non-templated (03-02)
- Minimal interface scope: Only methods that exist in enjin2 implementations exposed (03-02)
- Forward declarations: Abstract headers use forward declarations to avoid implementation dependencies (03-02)
- Compile-time routing: Use #if USE_ENJIN1_BACKEND (not #ifdef) to allow CMake to set 0 or 1 and work correctly (03-03)
- Deprecated runtime switching: Keep legacy methods with [[deprecated]] attributes for backward compatibility during migration (03-03)
- Single-header vendor libraries: Use enjin2/vendor/ directory for zero-dependency third-party libraries (04-01)
- Template explicit instantiation: Template methods in .cpp files require explicit instantiation for static library linking (04-01)
- BMP format conversion: 8-bit grayscale converted to 24-bit RGB (gray=R=G=B) for broad BMP compatibility (04-01)
- Image loading library: stb_image.h separate from stb_image_write.h for BMP loading functionality (04-02)
- Pixel difference tolerance: 3% threshold for shadow mode verification (04-02)
- Manual testing structure: Checklist with Objective/Test/Expected fields for structured human verification (04-02)
- Shadow mode execution: Use absolute paths in shell script to avoid directory navigation issues, timestamped results directories, no fail-fast execution with comprehensive summary (04-03)
- Test result parsing: Use summary.txt format (shadow-test.sh creates summary.txt, not comparison.txt) for consistent parsing (04-04)
- Final cleanup: Removed all conditional compilation and CMake options for enjin1 backend - enjin2-only build system (05-01)
- Doxygen XML structure: Standard Doxygen output creates individual class/namespace files (class*.xml, namespace*.xml) instead of aggregate files (classes.xml, namespaces.xml) - index.xml provides master reference (06-01)
- Documentation generation: CMake docs target with find_package(Doxygen) enables automated XML generation via cmake --build . --target docs (06-01)
- Docusaurus root routing: Changed routeBasePath from '/docs' to '/' to serve docs from root path (/enjin/) - provides cleaner URLs and avoids broken logo links (06-02)
- Home page requirement: Created index.md as Docusaurus home page since navbar logo links to root path, which requires valid content (06-02)
- API sidebar organization: Structured by module (Core, Graphics, Utils) instead of alphabetical A-Z to match project header organization (06-02)
  - CSS branding variables: Used Docusaurus CSS custom properties (--ifm-color-primary-*) instead of swizzling theme components for easier theme updates (06-02)
  - Documentation tone: One sentence per concept, practical/concise, short paragraphs with white space for internal audience (06-03)
  - XML parsing with xml2js: Use xml2js library instead of manual regex parsing for Doxygen XML to handle C++ templates, namespaces, and overloads correctly (06-04)
 - MDX angle bracket escaping: Escape template parameters (< and >) in markdown output with HTML entities (&lt; and &gt;) to prevent MDX from interpreting them as JSX tags (06-04)
 - Module-based API organization: Organize API reference by module (core, graphics, ui, utils, etc.) instead of alphabetical A-Z for better navigation matching project structure (06-04)
 - Guide-to-API cross-links: Add "See Also" sections to guide pages for cross-referencing between narrative guides and API reference pages (06-04)
 - Automated API generation: Integrate generate-api-docs.js into CMake docs target to generate both Doxygen XML and Docusaurus markdown with single command (06-04)
 - CI/CD deployment triggers: GitHub Actions workflow triggers on docs/include/workflow changes only to avoid unnecessary builds (06-05)
 - GitHub Pages deployment method: Use actions/deploy-pages@v4 with GitHub Actions source instead of gh-pages branch for better integration (06-05)
 - Local preview testing: Single deploy-docs.sh script handles full build process and serves with python3 http.server for dependency-free testing (06-05)
 - Deployment documentation: Comprehensive troubleshooting section in deployment.md covers common GitHub Pages issues and configuration steps (06-05)
 - Docusaurus dual-plugin setup: Separate plugins for guides (classic preset) and API (@docusaurus/plugin-content-docs with id: 'api') with exclude: ['api/**'] in classic preset to prevent document ID conflicts (06-06)
 - API navigation configuration: Module-based sidebar (docs/api-sidebar.js) with paths without 'api/' prefix (e.g., 'core/Object' not 'api/core/Object') to match plugin's document ID generation (06-06)

### Roadmap Evolution

- Phase 6 added: Create library docs, using doxygen + Docusaurus

### Pending Todos

[From .planning/todos/pending/ — ideas captured during sessions]

None yet.

### Blockers/Concerns

[Issues that affect future work]

- **MDX syntax errors in API files**: All 59 generated API markdown files have malformed code blocks causing Docusaurus build failures. Code blocks have incorrect backtick syntax (e.g., ````javascript\n```void fill(...)=0```````). The `generate-api-docs.js` script from plan 06-04 needs to be fixed and all API files regenerated.
- 210 Doxygen warnings during XML generation indicate incomplete documentation in some headers (acceptable for initial setup, to be addressed in later phase)

## Session Continuity

Last session: 2026-02-01
Stopped at: Completed Plan 05 (Set up CI/CD deployment)
Resume file: None
