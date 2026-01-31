# Phase 6: Create library docs, using doxygen + Docusaurus - Research

**Researched:** 2026-01-31
**Domain:** C++ documentation using Doxygen XML + Docusaurus static site generator
**Confidence:** MEDIUM

## Summary

This phase involves setting up a modern documentation pipeline for the enjin2 C++ library using Doxygen for API extraction and Docusaurus for a polished web documentation site. The research identified that the codebase already contains extensive Doxygen-style comments (3031 lines across 50+ header files), making it well-suited for automated documentation generation.

The standard approach is to use Doxygen to generate XML output from the C++ source code, then consume this XML with Docusaurus plugins or custom scripts to produce a modern, searchable documentation website. Key requirements include configuring Doxygen for XML generation, setting up a Docusaurus site with appropriate plugins, and automating the build pipeline with CMake and CI/CD.

**Primary recommendation:** Use Doxygen 1.15+ for XML generation combined with Docusaurus 3.9+ using community plugins for doxygen XML consumption. Deploy to GitHub Pages with automated CI/CD workflow.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Doxygen | 1.15+ | Generate XML from C++ comments | Industry standard for C++ documentation, supports XML output, widely compatible |
| Docusaurus | 3.9+ | Modern static site generator | React-based, excellent DX, great documentation features, SEO-friendly |
| Node.js | 18+ | JavaScript runtime for Docusaurus | Required for Docusaurus build system |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| @docusaurus/plugin-content-docs | 3.9+ | Core docs plugin | Always needed for documentation content |
| doxygen-docusaurus-plugin | Community | Parse doxygen XML for Docusaurus | When XML integration needed |
| CMake | 3.16+ | Build system automation | Already in use, add doc generation target |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Doxygen XML | Doxygen HTML | XML is more machine-readable for Docusaurus consumption |
| Docusaurus | MkDocs, VitePress | Docusaurus has better React ecosystem and plugin support |
| GitHub Pages | Netlify, Vercel | GitHub Pages is free and integrated with repo |

**Installation:**
```bash
# Doxygen (already installed: 1.15.0)
sudo apt-get install doxygen graphviz  # for diagrams

# Docusaurus site setup
npx create-docusaurus@latest docs-site classic

# Optional: doxygen XML parser plugin
npm install --save-dev doxygen-docusaurus-plugin
```

## Architecture Patterns

### Recommended Project Structure
```
docs/
├── docusaurus.config.js       # Docusaurus configuration
├── Doxyfile                  # Doxygen configuration
├── static/                   # Static assets (images, etc.)
├── src/
│   ├── api/                  # Auto-generated from doxygen XML
│   │   ├── classes/
│   │   ├── files/
│   │   └── namespaces/
│   ├── guides/                # Hand-written user guides
│   ├── examples/              # Code examples
│   └── intro.md              # Landing page
└── xml/                     # Generated doxygen XML (gitignored)
```

### Pattern 1: Doxygen XML Generation

**What:** Generate structured XML from C++ code comments

**When to use:** For API reference documentation automation

**Example:**
```cmake
# CMakeLists.txt - add custom target for docs
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/docs)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
    )
endif()
```

**Doxyfile configuration:**
```makefile
PROJECT_NAME           = "enjin2"
PROJECT_NUMBER         = "2.0.0"
OUTPUT_DIRECTORY       = docs/xml
GENERATE_XML          = YES
GENERATE_HTML         = NO
INPUT                 = include
FILE_PATTERNS         = *.hpp
RECURSIVE             = YES
EXTRACT_ALL            = YES
HAVE_DOT              = YES
CALL_GRAPH            = YES
CALLER_GRAPH          = YES
```

### Pattern 2: Docusaurus XML Consumption

**What:** Parse doxygen XML and render as Docusaurus pages

**When to use:** When automated API docs are needed alongside narrative docs

