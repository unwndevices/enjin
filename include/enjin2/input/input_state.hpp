#pragma once
#include <stdint.h>

namespace enjin2 {

/**
 * @brief Platform-agnostic input state for one frame.
 *
 * Holds current and previous frame button bitmask and axis values.
 * Edge-detection methods compare the two frames.
 * Call input_advance_frame() once per frame BEFORE the platform poll.
 *
 * Axes are normalized to [-1.0, 1.0] regardless of source hardware.
 * Button indices are project-defined; cast your enum value to int.
 * Maximum 16 buttons (indices 0-15), 8 float axes (indices 0-7).
 */
struct InputState {
    uint16_t buttons;       ///< Current frame: bit N = button N held
    uint16_t prev_buttons;  ///< Previous frame button bitmask

    float axes[8];          ///< Current frame axis values, normalized -1.0 to 1.0
    float prev_axes[8];     ///< Previous frame axis values

    /// @brief True only on the first frame a button transitions released->pressed
    inline bool justPressed(int btn) const {
        uint16_t mask = static_cast<uint16_t>(1u << btn);
        return !(prev_buttons & mask) && (buttons & mask);
    }

    /// @brief True every frame the button is held down
    inline bool held(int btn) const {
        return (buttons & static_cast<uint16_t>(1u << btn)) != 0;
    }

    /// @brief True only on the first frame a button transitions pressed->released
    inline bool justReleased(int btn) const {
        uint16_t mask = static_cast<uint16_t>(1u << btn);
        return (prev_buttons & mask) && !(buttons & mask);
    }
};

/**
 * @brief Advance the input state by one frame.
 *
 * Snapshots current -> prev, then zeroes current fields.
 * Call at the start of each frame before input_platform_poll().
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
