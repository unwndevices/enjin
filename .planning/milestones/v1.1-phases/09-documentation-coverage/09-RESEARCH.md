# Phase 9: Documentation Coverage - Research

**Researched:** 2026-02-03
**Domain:** Doxygen documentation for C++ embedded game engine
**Confidence:** HIGH

## Summary

This phase focuses on improving Doxygen documentation quality across all public APIs in the enjin2 embedded game engine. The project currently has 210 Doxygen warnings that need to be reduced to under 20. The codebase consists of 56 header files organized across 10 modules (core, graphics, UI, scripting, etc.) with partial documentation already in place. The current Doxyfile is configured to only document entities with comments (EXTRACT_ALL=NO) and warns about undocumented public APIs.

Key findings:
- Project uses good Doxygen practices already (@brief, @param, @return) but coverage is incomplete
- Modules need overview pages explaining their purpose (not design details)
- Standard C++ Doxygen patterns apply: Javadoc/Qt style comments, structural commands for classes/files
- Warning types likely include: undocumented classes/functions, missing file documentation for global entities, incomplete parameter documentation
- Boilerplate documentation approach is appropriate for APIs lacking current docs

**Primary recommendation:** Use Doxygen's built-in structural commands (@class, @file, @defgroup) and standard comment styles (/** */ or ///) to add minimal boilerplate documentation to all public APIs, create module overview pages using @defgroup or @page, and enable WARN_NO_PARAMDOC=YES to identify missing parameter documentation.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Doxygen | 1.16+ (current) | Generate documentation from C++ source code | Industry standard, supports all required features, handles templates, generates multiple output formats |
| XML output | Doxygen standard | Machine-readable documentation for analysis | Enables automated checking, integration with CI/CD, cross-reference with other tools |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| HTML output (optional) | Doxygen standard | Human-readable documentation | Not required for phase goals (currently disabled), useful for final review |
| Markdown support | Doxygen built-in | README documentation files | If project wants markdown-based guides (not required for this phase) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Doxygen | Sphinx | Sphinx requires more setup, better for Python, Doxygen is native for C++ |
| Doxygen | Doxypypy | Python-based, newer, less mature, smaller community |
| XML only | HTML + XML | HTML requires more configuration, adds build time, not needed for warning analysis |

**Installation:**
Doxygen is already installed and configured. Version check:
```bash
doxygen --version
```

## Architecture Patterns

### Recommended Project Structure
```
include/enjin2/
├── core/              # Core types, objects, components (7 files)
├── graphics/          # Canvas, rendering, fonts (6 files)
├── ui/               # UI components, widgets (4 files)
├── scripting/         # Lua integration (4 files)
├── animation/         # Animation system (2 files)
├── utils/             # Utilities, helpers (2 files)
├── abstract/          # Abstract interfaces
├── compat/            # Compatibility layer
├── components/        # Component implementations
└── effects/           # Visual effects
```

Each module directory contains public API headers (`.hpp` files) that need documentation coverage.

### Pattern 1: Essential Documentation Template

For all public APIs, use minimal documentation template:
```cpp
/**
 * @brief One-sentence description of what this does
 *
 * Detailed description (optional, one paragraph max).
 */
class ClassName {
public:
    /**
     * @brief Constructor description
     * @param param1 Description of parameter
     * @param param2 Description of parameter
     * @return Description of return value (if applicable)
     */
    ClassName(Type1 param1, Type2 param2);

    /**
     * @brief Method description
     * @return Description of return value
     */
    ReturnType method();
};
```

**Source:** Based on Doxygen manual: https://www.doxygen.nl/manual/docblocks.html

### Pattern 2: Module Overview Page

Create module overview at `include/enjin2/<module>/module.md` or document the first file:
```cpp
/**
 * @file <module>_overview.hpp
 * @brief <Module name> module
 *
 * One-paragraph description of module purpose and what it provides.
 * No design notes, no examples (per CONTEXT.md requirements).
 */
```

Or using topic grouping:
```cpp
/**
 * @defgroup <module>_group <Module Name>
 * @brief One-paragraph module purpose
 */
```

**Source:** Doxygen grouping manual: https://www.doxygen.nl/manual/grouping.html

### Pattern 3: File Documentation for Global Entities

