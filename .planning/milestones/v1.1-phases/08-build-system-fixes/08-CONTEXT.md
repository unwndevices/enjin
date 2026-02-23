# Phase 8: Build System Fixes - Context

**Gathered:** 2026-02-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix build system issues so users can build enjin2 successfully. Make Lua dependency optional for builds that don't need it (e.g., docs deployment). Document all dependencies in README so users know what's required.

</domain>

<decisions>
## Implementation Decisions

### Lua dependency handling
- Make Lua compilation optional via CMake option `USE_LUA`
- When `USE_LUA=OFF`, Lua-dependent code is excluded from build
- CI/CD: Only the docs deployment job uses `USE_LUA=OFF`; other CI jobs unchanged
- Clear error message when Lua is missing and required: includes generic link to Lua.org for install instructions
- Only Lua gets special error handling; other dependencies use standard CMake errors

### Dependencies documentation
- Document all dependencies in README (no separate DEPENDENCIES.md file)
- Categorize dependencies as Required vs Optional
- Only document external/third-party dependencies (Adafruit-GFX, stb_image, Lua)
- Use simple bulleted list format
- Mention the `USE_LUA` option and its purpose

### Claude's Discretion
- Default value for `USE_LUA` CMake option (likely ON for full builds)
- Level of detail for Lua documentation in README (brief mention with CMake option or full details)
- Error timing when Lua required but missing (fail during configure vs build phase)

</decisions>

<specifics>
## Specific Ideas

- Fix is targeted at CI/CD docs deployment failing due to missing Lua
- Docs pipeline (Docusaurus/Doxygen) doesn't need Lua compilation, so make it optional
- Keep full Lua support for regular builds - this is about flexibility, not removing features

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 08-build-system-fixes*
*Context gathered: 2026-02-02*
