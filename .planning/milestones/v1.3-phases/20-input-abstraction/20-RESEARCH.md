# Phase 20: Input Abstraction - Research

**Researched:** 2026-02-24
**Domain:** C++ platform-agnostic input interface, bitmask edge detection, host-only unit tests
**Confidence:** HIGH

## Summary

This phase introduces the input abstraction layer for enjin2: a single header-defined `InputState` struct with inline bitmask methods, a free function `input_advance_frame`, and a declared-but-not-defined `input_platform_poll`. The design is entirely self-contained within `<stdint.h>` and standard C++ — no SDL3, Emscripten, or ESP32 headers cross the boundary.

The implementation is small and well-bounded. The user's decisions in CONTEXT.md leave almost no ambiguity about the public API. The primary research questions are: (1) where does this live in the directory/CMake structure, (2) what exact bitmask arithmetic goes in the inline methods, and (3) how does the host test binary fit into the existing test infrastructure.

All findings below are based on direct inspection of the enjin2 codebase (HIGH confidence). No third-party libraries are involved; this phase is pure C++ data-structure work.

**Primary recommendation:** Create `include/enjin2/input/input_state.hpp` (header-only, inline methods) and `src/input/input.cpp` (definition of `input_advance_frame`). Add a new `enjin2_input` static library to `CMakeLists.txt`. Add `tests/input_test.cpp` following the identical `printf/ASSERT` macro pattern used by `palette_test.cpp`.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Button naming scheme**
- Projects define their own semantic enum (e.g. `BTN_ACTION`, `BTN_BACK`, `AXIS_X`) — core has no knowledge of project enums
- Core uses raw `int` indices internally; project enum values cast to int are the button IDs
- Each project (Eisei, Tomodachi, etc.) has very different hardware — the abstraction must be project-configurable, not a fixed gamepad layout
- Axis values are all normalized to -1.0 to 1.0 regardless of axis type (encoder, pot, touchpad x/y, touchwheel position)

