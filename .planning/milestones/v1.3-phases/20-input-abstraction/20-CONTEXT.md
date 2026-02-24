# Phase 20: Input Abstraction - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

A platform-agnostic input interface: headers, `InputState` struct, button bitmask, float axes, and edge detection (`justPressed`, `held`, `justReleased`) that compiles cleanly on ESP32, WASM, and SDL3 with zero platform type leakage. Platform implementations (SDL3 in phase 21, WASM/ESP32 later) are out of scope — only the interface and core logic are delivered here.

</domain>

<decisions>
## Implementation Decisions

### Button naming scheme
- Projects define their own semantic enum (e.g. `BTN_ACTION`, `BTN_BACK`, `AXIS_X`) — core has no knowledge of project enums
- Core uses raw `int` indices internally; project enum values cast to int are the button IDs
- Each project (Eisei, Tomodachi, etc.) has very different hardware — the abstraction must be project-configurable, not a fixed gamepad layout
- Axis values are all normalized to -1.0 to 1.0 regardless of axis type (encoder, pot, touchpad x/y, touchwheel position)

### Struct layout
- 16 buttons: `uint16_t buttons` bitmask (ceiling for any project)
- 8 float axes: `float axes[8]` (covers Tomodachi's touchpad x/y, encoder, sensors)
- Previous frame stored inside `InputState` itself: `uint16_t prev_buttons` and `float prev_axes[8]`
- `InputState` is self-contained — no external state manager needed for edge detection

### API style
- `InputState` is a C++ struct with inline methods: `justPressed(int btn)`, `held(int btn)`, `justReleased(int btn)`
- Methods take raw `int` (project enum value) — no core-defined button enum
- Plain struct with no enjin dependencies — only `<stdint.h>` and standard C++ types
- Consistent with core's C++ style and static memory emphasis

### Frame advance cycle
- Core provides `input_advance_frame(InputState*)`: copies `buttons → prev_buttons`, `axes → prev_axes`, then clears current fields
- After advance, platform writes new state directly into the struct: `state->buttons |= (1 << BTN_A)` etc.
- `InputState` instance lives as a static/global in the platform layer — no heap allocation

### Platform integration contract
- Core declares `void input_platform_poll(InputState*)` in the interface header — no definition provided
- Each platform provides its own `.cpp` implementing `input_platform_poll` (SDL3 in phase 21, WASM/ESP32 later)
- Phase 20 delivers: declaration only, no platform `.cpp` files

### Verification
- Unit tests compile and run on the host with no platform SDK — synthetic button/axis injection exercises all three edge-detection methods
- No SDL3 or ESP32 headers required to build the test binary

### Claude's Discretion
- Internal bitmask math implementation for `justPressed`/`held`/`justReleased`
- File naming and directory placement for the input headers
- Whether to use `inline` on struct methods or leave to compiler
- CMake target name and visibility (public vs private headers)

</decisions>

<specifics>
## Specific Ideas

- Projects have radically different hardware: Eisei has buttons + touchwheel + encoder + pots + output LEDs; Tomodachi has buttons + encoder + touchpad + many sensors — the abstraction must be generic enough to cover both via project-defined enums
- Output LEDs are explicitly out of scope — they belong in a hardware output abstraction in a future phase
- WASM sensor simulation via JS: deferred entirely, no convention needed in phase 20

</specifics>

<deferred>
## Deferred Ideas

- Output LED abstraction (Eisei hardware) — future phase
- WASM JS-to-InputState memory bridge convention — phase 22 or WASM-specific phase
- ESP32 GPIO → InputState implementation — phase after SDL3 runner
- Per-project compile-time axis/button count tuning (`#define ENJIN_MAX_BUTTONS`) — noted as possible future optimization, not needed now

</deferred>

---

*Phase: 20-input-abstraction*
*Context gathered: 2026-02-24*