**Example:**
```javascript
// docusaurus.config.js
const config = {
  title: 'enjin2',
  tagline: 'A lightweight C++ game engine',
  url: 'https://unwndevices.github.io/enjin',

  plugins: [
    [
      'doxygen-docusaurus-plugin',
      {
        doxyfilePath: '../Doxyfile',
        docsPath: 'api',
        // Additional configuration options
      },
    ],
  ],

  themeConfig: {
    navbar: {
      items: [
        {to: '/docs/intro', label: 'Introduction'},
        {to: '/api', label: 'API Reference'},
      ],
    },
  },
};

module.exports = config;
```

### Pattern 3: Hybrid Documentation

**What:** Combine auto-generated API docs with hand-written guides

**When to use:** For comprehensive documentation (tutorial + reference)

**Structure:**
```
src/
├── api/                    # Auto-generated (read-only)
│   ├── classes/
│   │   ├── Object.md
│   │   ├── Scene.md
│   │   └── ...
│   └── files/
└── guides/                 # Hand-written (versioned)
    ├── getting-started.md
    ├── architecture.md
    └── components.md
```

### Anti-Patterns to Avoid

- **Manual doc maintenance:** Don't manually update API docs - use doxygen automation
- **Ignoring existing comments:** Don't rewrite documentation - leverage existing 3031 comment lines
- **Over-complicating XML parsing:** Don't build custom XML parsers from scratch
- **Skipping versioning:** Don't ignore versioning for API docs that change

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| XML parsing | Custom regex/XML parser | xml2js, fast-xml-parser | Edge cases, C++ templates, namespaces |
| Doc generation | Custom script | Doxygen with templates | Handles all C++ constructs |
| Search functionality | Custom search | Docusaurus search plugin | Algolia integration, better UX |
| Code syntax highlighting | Custom highlighting | Prism.js (Docusaurus default) | Better language support, themes |
| Navigation | Custom sidebar | Docusaurus docs plugin | Auto-generated, collapsible, mobile-friendly |

**Key insight:** Doxygen's XML output is complex with C++--specific constructs (templates, overloads, namespaces). Building a robust parser from scratch would take weeks and likely miss edge cases. Community plugins have already solved these problems.

## Common Pitfalls

### Pitfall 1: Doxygen XML Not Consumed by Docusaurus

**What goes wrong:** Doxygen generates XML, but Docusaurus can't find or parse it

**Why it happens:** Incorrect paths in plugin configuration or wrong XML output format

**How to avoid:**
1. Verify Doxyfile has `GENERATE_XML=YES` and correct `OUTPUT_DIRECTORY`
2. Check plugin configuration points to the correct XML directory
3. Test XML generation locally with `doxygen Doxyfile` first
4. Verify XML structure before integrating with Docusaurus

**Warning signs:** Docusaurus build succeeds but API pages are missing 404 errors

### Pitfall 2: Documentation Inconsistent with Code

**What goes wrong:** Generated docs don't match actual API behavior

**Why it happens:** Outdated comments or comments don't match code structure

**How to avoid:**
1. Set up CI/CD to fail on undocumented APIs (`WARN_IF_UNDOCUMENTED=YES`)
2. Use `doxygen -g` to regenerate Doxyfile from scratch occasionally
3. Enable `WARN_AS_ERROR` in CI but not locally
4. Review doxygen warnings regularly

**Warning signs:** Many doxygen warnings in build output, broken links in docs

### Pitfall 3: Template and Overload Documentation Issues

**What goes wrong:** C++ templates, overloads, and special members documented incorrectly

**Why it happens:** Doxygen's template handling is complex, comments placed incorrectly

**How to avoid:**
1. Use `\tparam` for template parameters (not `\param`)
2. Document overloads with `\overload` command
3. Use `\name` for special members if needed
4. Test documentation for templated classes first

**Warning signs:** Templates show with `<T>` not actual type, overloads missing

### Pitfall 4: Large XML Files Cause Slow Builds

**What goes wrong:** XML generation takes too long, Docusaurus builds are slow

