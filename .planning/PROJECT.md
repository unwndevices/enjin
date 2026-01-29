# Enjin Migration

## What This Is

Complete migration from enjin to enjin2 - making enjin2 fully self-contained with Lua/WASM integration, non-dynamic memory allocation, and clean intelligent structure. Currently enjin2 exists in a separate directory but heavily depends on enjin1 for infrastructure, utilities, and feature code. The goal is to eliminate all enjin1 references so enjin1 can be deleted.

## Core Value

enjin2 works independently without any enjin1 dependencies.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] Move core infrastructure from enjin1 to enjin2 (base types, structs, fundamental abstractions)
- [ ] Move utilities/helpers from enjin1 to enjin2
- [ ] Move feature code from enjin1 to enjin2
- [ ] Remove all enjin1 references from enjin2
- [ ] Verify basic execution works in enjin2
- [ ] Delete enjin1 directory

### Out of Scope

- [Keeping enjin1] — Target is enjin2-only
- [Features not already in enjin2] — Focus on migration, not new features

## Context

Two libraries exist in separate directories:
- enjin1: Original implementation, fully functional
- enjin2: New implementation with Lua/WASM integration and non-dynamic memory allocation, but depends on enjin1 for core infrastructure, utilities, and feature code

The codebase has tight coupling across all three categories. Migration is needed to achieve clean separation.

## Constraints

- **Structure**: Clean and intelligent organization, no fuss
- **Validation**: Manual testing (no automated test suite)
- **Outcome**: Only enjin2 directory remains

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Fully independent enjin2 | User wants to keep only enjin2 in the end | — Pending |

---
*Last updated: 2026-01-29 after initialization*