**Struct layout**
- 16 buttons: `uint16_t buttons` bitmask (ceiling for any project)
- 8 float axes: `float axes[8]` (covers Tomodachi's touchpad x/y, encoder, sensors)
- Previous frame stored inside `InputState` itself: `uint16_t prev_buttons` and `float prev_axes[8]`
- `InputState` is self-contained — no external state manager needed for edge detection

**API style**
- `InputState` is a C++ struct with inline methods: `justPressed(int btn)`, `held(int btn)`, `justReleased(int btn)`
- Methods take raw `int` (project enum value) — no core-defined button enum
- Plain struct with no enjin dependencies — only `<stdint.h>` and standard C++ types
- Consistent with core's C++ style and static memory emphasis

**Frame advance cycle**
- Core provides `input_advance_frame(InputState*)`: copies `buttons → prev_buttons`, `axes → prev_axes`, then clears current fields
- After advance, platform writes new state directly into the struct: `state->buttons |= (1 << BTN_A)` etc.
- `InputState` instance lives as a static/global in the platform layer — no heap allocation

**Platform integration contract**
- Core declares `void input_platform_poll(InputState*)` in the interface header — no definition provided
- Each platform provides its own `.cpp` implementing `input_platform_poll` (SDL3 in phase 21, WASM/ESP32 later)
- Phase 20 delivers: declaration only, no platform `.cpp` files

**Verification**
- Unit tests compile and run on the host with no platform SDK — synthetic button/axis injection exercises all three edge-detection methods
- No SDL3 or ESP32 headers required to build the test binary

### Claude's Discretion
- Internal bitmask math implementation for `justPressed`/`held`/`justReleased`
- File naming and directory placement for the input headers
- Whether to use `inline` on struct methods or leave to compiler
- CMake target name and visibility (public vs private headers)

### Deferred Ideas (OUT OF SCOPE)
- Output LED abstraction (Eisei hardware) — future phase
- WASM JS-to-InputState memory bridge convention — phase 22 or WASM-specific phase
- ESP32 GPIO → InputState implementation — phase after SDL3 runner
- Per-project compile-time axis/button count tuning (`#define ENJIN_MAX_BUTTONS`) — noted as possible future optimization, not needed now
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| INP-01 | Platform-agnostic input interface with zero platform types in headers | Header uses only `<stdint.h>` + `<cstring>` (for memcpy/memset). Declaration of `input_platform_poll` requires no platform include. |
| INP-02 | InputState with button bitmask and float analog axes | `uint16_t buttons`, `uint16_t prev_buttons`, `float axes[8]`, `float prev_axes[8]` — all POD, no platform types. |
| INP-03 | Edge detection (justPressed, held, justReleased) in shared layer | Inline methods on the struct using bitmask arithmetic: `(~prev & curr)`, `curr`, `(prev & ~curr)` respectively. |
</phase_requirements>

## Standard Stack

### Core

| Component | Version / Type | Purpose | Why Standard |
|-----------|----------------|---------|--------------|
| `<stdint.h>` / `<cstdint>` | C99 / C++11 | `uint16_t`, `float` fixed-width types | Zero dependency, portable to ESP32/WASM/desktop |
| `<cstring>` | C++ std | `memcpy` / `memset` in `input_advance_frame` | Avoids manual loops, compiler-optimized |
| Inline struct methods | C++11 | Edge detection logic | Consistent with `Palette` / `RGB` struct style in this codebase |
| Static library `enjin2_input` | CMake | Encapsulates `input_advance_frame` implementation | Matches existing `enjin2_core`, `enjin2_graphics`, `enjin2_ui` pattern |

### Supporting

| Component | Purpose | When to Use |
|-----------|---------|-------------|
| `add_test(NAME input_test ...)` in `tests/CMakeLists.txt` | CTest integration | Follow existing `palette_test` registration pattern |
| `printf` + `ASSERT` macro | Test output | Matches existing `palette_test.cpp` — no external test framework installed |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Inline struct methods | Free functions taking `const InputState&` | Inline methods match project style (`RGB::operator==`, etc.) and keep logic next to data |
| `memcpy` in `input_advance_frame` | Manual field assignment | Both are equivalent; `memcpy` is fine for POD structs |
| Separate `input.h` (C header) | C++ header with `#pragma once` | Project uses `.hpp` + `namespace enjin2` throughout — keep consistent |

**Installation:** No new packages required. This is pure C++ with no external dependencies.

## Architecture Patterns

### Recommended Project Structure

```
include/enjin2/input/
└── input_state.hpp        # InputState struct + inline methods + input_platform_poll declaration

src/input/
└── input.cpp              # input_advance_frame definition

tests/
├── CMakeLists.txt         # add input_test executable + add_test (existing file — modify)
└── input_test.cpp         # host-only unit test: synthetic injection, edge detection
```

### Pattern 1: Struct With Inline Methods (project convention)

**What:** `InputState` is a plain C++ struct with `inline` methods. No virtual dispatch. No heap. Follows the `Palette` and `RGB` patterns already in this codebase.

**When to use:** Always — this is the existing project style for value types with behavior.

**Example (mirroring `palette.hpp` style):**

```cpp
// include/enjin2/input/input_state.hpp
#pragma once
#include <stdint.h>

namespace enjin2 {

struct InputState {
    uint16_t buttons;           ///< Current frame button bitmask
    uint16_t prev_buttons;      ///< Previous frame button bitmask
    float    axes[8];           ///< Current frame axes, normalized -1.0 to 1.0
    float    prev_axes[8];      ///< Previous frame axes

    /// @brief True only on the frame the button transitioned from released to pressed
    inline bool justPressed(int btn) const {
        uint16_t mask = static_cast<uint16_t>(1 << btn);
        return !(prev_buttons & mask) && (buttons & mask);
    }

    /// @brief True while the button is held (current frame)
    inline bool held(int btn) const {
        return (buttons & static_cast<uint16_t>(1 << btn)) != 0;
    }

    /// @brief True only on the frame the button transitioned from pressed to released
    inline bool justReleased(int btn) const {
        uint16_t mask = static_cast<uint16_t>(1 << btn);
        return (prev_buttons & mask) && !(buttons & mask);
    }
};

/// @brief Declared by core; each platform provides its own definition
void input_platform_poll(InputState* state);

} // namespace enjin2
```

### Pattern 2: `input_advance_frame` as a Free Function in `.cpp`

**What:** A free function in `src/input/input.cpp` that snapshots current → prev and clears current fields. Platform then calls this once per frame before polling.

**When to use:** Always — frame advance is core logic, not platform logic.

**Example:**

```cpp
// src/input/input.cpp
#include "../../include/enjin2/input/input_state.hpp"
#include <cstring>

namespace enjin2 {

void input_advance_frame(InputState* state) {
    state->prev_buttons = state->buttons;
    memcpy(state->prev_axes, state->axes, sizeof(state->axes));
    state->buttons = 0;
    memset(state->axes, 0, sizeof(state->axes));
}

} // namespace enjin2
```

### Pattern 3: CMake Static Library (`enjin2_input`)

**What:** Follows the exact pattern of `enjin2_core`, `enjin2_graphics`, `enjin2_ui` in the root `CMakeLists.txt`. Public headers under `include/`, sources under `src/`. Private include directory scoping.

**Example addition to `CMakeLists.txt`:**

```cmake
add_library(enjin2_input STATIC)
target_sources(enjin2_input PRIVATE
    src/input/input.cpp
)
target_include_directories(enjin2_input
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Add to the aggregate INTERFACE library
target_link_libraries(enjin2 INTERFACE
    enjin2_core
    enjin2_graphics
    enjin2_ui
    enjin2_input
    $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>
)
```

### Pattern 4: Host-Only Unit Test (`tests/input_test.cpp`)

**What:** Matches `tests/palette_test.cpp` exactly — `printf` messages, custom `ASSERT` macro, `int main()`, returns `failures == 0 ? 0 : 1`. Links against `enjin2` aggregate library. No SDL3 or ESP32 headers needed.

**Key test scenarios to cover:**

1. Fresh `InputState{}` — all methods return `false` (zero-initialized state)
2. Set `buttons = (1 << 0)`, prev = 0 → `justPressed(0)` true, `held(0)` true, `justReleased(0)` false
3. Set `buttons = (1 << 0)`, prev = same → `justPressed(0)` false, `held(0)` true, `justReleased(0)` false
4. Set `buttons = 0`, prev = `(1 << 0)` → `justPressed(0)` false, `held(0)` false, `justReleased(0)` true
5. `input_advance_frame` correctly copies current → prev and zeroes current
6. Axis read: set `axes[2] = 0.5f`, call advance, verify `prev_axes[2] == 0.5f` and `axes[2] == 0.0f`
7. No cross-contamination: button 1 state doesn't affect button 0

**Example:**

```cpp
// tests/input_test.cpp
#include <enjin2/input/input_state.hpp>
#include <cstdio>
using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { passes++; } \
    } while(0)

static void test_zero_state() { /* ... */ }
static void test_just_pressed() { /* ... */ }
// etc.

int main() {
    printf("=== input_test ===\n");
    test_zero_state();
    test_just_pressed();
    /* ... */
    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
```

### Anti-Patterns to Avoid

- **Including SDL3/Emscripten/ESP32 headers in `input_state.hpp`:** This is the core violation of INP-01. Any `SDL_Event`, `SDL_Scancode`, `<Arduino.h>`, or `<emscripten.h>` in the interface header fails the phase.
- **Using `bool` as the bitmask store:** `uint16_t` is the decided type. A `bool buttons[16]` array would leak different alignment behavior and miss the bitmask contract.
- **Defining `input_platform_poll` in the core library:** The declaration belongs in the header, the definition belongs to each platform. Providing a stub definition in `input.cpp` would cause link-time multiple-definition errors when a platform `.cpp` is also compiled.
- **Heap allocation for `InputState`:** The decided pattern is static/global lifetime in the platform layer. No `new InputState()`.
- **Checking `btn >= 16` with an assert in inline methods:** The struct currently has no bounds check in the decided design. Keep it simple — the project style does not add runtime checks in hot-path inline methods (`Pixel4`, `RGB` have none).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Frame-advance copy | Manual field-by-field assignment of 8 floats | `memcpy` + `memset` | More readable, compiler-vectorizable, less typo surface |
| Test framework | Custom assertion library | Project's existing `printf + ASSERT` macro pattern | Already established in `palette_test.cpp`; no external framework is installed |

**Key insight:** This phase is a data-structure problem, not a framework problem. Resist adding complexity. The entire implementation is ~50 lines of C++.

## Common Pitfalls

### Pitfall 1: Platform Type Leakage
**What goes wrong:** A transitive `#include` in `input_state.hpp` pulls in platform headers. E.g., including a project header that includes `<SDL3/SDL.h>`.
**Why it happens:** Easy to accidentally include a "convenient" project header.
**How to avoid:** The header must only include `<stdint.h>` (or `<cstdint>`). Audit the include chain manually.
**Warning signs:** Test binary fails to compile without SDL3 present.

### Pitfall 2: `input_platform_poll` Definition in the Core Library
**What goes wrong:** Providing a stub definition of `input_platform_poll` in `src/input/input.cpp` to make tests link cleanly. This will cause multiple-definition errors in phase 21 when SDL3 also provides a definition.
**Why it happens:** Tests need to link; the temptation is to provide a no-op body.
**How to avoid:** Tests should exercise `input_advance_frame` and direct struct manipulation — they do NOT call `input_platform_poll`. The test binary simply never calls the poll function, so no definition is needed for it to link.
**Warning signs:** `multiple definition of 'enjin2::input_platform_poll'` linker error in phase 21.

### Pitfall 3: Integer Narrowing in Bitmask Shift
**What goes wrong:** `1 << btn` is a signed `int` shift. If `btn` is 15, `1 << 15` is `0x8000` — fine as signed int, but the intermediate result is then cast to `uint16_t`. On platforms where `int` is 16 bits this could be implementation-defined.
**Why it happens:** C++ integer promotion rules.
**How to avoid:** Cast explicitly: `static_cast<uint16_t>(1u << btn)`. Using `1u` (unsigned) avoids signed overflow. The cast to `uint16_t` is safe since values fit.
**Warning signs:** Compiler warning `-Wshift-sign-overflow` or `-Wconversion`.

### Pitfall 4: `input_advance_frame` Clears Current Before Platform Writes
**What goes wrong:** If the order of operations is wrong — e.g., platform writes first, then advance clears — the current frame's input is lost.
**Why it happens:** Misunderstanding of the frame cycle.
**How to avoid:** The decided cycle is: advance (prev = current, clear current) → platform poll (write to current) → game reads current. The `input_advance_frame` call comes first each frame.
**Warning signs:** All buttons read as `justReleased` on the second frame; `justPressed` never fires after frame 0.

### Pitfall 5: `axes` Not Zeroed After Advance (Sticky Axes)
**What goes wrong:** `input_advance_frame` copies axes but forgets to zero `axes[]`. Axes from the previous frame persist as the current value.
**Why it happens:** Forgetting `memset(state->axes, 0, sizeof(state->axes))`.
**How to avoid:** Both `buttons = 0` and `memset(axes)` are required in `input_advance_frame`.
**Warning signs:** Axis test: after an advance with no platform poll, `axes[i]` should be `0.0f`, not the last value.

## Code Examples

### Complete `input_state.hpp`

```cpp
// Source: project pattern — modeled after include/enjin2/graphics/palette.hpp
#pragma once
#include <stdint.h>

namespace enjin2 {

/**
 * @brief Platform-agnostic input state for one frame
 *
 * Holds current and previous frame button bitmask and axis values.
 * Edge-detection methods (justPressed, held, justReleased) compare the two.
 * Call input_advance_frame() once per frame before the platform poll.
 *
 * Axes are normalized to [-1.0, 1.0] regardless of source hardware.
 * Button indices are project-defined; cast your enum value to int.
 */
struct InputState {
    uint16_t buttons;       ///< Current frame: bit N = button N held
    uint16_t prev_buttons;  ///< Previous frame button bitmask

    float axes[8];          ///< Current frame axis values, normalized -1.0 to 1.0
    float prev_axes[8];     ///< Previous frame axis values

    /// @brief True only on the first frame a button is pressed (off→on edge)
    inline bool justPressed(int btn) const {
        uint16_t mask = static_cast<uint16_t>(1u << btn);
        return !(prev_buttons & mask) && (buttons & mask);
    }

    /// @brief True every frame the button is held down
    inline bool held(int btn) const {
        return (buttons & static_cast<uint16_t>(1u << btn)) != 0;
    }

    /// @brief True only on the first frame a button is released (on→off edge)
    inline bool justReleased(int btn) const {
        uint16_t mask = static_cast<uint16_t>(1u << btn);
        return (prev_buttons & mask) && !(buttons & mask);
    }
};

/**
 * @brief Advance the input state by one frame.
 *
 * Snapshots current → prev, then zeroes current fields.
 * Call this at the start of each frame before input_platform_poll().
 *
 * @param state Pointer to the InputState to advance (must not be null)
 */
void input_advance_frame(InputState* state);

/**
 * @brief Platform-provided input poll function.
 *
 * Declared here; each platform provides exactly one definition.
 * Reads hardware or OS input and writes into state->buttons / state->axes.
 * Must be called AFTER input_advance_frame() each frame.
 *
 * @param state Pointer to the InputState to populate
 */
void input_platform_poll(InputState* state);

} // namespace enjin2
```

### Complete `input.cpp`

```cpp
// Source: project pattern — modeled after src/graphics/palette.cpp
#include "../../include/enjin2/input/input_state.hpp"
#include <cstring>

namespace enjin2 {

void input_advance_frame(InputState* state) {
    state->prev_buttons = state->buttons;
    memcpy(state->prev_axes, state->axes, sizeof(state->axes));
    state->buttons = 0;
    memset(state->axes, 0, sizeof(state->axes));
}

} // namespace enjin2
```

### CMake addition to root `CMakeLists.txt`

```cmake
# Input abstraction library
add_library(enjin2_input STATIC)
target_sources(enjin2_input PRIVATE
    src/input/input.cpp
)
target_include_directories(enjin2_input
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Existing aggregate: add enjin2_input to the INTERFACE library
# (Modify the existing target_link_libraries(enjin2 INTERFACE ...) block)
```

### CMake addition to `tests/CMakeLists.txt`

```cmake
add_executable(input_test
    input_test.cpp
)
target_include_directories(input_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(input_test PRIVATE
    enjin2
)
add_test(NAME input_test COMMAND input_test)
```

## State of the Art

| Old Approach | Current Approach | Notes |
|--------------|------------------|-------|
| Input libraries (Gainput, etc.) | Custom `InputState` | Project explicitly chose custom (REQUIREMENTS.md Out of Scope) |
| Virtual `IInputProvider` | Plain struct + free functions | Zero-alloc constraint, embedded targets, C++17 project style |

**No deprecated approaches apply:** This is a greenfield input layer, not a migration.

## Open Questions

1. **Should `enjin2_input` link into the aggregate `enjin2` INTERFACE library, or stay standalone?**
   - What we know: All other subsystems (`enjin2_core`, `enjin2_graphics`, etc.) are wired into the `enjin2` INTERFACE target.
   - What's unclear: Phase 21 needs `enjin2_input` for its SDL3 runner. If it's in the aggregate, every consumer automatically gets it — consistent with the pattern.
   - Recommendation: Add it to the aggregate `enjin2` INTERFACE library. This is consistent with every other subsystem.

2. **Should `input_state.hpp` be under `include/enjin2/input/` or directly `include/enjin2/`?**
   - What we know: Existing headers are organized by subsystem (`graphics/palette.hpp`, `core/types.hpp`, `abstract/icanvas.hpp`).
   - Recommendation: `include/enjin2/input/input_state.hpp` matches the established subdirectory-per-subsystem layout.

3. **Does the test need to avoid calling `input_platform_poll` to link cleanly?**
   - What we know: `input_platform_poll` is declared but never defined in phase 20. Linkers fail at link time on undefined symbols — but only if the symbol is referenced.
   - Recommendation: Test file must NOT call `input_platform_poll`. Tests use direct struct manipulation + `input_advance_frame` only. This is already the intent per CONTEXT.md verification decision.

## Sources

### Primary (HIGH confidence)
- Direct inspection of `/home/unwn/dev/enjin/CMakeLists.txt` — CMake structure, library pattern, option guards
- Direct inspection of `/home/unwn/dev/enjin/include/enjin2/graphics/palette.hpp` — struct + inline method style
- Direct inspection of `/home/unwn/dev/enjin/src/graphics/palette.cpp` — `.cpp` implementation pattern, include path style
- Direct inspection of `/home/unwn/dev/enjin/tests/palette_test.cpp` — test framework: printf + ASSERT macro + main
- Direct inspection of `/home/unwn/dev/enjin/tests/CMakeLists.txt` — add_executable + add_test pattern
- Direct inspection of `/home/unwn/dev/enjin/.planning/phases/20-input-abstraction/20-CONTEXT.md` — locked user decisions
- Direct inspection of `/home/unwn/dev/enjin/.planning/REQUIREMENTS.md` — INP-01, INP-02, INP-03 definitions

### Secondary (MEDIUM confidence)
- C++ standard bitmask arithmetic (`~`, `&`, `|`, `<<`) — standard knowledge, no external source needed

### Tertiary (LOW confidence)
- None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — entire stack is project-internal C++ with no external libraries
- Architecture: HIGH — derived directly from existing patterns in the codebase
- Pitfalls: HIGH — bitmask arithmetic and platform isolation pitfalls are standard C++ knowledge verified against project conventions

**Research date:** 2026-02-24
**Valid until:** 2026-04-24 (stable domain — pure C++ data structure work, no external APIs)