**Why it happens:** `EXTRACT_ALL=YES` on large codebase, too many files

**How to avoid:**
1. Use `EXTRACT_ALL=NO` and document only public APIs
2. Exclude test files and internal headers with `EXCLUDE_PATTERNS`
3. Use `CREATE_SUBDIRS=YES` to spread XML across directories
4. Consider incremental builds with file watching

**Warning signs:** Doxygen takes >5 minutes, Docusaurus build >2 minutes

### Pitfall 5: Deployment to GitHub Pages Fails

**What goes wrong:** Docs build locally but fail when deploying to GitHub Pages

**Why it happens:** Incorrect branch settings, paths not matching, subdirectory issues

**How to avoid:**
1. Configure `docusaurus.config.js` with correct `url` and `baseUrl`
2. Set GitHub Pages source to `/docs` folder or `gh-pages` branch
3. Use proper absolute URLs for assets and links
4. Test deployment with `docusaurus deploy` command locally

**Warning signs:** 404 errors after deployment, broken images, wrong paths

## Code Examples

Verified patterns from official sources:

### Doxygen Configuration for XML Output

```makefile
# Doxyfile - minimal XML generation config
# Source: https://www.doxygen.nl/manual/config.html

PROJECT_NAME           = "enjin2"
PROJECT_NUMBER         = "2.0.0"
PROJECT_BRIEF          = "Lightweight C++ game engine"

# Output settings
OUTPUT_DIRECTORY       = docs/xml
GENERATE_XML          = YES
GENERATE_HTML         = NO
XML_OUTPUT             = xml

# Input settings
INPUT                  = include
FILE_PATTERNS         = *.hpp
RECURSIVE             = YES
EXTRACT_ALL            = NO  # Only document what has comments
WARN_IF_UNDOCUMENTED   = YES

# C++ specific
OPTIMIZE_OUTPUT_JAVA   = NO  # Use C++ settings
BUILTIN_STL_SUPPORT    = YES  # Better std:: documentation

# Graphs
HAVE_DOT              = YES
CALL_GRAPH            = YES
CALLER_GRAPH          = YES
CLASS_GRAPH           = YES
```

### CMake Integration for Documentation Target

```cmake
# CMakeLists.txt - documentation build integration
# Source: Standard CMake documentation patterns

# Find doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
    # Configure doxyfile with version and paths
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/docs)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        BYPRODUCTS ${DOXYGEN_OUT}/xml/index.xml
    )

    # Make docs depend on headers
    add_dependencies(docs enjin2_core enjin2_graphics enjin2_ui)
endif()
```

### Docusaurus Configuration for API Docs

```javascript
// docusaurus.config.js
// Source: https://docusaurus.io/docs/configuration

const config = {
  title: 'enjin2 Documentation',
  tagline: 'Lightweight C++ game engine library',
  url: 'https://unwndevices.github.io/enjin',
  baseUrl: '/enjin/',

  presets: [
    [
      'classic',
      {
        docs: {
          path: 'src',
          sidebarPath: 'sidebars.js',
        },
        blog: false,
        theme: {
          customCss: ['src/css/custom.css'],
        },
      },
    ],
  ],

  plugins: [
    [
      '@docusaurus/plugin-content-docs',
      {
        id: 'api',
        path: 'api',
        routeBasePath: 'api',
        sidebarPath: 'api-sidebar.js',
        editUrl: null,  // Auto-generated, not editable
      },
    ],
  ],

  themeConfig: {
    navbar: {
      title: 'enjin2',
      items: [
        {to: '/docs/intro', label: 'Guides'},
        {to: '/api', label: 'API Reference', activeBaseRegex: '^/api'},
        {href: 'https://github.com/unwndevices/enjin', label: 'GitHub'},
      ],
    },
  },
};

module.exports = config;
```

### GitHub Actions Workflow for Docs Deployment