Required for documenting any global functions, variables, enums, typedefs, or defines:
```cpp
/**
 * @file filename.hpp
 * @brief One-line file description
 *
 * More detailed description if needed.
 */
```

**Source:** Doxygen FAQ: "When I set EXTRACT_ALL to NO none of my functions are shown"

### Pattern 4: Boilerplate for Undocumented APIs

For APIs with no current documentation, add minimal boilerplate:
```cpp
/**
 * @brief [TODO: Describe what this does]
 */
void existingFunction(int param);

/**
 * @brief [TODO: Describe what this class represents]
 */
struct ExistingStruct {
    int field; ///< [TODO: Describe field]
};
```

Track these in a separate list (e.g., `PLACEHOLDER_DOCS.md`) for future review.

### Anti-Patterns to Avoid
- **Verbose documentation for simple getters/setters:** `/** @brief Get value */` is sufficient
- **Code examples in API reference:** Examples belong in tutorials/guides (per CONTEXT.md)
- **Pre/postconditions documentation:** Not required for essential level (per CONTEXT.md)
- **Over-documenting internal APIs:** src/ code is internal, skip unless high value (Claude's discretion per CONTEXT.md)
- **Design notes in module overviews:** Keep overviews to purpose only (per CONTEXT.md)

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Custom documentation checker | Regex/parse XML yourself | Doxygen's WARN_NO_PARAMDOC, WARN_IF_UNDOCUMENTED | Doxygen validates structure, detects inconsistencies, handles C++ syntax correctly |
| Manual API listing | Shell script to find classes | Doxygen XML output | Structured data, includes inheritance info, can be queried/parsed |
| Documentation generator | Custom template system | Doxygen + Doxywizard | Handles cross-references, generates multiple formats, standard tool |
| Module grouping | Manual organization | @defgroup/@ingroup | Automatic hierarchy, proper linking, generated navigation |

**Key insight:** Doxygen has built-in validation and organization features that cover all phase requirements. Custom solutions would reinvent functionality Doxygen already provides robustly.

## Common Pitfalls

### Pitfall 1: Missing File Documentation
**What goes wrong:** Global functions, enums, typedefs don't appear in documentation even if documented
**Why it happens:** Doxygen requires file-level documentation (@file) for global entities when EXTRACT_ALL=NO
**How to avoid:** Add `/** @brief File description */` at top of every header file
**Warning signs:** "documented function not found" warnings, global entities missing from generated docs

### Pitfall 2: Undocumented Parameters
**What goes wrong:** Functions have @brief but parameters are undocumented
**Why it happens:** WARN_NO_PARAMDOC=NO in current config hides these warnings
**How to avoid:** Enable WARN_NO_PARAMDOC=YES temporarily to find missing @param tags
**Warning signs:** Check docs/Doxyfile configuration, run doxygen and count warnings

### Pitfall 3: Brief Not Terminated Correctly
**What goes wrong:** @brief description runs into detailed description
**Why it happens:** Missing blank line or explicit @details tag
**How to avoid:** Always use blank line between @brief and details:
```cpp
/**
 * @brief This is brief
 *
 * This is detailed description.
 */
```
**Warning signs:** Brief text appears in detailed section, output looks odd

### Pitfall 4: Template Parameter Documentation Missing
**What goes wrong:** Template classes/functions have parameter docs only for function params
**Why it happens:** Easy to forget @tparam for template parameters
**How to avoid:** Always add @tparam for each template parameter:
```cpp
/**
 * @tparam T Description of template type parameter
 * @tparam N Description of non-type template parameter
 * @brief Class description
 */
template<typename T, size_t N>
class Container;
```
**Warning signs:** Template documentation looks incomplete in generated output

### Pitfall 5: Inconsistent Comment Style
**What goes wrong:** Mix of /** */ and /// styles, confusing to maintainers
**Why it happens:** Multiple developers, gradual codebase growth
**How to avoid:** Pick one style (Javadoc /** */ is more common for files, /// for members)
**Warning signs:** Code review reveals inconsistency, generated docs have inconsistent formatting

## Code Examples

Verified patterns from official sources:

### Essential Function Documentation
```cpp
// Source: https://www.doxygen.nl/manual/docblocks.html
/**
 * @brief Adds two integers
 * @param a First integer
 * @param b Second integer
 * @return Sum of a and b
 */
int add(int a, int b);
```

### Essential Class Documentation
```cpp
// Source: https://www.doxygen.nl/manual/docblocks.html
/**
 * @brief Represents a 2D point with integer coordinates
 *
 * Provides basic operations like addition and subtraction.
 */
struct Point {
    int x, y; ///< X and Y coordinates

    /**
     * @brief Default constructor
     */
    Point() : x(0), y(0) {}
};
```

### File Documentation
```cpp
// Source: https://www.doxygen.nl/manual/faq.html
/**
 * @file math.hpp
 * @brief Mathematical utility functions
 *
 * Provides basic arithmetic operations for 2D geometry.
 */
```

### Module Grouping
```cpp
// Source: https://www.doxygen.nl/manual/grouping.html
/**
 * @defgroup core Core Types
 * @brief Fundamental data types for the game engine
 *
 * Provides basic structures like Point, Size, Rect used
 * throughout the engine.
 */
```

### Boilerplate for Undocumented API
```cpp
/**
 * @brief [TODO: Describe what this function does]
 * @param input [TODO: Describe parameter]
 * @return [TODO: Describe return value]
 */
int placeholderFunction(int input);
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|----------------|--------------|--------|
| EXTRACT_ALL=YES | EXTRACT_ALL=NO | Current config | Only documented entities appear, easier to track progress |
| WARN_NO_PARAMDOC=NO | Enable for analysis | Phase 9 | Needed to find missing @param tags |
| No module pages | Use @defgroup | Phase 9 | Creates module hierarchy for navigation |
| Manual warnings | Doxygen built-in | All phases | Standardized warning checking, CI integration |

**Deprecated/outdated:**
- No specific deprecated features in current Doxyfile

## Open Questions

1. **Should WARN_NO_PARAMDOC be enabled permanently or just for analysis?**
   - What we know: Currently set to NO to allow temporary gaps
   - What's unclear: Whether to leave enabled post-phase or revert
   - Recommendation: Enable during phase for finding missing @param, then decide based on final quality

2. **How many undocumented APIs exist?**
   - What we know: 46 of 56 files have some documentation
   - What's unclear: How many individual classes/functions lack @brief
   - Recommendation: Run doxygen with full warnings first to get baseline count

3. **Which modules need overview pages vs can skip?**
   - What we know: 10 modules exist (core, graphics, UI, scripting, animation, utils, abstract, compat, components, effects)
   - What's unclear: Which are large/complex enough to warrant overviews
   - Recommendation: Document all 10 modules first, can consolidate/skip based on complexity (Claude's discretion)

4. **Detail level for template parameter documentation?**
   - What we know: Templates exist (Object::addComponent, Canvas4/8 classes)
   - What's unclear: How much detail for @tparam tags
   - Recommendation: Essential level: one-line description of template parameter purpose

## Sources

### Primary (HIGH confidence)
- Doxygen official documentation - docblocks, grouping, FAQ, configuration
  - https://www.doxygen.nl/manual/docblocks.html
  - https://www.doxygen.nl/manual/grouping.html
  - https://www.doxygen.nl/manual/faq.html
  - https://www.doxygen.nl/manual/config.html
- Project Doxyfile configuration
  - /home/unwn/dev/enjin/docs/Doxyfile
- Project header files (analyzed for current documentation state)
  - include/enjin2/core/object.hpp
  - include/enjin2/core/types.hpp
  - include/enjin2/scripting/lua_engine.hpp
  - include/enjin2/graphics/canvas.hpp

### Secondary (MEDIUM confidence)
- Phase 09-CONTEXT.md (user decisions and constraints)
  - .planning/phases/09-documentation-coverage/09-CONTEXT.md

### Tertiary (LOW confidence)
- None - all findings verified with official documentation or project code

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Doxygen is de facto standard for C++ documentation
- Architecture: HIGH - Direct analysis of project structure and Doxygen manual
- Pitfalls: HIGH - All pitfalls documented in official Doxygen FAQ and manuals

**Research date:** 2026-02-03
**Valid until:** 2026-03-05 (30 days - Doxygen stable, documentation requirements stable)
