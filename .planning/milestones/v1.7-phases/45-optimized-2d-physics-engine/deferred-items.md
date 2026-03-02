# Deferred Items — Phase 45

## Out-of-Scope Pre-existing Issues

### sprite_load_test.cpp: missing lua_wrapper.hpp
- **Found during:** Task 1 (full build verification)
- **Issue:** `tests/sprite_load_test.cpp:3` references `../include/enjin2/scripting/lua_wrapper.hpp` which does not exist
- **Status:** Pre-existing issue — not caused by Phase 45 changes
- **Action:** Deferred — do not fix as part of this phase