```yaml
# .github/workflows/docs.yml
# Source: Docusaurus deployment documentation

name: Deploy Documentation

on:
  push:
    branches: [main]
  pull_request:

jobs:
  deploy:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v3

      - name: Setup Node.js
        uses: actions/setup-node@v3
        with:
          node-version: 18

      - name: Install dependencies
        run: cd docs && npm ci

      - name: Build doxygen XML
        run: |
          sudo apt-get install -y doxygen graphviz
          doxygen docs/Doxyfile

      - name: Build Docusaurus site
        run: cd docs && npm run build

      - name: Deploy to GitHub Pages
        uses: peaceiris/actions-gh-pages@v3
        if: github.ref == 'refs/heads/main'
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: ./docs/build
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Doxygen HTML only | Doxygen XML + Docusaurus | ~2020+ | Better UX, modern design, search |
| Static HTML | React-based SPA | ~2019+ | Fast navigation, interactive features |
| Manual deployment | CI/CD automation | ~2018+ | Consistent updates, less manual work |

**Deprecated/outdated:**
- **Doxygen HTML output only:** XML is better for integration with modern site generators
- **Jekyll for C++ docs:** Not optimized for C++ APIs, requires heavy customization
- **Manual doc maintenance:** Automation eliminates drift between code and docs

## Open Questions

1. **Doxygen XML Parser Plugin Quality**
   - What we know: Several community plugins exist (doxygen-docusaurus-plugin, @cmfcmf/doxygen-docusaurus)
   - What's unclear: Which plugins are actively maintained and work with Docusaurus 3.9+
   - Recommendation: Test 2-3 plugins, evaluate compatibility and features, fallback to custom XML parsing if needed

2. **Code Coverage for Documentation**
   - What we know: 50 header files with 3031 comment lines
   - What's unclear: Percentage of public APIs actually documented
   - Recommendation: Run doxygen with `WARN_IF_UNDOCUMENTED=YES` to identify gaps, prioritize documenting core APIs first

3. **Graph Generation Performance**
   - What we know: Doxygen can generate call graphs and inheritance diagrams with Graphviz
   - What's unclear: Impact on build time for the enjin2 codebase
   - Recommendation: Enable graphs selectively, measure build time, disable if too slow

4. **Deployment Frequency**
   - What we know: GitHub Pages can be automated with GitHub Actions
   - What's unclear: Desired deployment frequency (on every push? on releases?)
   - Recommendation: Deploy on main branch pushes, consider release-based deployment for stable docs

## Sources

### Primary (HIGH confidence)
- Doxygen Official Documentation - Configuration options (https://www.doxygen.nl/manual/config.html)
- Doxygen Official Documentation - XML Commands (https://www.doxygen.nl/manual/xmlcmds.html)
- Doxygen Official Documentation - Special Commands (https://www.doxygen.nl/manual/commands.html)
- Docusaurus Official Documentation - Configuration (https://docusaurus.io/docs/configuration)
- Docusaurus Official Documentation - Markdown Features (https://docusaurus.io/docs/markdown-features)
- Local codebase analysis - 50 header files with doxygen comments (3031 lines)

### Secondary (MEDIUM confidence)
- Doxygen installation verification (system: doxygen 1.15.0)
- CMakeLists.txt structure analysis (existing build system)
- Git repository configuration (git@github.com:unwndevices/enjin.git)

### Tertiary (LOW confidence)
- Community doxygen-docusaurus plugins (need verification of compatibility with Docusaurus 3.9+)
- Best practices for C++ documentation structure (requires research into similar projects)

## Metadata

**Confidence breakdown:**
- Standard stack: MEDIUM - Doxygen and Docusaurus are well-established, but XML integration plugins need verification
- Architecture: MEDIUM - Patterns are standard, but specific plugin integration needs testing
- Pitfalls: HIGH - Based on official documentation and common issues reported

**Research date:** 2026-01-31
**Valid until:** 2026-03-01 (60 days - plugin ecosystem evolves quickly)
